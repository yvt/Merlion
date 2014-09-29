using System;
using log4net;

namespace Merlion.Server
{
	public class ServerLogAppender: log4net.Appender.AppenderSkeleton
	{
		protected override void Append (log4net.Core.LoggingEvent loggingEvent)
		{
			if (AppConfiguration.ForwardLogToMaster) {
				var node = MainClass.NodeServer;
				if (node != null) {
					// TODO: node.SendLog (loggingEvent);
				}
			}
		}
	}
}

