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

#include "khtmlentities.h"
#include "kutf8.h"
#include "kstringutils.h"
#include "kctype.h"
#include "kwrite.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KHTMLEntity::ToHex(uint32_t ch, KStringRef& sOut)
//-----------------------------------------------------------------------------
{
	sOut += "&#x";
	sOut += kUnsignedToString<KStringRef>(ch, 16, true, false);
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
void KHTMLEntity::EncodeMandatory(std::ostream& Out, KStringView sIn)
//-----------------------------------------------------------------------------
{
	for (auto ch : sIn)
	{
		switch (ch)
		{
			case '"':
				kWrite(Out, "&quot;");
				break;
			case '&':
				kWrite(Out, "&amp;");
				break;
			case '\'':
				kWrite(Out, "&apos;");
				break;
			case '<':
				kWrite(Out, "&lt;");
				break;
			case '>':
				kWrite(Out, "&gt;");
				break;
			default:
				kWrite(Out, ch);
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

DEKAF2_NAMESPACE_END
