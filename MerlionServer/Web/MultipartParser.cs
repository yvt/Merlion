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
using System.Collections.Generic;

namespace Merlion.Server.Web
{
	static class MultipartParser
	{
		public sealed class Item: IDisposable
		{
			public Item(IDictionary<string, string> headers, 
				Stream stream, string fileName)
			{
				this.Headers = headers;
				this.Stream = stream;
				this.fileName = fileName;
			}

			public readonly IDictionary<string, string> Headers;

			public readonly Stream Stream;

			string fileName;

			public string Name
			{
				get {
					string disp;
					if (Headers.TryGetValue("Content-Disposition", out disp)) {
						var parts = disp.Split (';');
						foreach (var part in parts) {
							var p = part.Trim ();
							if (p.StartsWith("name=",
								StringComparison.InvariantCultureIgnoreCase)) {
								p = p.Substring (5);
								if (p.StartsWith ("\"", StringComparison.InvariantCulture) &&
									p.EndsWith ("\"", StringComparison.InvariantCulture))
									p = p.Substring (1, p.Length - 2);
								return p;
							}
						}
					}
					return string.Empty;
				}
			}

			public void Dispose ()
			{
				Stream.Dispose ();
				if (fileName != null) {
					try {
						System.IO.File.Delete(fileName);
					} catch(Exception) {}
				}
			}
		}

		static System.Text.UTF8Encoding utf8 = new System.Text.UTF8Encoding();
		static readonly byte[] crlf;

		static MultipartParser()
		{
			crlf = utf8.GetBytes("\r\n");
		}


		static string GetBoundary(string contentType)
		{
			var parts = contentType.Split(';');
			foreach (var part in parts) {
				var p = part.Trim ();
				if (p.StartsWith("boundary=",
					StringComparison.InvariantCulture)) {
					return "\r\n--" + p.Substring (9);
				}
			}

			throw new InvalidOperationException ("Boundary is not specified.");
		}

		static KeyValuePair<string, string>? ReadHeader(Stream stream)
		{
			var bytes = Utils.StreamUtils.ReadAllBytes (stream);
			if (bytes.Length == 0) {
				return null;
			}
			var text = utf8.GetString (bytes);
			if (text == "--") {
				return new KeyValuePair<string, string>("--", "");
			}
			var index = text.IndexOf (':');
			if (index == -1) {
				throw new System.IO.InvalidDataException ("Invalid header.");
			}
			return new KeyValuePair<string, string> (
				text.Substring(0, index).Trim(),
				text.Substring(index + 1).Trim()
			);
		}

		public static Item[] Parse(Stream stream, byte[] boundary)
		{
			if (stream == null)
				throw new ArgumentNullException ("stream");
			if (boundary == null)
				throw new ArgumentNullException ("boundary");

			var splitter = new Utils.StreamSplitter (stream, boundary);
			var list = new List<Item> ();

			try {
				bool first = true;

				foreach (var part in splitter) {
					if (first) {
						first = false;
						// skip first boundary
						byte[] buf = new byte[boundary.Length - 2];
						part.Read(buf, 0, buf.Length);
					}

					var dic = new Dictionary<string, string>();
					var tmpFileName = System.IO.Path.GetTempFileName();
					var tmpStream = System.IO.File.Open(tmpFileName, FileMode.Open, FileAccess.ReadWrite,
						FileShare.Delete | FileShare.Inheritable );
					var item = new Item(dic, tmpStream, tmpFileName);
					list.Add(item);

					bool doneHeader = false;
					var splitter2 = new Utils.StreamSplitter (part, crlf);
					var firstHeader = true;
					foreach (var e in splitter2) {
						if (doneHeader) {
							// Reading contents.
							splitter2.IgnoreRemainingSeparators();
							Utils.StreamUtils.Copy(e, tmpStream);
						} else {
							var header = ReadHeader(e);
							if (header.HasValue) {
								if (firstHeader && header.Value.Key == "--") {
									// End of multipart.
									return list.ToArray();
								}
								dic.Add(header.Value.Key, header.Value.Value);
							} else if (!firstHeader) {
								doneHeader = true;
							}
							firstHeader = false;
						}
					}

					tmpStream.Seek(0, SeekOrigin.Begin);
				}
			} catch {
				foreach (var item in list)
					item.Dispose ();
				throw;
			}
			return list.ToArray ();
		}

		public static Item[] Parse(Stream stream, string contentType)
		{
			if (stream == null)
				throw new ArgumentNullException ("stream");

			var boundary = GetBoundary (contentType);
			return Parse (stream, utf8.GetBytes(boundary));
		}
	}
}

