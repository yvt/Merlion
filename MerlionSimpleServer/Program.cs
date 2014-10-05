using System;
using log4net;
using System.Net.Sockets;

namespace Merlion.SimpleServer
{
	sealed class MainClass
	{
		static readonly ILog log = LogManager.GetLogger(typeof(MainClass));
		static Server.Application app;

		public static void Main (string[] args)
		{
			if (args.Length < 3) {
				Console.Error.WriteLine ("USAGE: MerlionSimpleServer IP PORT PVK");
			}
			log4net.Config.BasicConfigurator.Configure ();

			var ip = System.Net.IPAddress.Parse (args [0]);
			var port = int.Parse (args [1]);

			log.InfoFormat ("Starting simple server at '{0}'.", AppDomain.CurrentDomain.BaseDirectory);
			var server = new Server.ApplicationServer (new NodeImpl ());
			server.Start ();

			var listener = new TcpListener (new System.Net.IPEndPoint(ip, port));
			listener.Start ();

			throw new NotImplementedException ();
			while (true) {
				var client = listener.AcceptTcpClient ();

			}
		}


	}

	sealed class NodeImpl: Server.ServerNode
	{
		static readonly ILog log = LogManager.GetLogger(typeof(NodeImpl));
		public override void Log (log4net.Core.LoggingEvent[] logEvents)
		{
			foreach (var e in logEvents)
				log.Logger.Log (e);
		}

		public override Merlion.Server.MerlionRoom CreateRoom ()
		{
			return new RoomImpl ();
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

	// Room is completely no-op because we only have one node
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
