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

		[Test]
		public void Deploy()
		{
			TestUtils.MakeTestServerEnvironment (() => {
				TestUtils.SetupSelfSignedServerCertificate();

				using (var master = new NativeMasterServer ()) {
					var vc = master.VersionManager;
					var ver = "TestApp";
					using (var s = vc.StartDeployVersion(ver)) {
						using (var ss = TestUtils.CreateApplicationPackage(typeof(Merlion.MerEcho.Application))) {
							ss.CopyTo(s);
						}
					}
					vc.EndDeployVersion(ver);
				}
			});
		}
	}
}

