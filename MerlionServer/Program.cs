using System;
using System.ComponentModel;
using System.Threading;
using log4net.Layout;
using log4net.Appender;

namespace Merlion.Server
{
	class MainClass
	{
		static CommandLineArguments Arguments;

		public static IMasterServer MasterServer;
		public static INodeServer NodeServer;

		public static void Main (string[] args)
		{
			var logConfig = AppConfiguration.Log4NetConfigFile;
			var logInfo = new System.IO.FileInfo (logConfig);

			Arguments = new CommandLineArguments (args);

			log4net.GlobalContext.Properties["Node"] = "Local";
			if (!logInfo.Exists)
			{
				log4net.LogManager.GetLogger (typeof(MainClass)).WarnFormat (
					"Log configuration file '{0}' was not found. Creating default one.", 
					logInfo.FullName);
				try {
					using (var s = logInfo.OpenWrite())
					using (var s2 = System.Reflection.Assembly.GetExecutingAssembly ()
						.GetManifestResourceStream ("Merlion.Server.Log4Net.config")){
						Utils.StreamUtils.Copy (s2, s);
					}
				} catch (Exception ex) {
					log4net.LogManager.GetLogger (typeof(MainClass)).Error (
						"Error while creating the default log configuration file.", ex);
				}
			}

			logInfo = new System.IO.FileInfo (logConfig);
			if (logInfo.Exists) {
				log4net.Config.XmlConfigurator.ConfigureAndWatch (logInfo);
				log4net.LogManager.GetLogger (typeof(MainClass)).InfoFormat (
					"Using log configuration file '{0}'.", 
					logInfo.FullName);
			} else {
				log4net.Config.BasicConfigurator.Configure();

				log4net.LogManager.GetLogger (typeof(MainClass)).WarnFormat (
					"Log configuration file '{0}' was not found. Using BasicConfigurator.", 
					logInfo.FullName);
			}

			NativeServerInterop.Logging.RegisterLogger ();

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
							log4net.LogManager.GetLogger(typeof(MainClass)).Fatal(
								"Error occured in web server.", ex
							);
							Environment.Exit(1);
						}
					});
					webThread.Name = "Web Interface";
					webThread.Start();

					webThread.Join();
					//MasterServer.Run();
				} catch (Exception ex) {
					log4net.LogManager.GetLogger(typeof(MainClass)).Fatal(
						"Error occured in master server.", ex
					);
					Environment.Exit(1);
				}

			} else {
				NodeServer = new NativeNodeServer (Arguments.NodeName, Arguments.MasterServerEndpoint);
				while (true) Thread.Sleep (1000);
				//NodeServer.Run ();
			}

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

		System.Net.IPEndPoint masterServerEndpoint = AppConfiguration.MasterServerAddress;

		public ServerMode ServerMode { get; private set; }

		public System.Net.IPEndPoint MasterServerEndpoint { get { return masterServerEndpoint; }}

		public string NodeName = AppConfiguration.NodeName;

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

		[Description("Shows the usage.")]
		void HandleHelp()
		{
			WriteUsageAndExit ();
		}
	}
}
