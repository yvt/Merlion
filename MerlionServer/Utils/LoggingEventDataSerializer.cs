using System;
using log4net.Core;
using System.IO;

namespace Merlion.Server.Utils
{
	sealed class LoggingEventDataSerializer
	{
		public LoggingEventDataSerializer ()
		{
		}

		public LoggingEventData Deserialize(Stream stream)
		{
			var br = new BinaryReader (stream);
			var data = new LoggingEventData ();
			data.Domain = br.ReadString ();
			data.ExceptionString = br.ReadString ();
			data.Identity = br.ReadString ();
			data.Level = new Level(br.ReadInt32(), br.ReadString(), br.ReadString());
			data.LocationInfo = null;
			data.LoggerName = br.ReadString ();
			data.Message = br.ReadString ();
			data.Properties = new log4net.Util.PropertiesDictionary ();
			data.ThreadName = br.ReadString ();
			data.TimeStamp = DateTime.FromOADate (br.ReadDouble ());
			data.UserName = br.ReadString ();
			return data;
		}

		public void Serialize(Stream stream, LoggingEventData data)
		{
			var bw = new BinaryWriter (stream);
			bw.Write (data.Domain);
			bw.Write (data.ExceptionString);
			bw.Write (data.Identity);
			bw.Write (data.Level.Value);
			bw.Write (data.Level.Name);
			bw.Write (data.Level.DisplayName);
			bw.Write (data.LoggerName);
			bw.Write (data.Message);
			bw.Write (data.ThreadName);
			bw.Write (data.TimeStamp.ToOADate ());
			bw.Write (data.UserName);
		}
	}
}

