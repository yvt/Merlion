using System;
using System.Threading;
using log4net;

namespace Merlion.Server
{
	interface IMasterServer
	{
		Balancer Balancer { get; }
		NodeStatus[] GetAllNodeInfos ();
		VersionManager VersionManager { get; }
	}

	sealed partial class MasterServer: IMasterServer
	{
		readonly static ILog log = LogManager.GetLogger(typeof(MasterServer));

		readonly NodeListener nodeListener;
		readonly ClientListener clientListener;
		readonly VersionManager versionManager;

		NodeServer node;
		Thread nodeThread;

		public VersionManager VersionManager
		{
			get { return versionManager; }
		}

		public MasterServer ()
		{
			try {
				log.InfoFormat ("Loading master server certificate from '{0}'.",
					AppConfiguration.MasterServerCertificate);
				var pathparts = AppConfiguration.MasterServerCertificate.Split(';');

				if (pathparts.Length == 1)
					serverCert = new System.Security.Cryptography.X509Certificates.X509Certificate2(pathparts[0]);
				else {
					serverCert = new System.Security.Cryptography.X509Certificates.X509Certificate2(pathparts[0], pathparts[1]);
					/*using (var pw = new System.Security.SecureString()) {
						foreach(var c in pathparts[1])
							pw.AppendChar(c);
						serverCert = new System.Security.Cryptography.X509Certificates.X509Certificate (
							pathparts[0], pw);
					}*/
				}

				log.Info ("Starting master server.");

				nodeListener = new NodeListener (this);
				clientListener = new ClientListener (this);
				versionManager = new VersionManager();
				balancer = new Balancer ();

				foreach (var ver in balancer.GetAllVersions()) {
					if (!versionManager.VersionExists(ver.Name)) {
						log.WarnFormat("Version '{0}' doesn't exist. Removing from the balancer.",
							ver.Name);
						balancer.RemoveVersion(ver.Name);
					}
				}

				balancer.VersionAdded += (sender, e) => {
					log.InfoFormat("Version '{0}' was added.", e.Version.Name);
					lock (nodes) {
						foreach (var node in nodes) {
							node.Value.RequestToLoadVersion(e.Version.Name);
						}
					}
				};
				balancer.VersionRemoved += (sender, e) => {
					log.InfoFormat("Version '{0}' was removed.", e.Version.Name);
					lock (nodes) {
						foreach (var node in nodes) {
							node.Value.RequestToUnloadVersion(e.Version.Name);
						}
					}
				};

				nodeListener.StartListening += (sender, e) => {
					log.Info("Starting local node.");
					nodeThread = new Thread (() => {
						var ep = AppConfiguration.MasterServerAddress;
						node = new NodeServer("Local", 
							new System.Net.IPEndPoint(System.Net.IPAddress.Loopback, ep.Port));
						node.Run();
					});
					nodeThread.Name = "Local Node";
					nodeThread.Start();
				};
			} catch (Exception ex) {
				log.Fatal ("Master server startup failed.", ex);
				throw;
			}

		}

		public void Run()
		{
			var th = new Thread (clientListener.Start);
			th.Name = "Master: Client Listener";
			th.Start ();
			var th2 = new Thread (nodeListener.Start);
			th2.Name = "Master: Node Listener";
			th2.Start ();

			// Do heartbeat
			while (true) {
				lock (nodes) {
					foreach (var node in nodes) {
						try {
							node.Value.Heartbeat ();
						} catch {}
					}
				}
				if (balancer.IsModified) {
					balancer.SaveConfig ();
				}
				Thread.Sleep (4000);
			}
		}
	}
}

