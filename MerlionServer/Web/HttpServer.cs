using System;
using System.Net.Sockets;
using System.Net;
using System.IO;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Threading;
using log4net;

namespace Merlion.Server.Web
{
	sealed class HttpServer
	{
		static readonly ILog log = LogManager.GetLogger(typeof(HttpServer));

		IPEndPoint endpoint;
		bool listening = false;
		TcpListener tcp;
		Thread listenThread;
		readonly ConcurrentQueue<HttpServerContext> queue = 
			new ConcurrentQueue<HttpServerContext>();
		readonly EventWaitHandle queueEvent = 
			new EventWaitHandle (false, EventResetMode.AutoReset);

		public HttpServer (IPEndPoint endpoint)
		{
			this.endpoint = endpoint;
		}

		public void Start()
		{
			if (listening)
				return;

			// Empty the queue
			HttpServerContext ctx;
			while (queue.TryDequeue(out ctx)) { }

			tcp = new TcpListener (endpoint);
			tcp.Start ();

			listenThread = new Thread (Listen);
			listenThread.Name = "HttpSever Listener";
			listenThread.Start ();
			listening = true;
		}

		public void Stop()
		{
			if (!listening)
				return;

			tcp.Stop ();
			tcp = null;
			listenThread.Join ();
			listenThread = null;
			listening = false;
		}

		void Listen()
		{
			while (true) {
				try {
					var client = tcp.AcceptTcpClient ();
					client.ReceiveTimeout = 5000;
					client.SendTimeout = 5000;

					var ctx = new HttpServerContext (client);
					new Task (() => {
						try {
							ctx.Request.EnsurePopulated();
							queue.Enqueue(ctx);
							queueEvent.Set();
						} catch (HttpRequestHeaderException ex) {
							ctx.Response.StatusCode = ex.Code;
							try {
								ctx.Dispose();
							} catch (Exception ex2) {
								log.Error("Unknown exception while returning error. Ignoring.", ex2);
							}
						} catch (Exception ex) {
							ctx.Response.StatusCode = 500;
							log.Error("Unknown exception while handling request.", ex);
							try {
								ctx.Dispose();
							} catch (Exception ex2) {
								log.Error("Unknown exception while returning error. Ignoring.", ex2);
							}
						}
					}).Start ();
				} catch (SocketException) {
					// Aborted.
					return;
				}
			}
		}

		public HttpServerContext GetContext()
		{
			HttpServerContext context;
			while (!queue.TryDequeue(out context)) {
				if (!listening) {
					throw new InvalidOperationException ("Not listening.");
				}
				queueEvent.WaitOne ();
			}
			return context;
		}


	}

	sealed class HttpServerContext: IDisposable
	{
		TcpClient tcp;
		Stream stream;
		public HttpServerRequest Request { get; private set; }
		public HttpServerResponse Response { get; private set; }

		public HttpServerContext(TcpClient tcp)
		{
			this.tcp = tcp;
			stream = new Utils.UnclosableStream(tcp.GetStream ());
			Request = new HttpServerRequest (stream, this);
			Response = new HttpServerResponse (stream);
		}

		public void Dispose()
		{
			Response.GetOutputStream ().Flush();
			tcp.Close ();
		}
	}

	enum HttpVersion
	{
		Http1_0,
		Http1_1
	}

	[Serializable]
	sealed class HttpRequestHeaderException: Exception
	{
		public int Code { get; private set; }
		public HttpRequestHeaderException(int code)
		{
			Code = code;
		}
	}

	sealed class HttpServerRequest
	{
		readonly System.IO.Stream stream;
		readonly Dictionary<string, string> headers = 
			new Dictionary<string, string>();
		bool populated = false;
		readonly HttpServerContext context;

		public HttpVersion Version { get; private set; }
		public string Method { get; private set; }
		public Uri Uri { get; private set; }

		Stream inputStream;

		static System.Text.UTF8Encoding utf8 = new System.Text.UTF8Encoding();
		static readonly byte[] crlf;

		static HttpServerRequest()
		{
			crlf = utf8.GetBytes("\r\n");
		}


		public HttpServerRequest(System.IO.Stream stream, HttpServerContext context)
		{
			this.stream = stream;
			this.context = context;
		}

		void ParseHttpHeader(string header)
		{
			int index = 0;
			var sb = new System.Text.StringBuilder ();
			while (index < header.Length) {
				var sep = header.IndexOf (':', index);
				if (sep < 0) {
					break;
				}

				var name = header.Substring (index, sep - index).Trim();
				index = sep + 1;
				while (index < header.Length && header[index] == ' ')
					++index;

				string value;
				if (header[index] == '"') {
					// quoted string
					++index;
					sb.Clear ();
					while (true) {
						var nextIndex = header.IndexOf ('"', index);
						if (nextIndex < 0) {
							throw new HttpRequestHeaderException (400);
						}
						sb.Append (header, index, nextIndex - index);
						if (nextIndex < header.Length - 1 && header[nextIndex + 1] == '"') {
							sb.Append ('"');
							index = nextIndex + 2;
						} else {
							nextIndex = header.IndexOf ("\r\n", nextIndex, StringComparison.Ordinal);
							if (nextIndex == -1)
								nextIndex = header.Length;
							index = nextIndex + 2;
							break;
						}
					}
					value = sb.ToString ();
				} else {
					var nextIndex = header.IndexOf ("\r\n", index, StringComparison.Ordinal);
					if (nextIndex == -1)
						nextIndex = header.Length;
					value = header.Substring (index, nextIndex - index).Trim();
					index = nextIndex + 2;
				}

				headers [name] = value;

			}
		}

		public void EnsurePopulated()
		{
			if (populated)
				return;

			populated = true;

			var splitter = new Utils.StreamSplitter(stream, crlf);
			int state = 0;
			var sb = new System.Text.StringBuilder ();

			foreach (var part in splitter) {
				if (state == 0) {
					// HTTP request line
					var line = Utils.StreamUtils.ReadAllString (part, 4096).Trim();
					var parts = line.Split (' ');
					if (parts.Length != 3) {
						throw new HttpRequestHeaderException (400);
					}
					Method = parts [0];
					if (parts[1].StartsWith("http://", StringComparison.Ordinal) ||
						parts[1].StartsWith("https://", StringComparison.Ordinal))
						Uri = new Uri (parts [1]);
					else
						Uri = new Uri ("http://unknown" + parts [1]);
					if (parts[2] == "HTTP/1.1") {
						Version = HttpVersion.Http1_1;
					} else if (parts[2] == "HTTP/1.0") {
						Version = HttpVersion.Http1_0;
					} else {
						throw new HttpRequestHeaderException (505);
					}
					state = 1;
				} else if (state == 1) {
					var line = Utils.StreamUtils.ReadAllString (part, 65536).Trim();
					if (line.Length == 0) {
						// End of header.
						ParseHttpHeader (sb.ToString ());
						state = 2;
					} else {
						sb.Append (line);
						sb.Append ("\r\n");
					}
				} else if (state == 2) {
					// Request body.
					splitter.IgnoreRemainingSeparators ();
					inputStream = part;
					break;
				}
			}

			if (inputStream == null) {
				inputStream = new MemoryStream ();
			}

		}

		public Stream GetInputStream()
		{
			return inputStream;
		}

		public IDictionary<string, string> Headers
		{
			get {
				EnsurePopulated ();
				return headers;
			}
		}

		public string ContentType
		{
			get { 
				string ret;
				if (headers.TryGetValue ("Content-Type", out ret))
					return ret;
				return "";
			}
		}

		public long ContentLength64
		{
			get {
				long value;
				if (long.TryParse (Headers ["Content-Length"], out value))
					return value;
				return -1;
			}
		}
	}

	sealed class HttpServerResponse
	{
		readonly System.IO.Stream stream;
		bool headerWritten = false;
		readonly Dictionary<string, string> headers = 
			new Dictionary<string, string>();

		static System.Text.UTF8Encoding utf8 = new System.Text.UTF8Encoding();

		HttpVersion version;
		public HttpVersion Version {
			get { return version; }
			set {
				CheckHeaderNotWritten ();
				version = value;
			}
		}
		int statusCode = 200;
		public int StatusCode {
			get { return statusCode; }
			set {
				CheckHeaderNotWritten ();
				statusCode = value;
			}
		}
		string statusText;
		public string StatusText {
			get { return statusText; }
			set {
				CheckHeaderNotWritten ();
				statusText = value;
			}
		}

		public string ContentType {
			get { 
				string ret;
				if (headers.TryGetValue ("Content-Type", out ret))
					return ret;
				return "";
			}
			set { AddHeader ("Content-Type", value); }
		}

		public HttpServerResponse(System.IO.Stream stream)
		{
			this.stream = stream;
		}

		void CheckHeaderNotWritten()
		{
			if (headerWritten)
				throw new InvalidOperationException ("Cannot modify header already sent.");
		}

		public void AddHeader(string key, string value)
		{
			headers [key] = value;
		}

		void WriteHeader()
		{
			if (headerWritten)
				return;

			if (string.IsNullOrEmpty(StatusText)) {
				switch (StatusCode) {
				case 100: statusText = "Continue"; break;
				case 101: statusText = "Switching Protocols"; break;
				case 200: statusText = "OK"; break;
				case 400: statusText = "Bad Request"; break;
				case 401: statusText = "Unauthorized"; break;
				case 404: statusText = "Not Found"; break;
				case 405: statusText = "Method Not Allowed"; break;
				case 408: statusText = "Request Time-out"; break;
				case 500: statusText = "Internal Server Error"; break;
				case 501: statusText = "Not Implemented"; break;
				default:
					statusText = "Blah";
					break;
				}
			}

			using (var ss = new System.IO.StreamWriter(stream, utf8)) {
				ss.NewLine = "\r\n";
				ss.WriteLine(string.Format("{0} {1} {2}",
					Version == HttpVersion.Http1_1 ? "HTTP/1.1" : "HTTP/1.0",
					StatusCode, StatusText));
				foreach (var h in headers) {
					ss.WriteLine (h.Key + ": " + h.Value);
					// ss.WriteLine (h.Key + ": \"" + h.Value.Replace("\"", "\"\"") + "\"");
				}
				ss.WriteLine ();
			}

			headerWritten = true;
		}

		public System.IO.Stream GetOutputStream()
		{
			WriteHeader ();
			return stream;
		}

		public void Redirect(string target)
		{
			StatusCode = 302;
			AddHeader ("Location", target);
		}
	}
}

