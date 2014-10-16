/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

using System;
using log4net;
using System.Collections.Generic;
using System.Runtime.Remoting.Lifetime;

namespace Merlion.Server
{
	sealed class RoomEventArgs: EventArgs
	{
		public byte[] RoomId;
	}

	sealed class Domain: IDisposable
	{
		static readonly ILog log = LogManager.GetLogger(typeof(Domain));
		static readonly log4net.Core.ILogger logger = log4net.Core.LoggerManager.GetLogger(
			System.Reflection.Assembly.GetExecutingAssembly(), typeof(Domain));

		readonly INodeServer node;

		readonly ClientSponsor sponsor = new ClientSponsor ();

		readonly string baseDirectory;

		AppDomain appDomain;
		ApplicationServer appServer;
		Utils.PipeStreamFactory pipeFactory;

		readonly string name;

		readonly Dictionary<byte[], MerlionRoomImpl> rooms =
			new Dictionary<byte[], MerlionRoomImpl>(Merlion.Utils.ByteArrayComparer.Instance);

		public event EventHandler<RoomEventArgs> RoomBeingAdded;
		public event EventHandler<RoomEventArgs> RoomRemoved;

		public Domain (INodeServer node, string version)
		{
			this.node = node;

			var versions = node.VersionManager;
			if (!versions.VersionExists (version)) {
				throw new InvalidOperationException (string.Format ("Version {0} is not yet deployed.", version));
			}

			baseDirectory = versions.GetDirectoryPathForVersion (version);

			name = node.Name + "/" + version;
			var setup = new AppDomainSetup ();

			setup.ApplicationBase = baseDirectory;
			setup.ApplicationName = "MerlionApp";

			try {
				log.DebugFormat ("{0}: Creating AppDomain.", name);

				appDomain = AppDomain.CreateDomain (name, null, setup,
					AppDomain.CurrentDomain.PermissionSet);

				log.DebugFormat ("{0}: Creating ApplicationServer.", name);

				var args = new object[] {new ServerNodeImpl(this)};
				appServer = (ApplicationServer)
					appDomain.CreateInstanceAndUnwrap ("Merlion", 
						typeof(ApplicationServer).FullName, false,
						System.Reflection.BindingFlags.CreateInstance, null,
						args, 
						null, null);

				sponsor.Register(appServer);

				log.DebugFormat ("{0}: Starting ApplicationServer.", name);

				appServer.Start();

				log.DebugFormat ("{0}: Creating PipeStreamFactory.", name);
				pipeFactory = (Utils.PipeStreamFactory)
					appDomain.CreateInstanceAndUnwrap(
						typeof(Utils.PipeStreamFactory).Assembly.FullName,
						typeof(Utils.PipeStreamFactory).FullName);

				sponsor.Register(pipeFactory);

			} catch (Exception ex) {
				try { Dispose (); }
				catch (Exception ex2) {
					log.Warn(string.Format("{0}: Exception thrown while cleaning-up Domain. Ignoring.", name), ex2);
				}
				throw new DomainCreationException (ex);
			}
		}

		public void Accept(long clientId, Utils.PipeStream pipe, byte[] roomId)
		{
			MerlionRoomImpl room;
			lock (rooms) {
				rooms.TryGetValue(roomId, out room);
			}

			pipe = pipeFactory.CreateWithHandle (pipe.OtherEndHandle);

			appServer.Accept(new MerlionClientImpl(clientId, pipe),
				room);
		}

		Dictionary<long, MerlionClientImpl> clientsToSetup = 
			new Dictionary<long, MerlionClientImpl>();

		public void AcceptWithoutStream(long clientId,  byte[] roomId)
		{
			MerlionRoomImpl room;
			lock (rooms) {
				rooms.TryGetValue(roomId, out room);
			}

			lock (clientsToSetup) {
				var cli = new MerlionClientImpl (clientId);
				clientsToSetup [clientId] = cli;
				new System.Threading.Tasks.Task (() => {
					appServer.Accept(cli, room);
				}).Start ();
			}
		}

		public bool SetupClient(long clientId, NativeServerInterop.ClientSetup setup)
		{
			lock (clientsToSetup) {
				MerlionClientImpl cli;
				if (clientsToSetup.TryGetValue(clientId, out cli)) {
					var handle = new Utils.MonoPipeStreamEndpoint ();
					handle.ReadingHandle = setup.ReadFileDescriptor;
					handle.WritingHandle = setup.WriteFileDescriptor;
					cli.Setup (pipeFactory.CreateWithHandle(handle));

					clientsToSetup.Remove (clientId);
					return true;
				}
			}
			return false;
		}

		public void DiscardClient(long clientId)
		{
			lock (clientsToSetup) {
				MerlionClientImpl cli;
				if (clientsToSetup.TryGetValue(clientId, out cli)) {
					clientsToSetup.Remove (clientId);
					cli.Discard ();
				}
			}
		}

		public void Dispose()
		{
			try {
				sponsor.Unregister(appServer);
			} catch (Exception ex) {
				log.Warn (string.Format (
					"{0}: Exception thrown while unregistering ApplicationServer from ClientSponsor. Ignoring.", name), ex);
			}
			try {
				sponsor.Unregister(pipeFactory);
			} catch (Exception ex) {
				log.Warn (string.Format (
					"{0}: Exception thrown while unregistering PipeStreamFactory from ClientSponsor. Ignoring.", name), ex);
			}
			if (appServer != null) {
				try {
					appServer.Unload();
				} catch (Exception ex) {
					log.Warn (string.Format (
						"{0}: Exception thrown while unloading the application server. Ignoring.", name), ex);
				}
				appServer = null;
			}

			if (appDomain != null) {
				try {
					AppDomain.Unload (appDomain);
				} catch (Exception ex) {
					log.Warn (string.Format (
						"{0}: Exception thrown while unloading the AppDomain. Ignoring.", name), ex);
				}
				appDomain = null;
			}

		}

		static readonly Random random = new Random();
		sealed class MerlionRoomImpl: MerlionRoom
		{
			static readonly ILog log = LogManager.GetLogger(typeof(MerlionRoomImpl));

			readonly byte[] roomId;
			readonly Domain domain;

			public MerlionRoomImpl(Domain domain)
			{
				roomId = new byte[64];
				random.NextBytes(roomId);

				this.domain = domain;
				lock (domain.rooms) {
					domain.rooms.Add(roomId, this);
				}
				try {
					var h = domain.RoomBeingAdded;
					if (h != null)
						h(domain, new RoomEventArgs() { RoomId = roomId });
				} catch (Exception ex) {
					lock (domain.rooms) {
						domain.rooms.Remove(roomId);
					}
					log.Error("Error occured while creating a room.", ex);
					throw;
				}
			}

			public override byte[] GetRoomId ()
			{
				return (byte[])roomId.Clone ();
			}

			public override void Dispose ()
			{
				lock (domain.rooms) {
					domain.rooms.Remove (roomId);
				}
				try {
					var h = domain.RoomRemoved;
					if (h != null)
						h(domain, new RoomEventArgs() { RoomId = roomId });
				} catch (Exception ex) {
					log.Error("Error occured while creating a room.", ex);
					throw;
				}
			}
		}

		sealed class MerlionClientImpl: MerlionClient
		{
			readonly long clientId;
			System.IO.Stream stream;
			readonly System.Threading.EventWaitHandle readyEvent =
				new System.Threading.EventWaitHandle (false,
					System.Threading.EventResetMode.ManualReset);

			public MerlionClientImpl(long clientId, System.IO.Stream stream)
			{
				this.stream = stream;
				this.clientId = clientId;
				readyEvent.Set();
			}
			public MerlionClientImpl(long clientId)
			{
				this.clientId = clientId;
			}

			public void Setup(System.IO.Stream stream)
			{
				this.stream = stream;
				readyEvent.Set ();
			}

			public void Discard()
			{
				stream = new Utils.InvalidStream ();
				readyEvent.Set ();
			}

			public override long ClientId {
				get {
					return clientId;
				}
			}

			public override System.IO.Stream Stream {
				get {
					readyEvent.WaitOne ();
					return stream;
				}
			}
		}

		sealed class ServerNodeImpl: ServerNode
		{
			readonly Domain domain;

			public ServerNodeImpl(Domain domain)
			{
				this.domain = domain;
			}

			public override void Log (log4net.Core.LoggingEvent[] logEvents)
			{
				foreach (var e in logEvents)
					logger.Log (e);
			}
			public override MerlionRoom CreateRoom ()
			{
				return new MerlionRoomImpl (domain);
			}
			public override string BaseDirectory {
				get {
					return domain.baseDirectory;
				}
			}
			public override string ServerDirectory {
				get {
					return AppDomain.CurrentDomain.BaseDirectory;
				}
			}
			public override string Name {
				get {
					return domain.appDomain.FriendlyName;
				}
			}
		}
	}

	[Serializable]
	sealed class DomainCreationException: Exception
	{
		public DomainCreationException(Exception inner):
		base("Failed to initialize an AppDomain.", inner) { }
	}

}

