/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

#include "kencode.h"
#include "kstringutils.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KString KEncode::Hex(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sRet;

	sRet.reserve(sIn.size() * 2);

	for (auto ch : sIn)
	{
		HexAppend(sRet, ch);
	}

	return sRet;

} // KEnc::Hex

//-----------------------------------------------------------------------------
KString KDecode::Hex(KStringView sIn)
//-----------------------------------------------------------------------------
{
	KString sRet;

	if ((sIn.size() & 1) == 1)
	{
		kWarning("uneven count of input characters");
		return sRet;
	}

	auto iTotal = sIn.size();
	sRet.reserve(iTotal / 2);

	for (size_t iCt = 0; iCt < iTotal;)
	{
		auto iVal1 = kFromBase36(sIn[iCt++]);

		if (DEKAF2_UNLIKELY(iVal1 > 15))
		{
			kWarning("invalid hex char");
			break;
		}

		auto iVal2 = kFromBase36(sIn[iCt++]);

		if (DEKAF2_UNLIKELY(iVal2 > 15))
		{
			kWarning("invalid hex char");
			break;
		}

		sRet += iVal1 * 16 + iVal2;
	}

	return sRet;

} // KDec::Hex

} // of namespace dekaf2

