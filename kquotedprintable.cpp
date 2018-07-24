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
KString KQuotedPrintable::Encode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString out;
	out.reserve(sInput.size());
	uint16_t iLineLen { 0 };
	KStringView::value_type lastCh { 0 };

	for (auto byte : sInput)
	{
		if (iLineLen >= 75)
		{
			// insert '=' if last char was not a space
			if (lastCh != 0x20)
			{
				out += '=';
			}
			out += '\n';
			iLineLen = 0;
		}

		if ((byte <= 126) && (byte >= 32) && (byte != 61))
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
void FlushRaw(KString& out, uint16_t iDecode, uint16_t iValue, KStringView::value_type ch = 'f')
//-----------------------------------------------------------------------------
{
	out += '=';

	if (iDecode == 1)
	{
		out += sxDigit[(iValue >> 4) & 0x0f];
	}

	// 'f' signals that the input starved, as 'f' is a valid xdigit
	if (ch != 'f')
	{
		out += ch;
	}

} // FlushRaw

//-----------------------------------------------------------------------------
KString KQuotedPrintable::Decode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString out;
	out.reserve(sInput.size());
	uint16_t iValue { 0 };
	uint16_t iDecode { 0 };

	for (auto ch : sInput)
	{
		if (iDecode)
		{
			if (!std::isxdigit(ch))
			{
				if (ch != '\r' && ch != '\n')
				{
					kDebug(2, "illegal encoding, flushing raw");
					FlushRaw(out, iDecode, iValue, ch);
				}
				iDecode = 0;
			}
			else if (--iDecode == 1)
			{
				iValue = kFromHexChar(ch) << 4;
			}
			else
			{
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
					iDecode = 0;
					break;

				case '=':
					iDecode = 2;
					break;

				default:
					out += ch;
					break;
			}
		}
	}

	if (iDecode)
	{
		kDebug(2, "QuotedPrintable decoding ended prematurely, flushing raw");
		FlushRaw(out, iDecode, iValue);
	}

	return out;

} // Decode

} // end of namespace dekaf2

