using System;
using System.IO;
using System.Threading;

namespace Merlion.Server.Utils
{
	sealed class StreamAsyncCopier
	{
		readonly Stream source;
		readonly Stream destination;
		readonly byte[] buffer;
		readonly bool flush;

		public StreamAsyncCopier (Stream source, Stream destination, int bufferSize, bool flush)
		{
			this.destination = destination;
			this.source = source;
			if (destination == null)
				throw new ArgumentNullException ("destination");
			if (source == null)
				throw new ArgumentNullException ("source");

			this.buffer = new byte[bufferSize];
			this.flush = flush;
		}

		sealed class AsyncResult: IAsyncResult
		{
			readonly Stream source;
			readonly Stream destination;
			readonly byte[] buffer;
			readonly bool flush;
			readonly AsyncCallback callback;
			readonly object state;
			readonly EventWaitHandle waitHandle = new EventWaitHandle(false, EventResetMode.ManualReset);
			volatile Exception exception;
			volatile bool completedSynchronously = true;
			volatile bool done = false;

			long totalBytes = 0;

			public AsyncResult(StreamAsyncCopier copier, AsyncCallback callback, object state)
			{
				this.state = state;
				this.callback = callback;

				source = copier.source;
				destination = copier.destination;
				buffer = copier.buffer;
				flush = copier.flush;

				DoCopy();
			
				completedSynchronously = false;
			}

			void DoCopy()
			{
				try {
					source.BeginRead(buffer, 0, buffer.Length, (result) => {
						try {
							int readCount = source.EndRead(result);
							totalBytes += readCount;
							if (readCount == 0) {
								Done(null);
							} else {
								destination.BeginWrite(buffer, 0, readCount, (result2) => {
									try {
										destination.EndWrite(result2);
										if (flush)
											destination.Flush();

										DoCopy();
									} catch (Exception ex) {
										Done(ex);
									}
								}, null);
							}
						} catch (Exception ex) {
							Done(ex);
						}
					}, null);
				} catch (Exception ex) {
					Done (ex);
				}
			}

			void Done(Exception ex)
			{
				exception = ex;
				done = true;
				waitHandle.Set ();
				if (callback != null)
					callback (this);
			}

			public object AsyncState {
				get {
					return state;
				}
			}

			public WaitHandle AsyncWaitHandle {
				get {
					return waitHandle;
				}
			}

			public bool CompletedSynchronously {
				get {
					return completedSynchronously;
				}
			}

			public bool IsCompleted {
				get {
					return done;
				}
			}

			public long End()
			{
				if (!done)
					throw new InvalidOperationException ("Operation is not completed.");
				if (exception != null)
					throw exception;
				return totalBytes;
			}
		}

		public IAsyncResult BeginCopy(AsyncCallback callback, object state)
		{
			return new AsyncResult (this, callback, state);
		}

		public long EndCopy(IAsyncResult result)
		{
			return ((AsyncResult)result).End ();
		}
	}
}

