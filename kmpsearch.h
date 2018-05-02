/*
 //-----------------------------------------------------------------------------//
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#pragma once

#include "kstringview.h"
#include "kreader.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMPSearch
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KMPSearch(KStringView sPattern);

	bool Match(char ch) const;
	KStringView::size_type Match(KStringView sHaystack) const;
	bool Match(KInStream& InStream) const;

	template <class ForwardIterator>
	std::pair<ForwardIterator, ForwardIterator> Match(ForwardIterator first, ForwardIterator last) const
	{
		index = 0;

		for (auto it = first; it != last; ++it)
		{
			if (Match(*it))
			{
				return { it - index, it - index + m_sPattern.size() };
			}
		}

		return { last, last };
	}

//------
private:
//------

	void BuildFSA();

	KString m_sPattern {};
	mutable size_t index { 0 };
	std::vector<int16_t> FSA {};

}; // KMPSearch


namespace frozen {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <std::size_t SIZE>
class KMPSearch
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	constexpr KMPSearch(const char (&szPattern)[SIZE+1])
	: m_sPattern { szPattern }
	{
		BuildFSA();
	}

	constexpr KMPSearch(KStringView sPattern)
	: m_sPattern { sPattern }
	{
		BuildFSA();
	}

	constexpr bool Match(char ch, std::size_t& index) const
	{
		while (ch != m_sPattern[index])
		{
			if (index == 0)
			{
				return false;
			}
			index = FSA[index];
		}

		if (++index < m_sPattern.size())
		{
			return false;
		}
		else
		{
			index = FSA[index];
			return true;
		}
	}

	template <class ForwardIterator>
	constexpr std::pair<ForwardIterator, ForwardIterator> Match(ForwardIterator first, ForwardIterator last) const
	{
		std::size_t index { 0 };

		for (auto it = first; it != last; ++it)
		{
			if (Match(*it, index))
			{
				return { it - index, it - index + SIZE };
			}
		}

		return { last, last };
	}

	constexpr KStringView::size_type Match(KStringView sHaystack) const
	{
		std::size_t index { 0 };

		for (KStringView::size_type iPos = 0; iPos < sHaystack.size(); ++iPos)
		{
			if (Match(sHaystack[iPos], index))
			{
				return iPos - (SIZE - 1);
			}
		}

		return KStringView::npos;
	}

	constexpr bool Match(KInStream& InStream) const
	{
		std::size_t index { 0 };
		std::istream::int_type ch { 0 };

		while ((ch = InStream.Read()) != std::istream::traits_type::eof())
		{
			if (Match(ch, index))
			{
				return true;
			}
		}

		return false;
	}

//------
private:
//------

	static_assert(SIZE <= INT16_MAX, "needle too long for implementation, max 2^15 chars allowed");

	constexpr void BuildFSA()
	{
		FSA[0] = -1;

		for (size_t i = 0; i < SIZE; ++i)
		{
			FSA[i + 1] = FSA[i] + 1;

			while (FSA[i + 1] > 0 && m_sPattern[i] != m_sPattern[FSA[i + 1] - 1])
			{
				FSA[i + 1] = FSA[FSA[i + 1] - 1] + 1;
			}
		}
	}

	KStringView m_sPattern {};
	std::array<int16_t, SIZE+1> FSA {};

}; // frozen::KMPSearch

template <std::size_t SIZE>
constexpr KMPSearch<SIZE-1> CreateKMPSearch(const char (&sPattern)[SIZE])
{
	return {sPattern};
}

template <std::size_t SIZE>
constexpr KMPSearch<SIZE-1> CreateKMPSearch(KStringView sPattern)
{
	return {sPattern};
}

} // end of namespace frozen

/*
static constexpr KStringView sv { "hello world" };
static constexpr frozen::KMPSearch<sv.size()> a( sv );

constexpr static auto xy { frozen::CreateKMPSearch("hello") };
constexpr static auto bb { frozen::CreateKMPSearch<sv.size()>(sv) };
*/

} // end of namespace dekaf2


