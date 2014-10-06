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

			param.VersionLoader = (ver) => new Task(() => {
				try {
					domains.LoadVersion(ver);
				} catch (Exception ex) {
					log.Warn(string.Format("Error while loading version '{0}'.", 
						ver), ex);
					throw;
				}
			}).Start();
			param.VersionUnloader = (ver) => new Task(() => {
				try {
					domains.UnloadVersion(ver);
				} catch (Exception ex) {
					log.Warn(string.Format("Error while unloading version '{0}'.", 
						ver), ex);
					throw;
				}
			}).Start();
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
				try {
					domains.DiscardClient((long)clientId);
				} catch (Exception ex) {
					log.Warn(string.Format("Error while discarding the client '{0}'.", 
						clientId), ex);
					throw;
				}
			};
			param.SetupClientMethod = (clientId, setup) => {
				try {
					domains.SetupClient((long)clientId, setup);
				} catch (Exception ex) {
					log.Warn(string.Format("Error while setting up the client '{0}'.", 
						clientId), ex);
					throw;
				}
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

		public void SendLog(log4net.Core.LoggingEvent e)
		{
			var entry = new NativeServerInterop.LogEntry();
			var data = e.GetLoggingEventData ();
			entry.Source = data.LoggerName;
			entry.Message = data.Message;

			if (data.Properties != null) {
				entry.Channel = data.Properties ["NDC"] as string;
				entry.Host = data.Properties ["Node"] as string;
			}

			if (e.Level.Value <= log4net.Core.Level.Debug.Value) {
				entry.Severity = NativeServerInterop.LogSeverity.Debug;
			} else if (e.Level.Value <= log4net.Core.Level.Info.Value) {
				entry.Severity = NativeServerInterop.LogSeverity.Info;
			} else if (e.Level.Value <= log4net.Core.Level.Warn.Value) {
				entry.Severity = NativeServerInterop.LogSeverity.Warn;
			} else if (e.Level.Value <= log4net.Core.Level.Error.Value) {
				entry.Severity = NativeServerInterop.LogSeverity.Error;
			} else {
				entry.Severity = NativeServerInterop.LogSeverity.Fatal;
			} 

			native.ForwardLog (entry);
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

