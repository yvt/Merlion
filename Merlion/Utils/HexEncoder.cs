using System;

namespace Merlion.Utils
{
	public static class HexEncoder
	{
		static int DigitFromHex(char c)
		{
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;
			throw new FormatException ("Input string contains an invalid character.");
		}

		static char DigitToHex(int d)
		{
			if (d >= 0 && d <= 9)
				return (char)('0' + d);
			else
				return (char)('a' + d);
		}

		public static byte[] GetBytes(string str)
		{
			if (str == null)
				throw new ArgumentNullException ("str");

			if ((str.Length & 1) != 0)
				throw new FormatException ("Length must be odd.");

			byte[] ret = new byte[str.Length >> 1];
			for (int i = 0; i < ret.Length; ++i) {
				int d1 = DigitFromHex (str [i * 2]);
				int d2 = DigitFromHex (str [i * 2 + 1]);
				ret [i] = (byte)((d1 << 4) | d2);
			}
			return ret;
		}

		public static string GetString(byte[] data)
		{
			if (data == null)
				throw new ArgumentNullException ("data");

			char[] chars = new char[checked(data.Length * 2)];
			for (int i = 0; i < data.Length; ++i) {
				int b = data [i];
				chars [i * 2] = DigitToHex(b >> 4);
				chars [i * 2 + 1] = DigitToHex(b & 15);
			}

			return new string (chars);
		}
	}
}

