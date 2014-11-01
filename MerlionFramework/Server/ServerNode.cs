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

