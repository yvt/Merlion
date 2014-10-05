/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

using System;
using System.Net.Sockets;
using System.IO;
using log4net;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;

namespace Merlion.Server
{

	interface INodeServer
	{
		VersionManager VersionManager { get; }
		void SendVersionLoaded(string name);
		void SendVersionUnloaded(string name);
		void VersionMissing();
		string Name { get; }
	}

	sealed partial class NodeServer: INodeServer
	{
		static readonly ILog log = LogManager.GetLogger(typeof(NodeServer));

		readonly NodeInfo info;
		readonly System.Net.IPEndPoint masterEndpoint;

		readonly VersionManager versions;
		readonly DomainManager domains;

		TcpClient client;
		NetworkStream stream;
		BinaryReader br;
		BinaryWriter bw;

		readonly Dictionary<long, Client> clients =
			new Dictionary<long, Client>();

		public NodeServer(): this(null, null)
		{ }

		public NodeServer (string name, System.Net.IPEndPoint ep)
		{
			if (name == null) {
				name = AppConfiguration.NodeName;
			}
			if (ep == null) {
				ep = AppConfiguration.MasterServerAddress;
			}

			this.masterEndpoint = ep;

			info = new NodeInfo () {
				Name = name
			};

			versions = new VersionManager ();
			domains = new DomainManager (this);
		}

		public VersionManager VersionManager
		{
			get { return versions; }
		}

		public string Name
		{
			get { return info.Name; }
		}

		public override string ToString ()
		{
			return "NodeServer: " + Name;
		}

		public void Run()
		{
			var th = new System.Threading.Thread (DoHeartbeats);
			th.Start ();

			using(log4net.NDC.Push(string.Format("Node:{0}", info.Name)))
				while (true) {
					RunInternal ();

					log.Info ("Waiting for 5secs for reconnection");
					System.Threading.Thread.Sleep (5000);
				}
		}
	
		void DoHeartbeats() {
			while (true) {
				try {
					SendHeartbeat ();
				} catch { }
				System.Threading.Thread.Sleep (3000);
			}
		}

		void SendHeartbeat() {
			var b = bw;
			if (b != null) {
				lock (b) {
					b.Write ((byte)NodeProtocol.MasterCommand.Nop);
					b.Flush ();
					stream.Flush ();
				}
			}
		}

		static readonly Utils.LoggingEventDataSerializer logFormatter = 
			new Merlion.Server.Utils.LoggingEventDataSerializer ();
		readonly MemoryStream logbuffer = new MemoryStream();

		public void SendLog(log4net.Core.LoggingEvent e)
		{
			var b = bw;
			if (b != null) {
				lock (b) {
					logbuffer.SetLength (0);
					logbuffer.Position = 0;

					logFormatter.Serialize (logbuffer, e.GetLoggingEventData());

					byte[] buffer = logbuffer.GetBuffer ();

					b.Write ((byte)NodeProtocol.MasterCommand.Log);
					b.Write ((int)logbuffer.Length);
					b.Write (buffer, 0, (int)logbuffer.Length);

					b.Flush ();
					stream.Flush ();
				}
			}
		}


		public void SendRejectClient(long name)
		{
			var b = bw;
			if (b != null) {
				lock (b) {
					b.Write ((byte)NodeProtocol.MasterCommand.RejectClient);
					b.Write (name);
					b.Flush ();
					stream.Flush ();
				}
			}
		}


		public void SendVersionLoaded(string name)
		{
			var b = bw;
			if (b != null) {
				lock (b) {
					b.Write ((byte)NodeProtocol.MasterCommand.VersionLoaded);
					b.Write (name);
					b.Flush ();
					stream.Flush ();
				}
			}
		}

		public void SendVersionUnloaded(string name)
		{
			var b = bw;
			if (b != null) {
				lock (b) {
					b.Write ((byte)NodeProtocol.MasterCommand.VersionUnloaded);
					b.Write (name);
					b.Flush ();
					stream.Flush ();
				}
			}
		}

		Client GetClient(long clientId)
		{
			lock (clients) {
				Client ret;
				clients.TryGetValue (clientId, out ret);
				return ret;
			}
		}

		readonly object downloadSync = new object();
		Thread downloadThread;

		public void VersionMissing()
		{
			lock (downloadSync) {
				if (downloadThread != null)
					return;
				downloadThread = new Thread (DownloadRunner);
				downloadThread.Name = "Node Version Downloader";
				downloadThread.Start ();
			}
		}

		void DownloadRunner()
		{
			while (true) {
				string version;
				lock (downloadSync) {
					version = domains.GetMissingVersion ();
					if (version == null) {
						downloadThread = null;
						return;
					}
				}

				log.InfoFormat ("Downloading version '{0}'.", version);

				try {
					var cli = new TcpClient();
					cli.SendTimeout = 10000;
					cli.ReceiveTimeout = 10000;
					cli.Connect (masterEndpoint);

					using (var stream = cli.GetStream()) {

						var br = new BinaryReader(stream);
						var bw = new BinaryWriter(stream);

						log.DebugFormat ("Connected to download version '{0}'.", version);

						log.Debug("Sending download header.");
						bw.Write(NodeProtocol.VersionDownloadRequestMagic);
						bw.Write(version);
						bw.Flush();

						var size = br.ReadInt64();
						if (size == 0) {
							log.ErrorFormat("Version '{0}' could not be downloaded.", version);
							domains.UnloadVersion(version);
							continue;
						}

						log.Debug("Transfering...");

						using (var outstream = versions.StartDeployVersion(version)) {
							Utils.StreamUtils.Copy(stream, outstream, size);
						}

						log.InfoFormat ("Deploying version '{0}'.", version);
					}

					versions.EndDeployVersion(version);

					log.InfoFormat ("Deployed version '{0}'.", version);
					domains.VersionDownloaded(version);
				
				} catch (Exception ex) {
					log.Error (string.Format("Error while downloading version '{0}'.", version), ex);
				}
			}
		}

		void RunInternal() {
			var ep = masterEndpoint;
			log.InfoFormat ("Connecting to '{0}' as node '{1}'.", ep, info.Name);

			try {
				client = new TcpClient ();
				client.SendTimeout = 10000;
				client.ReceiveTimeout = 10000;
				client.Connect (ep);

				using (var str = this.stream = client.GetStream()) {

					var inBufferedStream = new System.IO.BufferedStream(stream);
					var outBufferedStream = new System.IO.BufferedStream(stream);

					br = new BinaryReader(inBufferedStream);

					// do not save to this.bw yet; other thread might send command before we send a header.
					var bw = new BinaryWriter(outBufferedStream);

					lock (bw) {
						log.Info("Sending header magic.");
						bw.Write(NodeProtocol.ControlStreamHeaderMagic);

						log.Info("Sending NodeInfo.");
						{
							var xmlser = new System.Xml.Serialization.XmlSerializer(typeof(NodeInfo));
							var sw = new StringWriter();
							xmlser.Serialize(sw, info);
							bw.Write(sw.ToString());
						}

						bw.Flush();
						stream.Flush();
					}
					this.bw = bw;

					while (true) {
						var cmd = (NodeProtocol.NodeCommand)br.ReadByte();
						switch (cmd) {
						case NodeProtocol.NodeCommand.Nop:
							break;
						case NodeProtocol.NodeCommand.LoadVersion:
							{
								var ver = br.ReadString();
								log.InfoFormat("Loading version '{0}' requested.", ver);
								domains.LoadVersion(ver);
							}
							break;
						case NodeProtocol.NodeCommand.UnloadVersion:
							{
								var ver = br.ReadString();
								log.InfoFormat("Unloading version '{0}' requested.", ver);
								domains.UnloadVersion(ver);
							}
							break;
						case NodeProtocol.NodeCommand.Connect:
							{
								var clientId = br.ReadInt64();
								string version = br.ReadString();
								int roomLen = br.ReadInt32();
								var room = br.ReadBytes(roomLen);

								new Task(() => {
									try {
										var domain = domains.GetDomain(version);
										if (domain == null) {
											throw new InvalidOperationException(string.Format(
												"Version '{0}' is not loaded.", version));
										}

										var cli = new Client(this, clientId);
										lock (clients) {
											clients.Add(clientId, cli);
											try {
												domain.Accept(clientId, cli.Stream, room);
											} catch {
												cli.Stream.Close();
												clients.Remove(clientId);
												throw;
											}
										}
									} catch (Exception ex) {
										log.Error(string.Format("Rejecting client '{0}' (version '{1}') because of an error.", 
											clientId, version), ex);
										SendRejectClient(clientId);
									}
								}).Start();
							}
							break;
						
						default:
							throw new System.IO.InvalidDataException(string.Format(
								"Received invalid command '{0}'.", cmd));
						}
					}
				}

			} catch (Exception ex) {
				log.Error (string.Format("Node '{0}' has disconnected.", info.Name), ex);
				client.Close ();
			} finally {
				br = null;
				bw = null;
				stream = null;
				client = null;

				var clis = clients.Values.ToArray ();
				foreach (var cli in clis) {
					cli.Shutdown ();
				}
				clients.Clear ();
			}
		}

		sealed class Client
		{
			readonly NodeServer node;
			public readonly long ClientId;
			public readonly Utils.PipeStream Stream; // Connection to AppDomain
			public TcpClient TcpClient;	 // Connection to MasterServer

			Stream tcpStream;

			Utils.StreamAsyncCopier upStream;
			Utils.StreamAsyncCopier downStream;

			readonly object sync = new object();

			public bool Down
			{
				get {
					lock (sync)
						return down;
				}
			}

			volatile bool down = false;

			public Client(NodeServer node, long clientId)
			{
				this.node = node;
				this.ClientId = clientId;
				Stream = Utils.PipeStream.CreateNew();

				TcpClient = new TcpClient();
				TcpClient.BeginConnect(node.masterEndpoint.Address,
					node.masterEndpoint.Port, ConnectDone, null);

			}

			void ConnectDone(IAsyncResult result)
			{
				try {
					TcpClient.EndConnect(result);

					tcpStream = TcpClient.GetStream ();

					var bw = new System.IO.BinaryWriter(tcpStream);
					bw.Write(NodeProtocol.DataStreamMagic);
					bw.Write(ClientId);
					bw.Flush();

					upStream = new Utils.StreamAsyncCopier(Stream.ReadStream, tcpStream, AppConfiguration.UpstreamBufferSize, true);
					downStream = new Utils.StreamAsyncCopier(tcpStream, Stream.WriteStream, AppConfiguration.DownstreamBufferSize, true);

					upStream.BeginCopy((result2) => {
						try {
							upStream.EndCopy(result2);
						} catch (IOException) { 
						} catch (Exception ex) {
							log.Warn (string.Format("{0}: Upstream failed.", ClientId), ex);
						}
						Shutdown();
					}, null);
					downStream.BeginCopy((result2) => {
						try {
							downStream.EndCopy(result2);
						} catch (IOException) { 
						} catch (Exception ex) {
							log.Warn (string.Format("{0}: Downstream failed.", ClientId), ex);
						}
						Shutdown();
					}, null);

				} catch (Exception ex) {
					log.Warn (string.Format("{0}: Failed to create data stream.", ClientId), ex);
					TcpClient = null;
					Shutdown ();
					return;
				}
			}

			public void Shutdown()
			{
				lock (sync) {
					if (down) {
						return;
					}
					down = true;
				}

				if (TcpClient != null) {
					tcpStream.Close ();
					TcpClient.Close ();
				}
				Stream.Dispose ();


				lock (node.clients) {
					node.clients.Remove (ClientId);
				}

			}
		}

	}
}

