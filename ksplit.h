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

/// @file ksplit.h
/// Highly configurable tokenizer template.

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Splits string into token container using delimiters, trim, and escape.
/// @param ctContainer needs to have a push_back() that can construct an element from
/// a KStringView.
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string containing chars to remove from token ends. Defaults to " \t\r\n\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreFrames if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
template<typename Container>
size_t kSplit (
        Container&  ctContainer,
        KStringView svBuffer,
        KStringView svDelim  = ",",             // default: comma delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0',            // default: ignore escapes
        bool        bCombineDelimiters = false, // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false  // default: treat double quotes like any other char
)
//-----------------------------------------------------------------------------
{
	// consider the string " a , b , c , d , e "

	while (!svBuffer.empty())
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

		KStringView element;
		bool have_quotes{false};

		if (bQuotesAreEscapes && svBuffer.front() == '"')
		{
			auto iQuote = kFindUnescaped(svBuffer, '"', chEscape, 1);
			if (iQuote != KStringView::npos)
			{
				// only treat this as a quoted token if we have a closing quote
				element = svBuffer.substr(1, iQuote - 1);
				svBuffer.remove_prefix(iQuote + 1);
				have_quotes = true;
			}
		}

		// Look for delimiter character, respect escapes
		auto iNext = kFindFirstOfUnescaped (svBuffer, svDelim, chEscape);

		if (iNext != KStringView::npos)
		{
			if (!have_quotes)
			{
				element = svBuffer.substr(0, iNext);
			}

			if (svBuffer[iNext] == ' ')
			{
				// if space is a delimiter we always treat consecutive spaces as one delimiter
				iNext = svBuffer.find_first_not_of(' ', iNext + 1);
			}
			else
			{
				++iNext;
			}

			if (bCombineDelimiters && !(svDelim.size() == 1 && svDelim.front() == ' '))
			{
				// skip all adjacent delimiters
				iNext = svBuffer.find_first_not_of(svDelim, iNext);
			}

			svBuffer.remove_prefix(iNext);
		}
		else
		{
			if (!have_quotes)
			{
				element = svBuffer;
			}

			svBuffer.clear();
		}

		if (!svTrim.empty() && !have_quotes)
		{
			//  Strip suffix space characters.
			auto iFound = element.find_last_not_of (svTrim);
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

//-----------------------------------------------------------------------------
/// Splits one element into a key value pair separated by chDelim, and trims on request
KStringViewPair kSplitToPair(
        KStringView svBuffer,
		const char chDelim   = '=',             // default: equal delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0'             // default: ignore escapes
        );
//-----------------------------------------------------------------------------

namespace detail {
namespace container_adaptor {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper template to use containers with insert() instead of push_back(),
/// splitting the value given to push_back() into a key value pair for insert()
template<typename Container>
class InsertPair
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	InsertPair(
	        Container& ctContainer,
	        KStringView svTrim,
	        const char chPairDelim,
	        const char chEscape
	        )
	//-----------------------------------------------------------------------------
	    : m_Container(ctContainer)
	    , m_svTrim(svTrim)
	    , m_chPairDelim(chPairDelim)
	    , m_chEscape(chEscape)
	{
	}

	//-----------------------------------------------------------------------------
	void push_back(KStringView sv)
	//-----------------------------------------------------------------------------
	{
		KStringViewPair svPair = kSplitToPair(sv, m_chPairDelim, m_svTrim, m_chEscape);
		m_Container.insert(svPair);
	}

	//-----------------------------------------------------------------------------
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Container.size();
	}

//------
private:
//------

	Container& m_Container;
	KStringView m_svTrim;
	const char m_chPairDelim;
	const char m_chEscape;

}; // InsertPair

} // end of namespace container_adaptor
} // end of namespace detail

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs.
/// @param ctContainer needs to have a insert() that can construct an element from
/// a KStringViewPair (std::pair<KStringView, KStringView>).
/// @param svBuffer the source char sequence.
/// @param chPairDelim the char that is used to separate keys and values in the sequence. Defaults to "=".
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string containing chars to remove from token ends. Defaults to " \t\r\n\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreFrames if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
template<typename Container>
size_t kSplitPairs(
        Container&  ctContainer,
        KStringView svBuffer,
        const char  chPairDelim = '=',
        KStringView svDelim  = ",",             // default: comma delimiter
        KStringView svTrim   = " \t\r\n\b",     // default: trim all whitespace
        const char  chEscape = '\0',            // default: ignore escapes
        bool        bCombineDelimiters = false, // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false  // default: treat double quotes like any other char
        )
//-----------------------------------------------------------------------------
{
	detail::container_adaptor::InsertPair<Container>
	        cAdaptor(ctContainer, svTrim, chPairDelim, chEscape);

	return kSplit(cAdaptor, svBuffer, svDelim, svTrim, chEscape,
	              bCombineDelimiters, bQuotesAreEscapes);
}

} // namespace dekaf2
