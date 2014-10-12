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
			TestUtils.MakeTestServerEnvironment (() => {
				TestUtils.SetupSelfSignedServerCertificate();

				using (var master = new NativeMasterServer ()) {
				}
			});
		}
	}
}

