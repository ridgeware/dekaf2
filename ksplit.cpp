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

namespace {

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool StripPrefix(KStringView& svBuffer, const KFindSetOfChars& Trim)
//-----------------------------------------------------------------------------
{
	// Strip prefix space characters.
	auto iFound = Trim.find_first_not_in(svBuffer);

	if (iFound == KStringView::npos)
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
void StripSuffix(KStringView& svElement, const KFindSetOfChars& Trim)
//-----------------------------------------------------------------------------
{
	//  Strip suffix space characters.
	auto iFound = Trim.find_last_not_in(svElement);

	if (iFound != KStringView::npos)
	{
		auto iRemove = svElement.size() - 1 - iFound;
		svElement.remove_suffix(iRemove);
	}
	else
	{
		svElement.clear();
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

}  // kSplitToPair

} // of anonymous namespace

//-----------------------------------------------------------------------------
KStringViewPair kSplitToPair(
        KStringView svBuffer,
        KStringView svPairDelim,
		const KFindSetOfChars& Trim,
        const char  chEscape
	)
//-----------------------------------------------------------------------------
{
	if (Trim.empty() && !chEscape)
	{
		return kSplitToPairInt(svBuffer, svPairDelim);
	}

	KStringViewPair svPair;

	if (DEKAF2_LIKELY(StripPrefix(svBuffer, Trim)))
	{
		// Look for delimiter character, respect escapes
		auto iNext = kFindUnescaped (svBuffer, svPairDelim, chEscape);

		if (DEKAF2_LIKELY(iNext != KStringView::npos))
		{
			svPair.first = svBuffer.substr(0, iNext);

			svBuffer.remove_prefix(iNext + svPairDelim.size());

			StripSuffix(svPair.first, Trim);

			if (DEKAF2_LIKELY(!svBuffer.empty()))
			{
				if (!StripPrefix(svBuffer, Trim))
				{
					// empty input
					return svPair;
				}

				svPair.second = svBuffer;

				StripSuffix(svPair.second, Trim);
			}
		}
		else
		{
			svPair.first = svBuffer;

			StripSuffix(svPair.first, Trim);

			// there is no second element
		}
	}

	return svPair;

}  // kSplitToPair

#if !defined(_MSC_VER) && (!defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION_MAJOR > 5)
// precompile for std::vector<KStringView>
template
std::size_t kSplit(
		std::vector<KStringView>& cContainer,
        KStringView svBuffer,
        const KFindSetOfChars& Delim   = ",",                  // default: comma delimiter
        const KFindSetOfChars& Trim    = detail::kASCIISpaces, // default: trim all whitespace
        const char  chEscape = '\0',                 // default: ignore escapes
        bool        bCombineDelimiters = false,      // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false       // default: treat double quotes like any other char
);
#endif // of _MSC_VER

} // end of namespace dekaf2
