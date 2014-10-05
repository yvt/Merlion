﻿/**
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

namespace Merlion.MerCat
{
	enum RunMode
	{
		NetCat = 0,
		Stress
	}

	class MainClass
	{
		static long numSent = 0;
		static long numReceived = 0;
		static readonly object sync = new object();

		static CommandLineArguments cmdArgs;

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

			if (args.Length == 1) {
			}
			var cmd = args[0];
			if (cmd.Equals("Stress", StringComparison.InvariantCultureIgnoreCase)) {

			}

			Console.Error.WriteLine ("Unknown command.");
	
		}

		static void PerformStressTest()
		{
			int count = cmdArgs.Concurrency;
			var threads = new List<Thread> ();
			byte[] buffer = new byte[cmdArgs.ChunkSize];

			new Random ().NextBytes (buffer);

			Console.Error.WriteLine ("Initializing...");
			for (int i = 0; i < count; ++i) {
				var client = new MerlionClient (cmdArgs.Address);
				int index = i;
				threads.Add (new Thread (() => {
					client.Connect();
					var s = client.Stream;

					new Thread(() => {
						while (true) {
							int readCount = 0;
							int read = 0;
							int retryCount = 100;

							while (read < buffer.Length && retryCount > 0) {

								while ((readCount = s.Read (buffer, read, buffer.Length - read)) > 0) {
									read += readCount;
									if (read == buffer.Length)
										break;
								}
								--retryCount;
								System.Threading.Thread.Sleep(100);
							}

							if (read < buffer.Length) {
								throw new System.IO.IOException ("Insufficient data");
							}

							lock (sync) {
								++numReceived;
							}
						}
					}).Start();

					while (true) {
						s.Write(buffer, 0, buffer.Length);
						s.Flush();
						lock (sync) {
							++numSent;
						}
						if (cmdArgs.Interval > 0)
							Thread.Sleep(cmdArgs.Interval);
					}
				}));
			}

			Console.Error.WriteLine ("Starting... [{0} thread(s)]", count);
			int cnt = 0;
			foreach (var th in threads) {
				Thread.Sleep(10);
				th.Start ();

				++cnt;
				Console.Error.WriteLine ("{0} of {1} thread(s) running", cnt, count);
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

		public int Concurrency = 1;
		public int Interval = 100;
		public int ChunkSize = 32;

		public CommandLineArguments(string[] args)
		{
			int i = 0;
			while (i < args.Length) {
				var arg = args [i];
				if (arg.Length == 0) {
					continue;
				}
				if (arg[0] == '-') {
					var name = arg.Substring (1);
					var meth = GetType ().GetMethod ("Handle" + name,
						System.Reflection.BindingFlags.Instance |
						System.Reflection.BindingFlags.NonPublic);
					if (meth == null) {
						Console.Error.WriteLine ("Unknown option: '{0}'", arg);
						WriteUsageAndExit ();
					}

					var param = meth.GetParameters ();
					try {
						if (param.Length == 0) {
							meth.Invoke (this, new object[0]);
						} else {
							++i;
							if (i >= args.Length) {
								Console.Error.WriteLine ("Option needs argument: '{0}'", arg);
								Environment.Exit (1);
							}
							arg += " " + args[i]; // for error message
							meth.Invoke (this, new object[] { args[i] });
						}
					} catch (System.Reflection.TargetInvocationException ex) {
						Console.Error.WriteLine ("Error while parsing argument '{0}':", arg);
						Console.Error.WriteLine (ex.Message);
					}
				} else {
					Console.Error.WriteLine ("Unknown option: '{0}'", arg);
					WriteUsageAndExit ();
				}
				++i;
			}

			if (Mode == null) {
				Console.Error.WriteLine ("Only one of -NetCat and -Stress must be specified.");
				Environment.Exit (1);
			}
			if (Address == null) {
				Console.Error.WriteLine ("-Address required.");
				Environment.Exit (1);
			}

		}

		void WriteUsage()
		{
			Console.WriteLine ("USAGE:");
			Console.WriteLine ();

			foreach (var meth in GetType().GetMethods(
				System.Reflection.BindingFlags.Instance |
				System.Reflection.BindingFlags.NonPublic)) {
				if (meth.Name.StartsWith("Handle", StringComparison.InvariantCulture)) {
					var name = meth.Name.Substring (6);
					var param = meth.GetParameters ();
					if (param.Length == 0) {
						Console.WriteLine ("-" + name);
					} else {
						var paramNameAttrs = param [0].GetCustomAttributes (typeof(DescriptionAttribute), false);
						var pdesc = param [0].Name.ToUpperInvariant ();
						if (paramNameAttrs.Length > 0) {
							pdesc = ((DescriptionAttribute)paramNameAttrs [0]).Description;
						}
						Console.WriteLine ("-" + name + " " + pdesc);
					}

					var desc = meth.GetCustomAttributes (typeof(DescriptionAttribute), false);
					if (desc.Length > 0) {
						Console.WriteLine ();
						Console.WriteLine ("    {0}", ((DescriptionAttribute)desc [0]).Description);
					}

					Console.WriteLine ();
				}
			}
		}

		void WriteUsageAndExit()
		{
			WriteUsage ();
			Environment.Exit (1);
		}

		[Description("MetCat behaves like NetCat.")]
		void HandleNetCat()
		{
			Mode = RunMode.NetCat;
		}

		[Description("Performs stress test.")]
		void HandleStress()
		{
			Mode = RunMode.Stress;
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
