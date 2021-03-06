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
using log4net;
using System.Threading;

namespace Merlion.MerEcho
{
	public class Application: Server.Application
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Application));
		public Application ()
		{
		}

		public override void Accept (Merlion.Server.MerlionClient client, Merlion.Server.MerlionRoom room)
		{
			log.Debug ("MerEcho started");
			client.Received += (sender, e) => {
				try {
					client.Send(e.Data);
				} catch (Exception ex) {
					log.Warn ("MerEcho error", ex);
					client.Close();
				}
			};
		}
	}
}

