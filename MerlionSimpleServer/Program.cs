using System;
using log4net;
using System.Net.Sockets;
using System.Net.Security;
using System.Text.RegularExpressions;

namespace Merlion.SimpleServer
{
	public sealed class MainClass
	{
		static readonly ILog log = LogManager.GetLogger(typeof(MainClass));
		static internal Server.ApplicationServer App;
		static CommandLineArguments cmdArgs;
		static internal NodeImpl Node;
		static SuperWebSocket.WebSocketServer<ClientHandler> webSocketServer;

		static internal System.Security.Cryptography.X509Certificates.X509Certificate Certificate;

		public static void Main (string[] args)
		{
			cmdArgs = new CommandLineArguments (args);

			log4net.Config.BasicConfigurator.Configure ();

			Certificate = new System.Security.Cryptography.X509Certificates.X509Certificate2
				(cmdArgs.CertificateFile, cmdArgs.CertificatePassword);

			log.InfoFormat ("Starting simple server at '{0}'.", AppDomain.CurrentDomain.BaseDirectory);
			Node = new NodeImpl ();
			var server = App = new Server.ApplicationServer (Node);
			server.Start ();

			log.InfoFormat ("Listening at '{0}:{1}'.", cmdArgs.ListenAddress, cmdArgs.ListenPort);

			var rootConfig = new SuperSocket.SocketBase.Config.RootConfig ();

			var serverConfig = new SuperSocket.SocketBase.Config.ServerConfig ();
			serverConfig.Name = "Merlion Simple Server";
			serverConfig.Ip = cmdArgs.ListenAddress.ToString ();
			serverConfig.Port = cmdArgs.ListenPort;
			serverConfig.Mode = SuperSocket.SocketBase.SocketMode.Tcp;
			serverConfig.Security = "tls";

			var certConfig = new SuperSocket.SocketBase.Config.CertificateConfig ();
			certConfig.FilePath = cmdArgs.CertificateFile;
			certConfig.Password = cmdArgs.CertificatePassword;
			serverConfig.Certificate = certConfig;

			webSocketServer = new ServerImpl(new SuperWebSocket.SubProtocol.BasicSubProtocol<ClientHandler>("merlion.yvt.jp"));
			webSocketServer.Setup (rootConfig, serverConfig);
			webSocketServer.NewDataReceived += (session, value) => session.OnReceived (value);
			webSocketServer.Start ();

			while (true) {
				System.Threading.Thread.Sleep (1000);
			}
		}


	}

	sealed class CommandLineArguments
	{
		public System.Net.IPAddress ListenAddress = System.Net.IPAddress.Any;
		public int ListenPort = 16070;
		public string CertificateFile = null;
		public string CertificatePassword = string.Empty;

		public CommandLineArguments(string[] args)
		{
			try {
				new Utils.CommandLineArgsSerializer<CommandLineArguments>().DeserializeInplace(this, args);
			} catch (Utils.CommandLineArgsException ex) {
				Console.Error.WriteLine (ex.Message);
				if (ex.InnerException != null) {
					Console.Error.WriteLine (ex.InnerException.ToString ());
				}
				WriteUsageAndExit ();
			}

			if (CertificateFile == null) {
				Console.Error.WriteLine ("-Certificate must be specified.");
				Environment.Exit (1);
			}
		}

		void WriteUsageAndExit()
		{
			new Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsageHeader ();
			new Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsage (Console.Out);
			Environment.Exit (1);
		}

		public void HandleAddress(string ipaddr)
		{
			ListenAddress = System.Net.IPAddress.Parse (ipaddr);
		}

		public void HandlePort(string port)
		{
			ListenPort = int.Parse(port);
		}

		public void HandleCertificate(string filename)
		{
			CertificateFile = filename;
		}

		public void HandleCertificatePassword(string password)
		{
			CertificatePassword = password;
		}

	}

	sealed class ServerImpl : SuperWebSocket.WebSocketServer<ClientHandler>
	{
		public ServerImpl (SuperWebSocket.SubProtocol.ISubProtocol<ClientHandler> subProtocol) : base (subProtocol)
		{
		}
		
	}

	sealed class ClientHandler : SuperWebSocket.WebSocketSession<ClientHandler>
	{
		static readonly ILog log = LogManager.GetLogger(typeof(ClientHandler));
		static int nextClientId = 1;

		int clientId;


		sealed class ClientImpl: Merlion.Server.MerlionClient
		{
			static readonly ILog log = LogManager.GetLogger(typeof(ClientImpl));
			long clientId;
			ClientHandler session;
			public ClientImpl(long clientId, ClientHandler session)
			{
				this.clientId = clientId;
				this.session = session;
				session.Received += (sender, e) => {
					try {
						if (e.Data != null) {
							this.OnReceived (new Merlion.Server.ReceiveEventArgs (e.Data));
						} else {
							this.OnClosed (EventArgs.Empty);
						}
					} catch (Exception ex) {
						log.Error("Exception in Received or Closed handler.", ex);
					}
				};
			}

			public override long ClientId {
				get {
					return clientId;
				}
			}

			public override void Close ()
			{
				session.Close ();
			}

			public override void Send (byte[] data, int offset, int length)
			{
				session.Send (data, offset, length);
			}
		}

		public ClientHandler()
		{

		}

		public event EventHandler<Merlion.Server.ReceiveEventArgs> Received;

		public void OnReceived(byte[] b)
		{
			var handler = Received;
			if (handler != null)
				handler (this, new Merlion.Server.ReceiveEventArgs (b));
		}

		protected override void OnInit ()
		{
			base.OnInit ();
		}

		static Regex urlRegex = new Regex (@"^\/([^/]*)\/([^/]*)$");

		protected override void OnSessionStarted ()
		{
			clientId = System.Threading.Interlocked.Increment (ref nextClientId);

			NDC.Push (string.Format ("Client:{0} [{1}]", clientId, RemoteEndPoint));

			log.InfoFormat ("Client connected to '{0}'.", Path);

			try {

				Path = System.Net.WebUtility.UrlDecode(Path);
				var matches = urlRegex.Match(Path);
				if (!matches.Success) {
					throw new System.IO.InvalidDataException("Unrecognizable URL.");
				}
					
				var ver = matches.Groups[1].Value;
				var room = Merlion.Utils.HexEncoder.GetBytes(matches.Groups[2].Value);

				log.DebugFormat("Requested version '{0}'. We ignore this request.",
					ver);

				log.Info("Accepted.");

				var cli = new ClientImpl(clientId, this);
				MainClass.App.Accept(cli, MainClass.Node.GetRoom(room));

			} catch (Exception ex) {
				log.Error ("Error occured while servicing.", ex);

				try {
					Close();
				} catch {}
			}

			NDC.Pop ();

			base.OnSessionStarted ();
		}
		protected override void OnSessionClosed (SuperSocket.SocketBase.CloseReason reason)
		{
			OnReceived (null);
			base.OnSessionClosed (reason);
		}

	
		void Service()
		{

		}
	}

	sealed class NodeImpl: Server.ServerNode
	{
		static readonly ILog log = LogManager.GetLogger(typeof(NodeImpl));
		public override void Log (log4net.Core.LoggingEvent[] logEvents)
		{
			// Ignore. Log is already sent to console.
		}

		public override Merlion.Server.MerlionRoom CreateRoom ()
		{
			return new RoomImpl ();
		}

		internal RoomImpl GetRoom(byte[] room)
		{
			return null; // TODO
		}

		public override string Name {
			get {
				return "SimpleServer";
			}
		}

		public override string BaseDirectory {
			get {
				return AppDomain.CurrentDomain.BaseDirectory;
			}
		}

		public override string ServerDirectory {
			get {
				return AppDomain.CurrentDomain.BaseDirectory;
			}
		}
	}

	// Room is almost no-op because we only have one domain
	sealed class RoomImpl: Server.MerlionRoom
	{
		static Random r = new Random();
		byte[] roomId;

		public RoomImpl()
		{
			roomId = new byte[16];
			r.NextBytes (roomId);
		}

		public override byte[] GetRoomId ()
		{
			return (byte[])roomId.Clone();
		}

		public override void Dispose ()
		{
		}
	}
}
