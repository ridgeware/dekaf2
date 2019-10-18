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
#include <cctype>

namespace dekaf2 {

constexpr char sxDigit[] = "0123456789ABCDEF";

enum ETYPE : uint8_t
{
	NO, // no, do not encode
	YY, // yes, encode
	LF, // linefeed
	SL, // encode if at start of line
	MH  // encode if used in mail headers
};

constexpr ETYPE sEncodeCodepoints[256] =
{
// 0x0 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xA 0xB 0xC 0xD 0xE 0xF
	YY, YY, YY, YY, YY, YY, YY, YY, YY, MH, LF, YY, YY, LF, YY, YY, // 0x00
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0x10
	MH, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, SL, NO, // 0x20
	NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, YY, NO, MH, // 0x30
	NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, // 0x40
	NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, MH, // 0x50
	NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, // 0x60
	NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, NO, YY, YY, // 0x70
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0x80
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0x90
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xA0
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xB0
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xC0
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xD0
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xE0
	YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, // 0xF0
};

//-----------------------------------------------------------------------------
KString KQuotedPrintable::Encode(KStringView sInput, bool bForMailHeaders)
//-----------------------------------------------------------------------------
{
	KString out;
	out.reserve(sInput.size());
	uint16_t iLineLen { 0 };
	uint16_t iMaxLineLen { 75 };

	if (bForMailHeaders)
	{
		out += "=?UTF-8?Q?";
		// we already add the 2 chars we need to close the encoding, and reserve 15
		// chars for the 'Header: '
		iMaxLineLen = 75 - (10 + 2 + 15);
	}

	for (auto byte : sInput)
	{
		if (iLineLen >= iMaxLineLen)
		{
			if (bForMailHeaders)
			{
				out += "?=\r\n ";
				out += "=?UTF-8?Q?";
				iMaxLineLen = 75 - (1 + 10 + 2);
			}
			else
			{
				out += "=\r\n";
				iMaxLineLen = 75 - 1;
			}
			iLineLen = 0;
		}

		switch (sEncodeCodepoints[static_cast<uint8_t>(byte)])
		{
			case NO:
				// do not encode
				out += byte;
				++iLineLen;
				continue;

			case SL:
				// encode if at start of line
				if (iLineLen > 0)
				{
					out += byte;
					++iLineLen;
					continue;
				}
				// yes, encode
				break;

			case LF:
				// copy, reset line counter
				out += byte;
				iLineLen = 0;
				continue;

			case MH:
				// encode if in mail headers
				if (!bForMailHeaders)
				{
					out += byte;
					++iLineLen;
					continue;
				}
				// yes, encode
				break;

			case YY:
				// yes, encode
				break;
		}

		out += '=';
		out += sxDigit[(byte >> 4) & 0x0f];
		out += sxDigit[(byte     ) & 0x0f];
		iLineLen += 3;
	}

	if (bForMailHeaders)
	{
		out += "?=";
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
			else if (!KASCII::kIsXDigit(ch))
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
				out += static_cast<KString::value_type>(iValue);
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

