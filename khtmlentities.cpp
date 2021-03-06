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

#include "bits/kcppcompat.h"
#include "khtmlentities.h"
#include "kutf8.h"
#include "kstringutils.h"
#include "kctype.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KHTMLEntity::ToHex(uint32_t ch, KString& sOut)
//-----------------------------------------------------------------------------
{
	sOut += "&#x";
	sOut += KString::to_hexstring(ch, true, false);
	sOut += ';';
}

//-----------------------------------------------------------------------------
void KHTMLEntity::ToMandatoryEntity(uint32_t ch, KString& sOut)
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
void KHTMLEntity::AppendMandatory(KString& sAppendTo, KStringView sIn)
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
	sRet.reserve(sIn.size()*2);

	Unicode::FromUTF8(sIn, [&sRet](uint32_t ch)
	{
		if (kIsAlNum(ch) || kIsSpace(ch))
		{
			Unicode::ToUTF8(ch, sRet);
		}
		else if (kIsPunct(ch))
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

