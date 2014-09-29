using System;
using System.Collections.Generic;

namespace Merlion.Utils
{
	public class ByteArrayComparer: IEqualityComparer<byte[]>
	{
		public readonly static ByteArrayComparer Instance = new ByteArrayComparer();

		public bool Equals (byte[] x, byte[] y)
		{
			if ((x == null) != (y == null))
				return false;
			if (x == null)
				return true;
			if (x.Length != y.Length)
				return false;
			for (int i = 0; i < x.Length; ++i)
				if (x [i] != y [i])
					return false;
			return true;
		}

		public int GetHashCode (byte[] obj)
		{
			unchecked {
				int hash = 7;
				foreach (var b in obj) {
					hash = (int)b + hash * 21;
				}
				return hash;
			}
		}
	}
}

