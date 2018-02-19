/*
//////////////////////////////////////////////////////////////////////////////
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

#include "kstringview.h"

/// @file kwords.h
/// Split marked up input buffer into words + skeleton.

namespace dekaf2
{

namespace detail {
namespace splitting_parser {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class SimpleText
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	SimpleText(KStringView sInput)
		: m_sInput(sInput)
	{}

	bool empty()
	{
		return m_sInput.empty();
	}

	KStringViewPair NextPair();

//------
private:
//------

	KStringView m_sInput;

}; // SimpleText

} // of namespace splitting_parser
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
		cContainer.push_back(parser.NextPair().first);
	}

	return cContainer.size() - iStartSize;

} // kSplitWords


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Container, typename Parser = detail::splitting_parser::SimpleText>
class KWords
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KWords(KStringView sBuffer)
	{
		kSplitWords<Container, Parser>(m_Container, sBuffer);
	}

	const Container* operator->() const
	{
		return &m_Container;
	}

	Container* operator->()
	{
		return &m_Container;
	}

	const Container& operator*() const
	{
		return m_Container;
	}

	Container& operator*()
	{
		return m_Container;
	}

//------
private:
//------

	Container m_Container;

}; // KWords

using KSimpleWords = KWords<std::vector<KStringView>, detail::splitting_parser::SimpleText>;
using KSimpleSkeletonWords = KWords<std::vector<KStringViewPair>, detail::splitting_parser::SimpleText>;

} // namespace dekaf2
