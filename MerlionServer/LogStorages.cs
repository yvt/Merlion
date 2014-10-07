using System;
using System.Collections.Generic;

namespace Merlion.Server
{

	struct LogStorageEntry
	{
		public long Id;
		public LogEntry LogEntry;
		public LogStorageEntry(long id, LogEntry entry) {
			if (entry == null)
				throw new ArgumentNullException ("entry");
			this.Id = id; this.LogEntry = entry;
		}
	}

	interface ILogStorage
	{
		LogStorageEntry Log(LogEntry entry);
		LogStorageEntry[] GetLogEntries(long? since, long? before, int limit, LogSeverity minSeverity);
	}

	sealed class MemoryLogStorage: ILogStorage
	{
		readonly Utils.Deque<LogStorageEntry> entries = 
			new Utils.Deque<LogStorageEntry> (256);

		static readonly DateTime epochDate = new DateTime (1970, 1, 1);

		long nextId = 1;
		int entryCountLimit;

		public MemoryLogStorage(int entryCountLimit)
		{
			this.entryCountLimit = entryCountLimit;
		}

		long SerializeDateTime(DateTime d)
		{
			return (long)((d - epochDate).TotalMilliseconds);
		}
		DateTime DeserializeDateTime(long v)
		{
			return epochDate + TimeSpan.FromMilliseconds ((double)v);
		}

		public LogStorageEntry Log (LogEntry entry)
		{
			lock (entries) {
				var e = new LogStorageEntry (nextId++, entry);
				entries.AddLast (e);
				while (entries.Count > entryCountLimit)
					entries.RemoveFirst ();
				return e;
			}
		}

		int BinarySearch(long id)
		{
			var ents = entries;
			int first = 0, last = ents.Count;
			while (last > first) {
				int mid = (first + last) >> 1;
				var mident = ents [mid];
				if (id > mident.Id) {
					first = mid + 1;
				} else if (id < mident.Id) {
					last = mid;
				} else {
					return mid;
				}
			}
			return first;
		}

		public LogStorageEntry[] GetLogEntries (long? since, long? before, int limit, LogSeverity minSeverity)
		{
			lock (entries) {
				if (!since.HasValue) {
					since = long.MinValue;
				}
				if (!before.HasValue) {
					before = long.MaxValue;
				}

				int start = BinarySearch (since.Value);
				int end = BinarySearch (before.Value);
				end = Math.Max (end, start);

				var ret = new List<LogStorageEntry> ();
				ret.Capacity = Math.Min (limit, end - start);

				var ents = entries;
				for (int i = end - 1; i >= start; --i) {
					if (ret.Count >= limit) {
						break;
					}
					var e = ents [i];
					if ((int)e.LogEntry.Severity >= (int)minSeverity) {
						ret.Add (e);
					}
				}

				ret.Reverse ();

				return ret.ToArray ();
			}
		}
	}
}

