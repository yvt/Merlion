using System;
using System.Linq;
using System.Collections.Generic;
using log4net;

namespace Merlion.Server
{
	sealed class NativeMasterServer: IMasterServer
	{
		readonly static ILog log = LogManager.GetLogger(typeof(NativeMasterServer));

		NativeServerInterop.Master native;

		readonly VersionManager versionManager = new VersionManager();
		readonly Balancer balancer = new Balancer();

		readonly NativeNodeServer localNodeServer;

		public NativeMasterServer ()
		{

			var param = new NativeServerInterop.MasterParameters ();
			param.ClientEndpoint = AppConfiguration.EndpointAddress.ToString ();
			param.NodeEndpoint = AppConfiguration.MasterServerAddress.ToString ();
			param.SslCertificateFile = AppConfiguration.MasterServerCertificate;
			param.SslPrivateKeyFile = AppConfiguration.MasterServerPrivateKey;
			param.PackagePathProvider = versionManager.GetPackagePathForVersion;

			native = new NativeServerInterop.Master (param);

			foreach (var ver in balancer.GetAllVersions()) {
				if (!versionManager.VersionExists(ver.Name)) {
					log.WarnFormat("Version '{0}' doesn't exist. Removing from the balancer.",
						ver.Name);
					balancer.RemoveVersion(ver.Name);
				} else {
					log.InfoFormat("Adding version '{0}'.", ver.Name);
					native.AddVersion (ver.Name);
					native.SetVersionThrottle (ver.Name, ver.Throttle);
				}
			}

			foreach (var node in balancer.GetAllNodes()) {
				native.SetNodeThrottle (node.Name, node.Throttle);
			}

			balancer.VersionAdded += (sender, e) => {
				log.InfoFormat("Version '{0}' was added.", e.Version.Name);
				native.AddVersion(e.Version.Name);
				native.SetVersionThrottle (e.Version.Name, e.Version.Throttle);
			};
			balancer.VersionRemoved += (sender, e) => {
				log.InfoFormat("Version '{0}' was removed.", e.Version.Name);
				native.RemoveVersion(e.Version.Name);
			};
			balancer.VersionThrottleUpdated += (sender, e) => {
				log.DebugFormat("Throttle of the version '{0}' was changed to {1}.", e.Version.Name, e.Version.Throttle);
				native.SetVersionThrottle (e.Version.Name, e.Version.Throttle);
			};
			balancer.NodeThrottleUpdated += (sender, e) => {
				log.DebugFormat("Throttle of the node '{0}' was changed to {1}.", e.Node.Name, e.Node.Throttle);
				native.SetNodeThrottle (e.Node.Name, e.Node.Throttle);
			};

			System.Threading.Thread.Sleep (500);
			log.Info("Starting local node.");
			localNodeServer = new NativeNodeServer ("Local", new System.Net.IPEndPoint (
				System.Net.IPAddress.Loopback, AppConfiguration.MasterServerAddress.Port));

			new System.Threading.Thread (() => {
				// "Save balancer config" thread
				while (true) {
					if (balancer.IsModified) {
						balancer.SaveConfig ();
					}
					System.Threading.Thread.Sleep (3000);
				}
			}).Start ();
		}

		public VersionManager VersionManager
		{ get { return versionManager; } }

		public Balancer Balancer 
		{ get { return balancer; } }

		public NodeStatus[] GetAllNodeInfos()
		{
			var nodes = native.EnumerateNodes ();
			return nodes.Select(node => {
				var info = new NodeStatus() {
					ID = node.NodeName.GetHashCode(),
					Name = node.NodeName,
					HostName = "unknown",
					UpTime = TimeSpan.Zero,
					Domains = new Dictionary<string, DomainStatus>(),
					Throttle = balancer.GetNode(node.NodeName).Throttle
				};

				foreach (var domain in node.Domains) {
					info.Domains.Add(domain.VersionName, new DomainStatus() {
						NumRooms = domain.RoomCount,
						NumClients = domain.ClientCount
					});
					info.NumRooms += domain.RoomCount;
					info.NumClients += domain.ClientCount;
				}

				return info;
			}).ToArray();
		}
	}
}

