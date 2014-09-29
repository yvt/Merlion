using System;
using System.Xml.Serialization;

namespace Merlion.Server
{
	[XmlType]
	[XmlRoot("MerlionApplication")]
	public class ApplicationConfig
	{
		[XmlAttribute("TypeName")]
		public string ApplicationTypeName { get; set; }
	}


}

