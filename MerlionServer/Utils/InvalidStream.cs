using System;
using System.IO;

namespace Merlion.Server.Utils
{
	public class InvalidStream: Stream
	{
		public InvalidStream ()
		{
		}

		#region implemented abstract members of Stream

		public override void Flush ()
		{
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			throw new IOException ("Stream is invalid.");
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
			throw new IOException ("Stream is invalid.");
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
				return true;
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

		#endregion
	}
}

