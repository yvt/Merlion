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
using System.Collections;

namespace Merlion.Server.Utils
{
	sealed class StreamSplitter: MarshalByRefObject, IEnumerable<Stream>
	{
		Stream stream;
		Enumerator enumerator;
		byte[] separator;

		public StreamSplitter (Stream stream, byte[] separator)
		{
			if (separator == null)
				throw new ArgumentNullException ("separator");
			if (stream == null)
				throw new ArgumentNullException ("stream");
			if (separator.Length == 0)
				throw new ArgumentException ("Separator cannot be empty.", "separator");
			if (!stream.CanRead)
				throw new ArgumentException ("Stream must be readable.", "stream");

			this.stream = stream;
			this.separator = separator;

			if (separator.Length > 65536)
				throw new NotSupportedException ("Separator is too long.");
		}

		public void IgnoreRemainingSeparators()
		{
			if (enumerator == null)
				throw new InvalidOperationException ("Enumeration is not started yet.");
			enumerator.IgnoreRemainingSeparators ();
		}

		class Enumerator: MarshalByRefObject, IEnumerator<Stream>, IEnumerator
		{
			readonly Stream stream;
			readonly byte[] sep;
			readonly ByteArraySearch search;

			readonly byte[] buffer;
			int bufferPos = 0;
			int bufferLen = 0;
			int nextSplit;
			int readLimit;

			bool ignoreSeparator = false;
			bool reachedEof = false;
			bool readingDirectly = false;

			bool started = false;
			PartStream current;

			public Enumerator(StreamSplitter spl)
			{
				stream = spl.stream;
				sep = spl.separator;
				buffer = new byte[Math.Max(2048, sep.Length * 4)];
				search = new ByteArraySearch(sep);
			}

			public void IgnoreRemainingSeparators()
			{
				ignoreSeparator = true;
				nextSplit = -1;
				readLimit = bufferLen;
			}


			void FillBuffer()
			{
				int head = bufferLen - bufferPos;
				Buffer.BlockCopy (buffer, bufferPos, buffer, 0, head);

				bufferLen = head;
				bufferPos = 0;

				if (ignoreSeparator) {
					readLimit = bufferLen;
					return;
				}

				int readCount = 0;
				while (bufferLen < buffer.Length && (readCount = stream.Read (buffer, bufferLen, buffer.Length - bufferLen)) > 0) {
					var start = Math.Max(bufferLen - sep.Length + 1, 0);
					var end = bufferLen + readCount;
					nextSplit = search.IndexOf(buffer, start, end - start);
					if (nextSplit != -1) {
						bufferLen += readCount;
						readLimit = Math.Max(bufferLen - sep.Length + 1, nextSplit + sep.Length);
						return;
					}
					bufferLen += readCount;
				}

				if (readCount == 0 && bufferLen < buffer.Length) {
					reachedEof = true;
				}
					
				if (bufferLen < buffer.Length) {
					readLimit = bufferLen;
				} else {
					readLimit = bufferLen - sep.Length + 1;
				}
			}

			void FindNextSplit()
			{
				if (ignoreSeparator)
					nextSplit = -1;
				else
					nextSplit = search.IndexOf(buffer, bufferPos, bufferLen - bufferPos);
			}

			public bool MoveNext ()
			{
				if (!started) {
					FillBuffer ();
					started = true;
					current = new PartStream (this);
					return true;
				} else {
					// Already end?
					if (current == null) {
						return false;
					}

					if (ignoreSeparator) {
						return false;
					}

					// Find end
					while (Read (null, 0, 1024 * 1024) > 0) { }
					current.Invalidate ();

					if (bufferPos == nextSplit) {
						bufferPos += sep.Length;
						FindNextSplit ();
						current = new PartStream (this);
						return true;
					} else {
						current = null;
						return false;
					}
				}
			}

			public Stream Current {
				get {
					if (!started) {
						throw new InvalidOperationException ("MoveNext is not called yet.");
					}
					if (current == null) {
						throw new InvalidOperationException ("No more items.");
					}
					return current;
				}
			}
			object IEnumerator.Current {
				get {
					return this.Current;
				}
			}

			public void Reset ()
			{
				throw new NotSupportedException ();
			}


			public void Dispose ()
			{
			}

			int Read(byte[] buf, int offset, int length)
			{
				int readCount = 0;
				while (length > 0) {
					if (readingDirectly) {
						if (buf == null) {
							return 0;
						}
						readCount += stream.Read (buf, offset, length);
						return readCount;
					}

					int limit = nextSplit != -1 ? Math.Min (readLimit, nextSplit) : readLimit;
					int cnt = Math.Min (length, limit - bufferPos);

					if (cnt <= 0) {
						if (bufferPos == nextSplit || (bufferPos >= bufferLen && reachedEof)) {
							return readCount;
						}
						if (bufferPos == bufferLen && ignoreSeparator) {
							readingDirectly = true;
						} else {
							FillBuffer ();
						}
					} else {
						if (buf != null) {
							Buffer.BlockCopy (buffer, bufferPos, buf, offset, cnt);
							offset += cnt;
						}
						readCount += cnt;
						length -= cnt;
						bufferPos += cnt;
					}
				}
				return readCount;
			}

			sealed class PartStream: Stream
			{
				Enumerator e;

				public PartStream(Enumerator e)
				{
					this.e = e;
				}

				public void Invalidate()
				{
					e = null;
				}
				public override void Close ()
				{
					Invalidate ();
				}

				public override int Read (byte[] buffer, int offset, int count)
				{
					if (buffer == null)
						throw new ArgumentNullException ("buffer");
					if (e == null)
						throw new ObjectDisposedException ("PartStream");
					if (count == 0)
						return 0;
					if (offset < 0 || offset + count > buffer.Length)
						throw new ArgumentOutOfRangeException ("offset");
					if (count < 0)
						throw new ArgumentOutOfRangeException ("count");
					return e.Read (buffer, offset, count);
				}

				public override void Flush ()
				{ }
				public override long Seek (long offset, SeekOrigin origin)
				{ throw new NotSupportedException (); }
				public override void SetLength (long value)
				{ throw new NotSupportedException (); }
				public override void Write (byte[] buffer, int offset, int count)
				{ throw new NotSupportedException (); }
				public override bool CanRead {
					get { return true; }
				}
				public override bool CanSeek {
					get { return false; }
				}
				public override bool CanWrite {
					get { return false; }
				}
				public override long Length {
					get { throw new NotSupportedException (); }
				}
				public override long Position {
					get { throw new NotSupportedException (); }
					set { throw new NotSupportedException (); }
				}
			}
		}

		public IEnumerator<Stream> GetEnumerator ()
		{
			if (enumerator != null) {
				throw new InvalidOperationException ("Cannot enumerate twice.");
			}

			enumerator = new Enumerator (this);

			return enumerator;
		}

		System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator ()
		{
			foreach (var s in this) yield return s;
		}
	}
}

