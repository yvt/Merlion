using System;
using System.Net.Sockets;
using System.Threading;
using System.Collections.Generic;
using System.Linq;
using log4net;

namespace Merlion.Server
{
	sealed partial class MasterServer
	{

		readonly Dictionary<long, Node> nodes = 
			new Dictionary<long, Node>();
		long nextNodeId = 1;

		long AllocateNodeId()
		{
			lock (nodes) {
				return nextNodeId++;
			}
		}

		interface INodeHandler
		{
			string FullName { get; }
			void Handle();
		}

		sealed class NodeListener
		{
			readonly static ILog log = LogManager.GetLogger(typeof(NodeListener));

			readonly MasterServer master;
			TcpListener listener;

			public event EventHandler<EventArgs> StartListening;

			public NodeListener(MasterServer master)
			{
				this.master = master;
			}

			public void Start() 
			{
				listener = new TcpListener (AppConfiguration.MasterServerAddress);
				listener.Start ();

				log.InfoFormat ("Listening at {0}", listener.LocalEndpoint);

				var l = StartListening;
				if (l != null) {
					l (this, EventArgs.Empty);
				}

				while (true) {
					var client = listener.AcceptTcpClient ();
					new NodeConnection (client, master);
				}
			}
		}

		sealed class NodeConnection
		{
			readonly static ILog log = LogManager.GetLogger(typeof(NodeConnection));

			public readonly System.Net.EndPoint RemoteEndPoint;

			public readonly MasterServer Master;
			public readonly TcpClient Client;
			public readonly NetworkStream Stream;
			public readonly System.IO.BufferedStream OutBufferedStream;
			public readonly System.IO.BufferedStream InBufferedStream;

			public readonly System.IO.BinaryReader Reader;
			public readonly System.IO.BinaryWriter Writer;

			public readonly Thread HandlerThread;

			public event EventHandler Closing;
			public event EventHandler Closed;

			volatile int isOpen = 1;

			public NodeConnection(TcpClient client, MasterServer master)
			{
				Master = master;
				Client = client;
				RemoteEndPoint = Client.Client.RemoteEndPoint;
				Stream = client.GetStream();
				Stream.ReadTimeout = 10000;
				Stream.WriteTimeout = 10000;

				InBufferedStream = new System.IO.BufferedStream(Stream);
				OutBufferedStream = new System.IO.BufferedStream(Stream);
				Reader = new System.IO.BinaryReader(InBufferedStream, new System.Text.UTF8Encoding());
				Writer = new System.IO.BinaryWriter(OutBufferedStream, new System.Text.UTF8Encoding());

				HandlerThread = new Thread(Handle);
				HandlerThread.Name = "Master: Node Handler";
				HandlerThread.Start();
			}

			void OnClosing()
			{
				var c = Closing;
				if (c != null)
					c (this, EventArgs.Empty);
			}

			void OnClosed()
			{
				var c = Closed;
				if (c != null)
					c (this, EventArgs.Empty);
			}

			public void Shutdown()
			{
				int isOpenVal = Interlocked.Exchange (ref isOpen, 0);
				if (isOpenVal != 0) {
					OnClosing ();
					Stream.Close ();
					Client.Close ();
					OnClosed ();
				}
			}

			void Handle()
			{
				INodeHandler handler = null;
				try {
					var magic = Reader.ReadUInt32 ();

					if (magic == NodeProtocol.VersionDownloadRequestMagic) {
						// Node requests to download a version.
						handler = new VersionDownloadHandler (this);
					} else if (magic == NodeProtocol.ControlStreamHeaderMagic) {
						handler = new Node (this);
					} else if (magic == NodeProtocol.DataStreamMagic) {
						handler = new NodeClientDataConnectionHandler (this);
					} else {
						throw new InvalidOperationException ("Invalid header magic.");
					}
					handler.Handle();
				} catch (Exception ex) {
					string name = "(Unnamed)";
					try {
						if (handler != null) {
							name = handler.FullName;
						}
					} catch (Exception) {}
					log.Error(string.Format("{0}: Disconnected.", name), ex);
					Shutdown ();
				}
			}
		}

		sealed class Node: INodeHandler
		{
			readonly static ILog log = LogManager.GetLogger(typeof(Node));

			long nodeId = 0;

			readonly MasterServer master;
			readonly NodeConnection connection;

			readonly System.IO.Stream stream;
			readonly System.IO.BinaryReader br;
			readonly System.IO.BinaryWriter bw;

			public readonly DateTime StartTime = DateTime.UtcNow;

			readonly HashSet<string> loadedVersions = new HashSet<string>();
			readonly Dictionary<byte[], string> rooms = new Dictionary<byte[], string> (Merlion.Utils.ByteArrayComparer.Instance);
			readonly HashSet<long> clients = new HashSet<long> ();

			readonly Dictionary<string, long> versionNumClients = new Dictionary<string, long>();
			readonly Dictionary<string, long> versionNumRooms = new Dictionary<string, long>(); 


			NodeInfo info;

			public Node(NodeConnection connection)
			{
				this.connection = connection;
				master = connection.Master;
				stream = connection.Stream;
				br = connection.Reader;
				bw = connection.Writer;

				connection.Closing += BeforeClose;
			}

			void Shutdown()
			{
				connection.Shutdown ();
			}

			void BeforeClose(object sender, EventArgs e)
			{
				long[] cliIds;
				lock (clients) {
					cliIds = clients.ToArray();
					clients.Clear ();
				}
				foreach (var id in cliIds) {
					var cl = master.GetClientConnection (id);
					if (cl != null) {
						cl.NodeServerWasDown ();
					}
				}
				lock (master.nodes) {
					master.nodes.Remove (nodeId);
				}
			}

			public long NodeId
			{
				get { return nodeId; }
			}

			public NodeInfo NodeInfo
			{
				get { return info; }
			}

			public string Name
			{
				get {
					var info = NodeInfo;
					if (info != null)
						return info.Name;
					else
						return "(unknown)";
				}
			}

			public TimeSpan UpTime
			{
				get {
					return DateTime.UtcNow - StartTime;
				}
			}

			public string FullName
			{
				get {
					var name = Name;
					var ep = connection.RemoteEndPoint;
					if (ep == null) {
						return name;
					} else {
						return string.Format ("{0} at {1}", name,
							ep);
					}
				}
			}

			public bool IsVersionLoaded(string name)
			{
				lock (loadedVersions) {
					return loadedVersions.Contains (name);
				}
			}

			public bool HasRoom(byte[] roomId)
			{
				lock (rooms) {
					return rooms.ContainsKey (roomId);
				}
			}

			public long NumRooms
			{
				get {
					lock (rooms)
						return rooms.Count;
				}
			}

			public long NumClients
			{
				get {
					lock (clients)
						return clients.Count;
				}
			}

			public string GetVersionForRoom(byte[] room)
			{
				lock (rooms) {
					string ver;
					rooms.TryGetValue (room, out ver);
					return ver;
				}
			}

			readonly System.IO.MemoryStream logbuffer = new System.IO.MemoryStream();

			static readonly Utils.LoggingEventDataSerializer logFormatter = 
				new Merlion.Server.Utils.LoggingEventDataSerializer ();

			public void Handle()
			{
				log.InfoFormat("{0}: Starting control stream.", FullName);

				var text = br.ReadString ();
				var xmlser = new System.Xml.Serialization.XmlSerializer (typeof(NodeInfo));
				info = (NodeInfo)xmlser.Deserialize (new System.IO.StringReader (text));

				log.InfoFormat("{0}: Got NodeInfo.", FullName);

				// Node starts control stream.
				// Allocate a node id.
				nodeId = master.AllocateNodeId();
				lock (master.nodes) {
					master.nodes.Add(nodeId, this);
				}

				foreach (var ver in master.balancer.GetAllVersions()) {
					log.DebugFormat("{0}: Requesting to load '{0}'.", ver.Name);
					RequestToLoadVersion (ver.Name);
				}
				log.InfoFormat("{0}: Requested loading versions.", FullName);


				while (true) {
					var cmd = (NodeProtocol.MasterCommand)br.ReadByte ();
					switch (cmd) {
					case NodeProtocol.MasterCommand.Nop:
						break;
					case NodeProtocol.MasterCommand.VersionLoaded:
						lock (loadedVersions) {
							loadedVersions.Add (br.ReadString ());
						}
						break;
					case NodeProtocol.MasterCommand.VersionUnloaded:
						lock (loadedVersions) {
							loadedVersions.Remove (br.ReadString ());
						}
						break;
					case NodeProtocol.MasterCommand.RejectClient:
						{
							var clientId = br.ReadInt64 ();
							var cli = master.GetClientConnection (clientId);
							if (cli != null) {
								cli.Rejected ();
							}
						}
						break;
						/*
					case NodeProtocol.MasterCommand.SocketData:
						{
							var clientId = br.ReadInt64 ();
							var numBytes = br.ReadInt32 ();
							var bytes = br.ReadBytes (numBytes);
							var cli = master.GetClientConnection (clientId);
							if (cli != null && bytes.Length > 0) {
								cli.EnqueueSentData (bytes);
							}
						}
						break;
					case NodeProtocol.MasterCommand.SocketReset:
						{
							var clientId = br.ReadInt64 ();
							var cli = master.GetClientConnection (clientId);
							if (cli != null) {
								cli.EnqueueSentData (new byte[] {});
							}
						}
						break;*/
					case NodeProtocol.MasterCommand.BindRoom:
						lock (rooms) {
							var numBytes = br.ReadInt32 ();
							var roomName = br.ReadBytes (numBytes);
							var versionName = br.ReadString ();

							string version;
							if (rooms.TryGetValue(roomName, out version)) {
								rooms.Remove(roomName);
								lock (versionNumRooms)
									--versionNumRooms[version];
							}

							rooms.Add(roomName, versionName);
							lock (versionNumRooms) {
								if (versionNumRooms.ContainsKey(versionName))
									++versionNumRooms[versionName];
								else
									versionNumRooms[versionName] = 1;
							}
						}
						break;
					case NodeProtocol.MasterCommand.UnbindRoom:
						lock (rooms) {
							var numBytes = br.ReadInt32 ();
							var roomName = br.ReadBytes (numBytes);
							string version;

							if (rooms.TryGetValue(roomName, out version)) {
								rooms.Remove(roomName);
								lock (versionNumRooms)
									--versionNumRooms[version];
							}
						}
						break;
					case NodeProtocol.MasterCommand.Log:
						lock (logbuffer) {
							var numBytes = br.ReadInt32 ();
							var bytes = br.ReadBytes(numBytes);
							logbuffer.SetLength(0);
							logbuffer.Position = 0;
							logbuffer.Write(bytes, 0, bytes.Length);
							logbuffer.Position = 0;

							try {
								var evt = logFormatter.Deserialize(logbuffer);
								evt.Properties["Node"] = FullName;
								log.Logger.Log(new log4net.Core.LoggingEvent(evt));
							} catch (Exception ex) {
								log.Warn("Error occured while processing forwarded log.", ex);
							}
						}
						break;
					default:
						throw new InvalidOperationException (string.Format (
							"Node sent unknown command: {0}", cmd));
					}
				}

			}


			public void Heartbeat()
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.Nop);
					bw.Flush ();
					stream.Flush ();
				}
			}

			public void RequestToLoadVersion(string name)
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.LoadVersion);
					bw.Write (name);
					bw.Flush ();
					stream.Flush ();
				}
			}

			public void RequestToUnloadVersion(string name)
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.UnloadVersion);
					bw.Write (name);
					bw.Flush ();
					stream.Flush ();
				}
			}

			public void ClientConnect(long clientId, string version, byte[] room)
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.Connect);
					bw.Write (clientId);
					bw.Write (version);
					bw.Write (room.Length);
					bw.Write (room, 0, room.Length);
					bw.Flush ();
					stream.Flush ();
				}
				lock (clients) {
					clients.Add (clientId);
				}
				lock (versionNumClients) {
					if (versionNumClients.ContainsKey (version))
						++versionNumClients [version];
					else
						versionNumClients [version] = 1;
				}
			}
			public void ClientDisconnected(long clientId, string version)
			{
				lock (clients) {
					clients.Remove (clientId);
				}
				lock (versionNumClients)
					--versionNumClients [version];
			}
			/*
			public void ClientSocketReset(long clientId, string version)
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.SocketReset);
					bw.Write (clientId);
					bw.Flush ();
					stream.Flush ();
				}
				lock (clients) {
					clients.Remove (clientId);
				}
				lock (versionNumClients)
					--versionNumClients [version];
			}

			public void ClientSocketReceived(long clientId, byte[] data, int start, int length)
			{
				lock (bw) {
					bw.Write ((byte)NodeProtocol.NodeCommand.SocketData);
					bw.Write (clientId);
					bw.Write (length);
					bw.Write (data, start, length);
					bw.Flush ();
					stream.Flush ();
				}
			}
			*/
			public void Close()
			{
				Shutdown ();
			}

			public NodeStatus GetNodeStatus()
			{
				var ni = new NodeStatus () {
					ID = nodeId,
					Name = Name,
					HostName = connection.RemoteEndPoint != null ? connection.RemoteEndPoint.ToString() : "",
					UpTime = UpTime,
					NumClients = NumClients,
					NumRooms = NumRooms,
					Throttle = master.balancer.GetNode(Name, true).Throttle,
					Domains = new Dictionary<string, DomainStatus>()
				};

				string[] vers;
				lock (loadedVersions) {
					vers = loadedVersions.ToArray ();
				}

				foreach (var ver in vers)
					ni.Domains.Add (ver, new DomainStatus () { Version = ver });

				lock (versionNumClients)
					foreach (var ver in vers) {
						long count = 0;
						versionNumClients.TryGetValue(ver, out count);

						DomainStatus d;
						if (ni.Domains.TryGetValue (ver, out d))
							d.NumClients = count;
					}
				lock (versionNumRooms)
					foreach (var ver in vers) {
						long count = 0;
						versionNumRooms.TryGetValue(ver, out count);

						DomainStatus d;
						if (ni.Domains.TryGetValue (ver, out d))
							d.NumRooms = count;
					}

				return ni;
			}
		}

		sealed class NodeClientDataConnectionHandler: INodeHandler
		{
			readonly MasterServer master;
			readonly NodeConnection connection;

			readonly System.IO.Stream stream;
			readonly System.IO.BinaryReader br;
			readonly System.IO.BinaryWriter bw;

			public System.IO.Stream DataStream { get; private set; }
			long clientId;

			public NodeClientDataConnectionHandler(NodeConnection connection)
			{
				this.connection = connection;
				master = connection.Master;
				stream = connection.Stream;
				br = connection.Reader;
				bw = connection.Writer;
			}

			public void Handle ()
			{
				this.clientId = br.ReadInt64 ();
				lock (master.clients) {
					ClientConnection client;
					master.clients.TryGetValue (clientId, out client);
					if (client != null) {
						DataStream = stream;
						client.BindNodeDataStream (this);
					} else {
						connection.Shutdown ();
					}
				}
			}

			public string FullName {
				get {
					return "Client #" + clientId.ToString();
				}
			}

		}
	
		sealed class VersionDownloadHandler: INodeHandler
		{
			readonly MasterServer master;
			readonly NodeConnection connection;

			readonly byte[] buffer = new byte[16384];
			readonly System.IO.Stream stream;
			readonly System.IO.BinaryReader br;
			readonly System.IO.BinaryWriter bw;

			public VersionDownloadHandler(NodeConnection connection)
			{
				this.connection = connection;
				master = connection.Master;
				stream = connection.Stream;
				br = connection.Reader;
				bw = connection.Writer;
			}

			public string FullName
			{
				get { 
					return "(Version Download Handler)";
				}
			}

			void Shutdown()
			{
				connection.Shutdown();
			}

			public void Handle()
			{
				var versionName = br.ReadString ();

				if (!master.versionManager.VersionExists(versionName)) {
					// Version doesn't exist.
					bw.Write ((long)0);
					bw.Flush ();
					stream.Flush ();
					Shutdown ();
					return;
				}

				var filePath = master.versionManager.GetPackagePathForVersion (versionName);
				var file = new System.IO.FileInfo (filePath);

				bw.Write ((long)file.Length);

				int readCount;

				using (var fileStream = file.OpenRead()) {
					while ((readCount = fileStream.Read(buffer, 0, buffer.Length)) > 0) {
						bw.Write (buffer, 0, readCount);
					}
				}

				bw.Flush ();
				stream.Flush ();

				Shutdown ();
			}
		}


		public NodeStatus[] GetAllNodeInfos()
		{
			Node[] conns;
			lock (nodes) {
				conns = nodes.Values.ToArray ();
			}
			return conns.Select (conn => conn.GetNodeStatus ()).ToArray();
		}
	}

	public sealed class NodeStatus
	{
		public long ID;
		public string Name;
		public string HostName;
		public TimeSpan UpTime;
		public long NumClients;
		public long NumRooms;
		public Dictionary<string, DomainStatus> Domains;
		public double Throttle;
	}

	public sealed class DomainStatus
	{
		public string Version;
		public long NumClients;
		public long NumRooms;
	}
}

