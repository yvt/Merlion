/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

