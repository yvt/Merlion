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
using System.Net;
using Merlion.Client;
using System.Threading;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace Merlion.MerCat
{
	enum RunMode
	{
		NetCat = 0,
		Stress
	}

	enum ErrorMode
	{
		Respawn,
		LeaveClosed,
		Fatal
	}

	class MainClass
	{
		static readonly object sync = new object();

		static public CommandLineArguments cmdArgs;

		public static void Main (string[] args)
		{
			cmdArgs = new CommandLineArguments (args);

			switch (cmdArgs.Mode.Value) {
			case RunMode.NetCat:
				DoNetCat ();
				break;
			case RunMode.Stress:
				PerformStressTest ();
				break;
			default:
				throw new ArgumentOutOfRangeException ();
			}
		}

		static StressTest stressTest;
		static ManualResetEvent stressTestAbortEvent = new ManualResetEvent(false);

		static void PrintStressTestStatus()
		{
			lock (sync) {
				var numSent = stressTest.SentPacketCount;
				var numReceived = stressTest.ReceivedPacketCount;
				long chunkSize = cmdArgs.ChunkSize;
				double secs = stressTest.RunningTime.TotalSeconds;

				Console.WriteLine ("============================================");
				Console.WriteLine ();
				Console.WriteLine ("Time                    : {0}", stressTest.RunningTime);
				Console.WriteLine ();
				Console.WriteLine ("-----------------");
				Console.WriteLine ();
				Console.WriteLine ("Sent / Received Packets : {0:N0} / {1:N0} [packets]", 
					numSent, numReceived);
				Console.WriteLine (" (Sent - Received)      : {0:N0}", 
					numSent - numReceived);
				Console.WriteLine (" (per second)           : {0:N0} / {1:N0} [packets/sec]", 
					(double)numSent / secs,
					(double)numReceived / secs);

				Console.WriteLine ();
				Console.WriteLine ("Sent / Received Bytes   : {0:N0} / {1:N0} [bytes]", 
					numSent * chunkSize, numReceived * chunkSize);
				Console.WriteLine (" (Sent - Received)      : {0:N0} [bytes]", 
					(numSent - numReceived) * chunkSize);
				Console.WriteLine (" (per second)           : {0:N0} / {1:N0} [bytes/sec]", 
					(double)(numSent * chunkSize) / secs,
					(double)(numReceived * chunkSize) / secs);
				Console.WriteLine ();
				Console.WriteLine ("-----------------");
				Console.WriteLine ();
				Console.WriteLine ("Active Workers          : {0} [workers]", stressTest.ActiveWorkerCount);
				Console.WriteLine ("Total Workers           : {0} [workers]", stressTest.TotalWorkerCount);
				Console.WriteLine (" (per second)           : {0} [workers/sec]", 
					(double)stressTest.TotalWorkerCount / secs);
				Console.WriteLine ();
				Console.WriteLine ("-----------------");
				Console.WriteLine ();
				Console.WriteLine ("Major failure reasons:");

				var fails = stressTest.GetWorkerFailReasons ();
				Array.Sort (fails, (a, b) => b.Item2.CompareTo(a.Item2));

				if (fails.Length == 0) {
					Console.WriteLine ("  (no failures occured yet)");
				} else {
					for (int i = 0; i < fails.Length && i < 8; ++i) {
						Console.WriteLine ("  [{0,8}] {1}", fails[i].Item2, fails[i].Item1.Message);
					}
				}

				Console.Out.Flush();
			}
		}

		static void StressTestMonitorThread()
		{
			while (stressTest != null) {
				Console.ReadLine ();
				PrintStressTestStatus();
				Thread.Sleep (500);
			}
		}

		static void PerformStressTest()
		{
			Console.Error.WriteLine ("Starting stress test...");
			Console.Error.WriteLine ("Hit ENTER to display the current status.");
			Console.Error.WriteLine ("Hit Ctrl+C to abort the test.");
			Console.WriteLine ();

			stressTest = new StressTest ();
			stressTest.Aborted += (sender, e) => {
				stressTestAbortEvent.Set();
			};
			stressTest.PerformTest ();

			var monitorThread = new Thread (StressTestMonitorThread);
			monitorThread.Start ();

			int interrupted = 0;
			Console.CancelKeyPress += (sender, e) => {
				++interrupted;
				if (interrupted == 2) {
					lock (sync) {
						Console.WriteLine("Hit Ctrl+C again to terminate... (no report will be generated)");
						Console.Out.Flush();
					}
				}
				if (interrupted >= 3) {
					// Hit twice
					System.Diagnostics.Process.GetCurrentProcess().Kill();
					return;
				}
				lock (sync) {
					Console.WriteLine("Aborting...");
					Console.Out.Flush();
				}
				e.Cancel = true;
				stressTest.Abort();
			};

			while (!stressTestAbortEvent.WaitOne ());

			Console.WriteLine ();
			Console.WriteLine ("### Test completed.");
			Console.WriteLine ();
			PrintStressTestStatus ();

			monitorThread.Abort ();
			Environment.Exit (0);
		}
		static void DoNetCat()
		{
			var client = new MerlionClient (cmdArgs.Address);
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

	sealed class CommandLineArguments
	{

		public RunMode? Mode;

		public string Address;

		public ErrorMode ErrorMode = ErrorMode.Respawn;
		public int Concurrency = 1;
		public int Interval = 100;
		public int ChunkSize = 32;
		public int SpawnInterval = 100;
		public long SentCountLimit = long.MaxValue;
		public TimeSpan TimeoutTime = TimeSpan.FromSeconds(10);

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

			if (Mode == null) {
				Console.Error.WriteLine ("One of -NetCat and -Stress must be specified.");
				Environment.Exit (1);
			}
			if (Address == null) {
				Console.Error.WriteLine ("-Address required.");
				Environment.Exit (1);
			}

		}

		void WriteUsageAndExit()
		{
			new Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsageHeader ();
			new Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsage (Console.Out);
			Environment.Exit (1);
		}

		[Description("MetCat behaves like NetCat by connecting the standard input/output to the socket stream.")]
		void HandleNetCat()
		{
			Mode = RunMode.NetCat;
		}

		[Description("Performs stress test.")]
		void HandleStress()
		{
			Mode = RunMode.Stress;
		}

		[Description("Does not respawn a failed connection during the stress test. " +
			"Test will be stopped when no workers are running.")]
		void HandleLeaveClosed()
		{
			ErrorMode = ErrorMode.LeaveClosed;
		}

		[Description("Connection failure aborts the stress test.")]
		void HandleErrorsFatal()
		{
			ErrorMode = ErrorMode.Fatal;
		}

		[Description("Specifies the endpoint of the master server.")]
		void HandleAddress([Description("IPADDRESS[:PORT]")] string param)
		{
			Address = param;
		}

		[Description("Specifies the number of concurrent connections for stress test.")]
		void HandleConcurrency(string count)
		{
			Concurrency = int.Parse (count);
			if (Concurrency < 1)
				throw new ArgumentOutOfRangeException ("count", "Must be positive.");
		}

		[Description("Specifies time interval between sent packets for stress test.")]
		void HandleInterval(string msecs)
		{
			Interval = int.Parse (msecs);
			if (Interval < 0)
				throw new ArgumentOutOfRangeException ("msecs", "Must be non-zero.");
		}

		[Description("Specifies time interval of spawning workers during the stress test.")]
		void HandleSpawnInterval(string msecs)
		{
			SpawnInterval = int.Parse (msecs);
			if (SpawnInterval < 0)
				throw new ArgumentOutOfRangeException ("msecs", "Must be non-zero.");
		}

		[Description("Specifies the maximum number of packet sent per worker during the stress test. " +
			"Worker will fail after the specified number of packets.")]
		void HandleLimitCount(long count)
		{
			SentCountLimit = count;
			if (SentCountLimit < 0)
				throw new ArgumentOutOfRangeException ("count", "Must be non-zero.");
		}

		[Description("Specifies the number of bytes sent at once during stress test.")]
		void HandleChunkSize(string count)
		{
			ChunkSize = int.Parse (count);
			if (ChunkSize < 1)
				throw new ArgumentOutOfRangeException ("count", "Must be positive.");
			if (ChunkSize > 1024 * 1024)
				throw new ArgumentOutOfRangeException ("count", "Too big.");
		}

		[Description("Shows the usage.")]
		void HandleHelp()
		{
			WriteUsageAndExit ();
		}
	}
}
