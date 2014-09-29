using System;
using System.IO;
using System.Text;

namespace Merlion.Server.Utils
{
	static class StreamUtils
	{
		public static void Copy(Stream stream, Stream destination, long maximumBytes = long.MaxValue)
		{
			byte[] buffer = new byte[(int)Math.Min(1024L, maximumBytes)];
			int readCount;
			while ((readCount = stream.Read(buffer, 0, (int)Math.Min((long)buffer.Length, maximumBytes))) > 0) {
				destination.Write (buffer, 0, readCount);
				maximumBytes -= readCount;
			}
		}
		public static byte[] ReadAllBytes(Stream stream, int maximumBytes = int.MaxValue)
		{
			using (var ms = new System.IO.MemoryStream()) {
				Copy (stream, ms, maximumBytes);
				return ms.ToArray ();
			}
		}

		static UTF8Encoding utf8 = new UTF8Encoding ();
		public static string ReadAllString(Stream stream, int maximumBytes = int.MaxValue)
		{
			return utf8.GetString (ReadAllBytes (stream, maximumBytes));
		}
	}
}

