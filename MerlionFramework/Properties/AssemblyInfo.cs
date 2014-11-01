using System.Reflection;
using System.Runtime.CompilerServices;

// Information about this assembly is defined by the following attributes.
// Change them to the values specific to your project.

[assembly: AssemblyTitle ("MerlionFramework")]
[assembly: AssemblyDescription ("")]
#if DEBUG
[assembly: AssemblyConfiguration("Debug")]
#else
[assembly: AssemblyConfiguration("Release")]
#endif
[assembly: AssemblyCompany ("yvt")]
[assembly: AssemblyProduct ("Merlion")]
[assembly: AssemblyCopyright ("Copyright © 2014 yvt <i@yvt.jp>. Apache License 2.0 applies.")]
[assembly: AssemblyTrademark ("Merlion™")]

[assembly: InternalsVisibleTo("MerlionServer")]
[assembly: InternalsVisibleTo("MerlionSimpleServer")]

// The assembly version has the format "{Major}.{Minor}.{Build}.{Revision}".
// The form "{Major}.{Minor}.*" will automatically update the build and revision,
// and "{Major}.{Minor}.{Build}.*" will update just the revision.

[assembly: AssemblyVersion ("1.0.*")]

// The following attributes are used to specify the signing key for the assembly,
// if desired. See the Mono documentation for more information about signing.

//[assembly: AssemblyDelaySign(false)]
//[assembly: AssemblyKeyFile("")]

