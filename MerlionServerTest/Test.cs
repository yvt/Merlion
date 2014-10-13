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
		[Test ()]
		public void CreateAndWaitForSomeTime ()
		{
			TestUtils.MakeTestServerEnvironment (() => {
				TestUtils.SetupSelfSignedServerCertificate();

				using (var master = new NativeMasterServer ()) {
					System.Threading.Thread.Sleep(2000);
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

	[TestFixture]
	public class NodeTest
	{
		[Test]
		public void Create()
		{
			TestUtils.MakeTestServerEnvironment (() => {
				using (var node = new NativeNodeServer ("TestNode",
					new System.Net.IPEndPoint(System.Net.IPAddress.Loopback, 5000))) {
				}
			});
		}
		[Test]
		public void CreateAndWaitForSomeTime()
		{
			TestUtils.MakeTestServerEnvironment (() => {
				using (var node = new NativeNodeServer ("TestNode",
					new System.Net.IPEndPoint(System.Net.IPAddress.Loopback, 5000))) {
					System.Threading.Thread.Sleep(2000);
				}
			});
		}
	}
}

