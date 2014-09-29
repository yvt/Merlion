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

