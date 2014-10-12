using NUnit.Framework;
using System;
using System.Collections.Specialized;

namespace Merlion.Server.Test
{
	[TestFixture ()]
	public class MasterTest
	{
		[Test ()]
		public void Create ()
		{
			using (var baseDir = new TemporaryDirectory ()) {
				var settings = new NameValueCollection ();
				settings ["EndpointAddress"] = "0.0.0.0:16070";
				settings ["MasterServerAddress"] = "0.0.0.0:5000";
				settings ["BaseDirectory"] = baseDir.Path;
				AppConfiguration.AppSettings = settings;

				var p = new NativeServerInterop.MasterParameters ();
				p.ClientEndpoint = "0.0.0.0:16070";
				p.NodeEndpoint = "0.0.0.0:5000";

				using (var master = new NativeMasterServer (p)) {
				}
			}
		}
	}
}

