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
using System.IO;
using System.Linq;
using log4net;

namespace Merlion.Server
{
	sealed class VersionManager
	{
		static readonly ILog log = LogManager.GetLogger(typeof(VersionManager));

		readonly string baseDirectory;
		readonly object sync = new object();

		public VersionManager ()
		{
			baseDirectory = AppConfiguration.DeployDirectory;

			log.InfoFormat ("Deploy base directory: {0}", baseDirectory);
			if (!Directory.Exists(baseDirectory)) {
				Directory.CreateDirectory (baseDirectory);
			}
		}

		public string[] GetAllVersions()
		{
			return Directory.GetFiles (baseDirectory, "*.zip")
				.Select ((name) => name.Substring (0, name.Length - 4))
				.ToArray ();
		}

		public bool ValidateVersionName(string name)
		{
			foreach (var c in name) {
				if (!char.IsLetterOrDigit(c) && c != '_' && c != '.' &&
					c != '-') {
					return false;
				}
			}
			return true;
		}

		public string GetDownloadPathForVersion(string name)
		{
			return Path.Combine (baseDirectory, name + ".download");
		}

		public string GetPackagePathForVersion(string name)
		{
			return Path.Combine (baseDirectory, name + ".zip");
		}

		public string GetDirectoryPathForVersion(string name)
		{
			return Path.Combine (baseDirectory, name);
		}

		public bool VersionExists(string name)
		{
			return File.Exists (GetPackagePathForVersion (name));
		}

		public System.IO.Stream StartDeployVersion(string name)
		{
			if (!ValidateVersionName(name)) {
				throw new InvalidOperationException ("Invalid version name.");
			}

			if (VersionExists(name)) {
				throw new InvalidOperationException (
					string.Format("Version {0} already exists.", name));
			}


			var path = GetDownloadPathForVersion (name);

			log.InfoFormat ("Starting downloading version '{0}' to '{1}'.",
				name, path);

			return File.Open (path, FileMode.Create, FileAccess.Write, FileShare.None);
		}

		public void EndDeployVersion(string name)
		{
			log.InfoFormat ("Downloading version '{0}' completed.", name);
			try {
				log.InfoFormat ("Extracting '{0}' to '{1}'.",
					GetDownloadPathForVersion (name), GetDirectoryPathForVersion(name));
				using (var zip = new Ionic.Zip.ZipFile (GetDownloadPathForVersion (name))) {
					zip.ExtractAll (GetDirectoryPathForVersion(name));
				}
			} catch (Exception ex) {
				log.Error (string.Format("Failed to extract the version '{0}'.", name), ex);
				throw;
			}
			try {
				log.InfoFormat ("Moving '{0}' to '{1}'.",
					GetDownloadPathForVersion (name), GetPackagePathForVersion(name));
				File.Move (GetDownloadPathForVersion (name),
					GetPackagePathForVersion (name));
			} catch (Exception ex) {
				log.Error (string.Format("Failed to deploy the version '{0}'.", name), ex);
				throw;
			}
			log.InfoFormat ("Version '{0}' deployed.", name);
		}

	}
}

