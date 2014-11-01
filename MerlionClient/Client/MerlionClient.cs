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
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Threading;

namespace Merlion.Client
{
	public class MerlionClient: MarshalByRefObject
	{
		WebSocket4Net.WebSocket socket;

		byte[] room;
		string hostName;
		int port;
		public string RequestedVersion { get; set; }

		public event EventHandler Connected;
		public event EventHandler Closed;
		public event EventHandler<ErrorEventArgs> Error;
		public event EventHandler<ReceiveEventArgs> Received;

		volatile bool opened = false;

		public MerlionClient (string addr)
		{
			var idx = addr.LastIndexOf (':');

			hostName = idx >= 0 ? addr.Substring(0, idx) : addr;
			port = idx >= 0 ? int.Parse (addr.Substring(idx + 1)) : 16070;

			RequestedVersion = "%";
		}

		public void SetRoom(byte[] room)
		{
			this.room = room == null ? null : (byte[])room.Clone ();
		}

		public byte[] GetRoom()
		{
			return this.room == null ? null : (byte[])this.room.Clone ();
		}

		public void Connect()
		{
			string absPath = string.Format ("/{0}/{1}", RequestedVersion,
				room != null ? Merlion.Utils.HexEncoder.GetString(room) : "");
			string url = string.Format ("wss://{0}:{1}{2}", hostName, port, absPath);
		
			var s = new WebSocket4Net.WebSocket (url, "merlion.yvt.jp", WebSocket4Net.WebSocketVersion.Rfc6455);
			if (Interlocked.Exchange(ref socket, s) != null) {
				throw new InvalidOperationException ("Socket is already open.");
			}

			s.Opened += (sender, e) => {
				opened = true;

				var handler = Connected;
				if (handler != null)
					handler(this, EventArgs.Empty);
			};
			s.Closed += (sender, e) => {
				opened = false;

				var handler = Closed;
				if (handler != null)
					handler(this, EventArgs.Empty);
			};
			s.Error += (sender, e) => {
				Interlocked.CompareExchange(ref socket, null, s);

				var handler = Error;
				if (handler != null)
					handler(this, new ErrorEventArgs(e.Exception));
			};
			s.DataReceived += (sender, e) => {
				var handler = Received;
				if (handler != null)
					handler(this, new ReceiveEventArgs(e.Data));
			};
			s.Open ();
		}

		public void Close()
		{
			var s = Interlocked.Exchange (ref socket, null);
			if (s != null) {
				try { s.Close (); } catch(Exception) { ; }
			}
		}

		// FIXME: how to make this synchronous?
		public void Send(byte[] data, int offset, int length)
		{
			if (data == null)
				throw new ArgumentNullException ("data");
			var s = socket;
			if (s == null) {
				throw new InvalidOperationException ("Socket is closed.");
			} else {
				s.Send (data, offset, length);
			}
		}

		public void Send(byte[] data)
		{
			if (data == null)
				throw new ArgumentNullException ("data");
			Send (data, 0, data.Length);
		}
	}

	public class ErrorEventArgs: EventArgs
	{
		public Exception Exception { get; private set; }
		public ErrorEventArgs(Exception e)
		{
			Exception = e;
		}
	}

	public class ReceiveEventArgs: EventArgs
	{
		public byte[] Data { get; private set; }
		public ReceiveEventArgs(byte[] data)
		{
			Data = data;
		}
	}
}

