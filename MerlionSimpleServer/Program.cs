using System;
using log4net;
using System.Net.Sockets;
using System.Net.Security;

namespace Merlion.SimpleServer
{
	public sealed class MainClass
	{
		static readonly ILog log = LogManager.GetLogger(typeof(MainClass));
		static internal Server.ApplicationServer App;
		static CommandLineArguments cmdArgs;
		static internal NodeImpl Node;

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
			var listener = new TcpListener (new System.Net.IPEndPoint(cmdArgs.ListenAddress, cmdArgs.ListenPort));
			listener.Start ();

			while (true) {
				var client = listener.AcceptTcpClient ();
				client.ReceiveTimeout = 20000;
				client.SendTimeout = 20000;

				new System.Threading.Thread(c => new ClientHandler ((TcpClient)c)).Start(client);
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

	sealed class ClientHandler
	{
		static readonly ILog log = LogManager.GetLogger(typeof(ClientHandler));
		static int nextClientId = 1;

		TcpClient client;
		NetworkStream tcpStream;
		int clientId;
		SslStream ssl;

		sealed class ClientImpl: Merlion.Server.MerlionClient
		{
			long clientId;
			System.IO.Stream stream;
			public ClientImpl(long clientId, System.IO.Stream stream)
			{
				this.clientId = clientId;
				this.stream = stream;
			}

			public override long ClientId {
				get {
					return clientId;
				}
			}

			public override System.IO.Stream Stream {
				get {
					return stream;
				}
			}
		}

		public ClientHandler(TcpClient client)
		{
			this.client = client;
			this.tcpStream = client.GetStream ();

			clientId = System.Threading.Interlocked.Increment (ref nextClientId);
		
			NDC.Push (string.Format ("Client:{0} [{1}]", clientId, client.Client.RemoteEndPoint));

			log.Info ("Client connected.");

			Service ();

			log.Info ("Client disconnected.");

			NDC.Pop ();
		}

		void Service()
		{
			try {
				log.Debug("Performing SSL Handshake.");

				ssl = new SslStream(tcpStream, false);
				ssl.AuthenticateAsServer(MainClass.Certificate);

				log.Debug("Receiving prologue.");
				byte[] prologue = new byte[2048];

				if (ssl.Read(prologue, 0, 2048) < 2048) {
					throw new System.IO.InvalidDataException("Premature header");
				}

				int prologueIndex = 0;
				uint headerMagic = BitConverter.ToUInt32(prologue, prologueIndex) ;
				if (headerMagic != Merlion.Client.Protocol.HeaderMagic) {
					throw new System.IO.InvalidDataException(string.Format("Got invalid header magic '{0}'.", headerMagic));
				}

				prologueIndex += 4;

				int roomLen = BitConverter.ToInt32(prologue, prologueIndex);
				prologueIndex += 4;

				byte[] room = new byte[roomLen];
				Buffer.BlockCopy(prologue, prologueIndex, room, 0, roomLen);
				prologueIndex += roomLen;

				int verLen = BitConverter.ToInt32(prologue, prologueIndex);
				prologueIndex += 4;

				byte[] ver = new byte[verLen];
				Buffer.BlockCopy(prologue, prologueIndex, ver, 0, verLen);
				prologueIndex += verLen;

				log.DebugFormat("Requested version '{0}'. We ignore this request.",
					System.Text.Encoding.UTF8.GetString(ver));

				log.Info("Accepted.");

				ssl.WriteByte((byte)Merlion.Client.Protocol.PrologueResult.Success);
				ssl.Flush();

				var cli = new ClientImpl(clientId, ssl);
				MainClass.App.Accept(cli, MainClass.Node.GetRoom(room));

			} catch (Exception ex) {
				log.Error ("Error occured while servicing.", ex);
				try {
					tcpStream.WriteByte((byte)Merlion.Client.Protocol.PrologueResult.ProtocolError);
				} catch {
				}
				try {
					tcpStream.Close();
					tcpStream.Dispose();
				} catch {}
				try {
					client.Close();
				} catch {}
			}
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

	// Room is completely no-op because we only have one domain
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
