using System;
using System.Collections.Specialized;
using System.IO;
using System.Xml;

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

		public static Stream CreateApplicationPackage(Type appType)
		{
			if (!typeof(Application).IsAssignableFrom(appType)) {
				throw new InvalidOperationException(
					string.Format("Type '{0}' is not an Application.", appType.FullName));
			}

			var asm = appType.Assembly;
			var zip = new Ionic.Zip.ZipFile (null, Console.Error);

			zip.AddFile (asm.Location, "");

			zip.AddEntry ("MerlionApplicaiton.config", (ename, stream) => {
				var xd = new XmlDocument();

				var e = xd.CreateElement("MerlionApplication");
				e.SetAttribute("TypeName", appType.FullName);
				xd.AppendChild(e);

				xd.Save(stream);
			});

			var ms = new System.IO.MemoryStream ();
			try {
				zip.Save(ms);
			} catch {
				ms.Dispose ();
				throw;
			}
			ms.Position = 0;

			return ms;
		}
	}
}

