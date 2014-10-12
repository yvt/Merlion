using System;
using System.Collections.Specialized;

namespace Merlion.Server.Test
{
	static class TestUtils
	{
		public static void MakeTestServerEnvironment(Action a)
		{
			using (var baseDir = new TemporaryDirectory ()) {
				var settings = new NameValueCollection ();
				settings ["EndpointAddress"] = "0.0.0.0:16070";
				settings ["MasterServerAddress"] = "0.0.0.0:5000";
				settings ["BaseDirectory"] = baseDir.Path;
				
				AppConfiguration.AppSettings = settings;

				a ();
			}
		}

		public static void SetupSelfSignedServerCertificate()
		{
			var settings = AppConfiguration.AppSettings;
			if (settings["MasterServerCertificate"] != null &&
				settings["MasterServerPrivateKey"] != null) {
				return;
			}

			settings ["MasterServerCertificate"] = "cert.crt";
			settings ["MasterServerPrivateKey"] = "cert.key";

			using (var s = System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream
				("Merlion.Server.Test.Resources.TestCertificate.crt"))
			using (var s2 = System.IO.File.OpenWrite(AppConfiguration.MasterServerCertificate)) {
				s.CopyTo (s2);
			}
			using (var s = System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream
				("Merlion.Server.Test.Resources.TestCertificate.key"))
			using (var s2 = System.IO.File.OpenWrite(AppConfiguration.MasterServerPrivateKey)) {
				s.CopyTo (s2);
			}
		}
	}
}

