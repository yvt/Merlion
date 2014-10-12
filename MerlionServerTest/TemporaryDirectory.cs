using System;
using System.IO;

namespace Merlion.Server.Test
{
	sealed class TemporaryDirectory: IDisposable
	{
		readonly string path;

		static Random r = new Random();

		static string GeneratePath()
		{
			var tmpDir = System.IO.Path.GetTempPath ();
			string path;
			do {
				path = System.IO.Path.Combine(tmpDir, "MerlionServerTest" + r.Next().ToString());
			} while (Directory.Exists (path) || File.Exists (path));
			return path;
		}

		public string Path
		{
			get { return path; }
		}

		public TemporaryDirectory():
		this(GeneratePath()) { }
		public TemporaryDirectory (string path)
		{
			Directory.CreateDirectory (path);
			this.path = path;
		}

		public void Dispose()
		{
			Directory.Delete (path, true);
		}
	}
}

