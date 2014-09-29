using System;
using log4net;
using System.Collections.Generic;

namespace Merlion.Server
{
	class ServerLogManager: MarshalByRefObject
	{
		public static ServerLogManager Instance { get; set; }

		public ServerLogManager()
		{

		}

		public static ILog GetLogger(Type type)
		{
			if (Instance == null) {
				Instance = new ServerLogManager ();
			}
			return Instance.GetLoggerInternal (type);
		}

		ILog GetLoggerInternal(Type type)
		{
			return new LogImpl (LogManager.GetLogger (type));	
		}

		class LogImpl: MarshalByRefObject, ILog
		{
			readonly ILog baseLogger;

			public LogImpl(ILog logger)
			{
				baseLogger = logger;
			}

			public void Debug (object message)
			{
				baseLogger.Debug (message);
			}

			public void Debug (object message, Exception exception)
			{
				baseLogger.Debug (message, exception);
			}

			public void DebugFormat (string format, params object[] args)
			{
				baseLogger.DebugFormat (format, args);
			}

			public void DebugFormat (string format, object arg0)
			{
				baseLogger.DebugFormat (format, arg0);
			}

			public void DebugFormat (string format, object arg0, object arg1)
			{
				baseLogger.DebugFormat (format, arg0, arg1);
			}

			public void DebugFormat (string format, object arg0, object arg1, object arg2)
			{
				baseLogger.DebugFormat (format, arg0, arg1, arg2);
			}

			public void DebugFormat (IFormatProvider provider, string format, params object[] args)
			{
				baseLogger.DebugFormat (provider, format, args);
			}

			public void Info (object message)
			{
				baseLogger.Info (message);
			}

			public void Info (object message, Exception exception)
			{
				baseLogger.Info (message, exception);
			}

			public void InfoFormat (string format, params object[] args)
			{
				baseLogger.InfoFormat (format, args);
			}

			public void InfoFormat (string format, object arg0)
			{
				baseLogger.InfoFormat (format, arg0);
			}

			public void InfoFormat (string format, object arg0, object arg1)
			{
				baseLogger.InfoFormat (format, arg0, arg1);
			}

			public void InfoFormat (string format, object arg0, object arg1, object arg2)
			{
				baseLogger.InfoFormat (format, arg0, arg1, arg2);
			}

			public void InfoFormat (IFormatProvider provider, string format, params object[] args)
			{
				baseLogger.InfoFormat (provider, format, args);
			}

			public void Warn (object message)
			{
				baseLogger.Warn (message);
			}

			public void Warn (object message, Exception exception)
			{
				baseLogger.Warn (message, exception);
			}

			public void WarnFormat (string format, params object[] args)
			{
				baseLogger.WarnFormat (format, args);
			}

			public void WarnFormat (string format, object arg0)
			{
				baseLogger.WarnFormat (format, arg0);
			}

			public void WarnFormat (string format, object arg0, object arg1)
			{
				baseLogger.WarnFormat (format, arg0, arg1);
			}

			public void WarnFormat (string format, object arg0, object arg1, object arg2)
			{
				baseLogger.WarnFormat (format, arg0, arg1, arg2);
			}

			public void WarnFormat (IFormatProvider provider, string format, params object[] args)
			{
				baseLogger.WarnFormat (provider, format, args);
			}

			public void Error (object message)
			{
				baseLogger.Error (message);
			}

			public void Error (object message, Exception exception)
			{
				baseLogger.Error (message, exception);
			}

			public void ErrorFormat (string format, params object[] args)
			{
				baseLogger.ErrorFormat (format, args);
			}

			public void ErrorFormat (string format, object arg0)
			{
				baseLogger.ErrorFormat (format, arg0);
			}

			public void ErrorFormat (string format, object arg0, object arg1)
			{
				baseLogger.ErrorFormat (format, arg0, arg1);
			}

			public void ErrorFormat (string format, object arg0, object arg1, object arg2)
			{
				baseLogger.ErrorFormat (format, arg0, arg1, arg2);
			}

			public void ErrorFormat (IFormatProvider provider, string format, params object[] args)
			{
				baseLogger.ErrorFormat (provider, format, args);
			}

			public void Fatal (object message)
			{
				baseLogger.Fatal (message);
			}

			public void Fatal (object message, Exception exception)
			{
				baseLogger.Fatal (message, exception);
			}

			public void FatalFormat (string format, params object[] args)
			{
				baseLogger.FatalFormat (format, args);
			}

			public void FatalFormat (string format, object arg0)
			{
				baseLogger.FatalFormat (format, arg0);
			}

			public void FatalFormat (string format, object arg0, object arg1)
			{
				baseLogger.FatalFormat (format, arg0, arg1);
			}

			public void FatalFormat (string format, object arg0, object arg1, object arg2)
			{
				baseLogger.FatalFormat (format, arg0, arg1, arg2);
			}

			public void FatalFormat (IFormatProvider provider, string format, params object[] args)
			{
				baseLogger.FatalFormat (provider, format, args);
			}

			public bool IsDebugEnabled {
				get {
					return baseLogger.IsDebugEnabled;
				}
			}

			public bool IsInfoEnabled {
				get {
					return baseLogger.IsInfoEnabled;
				}
			}

			public bool IsWarnEnabled {
				get {
					return baseLogger.IsWarnEnabled;
				}
			}

			public bool IsErrorEnabled {
				get {
					return baseLogger.IsErrorEnabled;
				}
			}

			public bool IsFatalEnabled {
				get {
					return baseLogger.IsFatalEnabled;
				}
			}

			public log4net.Core.ILogger Logger {
				get {
					return baseLogger.Logger;
				}
			}

		}
	}
}

