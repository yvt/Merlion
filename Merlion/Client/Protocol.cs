using System;

namespace Merlion.Client
{
	static class Protocol
	{
		public const uint HeaderMagic = 0x918123ab;

		public enum PrologueResult : byte
		{
			Success,
			ServerFull,
			ProtocolError
		}
	}
}

