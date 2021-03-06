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
using log4net;
using System.Threading;
using System.Threading.Tasks;

namespace Merlion.Server
{
	sealed class NativeNodeServer: MarshalByRefObject, INodeServer
	{
		readonly static ILog log = LogManager.GetLogger(typeof(NativeMasterServer));

		readonly NativeServerInterop.NodeParameters param;
		readonly NativeServerInterop.Node native;

		readonly ReaderWriterLockSlim disposeLock = new ReaderWriterLockSlim ();

		readonly VersionManager versionManager = new VersionManager();

		readonly DomainManager domains;

		public event EventHandler NodeFailed;

		public bool Failed { get; private set; }

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

			domains.RoomBeingAdded += (sender, e) => {
				try {
					disposeLock.EnterReadLock();
					native.BindRoom(e.RoomId, e.Version);
				} finally {
					disposeLock.ExitReadLock ();
				}
			};
			domains.RoomRemoved += (sender, e) => {
				try {
					disposeLock.EnterReadLock();
					native.UnbindRoom(e.RoomId);
				} finally {
					disposeLock.ExitReadLock ();
				}
			};

			param.VersionLoader = (ver) => new Task(() => {
				try {
					domains.LoadVersion(ver);
				} catch (Exception ex) {
					log.Error(string.Format("Error while loading version '{0}'.", 
						ver), ex);
					throw;
				}
			}).Start();
			param.VersionUnloader = (ver) => new Task(() => {
				try {
					domains.UnloadVersion(ver);
				} catch (Exception ex) {
					log.Error(string.Format("Error while unloading version '{0}'.", 
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
			param.SetupClientMethod = (clientId, socket) => {
				try {
					domains.SetupClient((long)clientId, socket);
				} catch (Exception ex) {
					log.Warn(string.Format("Error while setting up the client '{0}'.", 
						clientId), ex);
					throw;
				}
			};
			param.FailureHandler = () => {
				Failed = true;
				var h = NodeFailed;
				if (h != null)
					h(this, EventArgs.Empty);
			};

			native = new NativeServerInterop.Node (param);

		}

		public string Name
		{
			get { return param.NodeName; }
		}

		public void SendVersionLoaded (string name)
		{
			try {
				disposeLock.EnterReadLock();
				native.VersionLoaded (name);
			} catch(InvalidOperationException ex) {
				// don't care...
				log.Debug ("SendVersionLoaded was called but state was invalid.");
			} finally {
				disposeLock.ExitReadLock ();
			}
		}

		public void SendVersionUnloaded (string name)
		{
			try {
				disposeLock.EnterReadLock();
				native.VersionUnloaded (name);
			} catch(InvalidOperationException) {
				// don't care...
				log.Debug ("SendVersionUnloaded was called but state was invalid.");
			} finally {
				disposeLock.ExitReadLock ();
			}
		}

		public void SendLog(LogEntry e)
		{
			try {
				disposeLock.EnterReadLock();
				native.ForwardLog (e);
			} finally {
				disposeLock.ExitReadLock ();
			}
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

		public void Dispose()
		{
			try {
				disposeLock.EnterWriteLock();
				native.Dispose ();
			} finally {
				disposeLock.ExitWriteLock ();
			}
		}
	}
}

