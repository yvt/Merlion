/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

