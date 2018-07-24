/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include "kquotedprintable.h"
#include "kstringutils.h"
#include "klog.h"

namespace dekaf2 {

constexpr char sxDigit[] = "0123456789ABCDEF";

//-----------------------------------------------------------------------------
KString KQuotedPrintable::Encode(KStringView sInput, bool bDotStuffing)
//-----------------------------------------------------------------------------
{
	KString out;
	out.reserve(sInput.size());
	uint16_t iLineLen { 0 };

	for (auto byte : sInput)
	{
		if (iLineLen >= 75)
		{
			out += '=';
			out += '\n';
			iLineLen = 0;
		}

		if (byte == '\n' || byte == '\r')
		{
			out += byte;
			iLineLen = 0;
		}
		else if ((byte <= 126) && (byte >= 32) && (byte != '=') && !(byte == '.' && iLineLen == 0))
		{
			out += byte;
			++iLineLen;
		}
		else
		{
			out += '=';
			out += sxDigit[(byte >> 4) & 0x0f];
			out += sxDigit[(byte     ) & 0x0f];
			iLineLen += 3;
		}
	}

	return out;

} // Encode

//-----------------------------------------------------------------------------
	void FlushRaw(KString& out, uint16_t iDecode, KString::value_type LeadChar, KString::value_type ch = 'f')
//-----------------------------------------------------------------------------
{
	out += '=';

	if (iDecode == 1)
	{
		out += LeadChar;
	}

	// 'f' signals that the input starved, as 'f' is a valid xdigit
	if (ch != 'f')
	{
		out += ch;
	}

} // FlushRaw

//-----------------------------------------------------------------------------
KString KQuotedPrintable::Decode(KStringView sInput, bool bDotStuffing)
//-----------------------------------------------------------------------------
{
	KString out;
	out.reserve(sInput.size());
	KString::value_type LeadChar { 0 };
	uint16_t iDecode { 0 };
	bool bStartOfLine { true };

	for (auto ch : sInput)
	{
		if (iDecode)
		{
			if (iDecode == 2 && (ch == '\r' || ch == '\n'))
			{
				bStartOfLine = true;
				if (ch == '\n')
				{
					iDecode = 0;
				}
			}
			else if (!std::isxdigit(ch))
			{
				if (ch == '\r' || ch == '\n')
				{
					bStartOfLine = true;
				}
				else
				{
					kDebug(2, "illegal encoding, flushing raw");
					FlushRaw(out, iDecode, LeadChar, ch);
				}
				iDecode = 0;
			}
			else if (--iDecode == 1)
			{
				LeadChar = ch;
			}
			else
			{
				uint16_t iValue = kFromHexChar(LeadChar) << 4;
				iValue += kFromHexChar(ch);
				out += iValue;
			}
		}
		else
		{
			switch (ch)
			{
				case '\r':
				case '\n':
					bStartOfLine = true;
					out += ch;
					break;

				case '=':
					bStartOfLine = false;
					iDecode = 2;
					break;

				default:
					if (bStartOfLine)
					{
						bStartOfLine = false;
						if (bDotStuffing && ch == '.')
						{
							break;
						}
					}
					out += ch;
					break;
			}
		}
	}

	if (iDecode)
	{
		kDebug(2, "QuotedPrintable decoding ended prematurely, flushing raw");
		FlushRaw(out, iDecode, LeadChar);
	}

	return out;

} // Decode

} // end of namespace dekaf2

