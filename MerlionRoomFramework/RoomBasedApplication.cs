using System;
using log4net;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Merlion.Server.RoomFramework
{
	public abstract class RoomBasedApplication: Application
	{
		static readonly ILog log = LogManager.GetLogger(typeof(RoomBasedApplication));

		readonly Strand strand = new Strand();

		readonly Dictionary<byte[], WeakReference<Room>> rooms =
			new Dictionary<byte[], WeakReference<Room>> (Merlion.Utils.ByteArrayComparer.Instance);

		protected RoomBasedApplication ()
		{
		}

		public Strand Strand
		{
			get { return strand; }
		}

		public override async void Accept (MerlionClient client, MerlionRoom room)
		{
			strand.Dispatch (() => {
				Client cli;
				try {
					cli = CreateClient();
					if (cli == null) {
						log.Debug("CreateClient returned null.");
						client.Close();
						return;
					}
				} catch (Exception ex) {
					log.Error("Exception thrown in CreateClient.", ex);
					client.Close();
					return;
				}

				WeakReference<Room> weakr;
				Room r = null;
				if (room == null || !rooms.TryGetValue(room.GetRoomId(), out weakr) || !weakr.TryGetTarget(out r)) {
					try {
						r = GetDefaultRoom();
						if (r == null) {
							log.Debug("GetDefaultRoom returned null.");
							client.Close();
							return;
						}
					} catch (Exception ex) {
						log.Error("Exception thrown in GetDefaultRoom.", ex);
						client.Close();
						return;
					}
				}

				try {
					cli.InitializeClient(client, r);
				} catch (Exception ex) {
					log.Error("Exception thrown in InitializeClient.", ex);
					client.Close();
					return;
				}
			});
		}

		protected virtual Client CreateClient()
		{
			return new Client ();
		}

		protected abstract Room GetDefaultRoom();

		public async Task RegisterRoom(Room room)
		{
			MerlionRoom r = null;
			await strand.InvokeAsync (() => {
				r = CreateRoom();
			});
			await room.InternalInitializeRoom(this, r);
		}

		public async Task UnregisterRoom(Room room)
		{
			var r = await room.InternalUninitializeRoom ();
			if (r == null) {
				return;
			}
			await DeleteRoom (r);
		}

		internal Task DeleteRoom(MerlionRoom room)
		{
			return strand.InvokeAsync (() => {
				try {
					rooms.Remove(room.GetRoomId());
				} catch (Exception ex) {
					log.Warn ("Error while deleteing room. Ignored.", ex);
				}
			});
		}
	}
}

