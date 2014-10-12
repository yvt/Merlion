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
using System.Configuration;
using System.IO;

namespace Merlion.Server
{
	static class AppConfiguration
	{
		static System.Collections.Specialized.NameValueCollection appset;

		public static System.Collections.Specialized.NameValueCollection AppSettings
		{
			get {
				if (appset == null)
					appset = ConfigurationManager.AppSettings;
				return appset; 
			}
			set {
				appset = value;
			}
		}

		public static object CreateObject(string text)
		{
			try {
				var parts = text.Split (';');
				var type = Type.GetType (parts [0], true);
				var ctor = type.GetConstructor(new Type[0]);
				if (ctor == null) {
					throw new InvalidOperationException(string.Format(
						"Type '{0}' doesn't have a nullary constructor.", type.FullName));
				}

				var obj = ctor.Invoke(new object[0]);

				for (int i = 1; i < parts.Length; ++i) {
					var parts2 = parts[i].Split('=');
					if (parts2.Length != 2) {
						throw new FormatException(string.Format("Malformed property part: '{0}'",
							parts[i]));
					}

					var name = parts2[0].Trim();
					var value = parts2[1].Trim();
					var field = type.GetField(name,
						System.Reflection.BindingFlags.Instance |
						System.Reflection.BindingFlags.Public);
					if (field != null) {
						field.SetValue(obj, Convert.ChangeType(value, field.FieldType));
						continue;
					}

					var meth = type.GetMethod("set_" + name,
						System.Reflection.BindingFlags.Instance |
						System.Reflection.BindingFlags.Public);
					if (meth != null) {
						var param = meth.GetParameters();
						if (param.Length != 1 ||
							param[0].IsOut) {
							throw new InvalidOperationException(string.Format(
								"Method '{0}' of '{1}' cannot be used as a property setter.",
								meth, type.FullName));
						}

						meth.Invoke(obj, new [] {
							Convert.ChangeType(value, param[0].ParameterType)
						});
						continue;
					}

					throw new InvalidOperationException(string.Format(
						"Could not find property or field '{0}' of the type '{1}'.",
						name, type.FullName));
				}

				return obj;
			} catch (Exception ex) {
				throw new ConfigurationErrorsException (
					string.Format ("Failed to load a object with specifier '{0}'.", text), ex);
			}
		}

		public static T CreateObject<T>(string text) where T : class
		{
			var obj = CreateObject (text) as T;
			if (obj == null) {
				throw new ConfigurationErrorsException (
					string.Format ("Failed to load a object with specifier '{0}' because " +
						"created object is not compatible with '{1}'.", text, 
						typeof(T).FullName));
			}

			return obj;
		}

		public static System.Net.IPEndPoint ParseIPEndPoint(string s)
		{
			var parts = s.Split (':');
			return new System.Net.IPEndPoint (
				System.Net.IPAddress.Parse (parts [0]),
				int.Parse (parts [1]));
		}

		public static string BaseDirectory
		{
			get { 
				var s = AppConfiguration.AppSettings["BaseDirectory"].Trim();
				if (string.IsNullOrEmpty(s)) {
					s = Environment.GetFolderPath (Environment.SpecialFolder.ApplicationData);
					s = System.IO.Path.Combine (s, "Merlion");
				}
				return s;
			}
		}

		public static System.Net.IPEndPoint EndpointAddress
		{
			get { 
				return ParseIPEndPoint(AppConfiguration.AppSettings["EndpointAddress"]); 
			}
		}

		public static System.Net.IPEndPoint MasterServerAddress
		{
			get { 
				return ParseIPEndPoint(AppConfiguration.AppSettings["MasterServerAddress"]); 
			}
		}

		public static string Log4NetConfigFile
		{
			get { 
				var s = AppConfiguration.AppSettings["Log4NetConfigFile"];
				if (string.IsNullOrEmpty(s)) {
					s = "Log4Net.config";
				}
				if (!Path.IsPathRooted(s)) {
					s = System.IO.Path.Combine (BaseDirectory, s);
				}
				return s;
			}
		}

		public static System.Net.IPEndPoint WebInterfaceAddress
		{
			get { return ParseIPEndPoint(AppConfiguration.AppSettings["WebInterfaceAddress"]); }
		}

		public static string WebInterfaceAuthentication
		{
			get { return AppConfiguration.AppSettings["WebInterfaceAuthentication"]; }
		}

		public static string MasterServerCertificate
		{
			get {
				var s = AppConfiguration.AppSettings["MasterServerCertificate"];
				if (!string.IsNullOrEmpty(s) && !Path.IsPathRooted(s)) {
					s = Path.Combine (BaseDirectory, s);
				}
				return s;
			}
		}
		public static string MasterServerPrivateKey
		{
			get { 
				var s = AppConfiguration.AppSettings["MasterServerPrivateKey"];
				if (!string.IsNullOrEmpty(s) && !Path.IsPathRooted(s)) {
					s = Path.Combine (BaseDirectory, s);
				}
				return s;
			}
		}

		public static string DeployDirectory
		{
			get { 
				var s = AppConfiguration.AppSettings["DeployDirectory"];
				if (string.IsNullOrWhiteSpace (s)) {
					s = "";
				} else {
					s = s.Trim ();
				}
				if (!Path.IsPathRooted(s)) {
					s = System.IO.Path.Combine (BaseDirectory, s);
				}

				return s;
			}
		}

		public static string BalancerConfigFilePath
		{
			get { 
				var s = AppConfiguration.AppSettings["BalancerConfigFilePath"];
				if (string.IsNullOrWhiteSpace(s)) {
					s = "Balancer.config";
				} else {
					s = s.Trim ();
				}
				if (!Path.IsPathRooted(s)) {
					s = System.IO.Path.Combine (BaseDirectory, s);
				}
				return s;
			}
		}

		public static bool DisallowClientSideVersionSpecification
		{
			get { return Convert.ToBoolean(AppConfiguration.AppSettings["DisallowClientSideVersionSpecification"]); }
		}

		public static string NodeName
		{
			get { return AppConfiguration.AppSettings["NodeName"]; }
		}
		public static bool ForwardLogToMaster
		{
			get { return Convert.ToBoolean(AppConfiguration.AppSettings["ForwardLogToMaster"]); }
		}

		public static int UpstreamBufferSize
		{
			get { return Convert.ToInt32(AppConfiguration.AppSettings["UpstreamBufferSize"]); }
		}
		public static int DownstreamBufferSize
		{
			get { return Convert.ToInt32(AppConfiguration.AppSettings["DownstreamBufferSize"]); }
		}

	}
}

