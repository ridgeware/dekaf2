//*////////////////////////////////////////////////////////////////////////////
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
//*////////////////////////////////////////////////////////////////////////////


#pragma once

#include <vector>

#include "kstring.h"
#include "kstringutils.h"

namespace dekaf2
{

/// Find delimiter char prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter chars prefixed by odd number of escapes.
template<typename Tsearch>
size_t kFindUnescaped(KStringView svBuffer, Tsearch tSearch, char iEscape)
//-----------------------------------------------------------------------------
{
	size_t iFound = svBuffer.find_first_of (tSearch);

	if (!iEscape || iFound == 0)
	{
		// If no escape char is given or
		// the searched character was first on the line...
		return iFound;
	}

	while (iFound != KStringView::npos)
	{
		size_t iEscapes = 0;
		size_t iStart = iFound;

		while (iStart)
		{
			// count number of escape characters
			--iStart;
			if (svBuffer[iStart] != iEscape)
			{
				break;
			}
			++iEscapes;
		} // while iStart

		if (!(iEscapes & 1))  // if even number of escapes
		{
			break;
		}

		iFound = svBuffer.find_first_of (tSearch, iFound + 1);
	} // while iFound
	return iFound;
} // kFindUnescaped


//-----------------------------------------------------------------------------
/// kSplit converts string into token container using delimiters and escape
/// kSplit (Container, Buffer, Delimiters, Trim, Escape)
/// Container is target iterable container like deque | vector.
/// Buffer is a source char sequence.
/// Delimiters is a string of delimiter characters.
/// Trim is a string containing chars to remove from token ends.
/// Escape (default '\0'). If '\\' parse ignores escaped delimiters.
template<typename Tcnt>
inline size_t kSplit (
		Tcnt& ctContainer,
		KStringView  svBuffer,
		KStringView  sDelim = ",",          // default: comma delimiter
		KStringView sTrim   = " \t\r\n\b",  // default: trim all whitespace
		char        iEscape = '\0'          // default: ignore escapes
)
//-----------------------------------------------------------------------------
{
	// consider the string " a , b , c , d , e "
	// where                    ^              ^   is the operational pair
	if (svBuffer.size () != 0)
	{
		if (sTrim.size())
		{
			// Strip suffix space characters.
			size_t iLeading  = svBuffer.find_first_not_of (sTrim);
			size_t iTrailing = svBuffer.find_last_not_of  (sTrim);
			if (iTrailing != KStringView::npos)
			{
				svBuffer.remove_suffix (svBuffer.size () - iTrailing);
			}
			if (iLeading != KStringView::npos)
			{
				svBuffer.remove_prefix (iLeading);
			}
		}

		while (svBuffer.size())
		{
			// svBuffer " a , b , c , d , e "
			// head/tail     ^              ^
			if (sTrim.size())
			{
				// Strip prefix space characters.
				size_t iFound = svBuffer.find_first_not_of (sTrim);
				svBuffer.remove_prefix (iFound);
			} // if (sTrim.size())

			// svBuffer " a , b , c , d , e "
			// head/tail      ^            ^
			size_t iChars = svBuffer.size ();
			if (!iChars)
			{
				// Stop if input buffer is empty.
				break;
			} // if (!iChars)

			// svBuffer " a , b , c , d , e "
			// head/tail      ^            ^
			// Whatever is at index 0 is to be stored in the member.
			ctContainer.push_back (svBuffer);
			KStringView& last = ctContainer.back();

			// Look for delimiter character.
			// NOTE no attempt in old code to handle escape characters.
			size_t iNext;

			// svBuffer " a , b , c , d , e "
			// head/tail      ^            ^
			// back           ^            ^
			if (iEscape == '\0')
			{
				// If no escape character is specified, do not look for escapes
				iNext = svBuffer.find_first_of(sDelim);
			}
			else
			{
				// Find earliest instance of odd-count escape characters.
				iNext = KStringView::npos;
				for (size_t ii = 0; ii < sDelim.size(); ++ii)
				{
					size_t iTemp = kFindUnescaped (svBuffer, sDelim[ii], iEscape);
					iNext = (iTemp < iNext) ? iTemp : iNext;
				}
			} // if (iEscape == '\0')

			// A delimiter or end-of-string was found.
			// Terminate the last stored member entry
			if (iNext != KStringView::npos)
			{
				last.remove_suffix (svBuffer.size () - iNext);

				// Carve off what was stored, and its delimiter.
				svBuffer.remove_prefix (iNext + 1);
			}
			else
			{
				svBuffer.remove_prefix (svBuffer.size());
			} // if (iNext != npos)

			// svBuffer " a , b , c , d , e "
			// head/tail         ^         ^
			// back           ^ ^
			if (sTrim.size())
			{
				//  Strip suffix space characters.
				size_t iFound = last.find_last_not_of (sTrim);
				if (iFound != KStringView::npos)
				{
					size_t iRemove = last.size() - iFound;
					last.remove_suffix(iRemove);
				}
			}
			// svBuffer " a , b , c , d , e "
			// head/tail         ^         ^
			// back           ^^

			// What remains is ready for the next parse round.
		} // while (svBuffer.size())
	}
	return ctContainer.size ();
} // kSplit with string of delimiters

} // namedpace dekaf2
