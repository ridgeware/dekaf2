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

KStringViewPair kSplitToPair(
        KStringView svBuffer,
        const char chDelim,
        KStringView svTrim,
        const char  chEscape
        )
{
	KStringViewPair svPair;

	for (int iElement = 1; iElement < 3; ++iElement)
	{
		if (!svBuffer.empty())
		{
			if (!svTrim.empty())
			{
				// Strip prefix space characters.
				auto iFound = svBuffer.find_first_not_of (svTrim);
				if (iFound != KStringView::npos)
				{
					if (iFound > 0)
					{
						svBuffer.remove_prefix (iFound);

						// actually it is a bug in our SSE implementation
						// that find_first_not_of() returns size()+x
						// instead of npos if not found. We have to
						// fix it and then we can remove this check.
						if (svBuffer.empty())
						{
							break;
						}
					}
				}
				else
				{
					break;
				}
			}

			KStringView svElement;

			if (iElement == 1)
			{
				// Look for delimiter character, but only in round 1, respect escapes
				auto iNext = kFindUnescaped (svBuffer, chDelim, chEscape);

				if (iNext != KStringView::npos)
				{
					svElement = svBuffer.substr(0, iNext);

					++iNext;

					svBuffer.remove_prefix(iNext);
				}
				else
				{
					svElement = svBuffer;

					svBuffer.clear();
				}
			}
			else
			{
				svElement = svBuffer;

				svBuffer.clear();
			}

			if (!svTrim.empty())
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

			// What remains is ready for the next parse round.
			if (iElement == 1)
			{
				svPair.first = svElement;
			}
			else
			{
				svPair.second = svElement;
			}
		}
	}

	return svPair;

}  // kSplitToPair

template
size_t kSplit (
        std::vector<KStringView>&  ctContainer,
        KStringView svBuffer,
        KStringView svDelim  = ",",             // default: comma delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0',            // default: ignore escapes
        bool        bCombineDelimiters = false, // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false  // default: treat double quotes like any other char
    );

template
size_t kSplitPairs(
        KProps<KStringView, KStringView>&  ctContainer,
        KStringView svBuffer,
        const char  chPairDelim = '=',
        KStringView svDelim  = ",",             // default: comma delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0',            // default: ignore escapes
        bool        bCombineDelimiters = false, // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false  // default: treat double quotes like any other char
    );

template
size_t kSplitPairs(
        KProps<KString, KString>&  ctContainer,
        KStringView svBuffer,
        const char  chPairDelim = '=',
        KStringView svDelim  = ",",             // default: comma delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0',            // default: ignore escapes
        bool        bCombineDelimiters = false, // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false  // default: treat double quotes like any other char
    );


} // end of namespace dekaf2
