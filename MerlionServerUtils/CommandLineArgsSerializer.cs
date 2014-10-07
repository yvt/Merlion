﻿using System;
using System.Collections.Generic;
using System.ComponentModel;

namespace Merlion.Utils
{
	public class CommandLineArgsSerializer
	{
		Type type;



		public CommandLineArgsSerializer(Type type)
		{
			if (type == null)
				throw new ArgumentNullException ("type");
			this.type = type;
		}


		public void WriteUsage(System.IO.TextWriter writer)
		{
			writer.WriteLine ("USAGE:");
			writer.WriteLine ();

			foreach (var meth in type.GetMethods(
				System.Reflection.BindingFlags.Instance |
				System.Reflection.BindingFlags.NonPublic |
				System.Reflection.BindingFlags.Public)) {
				if (meth.Name.StartsWith("Handle", StringComparison.InvariantCulture)) {
					var name = meth.Name.Substring (6);
					var param = meth.GetParameters ();
					if (param.Length == 0) {
						writer.WriteLine ("-" + name);
					} else {
						var paramNameAttrs = param [0].GetCustomAttributes (typeof(DescriptionAttribute), false);
						var pdesc = param [0].Name.ToUpperInvariant ();
						if (paramNameAttrs.Length > 0) {
							pdesc = ((DescriptionAttribute)paramNameAttrs [0]).Description;
						}
						writer.WriteLine ("-" + name + " " + pdesc);
					}

					var desc = meth.GetCustomAttributes (typeof(DescriptionAttribute), false);
					if (desc.Length > 0) {
						writer.WriteLine ();
						writer.WriteLine ("    {0}", ((DescriptionAttribute)desc [0]).Description);
					}

					writer.WriteLine ();
				}
			}
		}

		public void DeserializeObjectInplace(object obj, string[] args)
		{
			if (obj == null)
				throw new ArgumentNullException ("obj");
			if (args == null)
				throw new ArgumentNullException ("args");

			if (!type.IsInstanceOfType (obj)) {
				throw new InvalidOperationException (string.Format("Type of given object '{0}' cannot be used as '{1}'.",
					obj.GetType().ToString(), type.ToString()));
			}

			int i = 0;
			while (i < args.Length) {
				var arg = args [i];
				if (arg.Length == 0) {
					continue;
				}
				if (arg[0] == '-') {
					var name = arg.Substring (1);
					var meth = type.GetMethod ("Handle" + name,
						System.Reflection.BindingFlags.Instance |
						System.Reflection.BindingFlags.NonPublic|
						System.Reflection.BindingFlags.Public);
					if (meth == null) {
						throw new CommandLineArgsException (string.Format("Unknown option: '{0}'", arg));
					}

					var param = meth.GetParameters ();
					try {
						if (param.Length == 0) {
							meth.Invoke (obj, new object[0]);
						} else {
							++i;
							if (i >= args.Length) {
								throw new CommandLineArgsException (string.Format("Option needs argument: '{0}'", arg));
							}
							arg += " " + args[i]; // for error message
							meth.Invoke (obj, new object[] { args[i] });
						}
					} catch (System.Reflection.TargetInvocationException ex) {
						throw new CommandLineArgsException (string.Format("Error while parsing argument '{0}':", arg), ex);
					}
				} else {
					throw new CommandLineArgsException (string.Format("Unknown option: '{0}'", arg));
				}
				++i;
			}
		}
	}

	public class CommandLineArgsSerializer<T>: CommandLineArgsSerializer
	{
		public CommandLineArgsSerializer(): base(typeof(T))
		{ }

		public void DeserializeInplace(T obj, string[] args)
		{
			DeserializeObjectInplace (obj, args);
		}
	}

	[Serializable]
	public class CommandLineArgsException: Exception
	{
		public CommandLineArgsException(string message):
		base(message) { }
		public CommandLineArgsException(string message, Exception ex):
		base(message, ex) { }
	}
}
