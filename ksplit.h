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

// template<template approach to containers was discovered from here:
// https://stackoverflow.com/questions/16596422/
//  template-class-with-template-container
//## please remove the class definition. Create a function template instead
//## BTW, I do not see why you need the complex template template construct,
//## a simple template<typename Container> kSpit(Container& container, ..)
//## should suffice here. And when you change this to a function template
//## it will actually automatically deduce the type, so that you can
//## simply use it like vector<string> vec; kSplit(vec, ...).
template<template <typename... Args> class Container,typename... Types>
class KSplit
{

//------
public:
//------

	//## please make the description consistent with the current implementation,
	//## and consider making it shorter. As you have many sentences, use
	//## capitalization as usual in English, and put punctuation. All this
	//## text gets collapsed into one line in an IDE..
	//---------------------------------------------------------------------
	/// parse (container, buffer, maximum, delimiters, trim, escape) where
	/// container is typical of std iterable containers like deque | vector
	/// buffer is typical of std strings | string_views
	/// maximum is the largest permitted tokens to parse
	/// delimiters is a string any of which characters separate tokens
	///  (possible delimiters may be ",|;:" but may be any viable one)
	/// trim is a boolean which, when true, make " a " become "a"
	/// escape defaults to '\0', but if it is '\\' is used to parse past
	///  escaped characters.
	//## simply name this function kSplit
	static size_t parse (
			Container<Types...>& ctContainer,       // target
			KStringView     svBuffer,               // source
			KStringView     sDelim  = ",",          // variety of delimiters
			KStringView     sTrim   = " \t\r\n\b",  // trim token whitespace
			char            iEscape = '\\'          // recognize escapes
	);


//------
private:
//------

	//---------------------------------------------------------------------
	/// tcschrEsc implements industry standard _tcschr with escape detection.
	//## make this a standalone function in namespace dekaf2, and name it
	//## less Windowsy.. I mean, we are not using LPCTSTR, why should we
	//## use its function names? Why not simply kFindEscaped() ?
	//## And consider a second function which takes a KStringView as "target"
	//## (I would actually rename that parameter to chSearched to be more
	//## explicit..). BTW, this function does not need to be a template.
	static size_t tcschrEsc (
			KStringView svBuffer,
			char        iTarget,
			char        iEscape);

};

//-----------------------------------------------------------------------------
/// Find delimiter char prefixed by even number of escape characters (0, 2, ...)
template<template <typename... Args> class Container,typename... Types>
size_t KSplit<Container, Types...>::tcschrEsc (
		KStringView svBuffer, char iTarget, char iEscape)
//-----------------------------------------------------------------------------
{
	size_t iFound = svBuffer.find_first_of (iTarget);

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

		iFound = svBuffer.find_first_of (iTarget, iFound + 1);
	} // while iFound
	return iFound;
} // tcschrEsc

//-----------------------------------------------------------------------------
/// parse Delimited List with string of delimiters
template<template <typename... Args> class Container,typename... Types>
size_t KSplit<Container, Types...>::parse (
		Container<Types...>& ctContainer,
		KStringView     svBuffer,
		KStringView     sDelim,  // default to comma delimiter
		KStringView     sTrim,   // default to trimming whitespace
		char            iEscape  // default to recognizing escapes
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
					size_t iTemp = tcschrEsc (svBuffer, sDelim[ii], iEscape);
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
} // parse with string of delimiters

} // namedpace dekaf2
