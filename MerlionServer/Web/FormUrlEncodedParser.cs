using System;
using System.Collections.Generic;

namespace Merlion.Server.Web
{
	static class FormUrlEncodedParser
	{
		public static IDictionary<string, string> Parse(System.IO.Stream stream)
		{
			var text = Merlion.Server.Utils.StreamUtils.ReadAllString (stream);
			var parts = text.Split ('&');
			var ret = new Dictionary<string, string> ();
			foreach (var part in parts) {
				var bits = part.Split ('=');

				if (bits.Length == 1) {
					ret.Add (System.Web.HttpUtility.UrlDecode (bits [0]), string.Empty);
				} else {
					ret.Add (System.Web.HttpUtility.UrlDecode (bits [0]),
						System.Web.HttpUtility.UrlDecode (bits [1]));
				}
			}
			return ret;
		}
	}
}

