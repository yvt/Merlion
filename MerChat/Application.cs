using System;

namespace Merlion.MerChat
{
	public class Application : Merlion.Server.RoomFramework.RoomBasedApplication
	{
		Room defaultRoom = new Room();

		public Application ()
		{
			RegisterRoom (defaultRoom);
		}


		protected override Merlion.Server.RoomFramework.Room GetDefaultRoom ()
		{
			return defaultRoom;
		}

	}

	sealed class Room : Merlion.Server.RoomFramework.Room
	{
		public Room()
		{
			DataReceived += HandleDataReceived;
		}

		void HandleDataReceived (object sender, Merlion.Server.RoomFramework.DataReceivedEventArgs e)
		{
			foreach (var cli in Clients) {
				cli.Send (e.Data);
			}
		}
	}
}

