using System;

namespace Merlion.Server
{
	public abstract class ServerNode: MarshalByRefObject
	{
		protected ServerNode ()
		{
		}

		public abstract string Name { get; }

		public abstract string BaseDirectory { get; }
		public abstract string ServerDirectory { get; }

		public abstract void Log(log4net.Core.LoggingEvent[] logEvents);

		public abstract MerlionRoom CreateRoom();
	}
}

