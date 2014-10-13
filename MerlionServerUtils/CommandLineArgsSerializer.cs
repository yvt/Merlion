using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace Merlion.Utils
{
	public class CommandLineParameterAliasAttribute: Attribute
	{
		public string Alias { get; private set; }

		public CommandLineParameterAliasAttribute(string alias)
		{
			if (string.IsNullOrWhiteSpace(alias))
				throw new ArgumentNullException ("alias");
			Alias = alias;
		}
	}

	public class CommandLineArgsSerializer
	{
		Type type;

		sealed class Parameter
		{
			public string Name;
			public readonly List<Alias> Aliases = new List<Alias>();
			public Action<object, string> Handler;
			public string ParameterName;
			public string Description;
		}

		sealed class Alias
		{
			public string Name;
			public Parameter Parameter;
			public bool Ambiguous = false;
		}

		readonly SortedDictionary<string, Parameter> parameters =
			new SortedDictionary<string, Parameter>();
		readonly Dictionary<string, Alias> aliases =
			new Dictionary<string, Alias> ();

		public CommandLineArgsSerializer(Type type)
		{
			if (type == null)
				throw new ArgumentNullException ("type");
			this.type = type;

			foreach (var meth in type.GetMethods(
				System.Reflection.BindingFlags.Instance |
				System.Reflection.BindingFlags.NonPublic |
				System.Reflection.BindingFlags.Public)) {
				if (meth.Name.StartsWith("Handle", StringComparison.InvariantCulture)) {
					var name = meth.Name.Substring (6);
					var param = meth.GetParameters ();
					var p = new Parameter ();

					if (name == "Default") {
						name = string.Empty;
					}

					p.Name = name;
					if (param.Length == 0) {
						p.ParameterName = null;
						p.Handler = (obj, _) => {
							meth.Invoke(obj, new object[] {});
						};

						if (p.Name == "") {
							throw new InvalidOperationException ("HandleDefault must have a parameter.");
						}
					} else if (param.Length == 1) {
						var paramNameAttrs = param [0].GetCustomAttributes (typeof(DescriptionAttribute), false);
						var pdesc = param [0].Name.ToUpperInvariant ();
						if (paramNameAttrs.Length > 0) {
							pdesc = ((DescriptionAttribute)paramNameAttrs [0]).Description;
						}
						p.ParameterName = pdesc;
						p.Handler = (obj, paramValue) => {
							object converted;
							converted = Convert.ChangeType(paramValue, param[0].ParameterType);
							meth.Invoke(obj, new object[] { converted });
						};
					} else {
						// Not a valid parameter.
						continue;
					}

					var desc = meth.GetCustomAttributes (typeof(DescriptionAttribute), false);
					if (desc.Length > 0) {
						p.Description = ((DescriptionAttribute)desc [0]).Description;
					}

					AddAlias (p.Name, p);
					// AddAlias (p.Name.Substring (0, 1), p);

					var abbrev = GuessAbbreviation (p.Name);
					if (abbrev != null)
						AddAlias (abbrev, p);

					var aliasAttrs = meth.GetCustomAttributes (typeof(CommandLineParameterAliasAttribute), false);
					foreach (var aliasAttr in aliasAttrs) {
						AddAlias (((CommandLineParameterAliasAttribute)aliasAttr).Alias, p);
					}

					parameters.Add (p.Name.ToLowerInvariant(), p);

				}
			}
		}

		void AddAlias(string text, Parameter param)
		{
			var lower = text.ToLowerInvariant ();

			Alias a;
			if (aliases.TryGetValue(lower, out a)) {
				if (a.Parameter != param) {
					a.Ambiguous = true;
				}
				return;
			}


			aliases[lower] = a = new Alias() {
				Name = text,
				Parameter = param
			};
			param.Aliases.Add (a);
		}

		string GuessAbbreviation(string name)
		{
			var cap = new List<char> ();
			for (int i = 0; i < name.Length; ++i)
				if (char.IsUpper (name [i]))
					cap.Add (name [i]);
			if (cap.Count == 0)
				return null;
			return new string (cap.ToArray ());
		}

		public void WriteUsageHeader()
		{
			WriteUsageHeader (System.Reflection.Assembly.GetCallingAssembly ());
		}

		static T GetAssemblyAttribute<T>(System.Reflection.Assembly asm) where T : Attribute
		{
			var arr = asm.GetCustomAttributes (typeof(T), true);
			if (arr.Length == 0)
				return null;
			return (T)arr [0];
		}

		public void WriteUsageHeader(System.Reflection.Assembly asm)
		{
			Console.WriteLine ();
			Console.WriteLine ("  {0}", asm.GetName().Name);
			Console.WriteLine ("  ----------------------");
			Console.WriteLine ("  {0} {1}",
				asm.GetName().Version,
				GetAssemblyAttribute<System.Reflection.AssemblyDescriptionAttribute>(asm).Description);
			Console.WriteLine ("  {0}",
				GetAssemblyAttribute<System.Reflection.AssemblyCopyrightAttribute>(asm).Copyright);
			Console.WriteLine ("  {0}",
				GetAssemblyAttribute<System.Reflection.AssemblyTrademarkAttribute>(asm).Trademark);
			Console.WriteLine ();
			Console.WriteLine ();
		}

		public void WriteUsage(System.IO.TextWriter writer)
		{
			writer.WriteLine ("USAGE:");
			writer.WriteLine ();

			foreach (var p in parameters.Values) {
				var aliases = p.Aliases.Where (a => !a.Ambiguous).ToArray ();
				var parts = aliases.Select (alias => {
					if (p.ParameterName == null) {
						return "-" + alias.Name;
					} else {
						return "-" + alias.Name + " " + p.ParameterName;
					}
				});

				writer.WriteLine (string.Join (", ", parts));

				if (p.Description != null) {
					writer.WriteLine ();
					writer.WriteLine ("    {0}", p.Description);
				}

				writer.WriteLine ();
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

				string paramName;
				if (arg [0] == '-') {
					paramName = arg.Substring (1);
					++i;
				} else {
					paramName = string.Empty;
				}

				Alias alias;
				if (!aliases.TryGetValue(paramName.ToLowerInvariant(), out alias) || alias.Ambiguous) {
					throw new CommandLineArgsException (string.Format("Unknown option: '{0}'", arg));
				}

				var param = alias.Parameter;

				try {
					if (param.ParameterName == null) {
						param.Handler (obj, null);
					} else {
						if (i >= args.Length) {
							throw new CommandLineArgsException (string.Format("Option needs argument: '{0}'", arg));
						}
						arg += " " + args[i]; // for error message
						param.Handler (obj, args[i]);
						++i;
					}
				} catch (System.Reflection.TargetInvocationException ex) {
					throw new CommandLineArgsException (string.Format("Error while processing argument '{0}':", arg), ex);
				} catch (InvalidCastException ex) {
					throw new CommandLineArgsException (string.Format("Error while processing argument '{0}':", arg), ex);
				}

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

