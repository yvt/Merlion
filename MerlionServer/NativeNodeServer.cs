using System;
using log4net;
using System.Threading.Tasks;

namespace Merlion.Server
{
	sealed class NativeNodeServer: MarshalByRefObject, INodeServer
	{
		readonly static ILog log = LogManager.GetLogger(typeof(NativeMasterServer));

		readonly NativeServerInterop.NodeParameters param;
		readonly NativeServerInterop.Node native;

		readonly VersionManager versionManager = new VersionManager();

		readonly DomainManager domains;

		public NativeNodeServer (string name, System.Net.IPEndPoint ep)
		{
			if (name == null) {
				name = AppConfiguration.NodeName;
			}
			if (ep == null) {
				ep = AppConfiguration.MasterServerAddress;
			}

			param = new NativeServerInterop.NodeParameters ();
			param.NodeName = name;
			param.MasterEndpoint = ep.ToString ();
			param.PackagePathProvider = versionManager.GetPackagePathForVersion;
			param.PackageDownloadPathProvider = versionManager.GetDownloadPathForVersion;
			param.DeployPackageMethod = (ver) => {
				try {
					versionManager.EndDeployVersion(ver);
				} catch (Exception ex) {
					log.Error(string.Format("Failed to deploy version '{0}'.", ver), ex);
					throw;
				}
			};

			domains = new DomainManager (this);

			param.VersionLoader = (ver) => new Task(() => domains.LoadVersion(ver)).Start();
			param.VersionUnloader = (ver) => new Task(() => domains.UnloadVersion(ver)).Start();
			param.AcceptClientMethod = (clientId, version, room) => {
				try {
					var domain = domains.GetDomain(version);
					if (domain == null) {
						throw new InvalidOperationException(string.Format(
							"Version '{0}' is not loaded.", version));
					}

					domain.AcceptWithoutStream((long)clientId, room);
				} catch (Exception ex) {
					log.Error(string.Format("Rejecting client '{0}' (version '{1}') because of an error.", 
						clientId, version), ex);
					throw;
				}
			};
			param.DiscardClientMethod = (clientId) => {
				domains.DiscardClient((long)clientId);
			};
			param.SetupClientMethod = (clientId, setup) => {
				domains.SetupClient((long)clientId, setup);
			};


			native = new NativeServerInterop.Node (param);

		}

		public string Name
		{
			get { return param.NodeName; }
		}

		public void SendVersionLoaded (string name)
		{
			native.VersionLoaded (name);
		}

		public void SendVersionUnloaded (string name)
		{
			native.VersionUnloaded (name);
		}

		public void VersionMissing ()
		{
			// TODO
			throw new InvalidOperationException ();
		}

		public VersionManager VersionManager {
			get {
				return versionManager;
			}
		}
	}
}

