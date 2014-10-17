using System;
using log4net;
using System.Threading;
using System.Threading.Tasks;

namespace Merlion.Server
{
	sealed class NativeNodeServerManager: IDisposable
	{
		static readonly ILog log = LogManager.GetLogger(typeof(NativeNodeServerManager));
		readonly object sync = new object();

		State state = State.WaitingForReconnect;


		NativeNodeServer server;
		Timer timer;

		readonly string name;
		readonly System.Net.IPEndPoint ep;

		enum State
		{
			Running,
			WaitingForReconnect,
			Disposed
		}

		public NativeNodeServerManager (string name, System.Net.IPEndPoint ep)
		{
			lock (sync) {
				this.ep = ep;
				this.name = name;

				CreateServer ();
			}
		}

		public void Dispose ()
		{
			lock (sync) {
				var oldState = state;
				state = State.Disposed;
				if (oldState == State.Running) {
					var s = server;
					server = null;
					s.Dispose ();
				}
			}
		}

		void CreateServer()
		{
			lock (sync) {
				if (state != State.WaitingForReconnect) {
					return;
				}

				int called = 0;
				Action failureHandler = () => new Task (() => {
					if (System.Threading.Interlocked.Exchange (ref called, 1) == 0) {
						ServerFailed ();
					}
				}).Start ();
				state = State.Running;
				try {
					server = new NativeNodeServer (name, ep);
					server.NodeFailed += (sender, e) => failureHandler ();
					if (server.Failed) {
						failureHandler ();
					}
				} catch (Exception ex) {
					log.Error ("Creating node server failed.", ex);
					failureHandler ();
				}
			}
		}

		void ServerFailed()
		{
			lock (sync) {
				if (state != State.Running) {
					return;
				}

				log.ErrorFormat ("Rescheduling reconnection due to node failure.");
				var s = server;
				server = null;
				new Task (s.Dispose).Start ();

				state = State.WaitingForReconnect;
				timer = new Timer (favoreyo => {
					CreateServer();
				}, null, 5000, Timeout.Infinite);

			}
		}

		public INodeServer NodeServer
		{
			get {
				lock (sync) {
					return server;
				}
			}
		}
	}
}

