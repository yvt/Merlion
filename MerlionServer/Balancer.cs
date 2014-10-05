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
using System.Xml;
using System.Xml.Serialization;
using System.Collections.Generic;
using System.Linq;
using log4net;

namespace Merlion.Server
{

	public sealed class Balancer
	{
		static ILog log = LogManager.GetLogger(typeof(Balancer));

		readonly string configPath;

		[XmlType]
		[XmlRoot("BalancerConfig")]
		public sealed class Config
		{
			public NodeConfig[] Nodes { get; set; }
			public VersionConfig[] Versions { get; set; }
		}

		[XmlType]
		public sealed class NodeConfig
		{
			[XmlAttribute("Name")]
			public string Name { get; set; }

			[XmlAttribute("Throttle")]
			public double Throttle { get; set; }
		}

		[XmlType]
		public sealed class VersionConfig
		{
			[XmlAttribute("Name")]
			public string Name { get; set; }

			[XmlAttribute("Throttle")]
			public double Throttle { get; set; }
		}

		public sealed class Node
		{
			readonly Balancer balancer;
			double throttle;

			public string Name { get; private set; }
			public double Throttle {
				get { 
					lock (balancer.sync)
						return throttle;
				}
				set {
					lock (balancer.sync) {
						throttle = value;
						balancer.SetModified ();
						var handler = balancer.NodeThrottleUpdated;
						if (handler != null)
							handler (balancer, new NodeEventArgs (this));
					}
				}
			}

			public Node(Balancer balancer, NodeConfig cfg)
			{
				this.balancer = balancer;
				Name = cfg.Name;
				throttle = cfg.Throttle;
			}

			public Node(Balancer balancer, string name)
			{
				this.balancer = balancer;
				Name = name;
				throttle = 0.0;
			}

			public NodeConfig Config
			{
				get {
					return new NodeConfig () {
						Name = Name,
						Throttle = Throttle
					};
				}
			}
		}

		public sealed class Version
		{
			readonly Balancer balancer;
			double throttle;

			public string Name { get; private set; }
			public double Throttle {
				get { 
					lock (balancer.sync)
						return throttle;
				}
				set {
					lock (balancer.sync) {
						throttle = value;
						balancer.SetModified ();
						var handler = balancer.VersionThrottleUpdated;
						if (handler != null)
							handler (balancer, new VersionEventArgs (this));
					}
				}
			}

			public Version(Balancer balancer, VersionConfig cfg)
			{
				this.balancer = balancer;
				Name = cfg.Name;
				throttle = cfg.Throttle;
			}

			public Version(Balancer balancer, string name)
			{
				this.balancer = balancer;
				Name = name;
				throttle = 0.0;
			}

			public VersionConfig Config
			{
				get {
					return new VersionConfig () {
						Name = Name,
						Throttle = Throttle
					};
				}
			}
		}

		readonly object sync = new object();

		readonly Dictionary<string, Node> nodes = new Dictionary<string, Node>();
		readonly Dictionary<string, Version> versions = new Dictionary<string, Version>();

		bool modified = false;

		public sealed class NodeEventArgs: EventArgs
		{
			public Node Node { get; private set; }
			public NodeEventArgs(Node node)
			{
				Node = node;
			}
		}

		public sealed class VersionEventArgs: EventArgs
		{
			public Version Version { get; private set; }
			public VersionEventArgs(Version version)
			{
				Version = version;
			}
		}

		public event EventHandler<VersionEventArgs> VersionAdded;
		public event EventHandler<VersionEventArgs> VersionRemoved;
		public event EventHandler<VersionEventArgs> VersionThrottleUpdated;

		public event EventHandler<NodeEventArgs> NodeThrottleUpdated;

		public Balancer ()
		{
			configPath = AppConfiguration.BalancerConfigFilePath;

			try {
				LoadConfig ();
			} catch(Exception) {

			}
		}

		void SetModified()
		{
			lock (sync)
				modified = true;
		}

		public bool IsModified
		{
			get {
				lock (sync)
					return modified;
			}
		}

		public Node[] GetAllNodes()
		{
			lock (sync)
				return nodes.Values.ToArray ();
		}

		public Version[] GetAllVersions()
		{
			lock (sync)
				return versions.Values.ToArray ();
		}

		public Node GetNode(string name, bool addIfNotExist = false)
		{
			lock (sync) {
				Node node;
				if (!nodes.TryGetValue(name, out node)) {
					if (!addIfNotExist)
						return null;

					SetModified ();

					node = new Node (this, name);
					nodes.Add (name, node);
				}
				return node;
			}
		}

		public Version GetVersion(string name, bool addIfNotExist = false)
		{
			lock (sync) {
				Version ver;
				if (!versions.TryGetValue(name, out ver)) {
					if (!addIfNotExist)
						return null;
					ver = new Version (this, name);
					versions.Add (name, ver);

					SetModified ();

					var handler = VersionAdded;
					if (handler != null)
						handler (this, new VersionEventArgs (ver));
				}
				return ver;
			}
		}

		public void RemoveVersion(string name)
		{
			lock (sync) {
				Version ver;
				if (versions.TryGetValue (name, out ver)) {
					versions.Remove (name);

					SetModified ();

					var handler = VersionRemoved;
					if (handler != null)
						handler (this, new VersionEventArgs (ver));
				}
			}
		}


		void LoadConfig ()
		{
			lock (sync) {
				try {
					log.InfoFormat ("Loading balancer configuration from '{0}'.", configPath);
					using (var stream = System.IO.File.OpenRead (configPath)) {
						var ser = new XmlSerializer (typeof(Config));
						var cfg = (Config)ser.Deserialize (stream);
						foreach (var node in cfg.Nodes) {
							var n = new Node (this, node);
							nodes.Add (n.Name, n);
						}
						foreach (var ver in cfg.Versions) {
							var v = new Version (this, ver);
							versions.Add (v.Name, v);
						}
					}
				} catch(Exception ex) {
					log.Warn (string.Format("Could not load balancer configuration from '{0}'.", configPath), ex);
					SetModified ();
				}
			}
		}

		public void SaveConfig()
		{
			lock (sync) {
				log.InfoFormat ("Writing load balancer configuration to '{0}'.", configPath);

				try {
					System.IO.Directory.CreateDirectory (System.IO.Path.GetDirectoryName (configPath));

					var cfg = new Config ();
					cfg.Versions = versions.Values.Select (v => v.Config).ToArray ();
					cfg.Nodes = nodes.Values.Select (v => v.Config).ToArray ();

					var dest = configPath + ".tmp";
					var ser = new XmlSerializer (typeof(Config));
					using (var s = System.IO.File.OpenWrite(dest)) {
						ser.Serialize (s, cfg);
					}

					if (System.IO.File.Exists(configPath)) {
						System.IO.File.Replace (dest, configPath, null, true);
						System.IO.File.Delete (dest);
					} else {
						System.IO.File.Move (dest, configPath);
					}
					modified = false;
				} catch(Exception ex) {
					log.Error (string.Format("Could not write balancer configuration to '{0}'.", configPath), ex);
				}
			}
		}

		public class Balance<T>
		{
			public T Object { get; set; }
			public double Current { get; set; }
			public double Desired { get; set; }
		}

		public static T DoBalancing<T>(IEnumerable<Balance<T>> balances) where T : class
		{
			var balarray = balances.ToArray ();
			var total = (from b in balarray
				select b.Current).Sum();
			var desiredTotal = (from b in balarray
				select b.Desired).Sum();
			if (desiredTotal <= 0.0) {
				return null;
			}
			var scale = total / desiredTotal;
			T minObject = null;
			double minValue = 0.0;
			foreach (var e in balarray) {
				if (e.Desired <= 0.0) {
					continue;
				}
				var val = e.Current - e.Desired * scale;
				if (minObject == null || val < minValue) {
					minObject = e.Object;
					minValue = val;
				}
			}
			return minObject;
		}
	}
}

