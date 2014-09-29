using System;

namespace Merlion.Server
{
	public abstract class Application: MarshalByRefObject
	{
		protected Application ()
		{
		}

		public MerlionRoom CreateRoom() {
			return ApplicationServer.Instance.Node.CreateRoom ();
		}

		public abstract void Accept(MerlionClient client, MerlionRoom room);

	}
}

