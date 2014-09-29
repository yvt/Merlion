using System;

namespace Merlion
{
	[Serializable]
	public class MerlionException: Exception
	{
		public MerlionException (string msg):
		this(msg, null)
		{
		}
		public MerlionException (string msg, Exception ex):
		base(msg, ex)
		{
		}
	}

	[Serializable]
	public class ServerFullException: MerlionException
	{
		public ServerFullException():
		base("Server is full.") { }
	}

	[Serializable]
	public class ProtocolErrorException: MerlionException
	{
		public ProtocolErrorException():
		base("Protocol error occured.") { }
	}
}

