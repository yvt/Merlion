using NUnit.Framework;
using System;

namespace Merlion.Server.Test
{
	[TestFixture ()]
	public class MasterTest
	{
		[Test ()]
		public void Create ()
		{
			var p = new Merlion.Server.NativeServerInterop.MasterParameters ();
			p.ClientEndpoint = "0.0.0.0:16070";
			p.NodeEndpoint = "0.0.0.0:5000";

			using (var master = new NativeMasterServer (p)) { }
		}
	}
}

