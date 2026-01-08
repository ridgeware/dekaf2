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

#include "kurlencode.h"
#include "kstringutils.h"
#include "kctype.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {

/*
	Schema         = 0,
	User           = 1,
	Password       = 2,
	Domain         = 3,
	Port           = 4,
	Path           = 5,
	Query          = 6,
	Fragment       = 7
*/

KUrlEncodingTables KUrlEncodingTables::MyInstance {};

const char* KUrlEncodingTables::s_sExcludes[] =
{
//  "",      // used by Schema .. Port                https://tools.ietf.org/html/rfc3986#section-3
    ":;,=/", // used by Path (actually there is more) https://tools.ietf.org/html/rfc3986#section-3.3
    "/?"     // used by Query and Fragment            https://tools.ietf.org/html/rfc3986#section-3.4
};

bool* KUrlEncodingTables::EncodingTable[TABLECOUNT];
bool KUrlEncodingTables::Tables[INT_TABLECOUNT][256];

//-----------------------------------------------------------------------------
KUrlEncodingTables::KUrlEncodingTables() noexcept
//-----------------------------------------------------------------------------
{
	// set up the encoding tables
	for (auto table = 0; table < INT_TABLECOUNT; ++table)
	{
		std::memset(&Tables[table], false, 256);

		constexpr KStringView UnreservedValues("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
											   "abcdefghijklmnopqrstuvwxyz"
											   "0123456789"
											   "-_.~");

		for (auto value : UnreservedValues)
		{
			Tables[table][static_cast<unsigned char>(value)] = true;
		}
	}

	for (auto table = 1; table < INT_TABLECOUNT; ++table)
	{
		auto p = s_sExcludes[table-1];
		while (auto ch = *p++)
		{
			Tables[table][static_cast<unsigned char>(ch)] = true;
		}
	}

	// now set up the pointers into these tables, as some are shared..
	for (auto table = static_cast<int>(URIPart::Protocol); table < static_cast<int>(URIPart::Path); ++table)
	{
		EncodingTable[table] = Tables[0];
	}

	EncodingTable[static_cast<int>(URIPart::Path)]     = Tables[1];
	EncodingTable[static_cast<int>(URIPart::Query)]    = Tables[2];
	EncodingTable[static_cast<int>(URIPart::Fragment)] = Tables[2];
}

} // end of namespace detail

namespace detail {

//-----------------------------------------------------------------------------
int kx2c (int c1, int c2)
//-----------------------------------------------------------------------------
{
	int iValue = kFromHexChar(c1) << 4;
	iValue    += kFromHexChar(c2);

	return iValue;

} // kx2c

} // detail until here

#ifndef DEKAF2_IS_MSC
template void kUrlDecode(KStringRef& sDecode, bool pPlusAsSpace = false);
template void kUrlDecode(const KStringView& sSource, KStringRef& sTarget, bool bPlusAsSpace = false);
template KString kUrlDecode(const KStringView& sSource, bool bPlusAsSpace = false);
template void kUrlEncode (const KStringView& sSource, KStringRef& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false);

template class KURLEncoded<uint16_t>;
template class KURLEncoded<KString>;
template class KURLEncoded<KProps<KString, KString>, '&', '='>;
#endif // of DEKAF2_IS_MSC

DEKAF2_NAMESPACE_END
