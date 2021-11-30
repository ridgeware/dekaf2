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

#include "ksplit.h"

namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool StripPrefix(KStringView& svBuffer, const KStringView& svTrim)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(svBuffer.empty()))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(svTrim.empty()))
	{
		return true;
	}

	// Strip prefix space characters.
	auto iFound = svBuffer.find_first_not_of (svTrim);

	if (DEKAF2_UNLIKELY(iFound == KStringView::npos))
	{
		return false;
	}

	if (iFound > 0)
	{
		svBuffer.remove_prefix (iFound);
	}

	return true;

} // StripPrefix

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
void StripSuffix(KStringView& svElement, const KStringView& svTrim)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(!svTrim.empty()))
	{
		//  Strip suffix space characters.
		auto iFound = svElement.find_last_not_of (svTrim);
		if (iFound != KStringView::npos)
		{
			auto iRemove = svElement.size() - 1 - iFound;
			svElement.remove_suffix(iRemove);
		}
		else
		{
			svElement.clear();
		}
	}

} // StripSuffix

//-----------------------------------------------------------------------------
KStringViewPair kSplitToPairInt(
        KStringView svBuffer,
        KStringView svPairDelim
	)
//-----------------------------------------------------------------------------
{
	KStringViewPair svPair;

	if (DEKAF2_LIKELY(!svBuffer.empty()))
	{
		// Look for delimiter character
		auto iNext = svBuffer.find(svPairDelim);

		if (DEKAF2_LIKELY(iNext != KStringView::npos))
		{
			svPair.first = svBuffer.substr(0, iNext);

			svBuffer.remove_prefix(iNext + svPairDelim.size());

			svPair.second = svBuffer;
		}
		else
		{
			svPair.first = svBuffer;

			// there is no second element
		}
	}

	return svPair;

}  // detail::kSplitToPairInt

//-----------------------------------------------------------------------------
KStringViewPair kSplitToPairInt(
        KStringView svBuffer,
        KStringView svPairDelim,
        KStringView svTrim,
        const char  chEscape
	)
//-----------------------------------------------------------------------------
{
	KStringViewPair svPair;

	if (DEKAF2_LIKELY(StripPrefix(svBuffer, svTrim)))
	{
		// Look for delimiter character, respect escapes
		auto iNext = kFindUnescaped (svBuffer, svPairDelim, chEscape);

		if (DEKAF2_LIKELY(iNext != KStringView::npos))
		{
			svPair.first = svBuffer.substr(0, iNext);

			svBuffer.remove_prefix(iNext + svPairDelim.size());

			StripSuffix(svPair.first, svTrim);

			if (DEKAF2_LIKELY(!svBuffer.empty()))
			{
				if (!StripPrefix(svBuffer, svTrim))
				{
					// empty input
					return svPair;
				}

				svPair.second = svBuffer;

				StripSuffix(svPair.second, svTrim);
			}
		}
		else
		{
			svPair.first = svBuffer;

			StripSuffix(svPair.first, svTrim);

			// there is no second element
		}
	}

	return svPair;

}  // kSplitToPairInt

} // of namespace detail

#if !defined(_MSC_VER) && (!defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION_MAJOR > 5)
// precompile for std::vector<KStringView>
template
std::size_t kSplit(
		std::vector<KStringView>& cContainer,
        KStringView svBuffer,
        KStringView svDelim  = ",",                  // default: comma delimiter
        KStringView svTrim   = detail::kASCIISpaces, // default: trim all whitespace
        const char  chEscape = '\0',                 // default: ignore escapes
        bool        bCombineDelimiters = false,      // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false       // default: treat double quotes like any other char
);
#endif // of _MSC_VER

} // end of namespace dekaf2
