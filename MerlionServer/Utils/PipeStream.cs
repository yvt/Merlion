using System;
using System.IO;
using Mono.Unix;
using System.IO.Pipes;
using System.Reflection;

namespace Merlion.Server.Utils
{
	public abstract class PipeStream: Stream
	{
		public Stream ReadStream { get; protected set; }
		public Stream WriteStream { get; protected set; }
		public object OtherEndHandle { get; protected set; }

		protected PipeStream ()
		{
		}


		public override void Close ()
		{
			ReadStream.Close ();
			WriteStream.Close ();
			base.Close ();
		}

		static bool IsMono
		{
			get {
				return Type.GetType ("Mono.Runtime") != null;
			}
		}

		static readonly Func<PipeStream> createNew;
		static readonly Func<object, PipeStream> createWithHandle;

		static PipeStream()
		{
			Type type;
			if (IsMono) {
				type = Type.GetType ("Merlion.Server.Utils.MonoPipeStream", true);
			} else {
				throw new NotImplementedException ();
			}

			createNew = (Func<PipeStream>)type.GetMethod ("CreateNew")
				.CreateDelegate (typeof(Func<PipeStream>));

			createWithHandle = (Func<object, PipeStream>)type.GetMethod ("CreateWithHandle")
				.CreateDelegate (typeof(Func<object, PipeStream>));

		}

		public static PipeStream CreateNew()
		{
			return createNew (); 
		}

		public static PipeStream CreateWithHandle(object handle)
		{
			return createWithHandle (handle);
		}

		#region implemented abstract members of Stream

		public override void Flush ()
		{
			WriteStream.Flush ();
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			return ReadStream.Read (buffer, offset, count);
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
			WriteStream.Write (buffer, offset, count);
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

	public sealed class PipeStreamFactory: MarshalByRefObject
	{
		public PipeStream CreateNew()
		{
			return PipeStream.CreateNew ();
		}

		public PipeStream CreateWithHandle(object handle)
		{
			return PipeStream.CreateWithHandle (handle);
		}
	}

	[Serializable]
	public sealed class MonoPipeStreamEndpoint
	{
		public int ReadingHandle;
		public int WritingHandle;
	}

	sealed class MonoPipeStream: PipeStream
	{
		Mono.Unix.UnixPipes pipe1;
		Mono.Unix.UnixPipes pipe2;

		MonoPipeStream(Stream reading, Stream writing, MonoPipeStreamEndpoint other)
		{
			ReadStream = reading;
			WriteStream = writing;
			OtherEndHandle = other;
		}

		public new static PipeStream CreateNew()
		{
			var pipe1 = Mono.Unix.UnixPipes.CreatePipes ();
			var pipe2 = Mono.Unix.UnixPipes.CreatePipes ();
			var s = new MonoPipeStream (pipe1.Reading, pipe2.Writing,
				new MonoPipeStreamEndpoint () { 
					ReadingHandle = pipe2.Reading.Handle,
					WritingHandle = pipe1.Writing.Handle
				});

			// Prevent from GC
			s.pipe1 = pipe1;
			s.pipe2 = pipe2;
			return s;
		}

		public new static PipeStream CreateWithHandle(object handle)
		{
			if (handle == null)
				throw new ArgumentNullException ("handle");
			var ep = handle as MonoPipeStreamEndpoint;
			if (ep == null)
				throw new ArgumentException ("Not MonoPipeStreamEndpoint", "handle");
			return new MonoPipeStream (
				new UnixStream (ep.ReadingHandle, true), 
				new UnixStream (ep.WritingHandle, true), null);
		}
	}

}

