/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2019, Ridgeware, Inc.
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

#pragma once

#include <vector>
#include "kdefinitions.h"
#include "ktemplate.h"
#include "kstringview.h"

/// @file ksplit.h
/// Highly configurable tokenizer templates

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// Simplified version for single delimiter char.
/// Splits string into token container using one delimiter. Container is a sequence, like a vector.
/// @param cContainer needs to have a push_back() that can construct an element from
/// a KStringView.
/// @param svBuffer the source char sequence.
/// @param svDelim a char with the delimiter character.
/// @return count of added tokens.
///
/// @code
/// std::vector<KStringView> Words;
/// auto iWordCount = kSplit(Words, "This sentence has five words", ' ');
/// @endcode
template<typename Container,
	typename std::enable_if<
		detail::has_key_type<Container>::value == false &&
		std::is_constructible<typename Container::value_type, KStringViewPair>::value == false
	, int>::type = 0
>
DEKAF2_PUBLIC 
std::size_t kSplit (
        Container&  cContainer,
        KStringView svBuffer,
		const char  Delim
)
//-----------------------------------------------------------------------------
{
	if (svBuffer.empty()) return 0;

	std::size_t iStartSize = cContainer.size();

	if (Delim == ' ')
	{
		for(;;)
		{
			// Look for delimiter character, respect escapes
			auto iNext = kFind (svBuffer, ' ', 0);

			if (iNext != KStringView::npos)
			{
				cContainer.push_back(svBuffer.substr(0, iNext));

				// if space is a delimiter we always treat consecutive spaces as one delimiter
				iNext = kFindNot (svBuffer, ' ', ++iNext);

				if (iNext > svBuffer.size())
				{
					iNext = svBuffer.size();
				}

				svBuffer.remove_prefix(iNext);
			}
			else
			{
				if (!svBuffer.empty())
				{
					cContainer.push_back(svBuffer);
				}
				break;
			}
		}

	}
	else
	{
		for(;;)
		{
			// Look for delimiter character, respect escapes
			auto iNext = kFind (svBuffer, Delim, 0);

			if (iNext != KStringView::npos)
			{
				cContainer.push_back(svBuffer.substr(0, iNext));

				svBuffer.remove_prefix(++iNext);
			}
			else
			{
				cContainer.push_back(svBuffer);
				break;
			}
		}
	}

	return cContainer.size() - iStartSize;

} // kSplit with single split char and no trimming/escaping

//-----------------------------------------------------------------------------
/// Splits string into token container using delimiters, trim, and escape. Container is
/// a sequence, like a vector.
/// @param cContainer needs to have a push_back() that can construct an element from
/// a KStringView.
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string view containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return count of added tokens.
///
/// @code
/// std::vector<KStringView> Words;
/// auto iWordCount = kSplit(Words, "This sentence has five words", " ");
/// @endcode
template<typename Container,
	typename std::enable_if<
		detail::has_key_type<Container>::value == false &&
		std::is_constructible<typename Container::value_type, KStringViewPair>::value == false
	, int>::type = 0
>
DEKAF2_PUBLIC
std::size_t kSplit (
        Container&  cContainer,
        KStringView svBuffer,
		const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
		const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
		const char chEscape            = '\0',                    // default: no escape character
		bool  bCombineDelimiters       = false,                   // default: create empty elements for adjacent delimiters
        bool  bQuotesAreEscapes        = false                    // default: treat double quotes like any other char
)
//-----------------------------------------------------------------------------
{
	std::size_t iStartSize = cContainer.size();
	bool bAddLastEmptyElement { false };

	while (DEKAF2_LIKELY(!svBuffer.empty()))
	{
		if (DEKAF2_LIKELY(!Trim.empty()))
		{
			// Strip prefix trim characters.
			auto iFound = Trim.find_first_not_in(svBuffer);

			if (iFound != KStringView::npos)
			{
				if (iFound > 0)
				{
					svBuffer.remove_prefix (iFound);
				}
			}
			else
			{
				// input was all trimmable chars
				cContainer.push_back(KStringView());
				break;
			}
		}

		KStringView sElement;
		bool bHaveQuotes { false };

		for (;DEKAF2_UNLIKELY(bQuotesAreEscapes);)
		{
			char chQuote;
			if (svBuffer.front() == '"') chQuote = '"';
			else if (svBuffer.front() == '\'') chQuote = '\'';
			else break;

			auto iQuote = kFindUnescaped(svBuffer, chQuote, chEscape, 1);
			if (DEKAF2_LIKELY(iQuote != KStringView::npos))
			{
				// only treat this as a quoted token if we have a closing quote
				sElement = svBuffer.substr(1, iQuote - 1);
				svBuffer.remove_prefix(iQuote + 1);
				bHaveQuotes = true;
			}
			break;
		}

		// Look for delimiter character, respect escapes
		auto iNext = kFindFirstOfUnescaped (svBuffer, Delim, chEscape);

		if (DEKAF2_LIKELY(iNext != KStringView::npos))
		{
			if (DEKAF2_LIKELY(!bHaveQuotes))
			{
				sElement = svBuffer.substr(0, iNext);
			}

			auto thisDelimiter = svBuffer[iNext];

			if (DEKAF2_UNLIKELY(thisDelimiter == ' '))
			{
				// if space is a delimiter we always treat consecutive spaces as one delimiter
				iNext = svBuffer.find_first_not_of(' ', iNext + 1);
			}
			else
			{
				++iNext;
			}

			if (DEKAF2_UNLIKELY(bCombineDelimiters))
			{
				// skip all adjacent delimiters
				iNext = Delim.find_first_not_in(svBuffer, iNext);
			}

			if (iNext > svBuffer.size())
			{
				iNext = svBuffer.size();
			}

			svBuffer.remove_prefix(iNext);

			if (DEKAF2_UNLIKELY(svBuffer.empty()))
			{
				// add a last empty element if this delimiter is not a space and the trim sequence does not contain the delimiter either
				bAddLastEmptyElement = thisDelimiter != ' ' && !Trim.contains(thisDelimiter);
			}
		}
		else
		{
			if (DEKAF2_LIKELY(!bHaveQuotes))
			{
				sElement = svBuffer;
			}

			svBuffer.clear();
		}

		if (DEKAF2_LIKELY(!Trim.empty() && !bHaveQuotes))
		{
			//  Strip suffix space characters.
			auto iFound = Trim.find_last_not_in(sElement);

			if (iFound != KStringView::npos)
			{
				auto iRemove = sElement.size() - 1 - iFound;
				sElement.remove_suffix(iRemove);
			}
			else
			{
				sElement.clear();
			}
		}

		cContainer.push_back(sElement);

		// What remains is ready for the next parse round.
	}

	if (DEKAF2_UNLIKELY(bAddLastEmptyElement))
	{
		cContainer.push_back(KStringView{});
	}

	return cContainer.size() - iStartSize;

} // kSplit with string of delimiters

#if !defined(DEKAF2_IS_MSC) && (!defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION_MAJOR > 5)
// precompile for std::vector<KStringView>
extern template
DEKAF2_PUBLIC
std::size_t kSplit(
		std::vector<KStringView>& cContainer,
        KStringView svBuffer,
        const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
        const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
        const char  chEscape           = '\0',                 // default: ignore escapes
        bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
);
#endif // of DEKAF2_IS_MSC

//-----------------------------------------------------------------------------
/// Simplified version for single delimiter char.
/// Splits string into token container using one delimiter. Returned Container is a sequence, like a vector.
/// @param svBuffer the source char sequence.
/// @param svDelim a delimiter character.
/// @return a new Container, its type needs to have a push_back() that can construct
/// an element from a KStringView.
///
/// @code
/// auto Words = kSplits("This sentence has five words", ' ');
/// @endcode
template<typename Container = std::vector<KStringView>,
	typename std::enable_if<detail::has_key_type<Container>::value == false
								&& std::is_constructible<typename Container::value_type, KStringViewPair>::value == false, int>::type = 0 >
DEKAF2_NODISCARD DEKAF2_PUBLIC
Container kSplits(
			  KStringView svBuffer,
			  const char Delim
)
//-----------------------------------------------------------------------------
{
	Container ctContainer;
	kSplit(ctContainer, svBuffer, Delim);
	return ctContainer;
}

//-----------------------------------------------------------------------------
/// Splits string into token container using delimiters, trim, and escape. Returned
/// Container is a sequence, like a vector.
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string view containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return a new Container, its type needs to have a push_back() that can construct
/// an element from a KStringView.
///
/// @code
/// auto Words = kSplits("This sentence has five words", " ");
/// @endcode
template<typename Container = std::vector<KStringView>,
	typename std::enable_if<detail::has_key_type<Container>::value == false
								&& std::is_constructible<typename Container::value_type, KStringViewPair>::value == false, int>::type = 0 >
DEKAF2_NODISCARD DEKAF2_PUBLIC
Container kSplits(
			  KStringView svBuffer,
			  const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
			  const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
			  const char  chEscape           = '\0',                    // default: ignore escapes
			  bool        bCombineDelimiters = false,                   // default: create an element for each delimiter char found
			  bool        bQuotesAreEscapes  = false                    // default: treat double quotes like any other char
)
//-----------------------------------------------------------------------------
{
	Container ctContainer;
	kSplit(ctContainer, svBuffer, Delim, Trim, chEscape, bCombineDelimiters, bQuotesAreEscapes);
	return ctContainer;
}

//-----------------------------------------------------------------------------
/// Splits one element into a key value pair separated by chDelim, and trims on request.
/// @param svBuffer the source char sequence (a string view)
/// @param svPairDelim a string view that separates key from value. Defaults to "=".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @return the split key/value pair (KStringViewPair)
///
/// @code
/// auto Pair = kSplitToPair("Apples = Oranges");
/// // -> Pair.first == "Apples" and Pair.second == "Oranges"
/// @endcode
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringViewPair kSplitToPair(
        KStringView svBuffer,
        KStringView svPairDelim     = "=",                     // default: equal delimiter
		const KFindSetOfChars& Trim = detail::kASCIISpacesSet, // default: trim all whitespace
        const char  chEscape        = '\0'                     // default: ignore escapes
	);
//-----------------------------------------------------------------------------

namespace detail {
namespace container_adaptor {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper template to use containers with insert() instead of push_back(),
/// taking the value given to push_back() as the single value for insert()
template<typename Container>
class DEKAF2_PUBLIC InsertValue
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// the value_type is selected to satisfy the SFINAE condition of the first
	// kSplit version (for sequential types) - it is _not_ the real value type
	using value_type = KStringView;

	//-----------------------------------------------------------------------------
	InsertValue(
			Container& cContainer
			)
	//-----------------------------------------------------------------------------
		: m_Container(cContainer)
	{
	}

	//-----------------------------------------------------------------------------
	void push_back(KStringView sv)
	//-----------------------------------------------------------------------------
	{
		m_Container.insert(sv);
	}

	//-----------------------------------------------------------------------------
	std::size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Container.size();
	}

//------
private:
//------

	Container& m_Container;

}; // InsertValue

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper template to use containers with insert() instead of push_back(),
/// splitting the value given to push_back() into a key value pair for insert()
template<typename Container>
class DEKAF2_PUBLIC InsertPair
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// the value_type is selected to satisfy the SFINAE condition of the first
	// kSplit version (for sequential types) - it is _not_ the real value type
	using value_type = KStringView;

	//-----------------------------------------------------------------------------
	InsertPair(
	        Container& cContainer,
	        const KFindSetOfChars& Trim,
	        const KStringView svPairDelim,
	        const char chEscape
	        )
	//-----------------------------------------------------------------------------
	    : m_Container(cContainer)
	    , m_Trim(Trim)
	    , m_svPairDelim(svPairDelim)
	    , m_chEscape(chEscape)
	{
	}

	//-----------------------------------------------------------------------------
	void push_back(KStringView sv)
	//-----------------------------------------------------------------------------
	{
		KStringViewPair svPair = kSplitToPair(sv, m_svPairDelim, m_Trim, m_chEscape);
		auto pair = m_Container.insert({svPair.first, svPair.second});
        
        if (pair.second == false)
        {
            pair.first->second = svPair.second;
        }
	}

	//-----------------------------------------------------------------------------
	std::size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Container.size();
	}

//------
private:
//------

	Container& m_Container;
	const KFindSetOfChars& m_Trim;
	const KStringView m_svPairDelim;
	const char m_chEscape;

}; // InsertPair

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper template to use containers with push_back() on a pair instead of push_back(),
/// splitting the value given to push_back() into a key value pair for push_back()
template<typename Container>
class DEKAF2_PUBLIC PushBackPair
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// the value_type is selected to satisfy the SFINAE condition of the first
	// kSplit version (for sequential types) - it is _not_ the real value type
	using value_type = KStringView;

	//-----------------------------------------------------------------------------
	PushBackPair(
			Container& cContainer,
			const KFindSetOfChars& Trim,
			const KStringView svPairDelim,
			const char chEscape
			)
	//-----------------------------------------------------------------------------
		: m_Container(cContainer)
		, m_Trim(Trim)
		, m_svPairDelim(svPairDelim)
		, m_chEscape(chEscape)
	{
	}

	//-----------------------------------------------------------------------------
	void push_back(KStringView sv)
	//-----------------------------------------------------------------------------
	{
		KStringViewPair svPair = kSplitToPair(sv, m_svPairDelim, m_Trim, m_chEscape);
		m_Container.push_back({svPair.first, svPair.second});
	}

	//-----------------------------------------------------------------------------
	std::size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Container.size();
	}

//------
private:
//------

	Container& m_Container;
	const KFindSetOfChars& m_Trim;
	const KStringView m_svPairDelim;
	const char m_chEscape;

}; // PushBackPair

} // end of namespace container_adaptor
} // end of namespace detail

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs (container is a map).
/// @param cContainer needs to have a insert() that can construct an element from
/// a KStringViewPair (std::pair<KStringView, KStringView>).
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svPairDelim the string view that is used to separate keys and values in the sequence. Defaults to "=".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return count of added key/value pairs.
///
/// @code
/// std::map<KStringView, KStringView> Pairs;
/// auto iPairCount = kSplit(Pairs, "Apples = Oranges, Red = 1, Blue = 2,Green=3");
/// // -> iPairCount == 4
/// // -> Pairs == {{ "Apples", "Oranges" },{ "Red", "1" },{ "Blue, "2" },{ "Green", "3" }}
/// @endcode
template<typename Container,
	typename std::enable_if<detail::is_map_type<Container>::value == true, int>::type = 0 >
DEKAF2_PUBLIC
std::size_t kSplit(
        Container&  cContainer,
        KStringView svBuffer,
		const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
        KStringView svPairDelim        = "=",
        const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
        const char  chEscape           = '\0',                 // default: ignore escapes
        bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
        )
//-----------------------------------------------------------------------------
{
	detail::container_adaptor::InsertPair<Container>
	        cAdaptor(cContainer, Trim, svPairDelim, chEscape);

	return kSplit(cAdaptor, svBuffer, Delim, Trim, chEscape,
	              bCombineDelimiters, bQuotesAreEscapes);
}

//-----------------------------------------------------------------------------
/// Splitting strings into a series of values (container is a set).
/// @param cContainer needs to have a insert() that can construct an element from
/// a KStringView.
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return count of added values.
///
/// @code
/// std::set<KStringView> Fruits;
/// auto iCount = kSplit(Fruits, "Apples, Oranges, Kiwis ,Bananas ");
/// // -> iCount == 4
/// // -> Fruits == { "Apples", "Oranges", "Kiwis", "Bananas" };
/// @endcode
template<typename Container,
	typename std::enable_if<detail::is_set_type<Container>::value == true, int>::type = 0 >
DEKAF2_PUBLIC
std::size_t kSplit(
		Container&  cContainer,
		KStringView svBuffer,
		const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
		const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
		const char  chEscape = '\0',                           // default: ignore escapes
		bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
		bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
		)
//-----------------------------------------------------------------------------
{
	detail::container_adaptor::InsertValue<Container>
			cAdaptor(cContainer);

	return kSplit(cAdaptor, svBuffer, Delim, Trim, chEscape,
				  bCombineDelimiters, bQuotesAreEscapes);
}

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs (container is a set).
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return A new Container, its type needs to have an insert() that can construct
/// an element from a KStringView.
///
/// @code
/// auto Fruits = kSplits<std::set<KStringView>>("Apples, Oranges, Kiwis ,Bananas ");
/// // -> Fruits == { "Apples", "Oranges", "Kiwis", "Bananas" };
/// @endcode

template<typename Container,
	typename std::enable_if<detail::is_set_type<Container>::value == true, int>::type = 0 >
DEKAF2_NODISCARD DEKAF2_PUBLIC
Container kSplits(
			  KStringView svBuffer,
			  const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
			  const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
			  const char  chEscape = '\0',                           // default: ignore escapes
			  bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
			  bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
)
//-----------------------------------------------------------------------------
{
	Container cContainer;
	kSplit(cContainer, svBuffer, Delim, Trim, chEscape, bCombineDelimiters, bQuotesAreEscapes);
	return cContainer;
}

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs (container is a map).
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svPairDelim the string view that is used to separate keys and values in the sequence. Defaults to "=".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return A new Container, its type needs to have an insert() that can construct
/// an element from a KStringViewPair (std::pair<KStringView, KStringView>).
///
/// @code
/// auto Pairs = kSplits<std::map<KStringView, KStringView>>(Pairs, "Apples = Oranges, Red = 1, Blue = 2,Green=3");
/// // -> Pairs == {{ "Apples", "Oranges" },{ "Red", "1" },{ "Blue, "2" },{ "Green", "3" }}
/// @endcode

template<typename Container,
	typename std::enable_if<detail::is_map_type<Container>::value == true, int>::type = 0 >
DEKAF2_NODISCARD DEKAF2_PUBLIC
Container kSplits(
			  KStringView svBuffer,
			  const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
			  KStringView svPairDelim        = "=",
			  const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
			  const char  chEscape           = '\0',                 // default: ignore escapes
			  bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
			  bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
		)
//-----------------------------------------------------------------------------
{
	Container cContainer;
	kSplit(cContainer, svBuffer, svPairDelim, Delim, Trim, chEscape, bCombineDelimiters, bQuotesAreEscapes);
	return cContainer;
}

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs (container is a sequence type
/// like std::vector).
/// @param cContainer needs to have a push_back() that can construct an element from
/// a KStringViewPair (std::pair<KStringView, KStringView>).
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svPairDelim the string view that is used to separate keys and values in the sequence. Defaults to "=".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return count of added key/value pairs.
///
/// @code
/// std::vector<std::pair<KStringView, KStringView>> Pairs;
/// auto iPairCount = kSplit(Pairs, "Apples = Oranges, Red = 1, Blue = 2,Green=3");
/// // -> iPairCount == 4
/// // -> Pairs == {{ "Apples", "Oranges" },{ "Red", "1" },{ "Blue, "2" },{ "Green", "3" }}
/// @endcode
template<typename Container,
	typename std::enable_if<detail::has_key_type<Container>::value == false
								&& std::is_constructible<typename Container::value_type, KStringViewPair>::value == true, int>::type = 0 >
DEKAF2_PUBLIC
std::size_t kSplit(
        Container&  cContainer,
        KStringView svBuffer,
		const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
        KStringView svPairDelim        = "=",
        const KFindSetOfChars Trim     = detail::kASCIISpacesSet, // default: trim all whitespace
        const char  chEscape           = '\0',                 // default: ignore escapes
        bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
        bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
        )
//-----------------------------------------------------------------------------
{
	detail::container_adaptor::PushBackPair<Container>
	        cAdaptor(cContainer, Trim, svPairDelim, chEscape);

	return kSplit(cAdaptor, svBuffer, Delim, Trim, chEscape,
	              bCombineDelimiters, bQuotesAreEscapes);
}

//-----------------------------------------------------------------------------
/// Splitting strings into a series of key value pairs (container is a sequence type
/// like std::vector).
/// @param svBuffer the source char sequence.
/// @param svDelim a string view of delimiter characters. Defaults to ",".
/// @param svPairDelim the string view that is used to separate keys and values in the sequence. Defaults to "=".
/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
/// taken for found spaces if defined as delimiter). Defaults to false.
/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
/// double quotes are treated as literal chars, and quotes themselves are removed.
/// No trimming is applied inside the quotes (but outside). The quote has to be the
/// first character after applied trimming, and trailing content after the closing quote
/// is not considered part of the token. Defaults to false.
/// @return A new Container, its type needs to have a push_back() that can construct
/// an element from a KStringViewPair (std::pair<KStringView, KStringView>).
///
/// @code
/// auto Pairs = kSplits<std::vector<std::pair<KStringView, KStringView>>>(Pairs, "Apples = Oranges, Red = 1, Blue = 2,Green=3");
/// // -> Pairs == {{ "Apples", "Oranges" },{ "Red", "1" },{ "Blue, "2" },{ "Green", "3" }}
/// @endcode

template<typename Container,
	typename std::enable_if<detail::has_key_type<Container>::value == false
								&& std::is_constructible<typename Container::value_type, KStringViewPair>::value == true, int>::type = 0 >
DEKAF2_NODISCARD DEKAF2_PUBLIC
Container kSplits(
			  KStringView svBuffer,
			  const KFindSetOfChars& Delim   = detail::kCommaSet,       // default: comma delimiter
			  KStringView svPairDelim        = "=",
			  const KFindSetOfChars& Trim    = detail::kASCIISpacesSet, // default: trim all whitespace
			  const char  chEscape           = '\0',                 // default: ignore escapes
			  bool        bCombineDelimiters = false,                // default: create an element for each delimiter char found
			  bool        bQuotesAreEscapes  = false                 // default: treat double quotes like any other char
	)
//-----------------------------------------------------------------------------
{
	Container cContainer;
	kSplit(cContainer, svBuffer, svPairDelim, Delim, Trim, chEscape, bCombineDelimiters, bQuotesAreEscapes);
	return cContainer;
}

//-----------------------------------------------------------------------------
/// Splitting a command line style string into token container, modifying the source buffer
/// so that each token is followed by a null char, much like strtok()
/// @param cContainer needs to have a push_back() that can construct an element from
/// a char*.
/// @param sBuffer the source char sequence - will be modified.
/// @param svDelim a string view of delimiter characters. Defaults to " \f\n\r\t\v\b".
/// @param svQuotes a string view of quote characters. Defaults to "\"'".
/// @param chEscape Escape character for delimiters. Defaults to '\\'.
/// @return count of added tokens.
///
/// @code
/// std::vector<char*> Words;
/// auto iHaveWords = kSplitInPlace(Words, "This sentence has five words");
/// // -> iHaveWords == 5
/// @endcode
template<typename Container, typename String>
std::size_t kSplitArgsInPlace(
	Container&  cContainer,
	String&     sBuffer,
	KStringView svDelim  = detail::kASCIISpaces, // default: whitespace delimiter
	KStringView svQuotes = "\"'",                // default: dequote
	const char  chEscape = '\\'                  // default: escape with backslash
	)
//-----------------------------------------------------------------------------
{
	char* pStart { nullptr };
	char quoteChar { 0 };
	bool bEscaped { false };
	std::size_t iOriginalSize = cContainer.size();

	for (auto& ch : sBuffer)
	{
		if (DEKAF2_UNLIKELY(bEscaped))
		{
			bEscaped = false;
			if (DEKAF2_UNLIKELY(pStart == nullptr))
			{
				pStart = &ch;
			}
			continue;
		}

		if (DEKAF2_UNLIKELY(ch == chEscape))
		{
			bEscaped = true;
		}
		else if (DEKAF2_UNLIKELY(quoteChar))
		{
			if (DEKAF2_UNLIKELY(ch == quoteChar))
			{
				ch = quoteChar = 0;

				if (DEKAF2_LIKELY(pStart != nullptr))
				{
					cContainer.push_back(pStart);
					pStart = nullptr;
				}
			}
		}
		else
		{
			if (DEKAF2_UNLIKELY(svDelim.find(ch) != KStringView::npos))
			{
				ch = 0;

				if (pStart)
				{
					cContainer.push_back(pStart);
					pStart = nullptr;
				}
			}
			else if (DEKAF2_UNLIKELY(svQuotes.find(ch) != KStringView::npos))
			{
				quoteChar = ch;
				pStart = &ch + 1;
			}
			else if (DEKAF2_UNLIKELY(pStart == nullptr))
			{
				pStart = &ch;
			}
		}
	}

	if (DEKAF2_LIKELY(pStart != nullptr))
	{
		// last fragment if not quoted
		cContainer.push_back(pStart);
	}

	return cContainer.size() - iOriginalSize;

} // kSplitArgsInPlace

//-----------------------------------------------------------------------------
/// Splits argument list string into token container. Container is
/// a sequence, like a vector.
/// @param cContainer needs to have a push_back() that can construct an element from
/// a KStringView.
/// @param svBuffer the source char sequence.
/// @return count of added tokens.
///
/// @code
/// std::vector<KStringView> Arguments;
/// auto iArgumentCount = kSplit(Arguments, R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter));
/// @endcode
template<typename Container,
typename std::enable_if<
		detail::has_key_type<Container>::value == false &&
		std::is_constructible<typename Container::value_type, KStringViewPair>::value == false
	, int>::type = 0
>
DEKAF2_PUBLIC
std::size_t kSplitArgs (Container&  cContainer, KStringView svBuffer)
//-----------------------------------------------------------------------------
{
	return kSplit(cContainer, svBuffer, " ", detail::kASCIISpacesSet, '\\', false, true);
}

//-----------------------------------------------------------------------------
/// Splits argument list string into token container. Container is
/// a sequence, like a vector
/// @param svBuffer the source char sequence.
/// @return a new Container, its type needs to have a push_back() that can construct
/// an element from a KStringView  (defaults to std::vector<KStringView> )
///
/// @code
/// auto Arguments = kSplit(R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter)");
/// @endcode
template<typename Container = std::vector<KStringView>,
typename std::enable_if<
		detail::has_key_type<Container>::value == false &&
		std::is_constructible<typename Container::value_type, KStringViewPair>::value == false
	, int>::type = 0
>
DEKAF2_NODISCARD
Container kSplitsArgs (KStringView svBuffer)
//-----------------------------------------------------------------------------
{
	Container cContainer;
	kSplitArgs(cContainer, svBuffer);
	return cContainer;
}

DEKAF2_NAMESPACE_END
