using System;
using log4net;
using System.IO;
using System.Reflection;

namespace Merlion.Server
{
	public sealed class ApplicationServer: MarshalByRefObject
	{
		public static ApplicationServer Instance { get; private set; }

		public event EventHandler Unloading;

		public ServerNode Node { get; private set; }

		ApplicationConfig config;
		Application app;

		public ApplicationServer (object nodeobj)
		{
			if (nodeobj == null)
				throw new ArgumentNullException ("nodeobj");

			var node = (ServerNode)nodeobj;

			if (Instance != null) {
				throw new InvalidOperationException ("Cannot create multiple ServerDomains.");
			}

			Instance = this;

			log4net.Config.BasicConfigurator.Configure (new LogAppender());
			log4net.GlobalContext.Properties ["Domain"] = node.Name;

			Node = node;
		}

		static ApplicationConfig GetApplicationConfig()
		{
			var configFile = System.IO.Path.Combine (System.AppDomain.CurrentDomain.BaseDirectory, "MerlionApplication.config");
			if (!System.IO.File.Exists(configFile)) {
				throw new System.IO.FileNotFoundException (
					"Application configuration file (MerlionApplication.config) could not be found.", configFile);
			}

			var xser = new System.Xml.Serialization.XmlSerializer (typeof(ApplicationConfig));
			using (var ser = System.IO.File.OpenRead(configFile)) {
				var cfg = (ApplicationConfig)xser.Deserialize (ser);
				if (cfg == null) {
					throw new System.Configuration.ConfigurationErrorsException (
						"Failed to deserialize the application configuration file (MerlionApplication.config).");
				}

				return cfg;
			}
		}

		public void Start()
		{
			if (app != null) {
				throw new InvalidOperationException ("Cannot start twice.");
			}

			// Add server directory as assmebly probe path
			string serverPath = Node.ServerDirectory;
			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) => {
				string assemblyPath = Path.Combine(serverPath, new AssemblyName(args.Name).Name + ".dll");
				if (File.Exists(assemblyPath)) {
					return Assembly.LoadFrom(assemblyPath);
				}

				assemblyPath = Path.Combine(serverPath, new AssemblyName(args.Name).Name + ".exe");
				if (File.Exists(assemblyPath)) {
					return Assembly.LoadFrom(assemblyPath);
				}

				return null;
			};

			Type apptype;
			System.Reflection.ConstructorInfo ctor;
			try {
				config = GetApplicationConfig();

				apptype = Type.GetType(config.ApplicationTypeName, true);
				if (!typeof(Application).IsAssignableFrom(apptype)) {
					throw new InvalidOperationException(
						string.Format("Type '{0}' does not derive from '{1}'. " +
							"(note: base class of the type is '{2}'.)",
							apptype.FullName, typeof(Application).FullName,
							apptype.BaseType));
				}
				if (apptype.ContainsGenericParameters) {
					throw new InvalidOperationException(
						string.Format("Type '{0}' is open generic type.",
							apptype.FullName));
				}
				if (apptype.IsAbstract) {
					throw new InvalidOperationException(
						string.Format("Type '{0}' is abstract.",
							apptype.FullName));
				}

				ctor = apptype.GetConstructor(new Type[0]);

				if (ctor == null) {
					throw new InvalidOperationException(
						string.Format("Nullary constructor of '{0}' was not found.",
							apptype.FullName));
				}

			} catch (System.Configuration.ConfigurationErrorsException) {
				throw;
			} catch (Exception ex) {
				throw new System.Configuration.ConfigurationErrorsException (
					"Error occured while processing application configuration file.", ex);
			}

			app = (Application)ctor.Invoke (new object[0]);
		}

		public void Accept(MerlionClient client, MerlionRoom room) {
			app.Accept (client, room);
		}

		public void Unload()
		{
			var u = Unloading;
			if (u != null) {
				Unloading (this, EventArgs.Empty);
			}
		}

		public Stream WrapStream(RemotableStream s)
		{
			return new RemotableStreamWrapper(s);
		}

		sealed class LogAppender: log4net.Appender.BufferingAppenderSkeleton
		{
			protected override void SendBuffer (log4net.Core.LoggingEvent[] events)
			{
				ApplicationServer.Instance.Node.Log (events);
			}
		}
	}

	public abstract class RemotableStream: MarshalByRefObject
	{
		public abstract void Write(byte[] buffer);
		public abstract byte[] Read(int count);
		public abstract void Close();
	}

	sealed class RemotableStreamWrapper: Stream
	{
		readonly RemotableStream inner;

		public RemotableStreamWrapper(RemotableStream inner)
		{
			this.inner = inner;
			if (inner == null)
				throw new ArgumentNullException ("inner");
		}

		public override void Flush ()
		{
		}

		public override void Close ()
		{
			inner.Close ();
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			var b = inner.Read (count);
			Buffer.BlockCopy (b, 0, buffer, offset, b.Length);
			return b.Length;
		}

		public override long Seek (long offset, SeekOrigin origin)
		{
			throw new NotSupportedException ();
		}

		public override void SetLength (long value)
		{
			throw new NotSupportedException ();
		}

		public override void Write (byte[] buffer, int offset, int count)
		{
			if (count == buffer.Length && offset == 0) {
				inner.Write (buffer);
			} else {
				var b = new byte[count];
				Buffer.BlockCopy (buffer, offset, b, 0, count);
				inner.Write (b);
			}
		}

		public override bool CanRead {
			get {
				return true;
			}
		}

		public override bool CanSeek {
			get {
				return false;
			}
		}

		public override bool CanWrite {
			get {
				return true;
			}
		}

		public override long Length {
			get {
				throw new NotImplementedException ();
			}
		}

		public override long Position {
			get {
				throw new NotSupportedException ();
			}
			set {
				throw new NotSupportedException ();
			}
		}
	}
}

