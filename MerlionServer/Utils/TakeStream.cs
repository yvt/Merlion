using System;
using System.IO;

namespace Merlion.Server.Utils
{
	sealed class TakeStream: Stream
	{
		Stream baseStream;
		long remainingBytes;

		public TakeStream (Stream baseStream, long numBytes)
		{
			this.baseStream = baseStream;
			remainingBytes = numBytes;
		}

		public override void Flush ()
		{
			baseStream.Flush ();
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			count = (int)Math.Min ((long)count, remainingBytes);
			var readCount = baseStream.Read (buffer, offset, count);
			remainingBytes -= readCount;
			return readCount;
		}

		public override long Seek (long offset, SeekOrigin origin)
		{
			throw new NotSupportedException ();
		}

		public override void SetLength (long value)
		{
			throw new NotSupportedException ();
		}

		public override void Write (byte[] buffer, int offset, int count)
		{
			throw new NotSupportedException ();
		}

		public override bool CanRead {
			get {
				return true;
			}
		}

		public override bool CanSeek {
			get {
				return false;
			}
		}

		public override bool CanWrite {
			get {
				return false;
			}
		}

		public override long Length {
			get {
				throw new NotSupportedException ();
			}
		}

		public override long Position {
			get {
				throw new NotSupportedException ();
			}
			set {
				throw new NotSupportedException ();
			}
		}

	}
}

