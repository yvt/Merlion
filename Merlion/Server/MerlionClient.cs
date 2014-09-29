using System;

namespace Merlion.Server
{
	public abstract class MerlionClient: MarshalByRefObject
	{
		protected MerlionClient ()
		{
		}

		public abstract long ClientId { get; }

		public abstract System.IO.Stream Stream { get; }

		public void Close()
		{
			Stream.Close();
		}
	}
}

