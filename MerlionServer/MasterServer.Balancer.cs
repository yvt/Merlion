using System;
using System.Collections.Generic;
using System.Linq;

namespace Merlion.Server
{
	partial class MasterServer
	{

		readonly Balancer balancer;

		public Balancer Balancer
		{
			get { return balancer; }
		}

		Tuple<Node, string> BindClientToDomain(byte[] room)
		{
			Node[] nodeList;
			lock (nodes) {
				nodeList = this.nodes.Values.ToArray();
			}

			// Is client already bound to a app domain?
			foreach (var node in nodeList) {
				var ver = node.GetVersionForRoom (room);
				if (ver != null) {
					return Tuple.Create (node, ver);
				}
			}

			// Use auto balancer.
			var vers = balancer.GetAllVersions ()
				.Select (v => new Balancer.Balance<string>() {
					Object = v.Name,
					Desired = v.Throttle
				});
			lock (versionNumClients) {
				foreach (var ver in vers) {
					long count = 0;
					if (versionNumClients.TryGetValue(ver.Object, out count)) {
						ver.Current = (double)count;
					}
				}
			}

			var version = Balancer.DoBalancing(vers);
			if (version == null) {
				return null;
			}

			var servers = new List<Balancer.Balance<Node>> ();

			lock (nodes) {
				foreach (var node in nodes) {
					if (node.Value.IsVersionLoaded(version)) {
						var info = balancer.GetNode (node.Value.NodeInfo.Name);
						if (info != null && info.Throttle > 0.0) {
							servers.Add (new Balancer.Balance<Node> () {
								Object = node.Value,
								Current = (double)node.Value.NumClients,
								Desired = info.Throttle
							});
						}
					}
				}
			}

			var retnode = Balancer.DoBalancing(servers);
			if (retnode == null) {
				return null;
			}

			return Tuple.Create (retnode, version);
		}

		readonly Dictionary<string, long> versionNumClients = new Dictionary<string, long>();


	}
}

