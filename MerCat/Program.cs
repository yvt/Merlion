using System;
using System.Net;
using Merlion.Client;
using System.Threading;
using System.Collections.Generic;

namespace Merlion.MerCat
{
	class MainClass
	{
		static long numSent = 0;
		static long numReceived = 0;
		static readonly object sync = new object();


		public static void Main (string[] args)
		{
			if (args.Length == 0) {
				Console.Error.WriteLine ("Invalid usage.");
				return;
			}
			if (args.Length == 1) {
				var client = new MerlionClient (args[0]);
				client.Connect ();

				new Thread (() => DoWrite (client)).Start ();

				byte[] buffer = new byte[65536];
				var outstream = Console.OpenStandardOutput ();
				var instream = client.Stream;

				while (true) {
					int readCount = instream.Read (buffer, 0, buffer.Length);
					if (readCount > 0) {
						outstream.Write (buffer, 0, readCount);
						outstream.Flush ();
					} else {
						return;
					}
				}
				return;
			}
			var cmd = args[0];
			if (cmd.Equals("Stress", StringComparison.InvariantCultureIgnoreCase)) {
				if (args.Length != 3) {
					Console.Error.WriteLine ("USAGE: MerCat Stress HOST THREADS");
					return;
				}

				int count = int.Parse (args [2]);
				var threads = new List<Thread> ();

				Console.Error.WriteLine ("Initializing...");
				for (int i = 0; i < count; ++i) {
					var client = new MerlionClient (args[1]);
					int index = i;
					threads.Add (new Thread (() => {
						client.Connect();
						new Thread(() => StressRead(client)).Start();

						var s = client.Stream;
						while (true) {
							lock (sync) {
								++numSent;
							}
							s.WriteByte((byte)'.');
							s.Flush();
							Thread.Sleep(100);
						}
					}));
				}

				Console.Error.WriteLine ("Started... [{0} thread(s)]", count);
				foreach (var th in threads) {
					Thread.Sleep(10);
					th.Start ();
				}

				while (true) {
					lock (sync) {
						Console.WriteLine ("Sent =         {0}", numSent);
						Console.WriteLine ("Received =     {0}", numReceived);
						Console.WriteLine ("Not Received = {0}", numSent - numReceived);
						Console.WriteLine ("-----------------");
					}
					Thread.Sleep (1000);
				}

				return;
			}

			Console.Error.WriteLine ("Unknown command.");
	
		}

		static void StressRead(MerlionClient client)
		{
			var s = client.Stream;
			while (true) {
				s.ReadByte ();
				lock (sync) {
					++numReceived;
				}
			}
		}

		static void DoWrite(MerlionClient client)
		{
			byte[] buffer = new byte[65536];
			var instream = Console.OpenStandardInput ();
			var outstream = client.Stream;

			while (true) {
				int readCount = instream.Read (buffer, 0, buffer.Length);
				if (readCount > 0) {
					outstream.Write (buffer, 0, readCount);
				} else {
					return;
				}
			}
		}
	}
}
