/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
//////////////////////////////////////////////////////////////////////////////
*/

#pragma once

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"

/// @file kwords.h
/// Split marked up input buffer into words + skeleton.

DEKAF2_NAMESPACE_BEGIN

namespace detail {
namespace splitting_parser {

/// base class for all splitting parsers
struct DEKAF2_PUBLIC SplitBase
{
	SplitBase(KStringView sInput) : m_sInput(sInput) {}

	bool        empty() const            { return m_sInput.empty();        }
	void        SkipInput(size_t iCount) { m_sInput.remove_prefix(iCount); }
	KStringView GetRemaining() const     { return m_sInput;                }

protected:

	KStringView m_sInput;

}; // SplitBase

/// counting words broken at white space (returning empty string pairs)
struct DEKAF2_PUBLIC CountSpacedText : public SplitBase
{
	using SplitBase::SplitBase;
	KStringViewPair NextPair();
};

/// returning words broken at white space
struct DEKAF2_PUBLIC SimpleSpacedText : public SplitBase
{
	using SplitBase::SplitBase;
	KStringViewPair NextPair();
};

/// counting words broken at non-alphanumerical characters (returning empty string pairs)
struct DEKAF2_PUBLIC CountText : public SplitBase
{
	using SplitBase::SplitBase;
	KStringViewPair NextPair();
};

/// returning words broken at non-alphanumerical characters
struct DEKAF2_PUBLIC SimpleText : public SplitBase
{
	using SplitBase::SplitBase;
	KStringViewPair NextPair();
};

/// counting words in HTML code broken by anything non alphanumerical
struct DEKAF2_PUBLIC CountHTML : public SplitBase
{
	using SplitBase::SplitBase;
	KStringViewPair NextPair();
};

/// returning words and their leading non-words (HTML code) in HTML code broken by anything non alphanumerical
struct DEKAF2_PUBLIC SimpleHTML : public SplitBase
{
	using SplitBase::SplitBase;
	std::pair<KString, KStringView> NextPair();
};

/// returning words and their leading non-words (HTML code) in HTML code broken by anything non alphanumerical, normalizing (reducing spaces) the skeleton
struct DEKAF2_PUBLIC NormalizingHTML : public SplitBase
{
	using SplitBase::SplitBase;
	std::pair<KString, KString> NextPair();
};

} // of namespace splitting_parser

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A helper type that emulates a container, but is in fact only a counter.
/// For use with KWords. Returns count of added (emulated) elements via size()
template<typename ValueType = KStringView>
class KCountingContainer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using value_type = ValueType;
	using iterator = KCountingContainer*;
	using const_iterator = const KCountingContainer*;

	void           clear     ()           { m_iCounter = 0;     }
	std::size_t    size      () const     { return m_iCounter;  }
	iterator       begin     ()           { return this;        }
	iterator       end       ()           { return nullptr;     }
	const_iterator begin     () const     { return this;        }
	const_iterator end       () const     { return nullptr;     }
	void           reserve   (size_t)     {}
	void           push_back (value_type) { inc();              }
	iterator       insert    (value_type) { inc(); return this; }
	iterator       insert    (const_iterator pos, const_iterator first, const_iterator last) { m_iCounter += first->size(); return this; }

//------
private:
//------

	void inc() { ++m_iCounter; }

	std::size_t m_iCounter { 0 };

}; // KCountingContainer

} // of namespace detail


//-----------------------------------------------------------------------------
/// Version of kSplitWords that fills a container with KStringViewPairs, where
/// .first is the word and .second is the preceeding skeleton
template<typename Container, typename Parser = detail::splitting_parser::SimpleText>
typename std::enable_if<std::is_constructible<typename Container::value_type, KStringViewPair>::value, size_t>::type
kSplitWords (
        Container& cContainer,
        KStringView sBuffer
        )
//-----------------------------------------------------------------------------
{
	auto iStartSize = cContainer.size();

	Parser parser(sBuffer);

	while (!parser.empty())
	{
		cContainer.push_back(parser.NextPair());
	}

	return cContainer.size() - iStartSize;

} // kSplitWords


//-----------------------------------------------------------------------------
/// Version of kSplitWords that fills a container with KStringViews of words
template<typename Container, typename Parser>
typename std::enable_if<std::is_constructible<typename Container::value_type, KStringView>::value, size_t>::type
kSplitWords (
        Container& cContainer,
        KStringView sBuffer
        )
//-----------------------------------------------------------------------------
{
	auto iStartSize = cContainer.size();

	Parser parser(sBuffer);

	while (!parser.empty())
	{
		auto sNext = parser.NextPair();
		if (!sNext.first.empty())
		{
			cContainer.push_back(sNext.first);
		}
	}

	return cContainer.size() - iStartSize;

} // kSplitWords


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// word breaking class template constructed around a storage container and a word breaking parser
template<typename Container, typename Parser = detail::splitting_parser::SimpleText>
class KWords
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KWords() = default;

	/// Constructs from a buffer. Reserves space for iReserve items.
	KWords(KStringView sBuffer, std::size_t iReserve = 1)
	{
		m_Container.reserve(iReserve);
		Add(sBuffer);
	}

	/// Adding a new buffer. This adds to the existing content of the internal
	/// container. If you want to start a new round, clear() the container
	/// first or use Parse().
	std::size_t Add(KStringView sBuffer)
	{
		return kSplitWords<Container, Parser>(m_Container, sBuffer);
	}

	/// Parsing a new buffer. Clears the existing content and adds new content.
	std::size_t Parse(KStringView sBuffer)
	{
		m_Container.clear();
		return Add(sBuffer);
	}

	/// Parsing a new buffer. Clears the existing content and adds new content.
	KWords& operator=(KStringView sBuffer)
	{
		Parse(sBuffer);
		return *this;
	}

	/// Adding a new buffer. This adds to the existing content of the internal
	/// container.
	KWords& operator+=(KStringView sBuffer)
	{
		Add(sBuffer);
		return *this;
	}

	/// Adding another KWords instance. This adds to the existing content of the
	/// internal container.
	KWords& operator+=(const KWords& other)
	{
		m_Container.insert(m_Container.end(), other.m_Container.begin(), other.m_Container.end());
		return *this;
	}

	/// Returns a const pointer to the underlying container
	const Container* operator->() const
	{
		return &m_Container;
	}

	/// Returns a pointer to the underlying container
	Container* operator->()
	{
		return &m_Container;
	}

	/// Returns a const reference to the underlying container
	const Container& operator*() const
	{
		return m_Container;
	}

	/// Returns a reference to the underlying container
	Container& operator*()
	{
		return m_Container;
	}

//------
private:
//------

	Container m_Container;

}; // KWords

/// counting words broken at white space only
using KSimpleSpacedWordCounter      = KWords<detail::KCountingContainer<KStringView>, detail::splitting_parser::CountSpacedText>;
/// extracting words broken at white space only
using KSimpleSpacedWords            = KWords<std::vector<KStringView>, detail::splitting_parser::SimpleSpacedText>;
/// extracting words and their leading non-words broken at white space
using KSimpleSpacedSkeletonWords    = KWords<std::vector<KStringViewPair>, detail::splitting_parser::SimpleSpacedText>;

/// counting words broken by anything non alphanumerical
using KSimpleWordCounter            = KWords<detail::KCountingContainer<KStringView>, detail::splitting_parser::CountText>;
/// extracting words broken by anything non alphanumerical
using KSimpleWords                  = KWords<std::vector<KStringView>, detail::splitting_parser::SimpleText>;
/// extracting words and their leading non-words broken by anything non alphanumerical
using KSimpleSkeletonWords          = KWords<std::vector<KStringViewPair>, detail::splitting_parser::SimpleText>;

/// counting words in HTML code broken by anything non alphanumerical
using KSimpleHTMLWordCounter        = KWords<detail::KCountingContainer<KStringView>, detail::splitting_parser::CountHTML>;
/// extracting words in HTML code broken by anything non alphanumerical
using KSimpleHTMLWords              = KWords<std::vector<KString>, detail::splitting_parser::SimpleHTML>;
/// extracting words and their leading non-words (HTML code) in HTML code broken by anything non alphanumerical
using KSimpleHTMLSkeletonWords      = KWords<std::vector<std::pair<KString, KStringView>>, detail::splitting_parser::SimpleHTML>;

/// extracting words and their leading non-words (HTML code) in HTML code broken by anything non alphanumerical, normalizing (reducing spaces) the skeleton
using KNormalizingHTMLSkeletonWords = KWords<std::vector<std::pair<KString, KString>>, detail::splitting_parser::NormalizingHTML>;

DEKAF2_NAMESPACE_END
