using System;
using System.Net;
using System.Collections.Generic;
using log4net;
using System.Web.Script.Serialization;
using System.Linq;

namespace Merlion.Server.Web
{

	sealed class WebInterface: IDisposable
	{
		static readonly ILog log = LogManager.GetLogger(typeof(WebInterface));
	
		readonly HttpServer listener;
		//readonly IWebInterfaceAuthentication auth;

		readonly IMasterServer master;

		readonly Dictionary<string, byte[]> staticContents =
			new Dictionary<string, byte[]>();

		readonly Dictionary<string, Action<HttpServerContext>> handlers =
			new Dictionary<string, Action<HttpServerContext>>();

		public WebInterface (IMasterServer server)
		{
			this.master = server;

			listener = new HttpServer (AppConfiguration.WebInterfaceAddress);
			/*auth = AppConfiguration.CreateObject<IWebInterfaceAuthentication> 
				(AppConfiguration.WebInterfaceAuthentication);*/

			//listener.Prefixes.Add (AppConfiguration.WebInterfacePrefix);
			//listener.AuthenticationSchemeSelectorDelegate = auth.GetAuthenticationSchemes;

			AddStaticContents ("/Background.gif");
			AddStaticContents ("/WebPage.css");
			AddStaticContents ("/Default.html");
			AddStaticContents ("/Console.js");
			AddStaticContents ("/jquery-2.1.1.min.js");
			AddStaticContents ("/css/bootstrap.min.css");
			AddStaticContents ("/js/bootstrap.min.js");
			AddStaticContents ("/fonts/glyphicons-halflings-regular.eot");
			AddStaticContents ("/fonts/glyphicons-halflings-regular.ttf");
			AddStaticContents ("/fonts/glyphicons-halflings-regular.svg");
			AddStaticContents ("/fonts/glyphicons-halflings-regular.woff");

			handlers.Add ("/deploy", HandleDeploy);
			handlers.Add ("/status.json", MakeJsonApi(HandleGetStatus));
			handlers.Add ("/version/throttle/set.json", MakeJsonApi(HandleSetVersionThrottle));
			handlers.Add ("/version/destroy.json", MakeJsonApi(HandleDestroyVersion));
			handlers.Add ("/node/throttle/set.json", MakeJsonApi(HandleSetNodeThrottle));
			handlers.Add ("/node/destroy.json", MakeJsonApi(HandleDestroyNode));

			log.InfoFormat ("Running web server at {0}.", AppConfiguration.WebInterfaceAddress);

		}

		void AddStaticContents(string url)
		{
			using (var ms = new System.IO.MemoryStream())
			using (var res = System.Reflection.Assembly.GetExecutingAssembly ().GetManifestResourceStream (
				"Merlion.Server.Web" + url.Replace('/', '.'))) {
				var buffer = new byte[2048];
				int readCount;
				while ((readCount = res.Read(buffer, 0, buffer.Length)) > 0) {
					ms.Write (buffer, 0, readCount);
				}
				staticContents.Add (url, ms.ToArray ());
			}
		}

		public void Run ()
		{
			listener.Start ();
			/*
			if (!AppConfiguration.WebInterfacePrefix.StartsWith ("https")) {
				// Check web site periodically because Mono's HttpListener locks up often.
				var timer = new System.Threading.Timer (CheckHang);
				timer.Change (0, 1000);
			}*/

			while (true) {
				using (var ctx = listener.GetContext ()) {
					
					try {
						/*if (!auth.Authenticate(ctx)) {
							// Unauthorized.
							ctx.Response.ContentType = "text/plain";
							using (var writer = new System.IO.StreamWriter(ctx.Response.OutputStream,
								new System.Text.UTF8Encoding())) {
								writer.WriteLine ("401 Unauthorized");
							}
							continue;
						}
						*/
						Handle(ctx);
					} catch (Exception ex) {
						try {
							try {
								ctx.Response.ContentType = "text/plain";
							} catch {}
							using (var writer = new System.IO.StreamWriter(ctx.Response.GetOutputStream(),
								new System.Text.UTF8Encoding())) {
								writer.WriteLine(ex.Message);
								writer.WriteLine();
								writer.WriteLine(ex.StackTrace);
							}
						} catch {}
					}
				}
			}
		}
		/*
		void CheckHang(object blah)
		{
			try {
				var req = System.Net.WebRequest.CreateHttp(AppConfiguration.WebInterfacePrefix
					.Replace("+","localhost").Replace("*","localhost"));
				req.Timeout = 5000;

				var basic = auth as BasicWebInterfaceAuthentication;
				if (basic != null) {
					var cred = new System.Net.NetworkCredential();
					cred.UserName = basic.UserName;
					cred.Password = basic.Password;
					req.Credentials = cred;
				}

				using (var r = req.GetResponse()) {
					// okay!
				}

			} catch (Exception ex) {
				listener.Stop ();
				listener.Start ();
			}
		}*/

		void Handle(HttpServerContext context)
		{
			var path = context.Request.Uri.AbsolutePath;

			if (path == "/")
				path = "/Default.html";

			{
				Action<HttpServerContext> handler;
				if (handlers.TryGetValue(path, out handler)) {
					handler (context);
					return;
				}
			}

			{
				byte[] staticContent;
				if (staticContents.TryGetValue(path, out staticContent)) {
					var type = "application/x-octet-stream";
					if (path.EndsWith(".css", StringComparison.InvariantCultureIgnoreCase)) {
						type = "text/css";
					} else if (path.EndsWith(".js", StringComparison.InvariantCultureIgnoreCase)) {
						type = "text/javascript";
					} else if (path.EndsWith(".png", StringComparison.InvariantCultureIgnoreCase)) {
						type = "image/png";
					} else if (path.EndsWith(".jpeg", StringComparison.InvariantCultureIgnoreCase)) {
						type = "image/jpeg";
					} else if (path.EndsWith(".gif", StringComparison.InvariantCultureIgnoreCase)) {
						type = "image/gif";
					} else if (path.EndsWith(".html", StringComparison.InvariantCultureIgnoreCase)) {
						type = "text/html";
					}
					context.Response.ContentType = type;

					context.Response.GetOutputStream().Write (staticContent, 0, staticContent.Length);

					return;
				}
				context.Response.StatusCode = 404;
				OutputErrorPage (context, "404 Not Found");
			}
		}

		Action<HttpServerContext> MakeJsonApi(Func<HttpServerContext, IDictionary<string, string>, object> handler)
		{
			return MakeApi ((context, param) => {
				context.Response.ContentType = "application/json";
				object ret;
				try {
					ret = handler(context, param);
				} catch (Exception ex) {
					ret = new Dictionary<string, object> {
						{ "errorType", ex.GetType().FullName },
						{ "error", ex.Message },
						{ "detail", ex.StackTrace }
					};
					context.Response.StatusCode = 500;
				}
				var jsonSer = new JavaScriptSerializer ();
				var txt = jsonSer.Serialize (ret);

				using (var writer = new System.IO.StreamWriter(context.Response.GetOutputStream(),
					new System.Text.UTF8Encoding())) {
					writer.Write (txt);
				}

			});
		}

		Action<HttpServerContext> MakeApi(Action<HttpServerContext, IDictionary<string, string>> handler)
		{
			return context => {
				if (context.Request.Method != "POST") {
					handler(context, new Dictionary<string, string>());
					return;
				} 

				var contentType = context.Request.ContentType;
				if (contentType.Split(';')[0].Trim() != "application/x-www-form-urlencoded") {
					context.Response.StatusCode = 400;
					OutputErrorPage (context, "This endpoint only accepts application/x-www-form-urlencoded.");
					return;
				}

				var instream = context.Request.GetInputStream();
				if (context.Request.ContentLength64 >= 0) {
					instream = new Utils.TakeStream (instream, context.Request.ContentLength64);
				}

				handler(context, FormUrlEncodedParser.Parse(instream));
			};
		}

		object HandleSetVersionThrottle(HttpServerContext context, IDictionary<string, string> param)
		{
			if (!param.ContainsKey("version")) 
				throw new InvalidOperationException ("Version is not specified.");
			if (!param.ContainsKey("throttle")) 
				throw new InvalidOperationException ("Throttle is not specified.");

			string version = param ["version"];
			double throttle = double.Parse (param ["throttle"]);

			if (double.IsNaN(throttle) || double.IsInfinity(throttle) || throttle < 0.0 || throttle > 1.0)
				throw new InvalidOperationException ("Invalid throttle value.");

			var balancer = master.Balancer;
			var ver = balancer.GetVersion (version);

			if (ver != null) {
				ver.Throttle = throttle;
				return new Dictionary<string, object> {
					{ "result", "ok" }
				};
			} else {
				return new Dictionary<string, object> {
					{ "result", "notFound" }
				};
			}
		}

		object HandleDestroyVersion(HttpServerContext context, IDictionary<string, string> param)
		{
			if (!param.ContainsKey("version")) 
				throw new InvalidOperationException ("Version is not specified.");

			string version = param ["version"];
			var balancer = master.Balancer;
			balancer.RemoveVersion (version);
			return new Dictionary<string, object> {
				{ "result", "ok" }
			};
		}

		object HandleSetNodeThrottle(HttpServerContext context, IDictionary<string, string> param)
		{
			if (!param.ContainsKey("name")) 
				throw new InvalidOperationException ("Node name is not specified.");
			if (!param.ContainsKey("throttle")) 
				throw new InvalidOperationException ("Throttle is not specified.");

			string name = param ["name"];
			double throttle = double.Parse (param ["throttle"]);

			if (double.IsNaN(throttle) || double.IsInfinity(throttle) || throttle < 0.0 || throttle > 1.0)
				throw new InvalidOperationException ("Invalid throttle value.");

			var balancer = master.Balancer;
			var ver = balancer.GetNode (name);

			if (ver != null) {
				ver.Throttle = throttle;
				return new Dictionary<string, object> {
					{ "result", "ok" }
				};
			} else {
				return new Dictionary<string, object> {
					{ "result", "notFound" }
				};
			}
		}

		object HandleDestroyNode(HttpServerContext context, IDictionary<string, string> param)
		{
			if (!param.ContainsKey("name")) 
				throw new InvalidOperationException ("Node name is not specified.");

			string name = param ["name"];
			throw new NotImplementedException ();
		}

		object HandleGetStatus(HttpServerContext context, IDictionary<string, string> param)
		{
			var obj = new Dictionary<string, object> {
				{ "versions", GetVersionInfo() },
				{ "nodes", GetServerInfo() }
			};
			return obj;
		}

		object GetVersionInfo()
		{
			return master.Balancer.GetAllVersions().Select(ver => 
				new Dictionary<string, object> {
					{ "name", ver.Name },
					{ "throttle", ver.Throttle },
				});
		}

		object GetServerInfo()
		{
			return master.GetAllNodeInfos ().Select (node => GetOneServerInfo(node));
		}

		object GetOneServerInfo(NodeStatus info)
		{
			var vers = new Dictionary<string, object> ();
			foreach (var ver in info.Domains) {
				vers.Add (ver.Key, new Dictionary<string, object> {
					{ "numRooms", ver.Value.NumRooms },
					{ "numClients", ver.Value.NumClients }
				});
			}
			return new Dictionary<string, object> {
				{ "id", info.ID },
				{ "name", info.Name },
				{ "host", info.HostName },
				{ "numRooms", info.NumRooms },
				{ "numClients", info.NumClients },
				{ "domains", vers },
				{ "uptime", info.UpTime.TotalSeconds },
				{ "throttle", info.Throttle }
			};
		}
			
		void HandleDeploy(HttpServerContext context)
		{
			if (context.Request.Method != "POST") {
				context.Response.StatusCode = 400;
				OutputErrorPage (context, "This endpoint only accepts POST.");
				return;
			}

			var contentType = context.Request.ContentType;
			if (contentType.Split(';')[0].Trim() != "multipart/form-data") {
				context.Response.StatusCode = 400;
				OutputErrorPage (context, "This endpoint only accepts multipart/form-data.");
				return;
			}

			var instream = context.Request.GetInputStream();
			if (context.Request.ContentLength64 >= 0) {
				instream = new Utils.TakeStream (instream, context.Request.ContentLength64);
			}

			var parts = MultipartParser.Parse (instream, contentType);

			try {
				MultipartParser.Item nameItem = null, fileItem = null;
				foreach (var part in parts) {
					var n = part.Name;
					if (n == "name")
						nameItem = part;
					else if (n == "file")
						fileItem = part;
				}

				if (nameItem == null) {
					context.Response.StatusCode = 400;
					OutputErrorPage (context, "Name missing.");
					return;
				}


				var name = Utils.StreamUtils.ReadAllString(nameItem.Stream);
				if (name.Length == 0 || name.Length > 64) {
					context.Response.StatusCode = 400;
					OutputErrorPage (context, "Name too long or too short.");
					return;
				}

				var verman = master.VersionManager;

				if (!verman.VersionExists(name)) {

					if (fileItem == null) {
						context.Response.StatusCode = 400;
						OutputErrorPage (context, "File missing.");
						return;
					}

					if (fileItem.Stream.Length == 0){
						OutputErrorPage (context, "File is empty.");
						return;
					}

					using (var outstream = verman.StartDeployVersion(name)) {
						Utils.StreamUtils.Copy(fileItem.Stream, outstream);
					}

					verman.EndDeployVersion(name);

				}

				// Add this version to Balancer
				master.Balancer.GetVersion(name, true);

				context.Response.Redirect("/");
			} finally {
				foreach (var part in parts)
					part.Dispose ();
			}

			throw new NotImplementedException();
			//var name = context.Request;
		}

		void OutputErrorPage(HttpServerContext context, string message)
		{
			context.Response.ContentType = "text/plain";
			using (var writer = new System.IO.StreamWriter(context.Response.GetOutputStream(),
				new System.Text.UTF8Encoding())) {
				writer.WriteLine (message);
			}
		}

		#region IDisposable implementation

		public void Dispose ()
		{
			listener.Stop ();
		}

		#endregion
	}
	/*
	interface IWebInterfaceAuthentication
	{
		AuthenticationSchemes GetAuthenticationSchemes (HttpListenerRequest httpRequest);
		bool Authenticate (HttpServerContext context);
	}

	sealed class BasicWebInterfaceAuthentication: IWebInterfaceAuthentication
	{
		public string UserName { get; set; }
		public string Password { get; set; }

		public AuthenticationSchemes GetAuthenticationSchemes (HttpListenerRequest httpRequest)
		{
			return AuthenticationSchemes.Basic;
		}

		public bool Authenticate (HttpServerContext context)
		{
			var idt = context.User as System.Security.Principal.GenericPrincipal;
			var basicIdentity = idt.Identity as HttpListenerBasicIdentity;
			if (basicIdentity == null ||
				basicIdentity.Name != UserName ||
				basicIdentity.Password != Password) {
				context.Response.StatusCode = 401;
				context.Response.AddHeader ("WWW-Authenticate", "Basic realm=\"Authentication Required\"");
				return false;
			}
			return true;
		}
	}*/
}

