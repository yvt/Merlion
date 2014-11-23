using System;
using System.Threading.Tasks;
using System.Threading;
using log4net;

namespace Merlion.Server.RoomFramework
{
	public class Client: IDisposable
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Client));
		object sync = new object();

		readonly SemaphoreSlim moveSem = new SemaphoreSlim (1);

		static long nextId = 0;
		public long Id { get; private set; }

		public MerlionClient MerlionClient { get; internal set; }

		public Room Room { get; internal set; }



		public async Task MoveToRoom(Room newRoom)
		{
			// Leave the current room.
			Room oldRoom;
			lock (sync) {
				if (Room == newRoom) {
					return;
				}
				if (Room == null) {
					throw new InvalidOperationException ("Cannot add an orphaned client to a room. " +
						"(This might be caused by the ongoing move operation or the disposed client.)");
				}
				oldRoom = Room;
				Room = null;
			}

			await oldRoom.Strand.InvokeAsync (() => {
				oldRoom.InternalRemoveClient (this);
			});

			if (newRoom == null) {
				return;
			}

			// Add to the new room.
			try {
				await newRoom.Strand.InvokeAsync (() => {
					lock (sync) {
						Room = newRoom;
					}
					newRoom.InternalAddClient(this);
				});
			} catch (Exception ex) {
				// Adding failed.
				// Try to rollback.
				try {
					await oldRoom.Strand.InvokeAsync (() => {
						lock (sync) {
							Room = oldRoom;
						}
						oldRoom.InternalAddClient(this);
					});
				} catch (Exception ex2) {
					lock (sync) {
						Room = null;
					}
					Close ();
					throw new ClientMoveRollbackException (ex, ex2);
				}
				throw new ClientMoveException (ex);
			}
		}

		public event EventHandler<RoomChangedEventArgs> RoomChanged;
		protected virtual void OnRoomChanged(RoomChangedEventArgs args)
		{
			var handler = RoomChanged;
			if (handler != null)
				handler (this, args);
		}

		internal void InternalRaiseRoomChanged(Room room)
		{
			try {
				OnRoomChanged(new RoomChangedEventArgs(room));
			} catch (Exception ex) {
				log.Error ("Exception was thrown in RoomChanged.", ex);
			}
		}

		public event EventHandler Disconnected;
		protected virtual void OnDisconnected(EventArgs args)
		{
			var handler = Disconnected;
			if (handler != null)
				handler (this, args);
		}

		public Client ()
		{
			Id = Interlocked.Increment(ref nextId);
		}

		public void Dispose ()
		{

		}

		public void Close()
		{
			var cli = MerlionClient;
			if (cli != null) {
				cli.Close ();
			}
		}

		internal void InitializeClient(MerlionClient client, Room room)
		{
			lock (sync) {
				if (this.MerlionClient != null) {
					throw new InvalidOperationException ();
				}
				this.MerlionClient = client;
				this.Room = room;
				client.Closed += HandleClosed;
				client.Received += HandleReceived;
			}
			room.Strand.Dispatch (() => {
				room.InternalAddClient(this);
			});
		}

		void HandleReceived (object sender, ReceiveEventArgs e)
		{
			var r = Room;
			if (r != null)
				r.InternalReceived (this, e.Data);
		}

		async void HandleClosed (object sender, EventArgs e)
		{
			MerlionClient = null;
			try {
				await MoveToRoom(null);
			} catch (Exception ex) {
				log.Error ("Removing client from the room after it being closed has failed.", ex);
			}
			try {
				OnDisconnected(EventArgs.Empty);
			} catch (Exception ex) {
				log.Error ("Exception thrown in Disconnected.", ex);
			}
		}

		public async void Send(byte[] data, int start, int length)
		{
			// FIXME: check bounds safely
			try {
				await MerlionClient.SendAsync (data, start, length);
			} catch (Exception ex) {
				; // just ignore...

				// Even if Send succeeds we cannot guarantee that client can receive the
				// sent packet.
			}
		}

		public void Send(byte[] data)
		{
			Send (data, 0, data.Length);
		}
	}

	[Serializable]
	public class RoomChangedEventArgs: EventArgs
	{
		public Room Room { get; set; }
		public RoomChangedEventArgs(Room room)
		{
			this.Room = room;
		}
	}

	[Serializable]
	public class ClientMoveException: Exception
	{
		public ClientMoveException(Exception ex):
		base("Moving the client to another room failed.", ex)
		{ }
		public ClientMoveException(string message, Exception ex):
		base(message, ex)
		{ }
	}

	[Serializable]
	public class ClientMoveRollbackException: AggregateException
	{
		public ClientMoveRollbackException(Exception ex, Exception ex2):
		base("Moving the client to another room failed, and rolling back has failed too.", ex, ex2)
		{ }
	}
}

