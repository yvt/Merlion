using System;
using System.Threading;
using System.Threading.Tasks;

namespace Merlion.Server.RoomFramework.Utils
{
	struct SemaphoreSlimLocker: IDisposable
	{
		SemaphoreSlim semaphore;

		SemaphoreSlimLocker (SemaphoreSlim semaphore)
		{
			if (semaphore == null)
				throw new ArgumentNullException ("semaphore");
			this.semaphore = semaphore;
		}

		public void Dispose ()
		{
			semaphore.Release ();
			semaphore = null;
		}

		public static SemaphoreSlimLocker Lock(SemaphoreSlim semaphore)
		{
			semaphore.Wait ();
			return new SemaphoreSlimLocker (semaphore);
		}

		public static async Task<SemaphoreSlimLocker> LockAsync(SemaphoreSlim semaphore)
		{
			await semaphore.WaitAsync ();
			return new SemaphoreSlimLocker (semaphore);
		}
	}
}

