﻿/**
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
using System.Net.Sockets;
using System.IO;
using log4net;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;

namespace Merlion.Server
{

	interface INodeServer: IDisposable
	{
		VersionManager VersionManager { get; }
		void SendVersionLoaded(string name);
		void SendVersionUnloaded(string name);
		void SendLog(LogEntry e);
		void VersionMissing();
		string Name { get; }
		event EventHandler NodeFailed;
		bool Failed { get; }
	}

}

