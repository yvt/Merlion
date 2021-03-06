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
using System.ComponentModel;
using System.Threading;
using log4net.Layout;
using log4net.Appender;
using log4net;

namespace Merlion.Server
{
	class MainClass
	{
		readonly static ILog log = LogManager.GetLogger(typeof(MainClass));

		static CommandLineArguments Arguments;

		public static IMasterServer MasterServer;
		public static NativeNodeServerManager NodeServerManager;
		public static INodeServer NodeServer { 
			get {
				if (NodeServerManager == null)
					return null;
				return NodeServerManager.NodeServer;
			} 
		}

		public static void Main (string[] args)
		{
			var logConfig = AppConfiguration.Log4NetConfigFile;
			var logInfo = new System.IO.FileInfo (logConfig);

			Arguments = new CommandLineArguments (args);

			log4net.GlobalContext.Properties["Node"] = "Local";
			if (!logInfo.Exists)
			{
				log.WarnFormat (
					"Log configuration file '{0}' was not found. Creating default one.", 
					logInfo.FullName);
				try {
					using (var s = logInfo.OpenWrite())
					using (var s2 = System.Reflection.Assembly.GetExecutingAssembly ()
						.GetManifestResourceStream ("Merlion.Server.Log4Net.config")){
						Utils.StreamUtils.Copy (s2, s);
					}
				} catch (Exception ex) {
					log.Error (
						"Error while creating the default log configuration file.", ex);
				}
			}

			logInfo = new System.IO.FileInfo (logConfig);
			if (logInfo.Exists) {
				log4net.Config.XmlConfigurator.ConfigureAndWatch (logInfo);
				log.InfoFormat (
					"Using log configuration file '{0}'.", 
					logInfo.FullName);
			} else {
				log4net.Config.BasicConfigurator.Configure();

				log.WarnFormat (
					"Log configuration file '{0}' was not found. Using BasicConfigurator.", 
					logInfo.FullName);
			}

			NativeServerInterop.Logging.RegisterLogger ();

			log.InfoFormat (
				"Base directory is '{0}'.", 
				AppConfiguration.BaseDirectory);

			 /*
			new Thread (() => {
				Thread.Sleep(20000);
				Environment.Exit(0);
			}).Start (); /* */
			if (Arguments.ServerMode == ServerMode.Master) {

				try {
					MasterServer = new NativeMasterServer();

					//MasterServer = new MasterServer();

					var webThread = new Thread (() => {
						try {
							var web = new Web.WebInterface (MasterServer);
							web.Run ();
						} catch (Exception ex) {
							log.Fatal(
								"Error occured in web server.", ex
							);
							Environment.Exit(1);
						}
					});
					webThread.Name = "Web Interface";
					webThread.Start();
					if (Arguments.ExitByEnter) {
						Console.In.ReadLine ();
						webThread.Abort();
					} else {
						webThread.Join();
					}
					log.Info(
						"Exiting by user operation."
					);
					MasterServer.Dispose();
				} catch (Exception ex) {
					log.Fatal(
						"Error occured in master server.", ex
					);
					Environment.Exit(1);
				}

			} else {
				NodeServerManager = new NativeNodeServerManager (Arguments.NodeName, Arguments.MasterServerEndpoint);
				if (Arguments.ExitByEnter) {
					Console.In.ReadLine ();
				} else {
					while (true)
						Thread.Sleep (1000);
				}
				log.Info(
					"Exiting by user operation."
				);
				NodeServerManager.Dispose ();
			}

			NativeServerInterop.Library.Instance.Dispose ();

			Environment.Exit(0);
		}
	}

	enum ServerMode
	{
		Master,
		Node
	}

	sealed class CommandLineArguments
	{
		bool runAsMaster = false;
		bool runAsNode = false;

		public bool ExitByEnter = false;

		System.Net.IPEndPoint masterServerEndpoint = AppConfiguration.MasterServerAddress;

		public ServerMode ServerMode { get; private set; }

		public System.Net.IPEndPoint MasterServerEndpoint { get { return masterServerEndpoint; }}

		public string NodeName = AppConfiguration.NodeName;

		public CommandLineArguments(string[] args)
		{
			try {
				new Merlion.Utils.CommandLineArgsSerializer<CommandLineArguments>().DeserializeInplace(this, args);
			} catch (Merlion.Utils.CommandLineArgsException ex) {
				Console.Error.WriteLine (ex.Message);
				if (ex.InnerException != null) {
					Console.Error.WriteLine (ex.InnerException.ToString ());
				}
				WriteUsageAndExit ();
			}

			if (runAsMaster && runAsNode) {
				Console.Error.WriteLine ("Both of -Master and -Node were specified.");
				Environment.Exit (1);
			} else if (!runAsMaster && !runAsNode) {
				Console.Error.WriteLine ("Either of -Master and -Node must be specified.");
				Environment.Exit (1);
			} else {
				ServerMode = runAsNode ? ServerMode.Node : ServerMode.Master;
			}

		}

		void WriteUsage()
		{
			new Merlion.Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsageHeader ();
			new Merlion.Utils.CommandLineArgsSerializer<CommandLineArguments> ().WriteUsage (Console.Out);
		}

		void WriteUsageAndExit()
		{
			WriteUsage ();
			Environment.Exit (1);
		}

		[Description("Starts the Merlion server as a master server.")]
		void HandleMaster()
		{
			runAsMaster = true;
		}

		[Description("Starts the Merlion server as a node server.")]
		void HandleNode()
		{
			runAsNode = true;
		}

		[Description("Specifies the endpoint of the master server.")]
		void HandleMasterAddress([Description("IPADDRESS:PORT")] string param)
		{
			masterServerEndpoint = AppConfiguration.ParseIPEndPoint(param);
		}

		[Description("Specifies the name of the node.")]
		void HandleNodeName(string name)
		{
			NodeName = name;
		}

		[Description("Server can be terminated gracefully by hitting the enter key. Useful for " +
			"CPU profiling using mono.")]
		void HandleExitByEnter()
		{
			ExitByEnter = true;
		}

		[Description("Shows the usage.")]
		void HandleHelp()
		{
			WriteUsageAndExit ();
		}
	}
}
