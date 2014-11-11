using System;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using log4net;
using System.Linq;

namespace Merlion.Server.RoomFramework
{
	public abstract class Room: IDisposable
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Room));

		bool disposed = false;
		RoomBasedApplication app;
		MerlionRoom merlionRoom;
		readonly Strand strand = new Strand ();
		string name = "Uninitialized Room";

		Dictionary<long, Client> clients =
			new Dictionary<long, Client> ();

		public event EventHandler<ClientEventArgs> ClientEnter;
		public event EventHandler<ClientEventArgs> ClientLeave;
		public event EventHandler<DataReceivedEventArgs> DataReceived;

		protected Room ()
		{
		}

		public MerlionRoom MerlionRoom {
			get { return merlionRoom; }
		}

		public Strand Strand
		{
			get { return strand; }
		}

		public string Name
		{
			get { return name; }
			set {
				if (value == null)
					throw new ArgumentNullException ("value");
				name = value;
			}
		}

		protected virtual void OnClientEnter(ClientEventArgs args)
		{
			var handler = ClientEnter;
			if (handler != null) {
				handler (this, args);
			}
		}

		protected virtual void OnClientLeave(ClientEventArgs args)
		{
			var handler = ClientLeave;
			if (handler != null) {
				handler (this, args);
			}
		}

		protected virtual void OnDataReceived(DataReceivedEventArgs args)
		{
			var handler = DataReceived;
			if (handler != null) {
				handler (this, args);
			}
		}

		internal void InternalReceived(Client client, byte[] data)
		{
			strand.Dispatch (() => {
				if (clients.ContainsKey(client.Id)) {
					try {
						OnDataReceived(new DataReceivedEventArgs(client, data));
					} catch (Exception ex) {
						log.Error("Exception thrown in DataReceived.", ex);
					}
				}
			});
		}

		internal async Task InternalInitializeRoom(RoomBasedApplication ap, MerlionRoom room)
		{
			await strand.InvokeAsync (() => {
				if (app != null) {
					throw new InvalidOperationException ();
				}

				this.app = ap;
				this.merlionRoom = room;
			});
		}

		internal Task<MerlionRoom> InternalUninitializeRoom()
		{
			return Strand.InvokeAsync<MerlionRoom> (() => {
				if (app == null) {
					log.Info("Attempted to uninitialize an already uninitialized room.");
					return null;
				}

				var r = merlionRoom;

				merlionRoom = null;
				app = null;

				return r;
			});
		}

		internal void InternalAddClient(Client client)
		{
			strand.ThrowIfNotCurrent ();
			CheckNotDisposed ();

			clients [client.Id] = client;

			client.InternalRaiseRoomChanged (this);

			try {
				OnClientEnter(new ClientEventArgs(client));
			} catch (Exception ex) {
				log.Error("Exception thrown in ClientEnter.", ex);
			}
		}

		internal void InternalRemoveClient(Client client)
		{
			strand.ThrowIfNotCurrent ();

			clients.Remove (client.Id);

			try {
				OnClientLeave(new ClientEventArgs(client));
			} catch (Exception ex) {
				log.Error("Exception thrown in ClientLeave.", ex);
			}

			client.InternalRaiseRoomChanged(null);
		}

		public IEnumerable<Client> Clients
		{
			get {
				return clients.Values;
			}
		}

		void RemoveAllClients()
		{
			var clis = clients.Values.ToArray();
			clients.Clear ();

			foreach (var cli in clis) {
				try {
					cli.Close();
				} catch (Exception ex) {
					// shouldn't happen...
					log.Error("Exception thrown in Client.Close.", ex);
				}
			}
		}

		void CheckNotDisposed()
		{
			if (disposed) {
				throw new ObjectDisposedException ("Room");
			}
		}

		public void Dispose ()
		{
			strand.ThrowIfNotCurrent ();
			disposed = true;

			if (app != null) {
				app.DeleteRoom (merlionRoom);
				merlionRoom = null;
			}

			RemoveAllClients ();
		}

		~Room()
		{
			if (app != null) {
				app.DeleteRoom (merlionRoom);
			}
		}
	}

	public class ClientEventArgs: EventArgs
	{
		public Client Client { get; set; }

		public ClientEventArgs(Client client)
		{
			this.Client = client;
		}
	}

	public class DataReceivedEventArgs: EventArgs
	{
		public Client Client { get; set; }
		public byte[] Data { get; set; }

		public DataReceivedEventArgs(Client client, byte[] data)
		{
			this.Client = client;
			this.Data = data;
		}
	}
}

