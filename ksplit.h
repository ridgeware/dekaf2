/*
//////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
//////////////////////////////////////////////////////////////////////////////
*/

#pragma once

#include "kstringview.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Find delimiter char prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter chars prefixed by odd number of escapes.
size_t kFindFirstOfUnescaped(KStringView svBuffer, KStringView svDelimiter, char iEscape);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// kSplit converts string into token container using delimiters and escape.
/// Container needs to have a push_back() that can take a KStringView.
/// Buffer is a source char sequence.
/// Delimiters is a string of delimiter characters.
/// Trim is a string containing chars to remove from token ends.
/// Escape (default '\0' = inactive) escapes delimiters.
template<typename Container>
size_t kSplit (
		Container&  ctContainer,
		KStringView svBuffer,
		KStringView sDelim  = ",",          // default: comma delimiter
		KStringView sTrim   = " \t\r\n\b",  // default: trim all whitespace
		char        iEscape = '\0'          // default: ignore escapes
)
//-----------------------------------------------------------------------------
{
	// consider the string " a , b , c , d , e "

	while (!svBuffer.empty())
	{
		if (!sTrim.empty())
		{
			// Strip prefix space characters.
			auto iFound = svBuffer.find_first_not_of (sTrim);
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
						// Stop if input buffer is empty.
						ctContainer.push_back(KStringView());
						break;
					}
				}
			}
			else
			{
				// input was all trimmable chars
				ctContainer.push_back(KStringView());
				break;
			}
		}

		// Look for delimiter character.
		size_t iNext;

		if (iEscape == '\0')
		{
			// If no escape character is specified, do not look for escapes
			iNext = svBuffer.find_first_of(sDelim);
		}
		else
		{
			// Find earliest instance of odd-count escape characters.
			iNext = kFindFirstOfUnescaped (svBuffer, sDelim, iEscape);
		}

		KStringView element;

		if (iNext != KStringView::npos)
		{
			element = svBuffer.substr(0, iNext);
			svBuffer.remove_prefix(iNext + 1);
		}
		else
		{
			element = svBuffer;
			svBuffer.clear();
		}

		if (!sTrim.empty())
		{
			//  Strip suffix space characters.
			auto iFound = element.find_last_not_of (sTrim);
			if (iFound != KStringView::npos)
			{
				auto iRemove = element.size() - 1 - iFound;
				element.remove_suffix(iRemove);
			}
			else
			{
				element.clear();
			}
		}

		ctContainer.push_back(element);

		// What remains is ready for the next parse round.
	}

	return ctContainer.size ();

} // kSplit with string of delimiters

} // namespace dekaf2
