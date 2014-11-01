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
using System.Threading;
using System.Threading.Tasks;

namespace Merlion.Server
{
	public abstract class MerlionClient: MarshalByRefObject
	{
		protected MerlionClient ()
		{
		}

		public abstract long ClientId { get; }

		public event EventHandler<ReceiveEventArgs> Received;

		public abstract void Send(byte[] data, int offset, int length);
		public void Send(byte[] data)
		{
			Send (data, 0, data.Length);
		}

		class SendResult: IAsyncResult
		{
			readonly MerlionClient client;
			readonly byte[] data;
			readonly int offset;
			readonly int length;
			readonly ManualResetEvent waitHandle = new ManualResetEvent(false);
			readonly object state;
			readonly AsyncCallback callback;
			bool completed = false;
			Exception exception;

			public SendResult(MerlionClient client, byte[] data, int offset, int length, AsyncCallback callback, object state)
			{
				this.client = client;
				this.state = state;
				this.data = data;
				this.offset = offset;
				this.length = length;
				this.callback = callback;

				Task.Factory.StartNew(() => {
					try {
						client.Send(data, offset, length);
						waitHandle.Set();
						callback(this);
					} catch (Exception ex) {
						exception = ex;
						completed = true;
						waitHandle.Set();
						callback(this);
					}
				});
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
					return false;
				}
			}

			public bool IsCompleted {
				get {
					return completed;
				}
			}

			public void End()
			{
				waitHandle.WaitOne ();

				if (exception != null) {
					throw exception;
				}
			}
		}

		public virtual IAsyncResult BeginSend(byte[] data, int offset, int length, AsyncCallback callback, object asyncState)
		{
			return new SendResult (this, data, offset, length, callback, asyncState);
		}

		public virtual void EndSend(IAsyncResult result)
		{
			if (result == null)
				throw new ArgumentNullException ("result");
			var r = (SendResult)result;
			r.End ();
		}

		public virtual Task SendAsync(byte[] data, int offset, int length)
		{
			return Task.Factory.FromAsync ((cb, state) => BeginSend (data, offset, length, cb, state), EndSend, null);
		}
		public Task SendAsync(byte[] data)
		{
			return SendAsync (data, 0, data.Length);
		}

		protected void OnReceived(ReceiveEventArgs args)
		{
			var handler = Received;
			if (handler != null)
				handler (this, args);
		}

		public abstract void Close ();
	}

	[Serializable]
	public class ReceiveEventArgs: EventArgs
	{
		public byte[] Data { get; private set; }
		public ReceiveEventArgs(byte[] data)
		{
			Data = data;
		}
	}
}

