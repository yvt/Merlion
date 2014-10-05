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
using System.Xml.Serialization;

namespace Merlion.Server
{
	[Serializable]
	[XmlType]
	[XmlRoot("NodeInfo")]
	public sealed class NodeInfo
	{
		public string Name { get; set; }
	}
	static class NodeProtocol
	{
		public const uint ControlStreamHeaderMagic = 0xd29a1234;
		public const uint VersionDownloadRequestMagic = 0xe29b1234;
		public const uint DataStreamMagic = 0xf183a443;

		// Command that master accepts
		public enum MasterCommand : byte
		{
			Nop,
			RejectClient,
			VersionLoaded,
			VersionUnloaded,
			BindRoom,
			UnbindRoom,
			Log
		}

		// Command that node accepts
		public enum NodeCommand : byte
		{
			Nop,
			LoadVersion,
			UnloadVersion,
			Connect
		}
	}

}

