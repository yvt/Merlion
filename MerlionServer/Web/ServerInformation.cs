using System;
using System.Collections.Generic;
using System.Reflection;

namespace Merlion.Server.Web
{
	static class ServerInformation
	{
		public readonly static Dictionary<string, string> Information =
			new Dictionary<string, string>();

		static T GetAssemblyAttribute<T>(Assembly asm) where T : Attribute
		{
			var arr = asm.GetCustomAttributes (typeof(T), true);
			if (arr.Length == 0)
				return null;
			return (T)arr [0];
		}

		static ServerInformation ()
		{
			var asm = Assembly.GetExecutingAssembly ();

			Information.Add ("name", asm.GetName().Name);
			Information.Add ("version", asm.GetName().Version.ToString());
			Information.Add ("copyright", GetAssemblyAttribute<AssemblyCopyrightAttribute>(asm).Copyright);
			Information.Add ("id", GetAssemblyAttribute<AssemblyDescriptionAttribute>(asm).Description);
			Information.Add ("trademark", GetAssemblyAttribute<AssemblyTrademarkAttribute>(asm).Trademark);

			string osName = Environment.OSVersion.ToString ();
			try {
				osName = GetUnixOperatingSystemName().Trim();
			} catch (Exception) {
				// ignore...
			}

			Information.Add ("system", string.Format (
				"{0}, {1}",
				osName,
				Environment.MachineName));
		}

		static string TryFindFile(string name)
		{
			if (System.IO.File.Exists (name))
				return name;
			else
				return null;
		}

		static string GetUnixOperatingSystemName()
		{
			var path = TryFindFile ("/bin/uname")
				?? TryFindFile ("/usr/bin/uname")
				?? TryFindFile ("/usr/local/bin/uname")
				?? TryFindFile ("/sbin/uname")
				?? TryFindFile ("/usr/sbin/uname")
				?? TryFindFile ("/usr/local/sbin/uname");
			if (path == null) {
				throw new PlatformNotSupportedException ();
			}
			var startInfo = new System.Diagnostics.ProcessStartInfo ();
			startInfo.FileName = path;
			startInfo.Arguments = "-a";
			startInfo.UseShellExecute = false;
			startInfo.RedirectStandardInput = true;
			startInfo.RedirectStandardOutput = true;

			var p = System.Diagnostics.Process.Start (startInfo);
			p.StandardInput.Close ();

			var t = p.StandardOutput.ReadToEnd ();
			p.StandardOutput.Close ();

			p.Dispose ();

			return t;
		}
	}
}

