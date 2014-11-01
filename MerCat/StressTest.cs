using System;
using System.Threading;
using System.Collections.Generic;
using Merlion.Client;
using System.Linq;
using System.Threading.Tasks;

namespace Merlion.MerCat
{
	sealed class StressTest
	{
		readonly object sync = new object ();
		int running = 0;
		int stopped = 0;
		readonly ManualResetEvent stopEvent = new ManualResetEvent (false);
		byte[] sharedBuffer = new byte[65536];

		#region Worker

		sealed class Worker
		{
			static long nextWorkerId = 0;
			readonly public long WorkerId;
			readonly public StressTest Test;
			public MerlionClient Client;

			readonly object sync = new object ();
			TimeSpan timeoutTime;

			public enum WorkerState
			{
				NotStarted,
				Connecting,
				Running,
				Failed
			}

			public WorkerState State { get; private set; }

			public Worker (StressTest test)
			{
				Test = test;
				WorkerId = Interlocked.Increment (ref nextWorkerId);
				State = WorkerState.NotStarted;
				ResetTimeout();
			}

			public void CheckTimeout ()
			{
				lock (sync) {
					if (Test.RunningTime < timeoutTime)
						return;
				}
				Fail (new TimeoutException ());
			}

			void ResetTimeout()
			{
				lock (sync) {
					timeoutTime = Test.RunningTime + MainClass.cmdArgs.TimeoutTime;
				}
			}

			public void Abort()
			{
				Fail (new TestAbortedException ());
			}

			void Fail (Exception ex)
			{
				lock (sync) {
					if (State == WorkerState.Failed) {
						return;
					}
					State = WorkerState.Failed;
				}
				var exceptionType = ex.GetType ();
				var innerType = ex.InnerException != null ?
					ex.InnerException.GetType () : null;
				var msg = ex.Message;
				for (var e = ex.InnerException; e != null; e = e.InnerException)
					msg = msg + " --> " + e.Message;

				Test.CollectFailReason (new WorkerFailReason (exceptionType, innerType, msg));

				// Clean-up the worker
				if (Client != null) {
					try {
						Client.Close();
					} catch (Exception) {
						// a banana in my ear
					}
				}

				// Do something
				Test.WorkerFailed (this);
			}

			public void Start ()
			{
				lock (sync) {
					if (State == WorkerState.Failed) {
						return;
					}
					if (State != WorkerState.NotStarted) {
						throw new InvalidOperationException ();
					}
					try {
						Client = new MerlionClient (MainClass.cmdArgs.Address);
						lock (sync) {
							State = WorkerState.Connecting;
							Client.Connected += HandleConnectDone;
							Client.Error += HandleError;
							Client.Received += HandleReceived;
							Client.Connect();
						}
					} catch (Exception ex) {
						// Worker failed before it gets working.
						Fail (ex);
					}
				}
			}

			void HandleConnectDone (object sender, EventArgs args)
			{
				try {
					lock (sync) {
						if (State == WorkerState.Failed) {
							// FIXME: leaving EndConnect not called is not preferrable
							return;
						}
						ResetTimeout();

						State = WorkerState.Running;

						SendPacket();
					}
				} catch (Exception ex) {
					Fail (ex);
				}
			}

			void HandleError (object sender, ErrorEventArgs args)
			{
				Fail (args.Exception);
			}

			async Task SendPacket()
			{
				try {
					long sentCount = 0;
					while (true) {
						int remainingBytes = MainClass.cmdArgs.ChunkSize;

						lock (sync) {
							if (State == WorkerState.Failed) {
								return;
							}
						}

						Test.MarkSentPacket();
						while (remainingBytes > 0) {
							var count = Math.Min(Test.sharedBuffer.Length, remainingBytes);
							Client.Send(Test.sharedBuffer, 0, count); // FIXME: is this asynchronous?
							remainingBytes -= count;

							lock (sync) {
								if (State == WorkerState.Failed) {
									return;
								}
							}
						}
						++sentCount;

						if (sentCount >= MainClass.cmdArgs.SentCountLimit) {
							throw new SentPacketCountLimitReachedException();
						}

						await Task.Delay(MainClass.cmdArgs.Interval);
					}
				} catch (Exception ex) {
					Fail (ex);
					return;
				}
			}

			int receivedBytes = 0;
			void HandleReceived(object sender, ReceiveEventArgs args)
			{
				lock (sync) {
					if (State == WorkerState.Failed) {
						return;
					}

					receivedBytes += args.Data.Length;
					while (receivedBytes >= MainClass.cmdArgs.ChunkSize) {
						receivedBytes -= MainClass.cmdArgs.ChunkSize;
						Test.MarkReceivedPacket ();
					}
				}

			}

		}

		readonly Dictionary<long, Worker> workers = new Dictionary<long, Worker> ();

		void WorkerFailed(Worker worker)
		{
			lock (workers) {
				workers.Remove (worker.WorkerId);
			}

			if (!IsRunning) {
				return;
			}

			switch (MainClass.cmdArgs.ErrorMode) {
			case ErrorMode.Respawn:
				SpawnLater ();
				break;
			case ErrorMode.LeaveClosed:
				if (ActiveWorkerCount == 0) {
					// No workers left
					Abort ();
				}
				break;
			case ErrorMode.Fatal:
				Abort ();
				break;
			default:
				throw new InvalidOperationException ();
			}
		}

		async Task SpawnLater()
		{
			// FIXME: this should use other parameter
			await Task.Delay (MainClass.cmdArgs.SpawnInterval);
			SpawnWorker ();
		}

		#endregion

		#region Spawning Workers

		void SpawnWorker ()
		{
			var worker = new Worker (this);
			Interlocked.Increment (ref totalWorkerCount);

			lock (workers) {
				workers.Add (worker.WorkerId, worker);
				worker.Start ();
			}
		}

		Timer spawnerTimer;
		long spawnCount = 0;

		void SpawnInitialWorker()
		{
			var ret = Interlocked.Increment (ref spawnCount);
			if (ret > MainClass.cmdArgs.Concurrency) {
				spawnerTimer.Dispose ();
				return;
			}
			SpawnWorker ();
		}

		#endregion

		#region Timeout

		Timer timeoutCheckerTimer;

		void TimeoutChecker()
		{
			KeyValuePair<long, Worker>[] workerArray;
			lock (workers) {
				workerArray = workers.ToArray ();
			}
			foreach (var worker in workerArray)
				worker.Value.CheckTimeout ();
		}

		#endregion

		#region Statistics

		readonly System.Diagnostics.Stopwatch runningTimeStopwatch = new System.Diagnostics.Stopwatch ();
		long totalWorkerCount = 0;
		readonly WorkerFailReasonCollector failReasonCollector = new WorkerFailReasonCollector();
		long sentPacketCount = 0;
		long receivedPacketCount = 0;

		public TimeSpan RunningTime {
			get {
				return runningTimeStopwatch.Elapsed;
			}
		}

		public bool IsRunning {
			get {
				return !stopEvent.WaitOne (0);
			}
		}

		public int ActiveWorkerCount {
			get {
				lock (workers)
					return workers.Count;
			}
		}

		public long TotalWorkerCount {
			get {
				return Interlocked.Read (ref totalWorkerCount);
			}
		}

		public long SentPacketCount {
			get {
				return Interlocked.Read (ref sentPacketCount);
			}
		}

		public long ReceivedPacketCount {
			get {
				return Interlocked.Read (ref receivedPacketCount);
			}
		}

		public Tuple<WorkerFailReason, long>[] GetWorkerFailReasons()
		{
			return failReasonCollector.ToArray ();
		}

		void CollectFailReason(WorkerFailReason reason)
		{
			if (!IsRunning)
				return;
			failReasonCollector.Collect (reason);
		}

		void MarkSentPacket()
		{
			Interlocked.Increment (ref sentPacketCount);
		}

		void MarkReceivedPacket()
		{
			Interlocked.Increment (ref receivedPacketCount);
		}

		#endregion

		public event EventHandler Aborted;

		public void PerformTest ()
		{
			if (Interlocked.Exchange (ref running, 1) != 0) {
				throw new InvalidOperationException ();
			}

			runningTimeStopwatch.Start ();

			spawnerTimer = new Timer (state => SpawnInitialWorker());
			spawnerTimer.Change (0, MainClass.cmdArgs.SpawnInterval);

			timeoutCheckerTimer = new Timer (state => TimeoutChecker());
			timeoutCheckerTimer.Change (200, 1000);
		}

		public void Abort ()
		{
			if (Interlocked.Exchange (ref stopped, 1) != 0) {
				// Already stopped.
				return;
			}

			stopEvent.Set ();

			var waitEvent = new ManualResetEvent (false);
			spawnerTimer.Dispose (waitEvent);
			waitEvent.WaitOne ();

			waitEvent = new ManualResetEvent (false);
			timeoutCheckerTimer.Dispose (waitEvent);
			waitEvent.WaitOne ();

			runningTimeStopwatch.Stop ();

			KeyValuePair<long, Worker>[] workerArray;
			lock (workers) {
				workerArray = workers.ToArray ();
			}
			foreach (var worker in workerArray)
				worker.Value.Abort ();

			var handler = Aborted;
			if (handler != null)
				handler (this, EventArgs.Empty);
		}

	}

	[Serializable]
	sealed class WorkerFailReason
	{
		public readonly Type ExceptionType;
		public readonly Type InnerExceptionType;
		public readonly string Message;

		public WorkerFailReason (Type exceptionType, Type innerExceptionType, string message)
		{
			this.ExceptionType = exceptionType;
			this.InnerExceptionType = innerExceptionType;
			this.Message = message;
		}

		public override bool Equals (object obj)
		{
			if (obj == null)
				return false;
			if (ReferenceEquals (this, obj))
				return true;
			if (obj.GetType () != typeof(WorkerFailReason))
				return false;
			WorkerFailReason other = (WorkerFailReason)obj;
			return ExceptionType == other.ExceptionType && 
				InnerExceptionType == other.InnerExceptionType && 
				Message == other.Message;
		}
		

		public override int GetHashCode ()
		{
			unchecked {
				return (ExceptionType != null ? ExceptionType.GetHashCode () : 0) ^
					(InnerExceptionType != null ? InnerExceptionType.GetHashCode () : 0) ^
					(Message != null ? Message.GetHashCode () : 0);
			}
		}
		
	}

	sealed class WorkerFailReasonCollector
	{
		readonly Dictionary<WorkerFailReason, long> dic = new Dictionary<WorkerFailReason, long>();

		public void Collect(WorkerFailReason reason)
		{
			lock (dic) {
				long count;
				dic.TryGetValue (reason, out count);
				++count;
				dic [reason] = count;
			}
		}

		public Tuple<WorkerFailReason, long>[] ToArray()
		{
			lock (dic) {
				return dic
					.Select (pair => Tuple.Create (pair.Key, pair.Value))
					.ToArray ();
			}
		}
	}

	[Serializable]
	sealed class SentPacketCountLimitReachedException: System.IO.IOException
	{
		public SentPacketCountLimitReachedException () :
		base ("The number of sent packets reached the specified limit.")
		{
		}
	}

	[Serializable]
	sealed class WorkerTimedOutException: System.IO.IOException
	{
		public WorkerTimedOutException () :
			base ("The operation timed out.")
		{
		}
	}

	[Serializable]
	sealed class TestAbortedException: System.IO.IOException
	{
		public TestAbortedException () :
		base ("Test was aborted.")
		{
		}
	}
}

