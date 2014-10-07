using System;

namespace Merlion.Server
{

	public enum LogSeverity
	{
		Debug,
		Info,
		Warn,
		Error,
		Fatal
	}

	/// <summary>
	/// Application-standard log entry format.
	/// </summary>
	public sealed class LogEntry
	{
		public DateTime Timestamp = DateTime.UtcNow;

		public string Source;
		public string Channel;
		public string Host;
		public string Message;

		public LogSeverity Severity;

		public LogEntry() { }
		public LogEntry(log4net.Core.LoggingEventData data) {
			Source = data.LoggerName;
			Message = data.Message;

			if (data.Properties != null) {
				Channel = data.Properties ["NDC"] as string;
				Host = data.Properties ["Node"] as string;
			}

			if (data.Level.Value <= log4net.Core.Level.Debug.Value) {
				Severity = LogSeverity.Debug;
			} else if (data.Level.Value <= log4net.Core.Level.Info.Value) {
				Severity = LogSeverity.Info;
			} else if (data.Level.Value <= log4net.Core.Level.Warn.Value) {
				Severity = LogSeverity.Warn;
			} else if (data.Level.Value <= log4net.Core.Level.Error.Value) {
				Severity = LogSeverity.Error;
			} else {
				Severity = LogSeverity.Fatal;
			} 
		}


	}

	static class LogView
	{
		static public readonly ILogStorage Storage;

		static LogView ()
		{
			Storage = new MemoryLogStorage (10000);
		}


	}
}

