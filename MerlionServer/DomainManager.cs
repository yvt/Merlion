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
using System.Collections.Generic;
using System.Threading.Tasks;
using log4net;
using System.Linq;

namespace Merlion.Server
{
	sealed class RoomAndVersionEventArgs: EventArgs
	{
		public byte[] RoomId;
		public string Version;
	}

	sealed class DomainManager
	{
		readonly static ILog log = LogManager.GetLogger(typeof(DomainManager));

		public readonly INodeServer NodeServer;

		readonly VersionManager versionManager;

		readonly Dictionary<string, Domain> domains = 
			new Dictionary<string, Domain>();

		readonly HashSet<string> loadedVersions =
			new HashSet<string>();

		readonly object sync = new object();

		public event EventHandler<RoomAndVersionEventArgs> RoomBeingAdded;
		public event EventHandler<RoomAndVersionEventArgs> RoomRemoved;

		public DomainManager (INodeServer node)
		{
			NodeServer = node;
			versionManager = node.VersionManager;
		}

		void StopVersion(string version)
		{
			lock (sync) {
				Domain domain;
				if (domains.TryGetValue(version, out domain)) {
					if (domain != null)
						new Task (domain.Dispose).Start ();
					domains.Remove (version);
					log.InfoFormat("Version '{0}' unloaded.", version);

					try {
						NodeServer.SendVersionUnloaded(version);
					} catch (Exception ex) {
						log.Warn("Error while sending 'VersionUnloaded'", ex);
					}
				}
			}
		}

		void StartVersion(string version)
		{
			lock (sync) {
				if (domains.ContainsKey (version))
					return;
				if (!loadedVersions.Contains (version))
					return;

				domains.Add (version, null);
				new Task (() => {
					try {
						var dom = new Domain(NodeServer, version);
						dom.RoomBeingAdded += (sender, e) => {
							var h = RoomBeingAdded;
							if (h != null)
								h(this, new RoomAndVersionEventArgs() {
									RoomId = e.RoomId,
									Version = version
								});
						};
						dom.RoomRemoved += (sender, e) => {
							var h = RoomRemoved;
							if (h != null)
								h(this, new RoomAndVersionEventArgs() {
									RoomId = e.RoomId,
									Version = version
								});
						};
						lock (sync) {
							domains[version] = dom;
							try {
								NodeServer.SendVersionLoaded(version);
							} catch (Exception ex) {
								log.Warn("Error while sending 'VersionLoaded'", ex);
							}
							log.InfoFormat("Version '{0}' loaded.", version);
						}
					} catch (Exception ex) {
						log.Error(string.Format("Failed to load version '{0}'.", version), ex);
						UnloadVersion(version);
						return;
					}
				}).Start ();
			}
		}

		public void LoadVersion(string version)
		{
			lock (sync) {
				if (loadedVersions.Contains(version)) {
					return;
				}

				loadedVersions.Add (version);
				if (versionManager.VersionExists(version)) {
					StartVersion (version);
				} else {
					NodeServer.VersionMissing ();
				}
			}
		}

		public void UnloadVersion(string version)
		{
			lock (sync) {
				if (!loadedVersions.Contains(version)) {
					return;
				}

				Domain domain;
				if (domains.TryGetValue(version, out domain)) {
					if (domain != null)
						new Task (domain.Dispose).Start ();
					domains.Remove (version);
				}

				loadedVersions.Remove (version);
			}
		}

		public void UnloadAll()
		{
			lock (sync) {
				loadedVersions.Clear ();
				var vers = domains.Keys.ToArray();
				foreach (var d in vers) {
					StopVersion (d);
				}
				domains.Clear ();
			}
		}

		public string GetMissingVersion()
		{
			lock (sync) {
				foreach (var ver in loadedVersions) {
					if (!versionManager.VersionExists (ver))
						return ver;
				}
				return null;
			}
		}

		public Domain GetDomain(string version)
		{
			Domain domain;
			lock (domains)
				domains.TryGetValue (version, out domain);
			return domain;
		}


		public void SetupClient(long clientId, NativeServerInterop.ClientSetup setup)
		{
			lock (domains)
				foreach (var d in domains)
					if (d.Value.SetupClient (clientId, setup))
						return;
			throw new InvalidOperationException (string.Format ("Client '{0}' was not found in setup queue.", clientId));
		}

		public void DiscardClient(long clientId)
		{
			lock (domains)
				foreach (var d in domains)
					d.Value.DiscardClient(clientId);
		}

		public void VersionDownloaded(string version)
		{
			if (!versionManager.VersionExists (version)) {
				NodeServer.VersionMissing ();
				return;
			}

			StartVersion (version);
		}
	}
}

