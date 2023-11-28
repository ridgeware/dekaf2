/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

// This file implements all of khtmlentities.h that does not need the large
// named entity list. The other functions are implemented in khtmlentities5.cpp.

#include "kcompatibility.h"
#include "khtmlentities.h"
#include "kutf8.h"
#include "kstringutils.h"
#include "kctype.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KHTMLEntity::ToHex(uint32_t ch, KStringRef& sOut)
//-----------------------------------------------------------------------------
{
	sOut += "&#x";
	sOut += KString::to_hexstring(ch, true, false);
	sOut += ';';
}

//-----------------------------------------------------------------------------
void KHTMLEntity::ToMandatoryEntity(uint32_t ch, KStringRef& sOut)
//-----------------------------------------------------------------------------
{
	switch (ch)
	{
		case '"':
			sOut += "&quot;";
			return;
		case '&':
			sOut += "&amp;";
			return;
		case '\'':
			sOut += "&apos;";
			return;
		case '<':
			sOut += "&lt;";
			return;
		case '>':
			sOut += "&gt;";
			return;
		default:
			Unicode::ToUTF8(ch, sOut);
			return;
	}

} // kMandatoryEntity

//-----------------------------------------------------------------------------
void KHTMLEntity::AppendMandatory(KStringRef& sAppendTo, KStringView sIn)
//-----------------------------------------------------------------------------
{
	sAppendTo.reserve(sAppendTo.size() + (sIn.size() * 5 / 4));

	for (auto ch : sIn)
	{
		switch (ch)
		{
			case '"':
				sAppendTo += "&quot;";
				break;
			case '&':
				sAppendTo += "&amp;";
				break;
			case '\'':
				sAppendTo += "&apos;";
				break;
			case '<':
				sAppendTo += "&lt;";
				break;
			case '>':
				sAppendTo += "&gt;";
				break;
			default:
				sAppendTo += ch;
				break;
		}
	}

} // AppendMandatory

//-----------------------------------------------------------------------------
void KHTMLEntity::EncodeMandatory(KOutStream& Out, KStringView sIn)
//-----------------------------------------------------------------------------
{
	for (auto ch : sIn)
	{
		switch (ch)
		{
			case '"':
				Out += "&quot;";
				break;
			case '&':
				Out += "&amp;";
				break;
			case '\'':
				Out += "&apos;";
				break;
			case '<':
				Out += "&lt;";
				break;
			case '>':
				Out += "&gt;";
				break;
			default:
				Out += ch;
				break;
		}
	}

} // EncodeMandatory

//-----------------------------------------------------------------------------
KString KHTMLEntity::EncodeMandatory(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sOut;
	AppendMandatory(sOut, sIn);
	return sOut;

} // EncodeMandatory

//-----------------------------------------------------------------------------
KString KHTMLEntity::Encode(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sRet;

	Unicode::FromUTF8(sIn, [&sRet](uint32_t ch)
	{
		auto Property = KCodePoint(ch).GetProperty();

		if (Property.IsAlNum() || Property.IsSpace())
		{
			Unicode::ToUTF8(ch, sRet);
		}
		else if (Property.IsPunct())
		{
			ToMandatoryEntity(ch, sRet);
		}
		else
		{
			ToHex(ch, sRet);
		}

		return true;
	});

	return sRet;

} // kHTMLEntityEncode


} // of namespace dekaf2

