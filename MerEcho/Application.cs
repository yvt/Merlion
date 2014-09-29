using System;
using log4net;
using System.Threading;

namespace Merlion.MerEcho
{
	public class Application: Server.Application
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Application));
		public Application ()
		{
		}

		public override void Accept (Merlion.Server.MerlionClient client, Merlion.Server.MerlionRoom room)
		{
			var stream = client.Stream;
			new Thread (() => DoEcho (stream)).Start ();
		}

		void DoEcho(System.IO.Stream stream)
		{
			byte[] buffer = new byte[16384];
			log.Info ("Start echo");
			try {
				while (true) {
					int readCount = stream.Read (buffer, 0, buffer.Length);
					if (readCount == 0)
						break;
					//log.DebugFormat ("Got {0} byte(s)", readCount);
					stream.Write (buffer, 0, readCount);
					stream.Flush ();
				}
			} catch (Exception ex) {
				log.Warn ("Echo", ex);
			} finally {
				stream.Close ();
			}
			log.Info ("End echo");
		}
	}
}

