using System;
using System.Net.Sockets;
using System.Net.Security;
using System.Security.Authentication;
using System.Collections.Generic;
using System.Linq;
using log4net;

namespace Merlion.Server
{
	sealed partial class MasterServer
	{
		System.Security.Cryptography.X509Certificates.X509Certificate serverCert;

		readonly Dictionary<long, ClientConnection> clients = 
			new Dictionary<long, ClientConnection>();
		long nextClientId = 1;

		long AllocateClientId()
		{
			lock (clients) {
				var id = nextClientId;
				++nextClientId;
				return id;
			}
		}

		ClientConnection GetClientConnection(long clientId)
		{
			ClientConnection conn;
			lock (clients) {
				clients.TryGetValue (clientId, out conn);
			}
			return conn;
		}

		sealed class ClientListener
		{
			readonly static ILog log = LogManager.GetLogger(typeof(ClientListener));

			readonly MasterServer master;
			TcpListener listener;

			public ClientListener(MasterServer master)
			{
				this.master = master;
			}

			public void Start() 
			{
				listener = new TcpListener (AppConfiguration.EndpointAddress);
				listener.Start ();

				log.InfoFormat ("Listening at {0}", listener.LocalEndpoint);

				while (true) {
					var client = listener.AcceptTcpClient ();
					new ClientConnection (client, master);
				}
			}
		}

		sealed class ClientConnection
		{
			readonly static ILog log = LogManager.GetLogger(typeof(ClientConnection));

			readonly MasterServer master;
			readonly long clientId;
			readonly TcpClient client;
			readonly NetworkStream stream;
			readonly SslStream ssl;
			readonly byte[] buffer = new byte[2048];

			bool sendingNow = false;
			readonly Queue<byte[]> sendQueue = new Queue<byte[]> ();

			Node node;
			NodeClientDataConnectionHandler nodeDataStream;

			readonly object readSync = new object();

			string version = null;

			public readonly string DisplayName;

			public ClientConnection(TcpClient client, MasterServer master)
			{
				DisplayName = client.Client.RemoteEndPoint.ToString();

				this.master = master;
				this.client = client;
				this.clientId = master.AllocateClientId();
				lock (master.clients) {
					master.clients.Add(clientId, this);
				}

				log.DebugFormat("{0}: Starting TLS authentication.", DisplayName);

				stream = client.GetStream();
				stream.ReadTimeout = 10000;
				stream.WriteTimeout = 10000;

				ssl = new SslStream(stream);
				ssl.BeginAuthenticateAsServer(master.serverCert, false,
					SslProtocols.Tls, true, AuthenticationDone, null);
			}

			void Closed()
			{
				log.DebugFormat("{0}: Closed.", DisplayName);

				lock (master.clients) {
					master.clients.Remove(clientId);

					var n = nodeDataStream;
					if (n != null) {
						n.DataStream.Close ();
						nodeDataStream = null;
					}
				}

				{
					var n = node;
					if (n != null) {
						n.ClientDisconnected (clientId, version);
					}
					node = null;
				}

				if (version != null) {
					lock (master.versionNumClients) {
						--master.versionNumClients[version];
					}
					version = null;
				}
			}

			void SendPrologueResult(Client.Protocol.PrologueResult result)
			{
				ssl.WriteByte ((byte)result);
				ssl.Flush ();
			}

			void ShutdownWithResult(Client.Protocol.PrologueResult result)
			{
				log.DebugFormat("{0}: Failing with '{1}'.", DisplayName, result);
				try {
					SendPrologueResult(result);
				} catch (Exception) {
					// ignore...
				}

				Shutdown ();
			}

			void Shutdown()
			{
				log.DebugFormat("{0}: Shutting down the connection.", DisplayName);
				Closed ();
				ssl.Close ();
				stream.Close ();
				client.Close ();
			}

			void AuthenticationDone (IAsyncResult ar)
			{
				try {
					ssl.EndAuthenticateAsServer(ar);
				} catch (Exception ex) {
					log.Warn (string.Format ("{0}: TLS Handshake error.", DisplayName), ex);
					Shutdown ();
					return;
				}

				log.DebugFormat("{0}: TLS authentication done.", DisplayName);

				// Read connection prologue block
				ssl.BeginRead (buffer, 0, 2048, HandlePrologue, null);
			}

			void HandlePrologue (IAsyncResult ar)
			{
				try {
					var count = ssl.EndRead(ar);
					if (count < 2048) {
						throw new System.IO.InvalidDataException("Incomplete prologue.");
					}

					log.DebugFormat("{0}: Got prologue.", DisplayName);

					uint magic = BitConverter.ToUInt32(buffer, 0);
					if (magic != Client.Protocol.HeaderMagic) {
						throw new System.IO.InvalidDataException("Invalid client header magic.");
					}

					int roomNameLength = BitConverter.ToInt32(buffer, 4);
					if (roomNameLength + 8 > buffer.Length || roomNameLength < 0) {
						throw new System.IO.InvalidDataException("Invalid room name length.");
					}

					var roomName = new byte[roomNameLength];
					Buffer.BlockCopy(buffer, 8, roomName, 0, roomNameLength);

					var bindTarget = master.BindClientToDomain(roomName);
					if (bindTarget == null) {
						log.DebugFormat("{0}: Rejecting because no suitable server were found.", DisplayName);
						ShutdownWithResult (Merlion.Client.Protocol.PrologueResult.ServerFull);
						return;
					}
					node = bindTarget.Item1;
					version = bindTarget.Item2;

					log.DebugFormat("{0}: Bound to server '{1}', version '{2}'.", DisplayName,
						node.FullName, version);

					lock (master.versionNumClients) {
						if (!master.versionNumClients.ContainsKey(version))
							master.versionNumClients[version] = 1;
						else
							++master.versionNumClients[version];
					}

					SendPrologueResult(Merlion.Client.Protocol.PrologueResult.Success);
					log.DebugFormat("{0}: Connecton success.", DisplayName);

					node.ClientConnect(clientId, version, roomName);

					// Node will reply with BindNodeDataStream later...
					// TODO: timeout

				} catch (Exception ex) {
					log.Warn (string.Format ("{0}: Protocol initiation error.", DisplayName), ex);
					ShutdownWithResult (Merlion.Client.Protocol.PrologueResult.ProtocolError);
				}
			}

			Utils.StreamAsyncCopier dataReader;
			Utils.StreamAsyncCopier dataWriter;

			public void BindNodeDataStream(NodeClientDataConnectionHandler handler)
			{
				if (nodeDataStream != null) {
					throw new InvalidOperationException ("Already have a data stream.");
				}
				nodeDataStream = handler;

				dataReader = new Utils.StreamAsyncCopier (ssl, nodeDataStream.DataStream, AppConfiguration.DownstreamBufferSize, true);
				dataWriter = new Utils.StreamAsyncCopier (nodeDataStream.DataStream, ssl, AppConfiguration.UpstreamBufferSize, true);

				dataReader.BeginCopy ((result) => {
					try {
						dataReader.EndCopy(result);
					} catch (SocketException) { 
					} catch (Exception ex) {
						log.Debug (string.Format("{0}: Read error.", DisplayName), ex);
					}
					Shutdown();
				}, null);
				dataWriter.BeginCopy ((result) => {
					try {
						dataWriter.EndCopy(result);
					} catch (SocketException) { 
					} catch (Exception ex) {
						log.Debug (string.Format("{0}: Write error.", DisplayName), ex);
					}
					Shutdown();
				}, null);
			}

			public void Rejected()
			{
				Shutdown ();
			}

			public void NodeServerWasDown()
			{
				node = null;
				Shutdown ();
			}
		}
	}
}

