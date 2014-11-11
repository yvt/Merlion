using System;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using log4net;

namespace Merlion.Server.RoomFramework
{
	public sealed class Strand
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Strand));

		readonly object sync = new object();

		readonly Queue<Action> tasks = new Queue<Action>(64);

		bool workerRunning = false;
		Thread invokingThread = null;

		readonly SemaphoreSlim invokeSem = new SemaphoreSlim(1);

		SynchronizationContext syncContext;

		public Strand ()
		{
		}

		public static Strand Current
		{
			get {
				var sc = SynchronizationContext.Current as StrandSynchronizationContext;
				if (sc != null)
					return sc.Strand;
				return null;
			}
		}

		async void Worker(object state)
		{
			while (true) {
				await invokeSem.WaitAsync ();
				Action task;
				lock (sync) {
					if (tasks.Count == 0) {
						// No more to do
						workerRunning = false;
						invokeSem.Release ();
						return;
					}
					task = tasks.Dequeue ();
					invokingThread = Thread.CurrentThread;
				}
				var lastSC = SynchronizationContext.Current;
				SynchronizationContext.SetSynchronizationContext (GetSynchronizationContext());
				try {
					task();
				} catch (Exception ex) {
					log.Fatal ("Unhandled exception thrown in the asynchronous callback.", ex);
				} finally {
					SynchronizationContext.SetSynchronizationContext (lastSC);
				}
				lock (sync) {
					invokingThread = null;
				}
				invokeSem.Release ();
			}
		}

		SynchronizationContext GetSynchronizationContext()
		{
			if (syncContext == null) {
				syncContext = new StrandSynchronizationContext(this);
			}
			return syncContext;
		}

		void StartWorker()
		{
			lock (sync) {
				workerRunning = true;
				ThreadPool.QueueUserWorkItem (Worker);
			}
		}

		public void Post (Action action)
		{
			lock (sync) {
				tasks.Enqueue (action);

				if (!workerRunning) {
					StartWorker ();
				}
			}
		}

		public void Dispatch (Action action)
		{
			lock (sync) {
				if (invokingThread == Thread.CurrentThread) {
					var lastSC = SynchronizationContext.Current;
					SynchronizationContext.SetSynchronizationContext (GetSynchronizationContext());
					try {
						Monitor.Exit (sync);
						action();
					} catch (Exception ex) {
						log.Fatal ("Unhandled exception thrown in the possibly asynchronous callback.", ex);
					} finally {
						Monitor.Enter (sync);
						SynchronizationContext.SetSynchronizationContext (lastSC);
					}
					return;
				}
				if (workerRunning || !invokeSem.Wait (0)) {
					Post (action);
					return;
				}
				invokingThread = Thread.CurrentThread;
			}
			try {
				action();
			} finally {
				lock (sync) {
					invokingThread = null;
				}
				invokeSem.Release ();
			}
		}

		public bool IsCurrent
		{
			get {
				return invokingThread == Thread.CurrentThread;
			}
		}

		public void ThrowIfNotCurrent()
		{
			if (!IsCurrent)
				throw new InvalidOperationException ("Method was called outside of the associated explicit strand.");
		}

		sealed class Invoker
		{
			readonly Strand strand;
			readonly Action action;
			Exception exception;
			bool done;

			public Invoker(Strand strand, Action action)
			{
				this.strand = strand;
				this.action = action;
			}

			void InternalInvoke()
			{
				try {
					action();
				} catch (Exception ex) {
					exception = ex;
				}
				lock (this) {
					done = true;
					Monitor.PulseAll (this);
				}
			}

			public void DoInvoke()
			{
				strand.Dispatch (InternalInvoke);
				lock (this)
					while (!done)
						Monitor.Wait (this);
				if (exception != null) {
					throw exception;
				}
			}
		}

		public void Invoke(Action action)
		{
			var i = new Invoker (this, action);
			i.DoInvoke ();
		}

		sealed class Invoker<T>
		{
			readonly Strand strand;
			readonly Func<T> action;
			T result;
			Exception exception;
			bool done;

			public Invoker(Strand strand, Func<T> action)
			{
				this.strand = strand;
				this.action = action;
			}

			void InternalInvoke()
			{
				try {
					result = action();
				} catch (Exception ex) {
					exception = ex;
				}
				lock (this) {
					done = true;
					Monitor.PulseAll (this);
				}
			}

			public T DoInvoke()
			{
				strand.Dispatch (InternalInvoke);
				lock (this)
					while (!done)
						Monitor.Wait (this);
				if (exception != null) {
					throw exception;
				}
				return result;
			}
		}

		public T Invoke<T>(Func<T> action)
		{
			var i = new Invoker<T> (this, action);
			return i.DoInvoke ();
		}

		public Task InvokeAsync(Action action)
		{
			var source = new TaskCompletionSource<object> ();
			Dispatch (() => {
				try {
					action();
					source.SetResult(null);
				} catch (Exception ex) {
					source.SetException(ex);
				}
			});
			return source.Task;
		}

		public Task<T> InvokeAsync<T>(Func<T> action)
		{
			var source = new TaskCompletionSource<T> ();
			Post (() => {
				try {
					source.SetResult(action());
				} catch (Exception ex) {
					source.SetException(ex);
				}
			});
			return source.Task;
		}
	}

	public sealed class StrandSynchronizationContext: SynchronizationContext
	{
		readonly Strand strand;

		public StrandSynchronizationContext(Strand strand)
		{
			this.strand = strand;
			if (strand == null)
				throw new ArgumentNullException ("strand");

		}

		public override void Post (SendOrPostCallback d, object state)
		{
			strand.Post (() => d (state));
		}

		public override void Send (SendOrPostCallback d, object state)
		{
			strand.Invoke (() => d (state));
		}

		public Strand Strand { get { return strand; }}
	}
}

