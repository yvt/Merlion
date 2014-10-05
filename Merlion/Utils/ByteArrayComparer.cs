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

