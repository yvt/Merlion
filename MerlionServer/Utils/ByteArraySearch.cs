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

namespace Merlion.Server.Utils
{
	sealed class ByteArraySearch
	{
		readonly byte[] pattern;
		readonly int[] charTable;
		readonly int[] offsetTable;

		public ByteArraySearch (byte[] pattern)
		{
			if (pattern == null)
				throw new ArgumentNullException ("pattern");

			this.pattern = pattern;

			offsetTable = new int[pattern.Length];

			charTable = MakeCharTable ();
			offsetTable = MakeOffsetTable ();
		}

		int[] MakeCharTable()
		{
			var table = new int[256];
			var pat = pattern;
			for (int i = 0; i < table.Length; ++i)
				table [i] = pat.Length;
			for (int i = 0, c = pat.Length - 1; i < c; ++i)
				table [pat [i]] = c - i;
			return table;
		}

		static bool IsPrefix(byte[] pat, int p)
		{
			for (int i = p, j = 0; i < pat.Length; ++i, ++j)
				if (pat [i] != pat [j])
					return false;
			return true;
		}

		static int SuffixLength(byte[] pat, int p)
		{
			int len = 0;
			for (int i = p, j = pat.Length - 1; i >= 0 && pat[i] == pat[j];
				--i, --j) ++len;
			return len;
		}

		int[] MakeOffsetTable()
		{
			var pat = pattern;
			var table = new int[pat.Length];
			int lp = pat.Length;

			for (int i = pat.Length - 1; i >= 0; --i) {
				if (IsPrefix(pat, i + 1)) {
					lp = i + 1;
				}
				table [pat.Length - 1 - i] = lp - i + pat.Length - 1;
			}
			for (int i = 0, c = pat.Length - 1; i < c; ++i) {
				int s = SuffixLength (pat, i);
				table [s] = pat.Length - 1 - i + s;
			}

			return table;
		}

		public int IndexOf(byte[] subject, int start, int length)
		{
			if (subject == null)
				throw new ArgumentNullException ("subject");
			if (start < 0) {
				length += start;
				start = 0;
			}
			if (length + start > subject.Length) {
				length = subject.Length - start;
			}
			if (length <= 0) {
				return -1;
			}

			var pat = pattern;
			int end = start + length;
			for (int i = start + pat.Length - 1; i < end;) {
				int j;
				for (j = pat.Length - 1; pat [j] == subject [i]; --i, --j)
					if (j == 0)
						return i;
				i += Math.Max (offsetTable[pat.Length - 1 - j],
					charTable[subject[i]]);
			}

			return -1;
		}

		public int IndexOf(byte[] subject)
		{
			return IndexOf (subject, 0, subject.Length);
		}
	}
}

