using System;

namespace Merlion.Server
{
	public abstract class MerlionRoom: MarshalByRefObject, IDisposable
	{
		protected MerlionRoom ()
		{
		}

		~MerlionRoom()
		{
			this.Dispose();
		}

		public abstract byte[] GetRoomId();

		public abstract void Dispose();
	}
}

