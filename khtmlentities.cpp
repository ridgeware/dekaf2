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

#include "khtmlentities.h"
#include "kutf8.h"
#include "kstringutils.h"

namespace dekaf2 {

struct entity_t
{
	KStringView sName;
	uint32_t iCodepoint;
};

/*
constexpr KStringView s_NamedEntities_HTML3[] = {
	{ "hello" }
};
*/

constexpr entity_t s_NamedEntitiesHTML4[] = {
    { "lt"   , '<' },
    { "gt"   , '>' },
    { "amp"  , '&' },
    { "quot" , '"' }
};

//-----------------------------------------------------------------------------
inline
void kEntity(uint32_t ch, KString& sOut)
//-----------------------------------------------------------------------------
{
	sOut += "&#x";
	sOut += KString::to_hexstring(ch, true, false);
	sOut += ';';
}

//-----------------------------------------------------------------------------
void kNamedEntity(uint32_t ch, KString& sOut)
//-----------------------------------------------------------------------------
{
	for (size_t i{0}; i < std::extent<decltype(s_NamedEntitiesHTML4)>::value; ++i)
	{
//		if (s_NamedEntitiesHTML4[i] == )
	}
	sOut += '&';
	sOut += KString::to_hexstring(ch, true, false);
	sOut += ';';
}

//-----------------------------------------------------------------------------
void kMandatoryEntity(uint32_t ch, KString& sOut)
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
}

//-----------------------------------------------------------------------------
KString kHTMLEntityEncode(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sRet;
	sRet.reserve(sIn.size()*2);

	Unicode::FromUTF8(sIn, [&sRet](uint32_t ch)
	{
		if (std::iswalnum(ch) || std::iswpunct(ch) || std::iswspace(ch))
		{
			kMandatoryEntity(ch, sRet);
		}
		else
		{
			sRet += "&#x";
			sRet += KString::to_hexstring(ch, true, false);
			sRet += ';';
		}
	});

	return sRet;
}

//-----------------------------------------------------------------------------
KString kHTMLEntityDecode(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sRet;
	sRet.reserve(sIn.size());

	for (KStringView::const_iterator it = sIn.cbegin(), ie = sIn.cend(); it != ie; )
	{
		if (*it != '&')
		{
			sRet += *it++;
		}
		else
		{
			// decode one char
			if (++it != ie)
			{
				if (*it == '#')
				{
					if (++it != ie)
					{
						uint32_t iChar{0};

						if (*it == 'x' || *it == 'X')
						{
							// hex
							for (;++it != ie;)
							{
								auto iCh = kFromHexChar(*it);
								if (iCh > 15)
								{
									break;
								}

								iChar *= 16;
								iChar += iCh;
							}
						}
						else
						{
							// decimal
							for (;++it != ie;)
							{
								if (!std::isdigit(*it))
								{
									break;
								}

								iChar *= 10;
								iChar += *it - '0';
							}
						}

						Unicode::ToUTF8(iChar, sRet);

						if (it != ie && *it  == static_cast<KStringView::value_type>(';'))
						{
							++it;
						}
					}
				}
				else
				{
					sRet += '&';
					sRet += *it++;
				}
			}
			else
			{
				sRet += '&';
			}
		}
	}

	return sRet;
}

} // of namespace dekaf2

