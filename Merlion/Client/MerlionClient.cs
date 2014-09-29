using System;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Threading;

namespace Merlion.Client
{
	public class MerlionClient: MarshalByRefObject
	{
		IPEndPoint endpoint;

		TcpClient client;
		SslStream ssl;
		byte[] room;
		string hostName;
		int port;

		public MerlionClient (string addr)
		{
			var parts = addr.Split (':');
			if (parts.Length > 2) {
				throw new ArgumentException ("Invalid address.", "addr");
			}

			hostName = parts [0];
			port = parts.Length >= 2 ? int.Parse (parts [1]) : 16070;
		}
		public MerlionClient (IPEndPoint endpoint)
		{
			if (endpoint == null)
				throw new ArgumentNullException ("endpoint");
			this.endpoint = endpoint;
		}

		public IPEndPoint EndPoint
		{
			get { return endpoint; }
			set { endpoint = value; }
		}

		public string HostName
		{
			get { return hostName; }
			set { hostName = value; }
		}

		public void SetRoom(byte[] room)
		{
			this.room = room == null ? null : (byte[])room.Clone ();
		}

		public byte[] GetRoom()
		{
			return this.room == null ? null : (byte[])this.room.Clone ();
		}

		sealed class ConnectAsyncResult: IAsyncResult
		{

			readonly MerlionClient client;
			readonly AsyncCallback callback;
			readonly object state;
			readonly EventWaitHandle waitHandle = new EventWaitHandle (false, EventResetMode.ManualReset);

			bool completed = false;
			Exception exception = null;
			bool synchronized = true;
			byte[] prologue;

			public ConnectAsyncResult(MerlionClient client, AsyncCallback callback, object state)
			{
				this.client = client;
				this.callback = callback;
				this.state = state;

				try {
					if (client.client != null) {
						throw new InvalidOperationException("Client is already connected or connecting now.");
					}

					using (var ms = new System.IO.MemoryStream()) {
						ms.Capacity = 2048;
						byte[] room = client.room ?? new byte[0];
						var bw = new System.IO.BinaryWriter(ms);
						bw.Write((uint)0x918123ab); // magic
						bw.Write(room.Length);
						bw.Write(room, 0, room.Length);

						if (ms.Length <= 2048) {
							ms.SetLength(2048);
						} else {
							throw new InvalidOperationException("Room ID is too long.");
						}

						prologue = ms.ToArray();
					}

					client.client = new TcpClient();

					if (client.endpoint != null) {
						client.client.BeginConnect(client.endpoint.Address, client.endpoint.Port, HandleTcpConnectDone, null);
					} else {
						Dns.BeginGetHostAddresses(client.hostName, HandleDnsDone, null);
					}

					synchronized = false;
				} catch (Exception ex) {
					Done(ex);
				}
			}

			void HandleDnsDone(IAsyncResult result)
			{
				try {
					var addrs = Dns.EndGetHostAddresses(result);
					client.client.BeginConnect(addrs, client.port, HandleTcpConnectDone, null);
				} catch (Exception ex) {
					Done(ex);
				}
			}

			void HandleTcpConnectDone(IAsyncResult result)
			{
				try {
					client.client.EndConnect(result);

					client.ssl = new SslStream(client.client.GetStream(), false, 
						HandleRemoteCertificateValidationCallback);
					client.ssl.BeginAuthenticateAsClient(client.hostName, null, 
						System.Security.Authentication.SslProtocols.Tls, false,
						HandleSslAuthenticateDone, null);
				} catch (Exception ex) {
					Done(ex);
				}
			}

			bool HandleRemoteCertificateValidationCallback (object sender, 
				System.Security.Cryptography.X509Certificates.X509Certificate certificate, 
				System.Security.Cryptography.X509Certificates.X509Chain chain, 
				SslPolicyErrors sslPolicyErrors)
			{
				return true;
			}

			void HandleSslAuthenticateDone(IAsyncResult result)
			{
				try {
					client.ssl.EndAuthenticateAsClient(result);

					client.ssl.BeginWrite(prologue, 0, prologue.Length, HandlePrologueWrote, null);
				} catch (Exception ex) {
					Done(ex);
				}
			}

			void HandlePrologueWrote(IAsyncResult result)
			{
				try {
					client.ssl.EndWrite(result);
					client.ssl.Flush();

					var buffer = new byte[1];
					client.ssl.BeginRead(buffer, 0, 1, HandlePrologueResult, buffer);
				} catch (Exception ex) {
					Done(ex);
				}
			}

			void HandlePrologueResult(IAsyncResult result)
			{
				try {
					int readCount = client.ssl.EndRead(result);
					var buffer = (byte[])result.AsyncState;

					if (readCount < 1){
						throw new MerlionException("Disconnected while reading a header.");
					}

					var presult = (Protocol.PrologueResult)buffer[0];
					switch (presult) {
					case Protocol.PrologueResult.Success:
						Done(null);
						break;
					case Protocol.PrologueResult.ServerFull:
						throw new ServerFullException();
					case Protocol.PrologueResult.ProtocolError:
						throw new ProtocolErrorException();
					default:
						throw new ProtocolErrorException ();
					}
				} catch (Exception ex) {
					Done(ex);
				}
			}

			void Done(Exception ex)
			{
				exception = ex;
				if (ex != null) {
					client.client = null;
					client.ssl = null;
				}
				completed = true;
				waitHandle.Set ();
				if (callback != null)
					callback (this);
			}

			public void End()
			{
				AsyncWaitHandle.WaitOne ();

				if (completed) {
					if (exception != null) {
						throw exception;
					}
				} else {
					throw new InvalidOperationException ("Connect operation is not yet completed.");
				}
			}

			#region IAsyncResult implementation

			public object AsyncState {
				get {
					return state;
				}
			}

			public System.Threading.WaitHandle AsyncWaitHandle {
				get {
					return waitHandle;
				}
			}

			public bool CompletedSynchronously {
				get {
					return synchronized;
				}
			}

			public bool IsCompleted {
				get {
					return completed;
				}
			}

			#endregion
		}

		public IAsyncResult BeginConnect(AsyncCallback callback, object state)
		{
			return new ConnectAsyncResult (this, callback, state);
		}

		public void EndConnect(IAsyncResult result)
		{
			((ConnectAsyncResult)result).End ();
		}

		public void Connect()
		{
			EndConnect (BeginConnect (null, null));
		}

		public void Close()
		{
			var s = ssl;
			if (s == null) {
				return;
			}
			s.Close ();
			ssl = null;
			client = null;
		}

		public System.IO.Stream Stream
		{
			get {
				var s = ssl;
				if (!s.IsAuthenticated) {
					return null;
				}
				return s;
			}
		}
	}
}

