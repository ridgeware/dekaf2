/*
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

/// @file kutf8iterator.h
/// provides iterator types for utf8 strings

#include "kutf8.h"
#include <iterator>

#ifdef DEKAF2
DEKAF2_NAMESPACE_BEGIN
#endif

namespace Unicode {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A non-modifying bidirectional iterator over a utf8 string
template<class NarrowString>
class UTF8ConstIterator
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = codepoint_t;
	using const_reference = const value_type&;
	using const_pointer = const value_type*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using self_type = UTF8ConstIterator;

	//-----------------------------------------------------------------------------
	/// ctor for const string iterators
	UTF8ConstIterator(const typename NarrowString::const_iterator it,
	                  const typename NarrowString::const_iterator ie,
	                  bool bToEnd = false)
	//-----------------------------------------------------------------------------
	: m_begin(it)
	, m_end(ie)
	, m_next(bToEnd ? ie : it)
	{
		operator++();
	}

	//-----------------------------------------------------------------------------
	/// ctor for const strings
	UTF8ConstIterator(const NarrowString& String, bool bToEnd = false)
	//-----------------------------------------------------------------------------
	: UTF8ConstIterator(String.begin(), String.end(), bToEnd)
	{
	}

	//-----------------------------------------------------------------------------
	/// prefix increment
	self_type& operator++()
	//-----------------------------------------------------------------------------
	{
		m_Value = m_next != m_end ? CodepointFromUTF8(m_next, m_end) : INVALID_CODEPOINT;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// postfix increment
	const self_type operator++(int)
	//-----------------------------------------------------------------------------
	{
		const self_type i = *this;
		operator++();
		return i;
	}

	//-----------------------------------------------------------------------------
	/// prefix decrement
	self_type& operator--()
	//-----------------------------------------------------------------------------
	{
		// stay at .end() if value is invalid
		if (KUTF8_LIKELY(m_Value != INVALID_CODEPOINT))
		{
			m_next -= UTF8Bytes(m_Value);
		}
		typename NarrowString::const_iterator hit = m_next;
		m_Value = PrevCodepointFromUTF8(m_begin, m_next);
		m_next = hit;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// postfix decrement
	const self_type operator--(int)
	//-----------------------------------------------------------------------------
	{
		const self_type i = *this;
		operator--();
		return i;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	const_reference operator*() const
	//-----------------------------------------------------------------------------
	{
		return m_Value;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	const_pointer operator->() const
	//-----------------------------------------------------------------------------
	{
		return &m_Value;
	}

	//-----------------------------------------------------------------------------
	/// equality operator
	bool operator==(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		// need to check for same value as well, as the end iterator points
		// to the same address, but has an invalid value (-1)
		return m_next == other.m_next && m_Value == other.m_Value;
	}

	//-----------------------------------------------------------------------------
	/// inequality operator
	bool operator!=(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(other);
	}

//-------
protected:
//-------

	typename NarrowString::const_iterator m_begin;
	typename NarrowString::const_iterator m_end;
	typename NarrowString::const_iterator m_next;
	value_type m_Value;

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A modifying bidirectional iterator over a utf8 string - new codepoints do not need to
/// have the same size in utf8-bytes as the codepoint they are replacing
// To adapt to changing string buffer addresses through inserts of UTF8 codepoints,
// this iterator has to work on character indexes, not on pointers to characters.
// Also, any string length manipulation outside of this iterator would invalidate it.
template<class NarrowString>
class UTF8Iterator
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = codepoint_t;
	using reference = value_type&;
	using pointer = value_type*;
	using const_reference = const value_type&;
	using const_pointer = const value_type*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using self_type = UTF8Iterator;

	//-----------------------------------------------------------------------------
	/// ctor for strings
	UTF8Iterator(NarrowString& String, bool bToEnd = false)
	//-----------------------------------------------------------------------------
	: m_String(&String)
	, m_next(bToEnd ? NarrowString::npos : 0)
	{
		operator++();
	}

	//-----------------------------------------------------------------------------
	/// copy constructor
	UTF8Iterator(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move constructor
	UTF8Iterator(self_type&& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor
	~UTF8Iterator()
	//-----------------------------------------------------------------------------
	{
		if (KUTF8_UNLIKELY(m_Value != m_OrigValue))
		{
			SaveChangedValue();
		}
	}

	//-----------------------------------------------------------------------------
	/// copy assignment
	self_type& operator=(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// prefix increment
	self_type& operator++()
	//-----------------------------------------------------------------------------
	{
		if (KUTF8_LIKELY(m_String != nullptr))
		{
			if (KUTF8_UNLIKELY(m_Value != m_OrigValue))
			{
				SaveChangedValue();
			}

			if (KUTF8_UNLIKELY(m_next != NarrowString::npos))
			{
				if (KUTF8_UNLIKELY(m_next == m_String->size()))
				{
					m_next = NarrowString::npos;
				}
				else
				{
					typename NarrowString::iterator it = m_String->begin() + m_next;
					m_Value = m_OrigValue = CodepointFromUTF8(it, m_String->end());
					m_next = it - m_String->begin();
				}
			}
		}

		return *this;
	}

	//-----------------------------------------------------------------------------
	/// postfix increment
	const self_type operator++(int)
	//-----------------------------------------------------------------------------
	{
		self_type i = *this;
		operator++();
		i.m_OrigValue = i.m_Value;
		return i;
	}

	//-----------------------------------------------------------------------------
	/// prefix decrement
	self_type& operator--()
	//-----------------------------------------------------------------------------
	{
		if (KUTF8_LIKELY(m_String != nullptr))
		{
			if (KUTF8_UNLIKELY(m_Value != m_OrigValue))
			{
				SaveChangedValue();
			}

			if (KUTF8_UNLIKELY(m_next != NarrowString::npos))
			{
				// stay at .end() if value is invalid
				if (KUTF8_LIKELY(m_Value != INVALID_CODEPOINT))
				{
					m_next -= UTF8Bytes(m_Value);
				}

				typename NarrowString::iterator it = m_String->begin() + m_next;
				m_Value = m_OrigValue = PrevCodepointFromUTF8(m_String->begin(), it);
			}
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// postfix decrement
	const self_type operator--(int)
	//-----------------------------------------------------------------------------
	{
		self_type i = *this;
		operator--();
		i.m_OrigValue = i.m_Value;
		return i;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	const_reference operator*() const
	//-----------------------------------------------------------------------------
	{
		return m_Value;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	reference operator*()
	//-----------------------------------------------------------------------------
	{
		return m_Value;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	const_pointer operator->() const
	//-----------------------------------------------------------------------------
	{
		return &m_Value;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	pointer operator->()
	//-----------------------------------------------------------------------------
	{
		return &m_Value;
	}

	//-----------------------------------------------------------------------------
	/// equality operator
	bool operator==(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return m_String->begin() == other.m_String->begin() && m_next == other.m_next;
	}

	//-----------------------------------------------------------------------------
	/// inequality operator
	bool operator!=(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(other);
	}

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	void SaveChangedValue()
	//-----------------------------------------------------------------------------
	{
		if (KUTF8_LIKELY(m_String != nullptr))
		{
			size_t iOrigLen = UTF8Bytes(m_OrigValue);
			size_t iNewLen = UTF8Bytes(m_Value);

			// create an iterator pointing to the start of the current sequence
			typename NarrowString::iterator it = m_String->begin() + m_next - iOrigLen;

			if (KUTF8_UNLIKELY(iOrigLen < iNewLen))
			{
				// make room in the string
				m_String->insert(it - m_String->begin(), iNewLen - iOrigLen, ' ');
			}
			else if (KUTF8_UNLIKELY(iOrigLen > iNewLen))
			{
				// shorten the string
				m_String->erase(it - m_String->begin(), iOrigLen - iNewLen);
			}

			// calc it again (the string buffer might have changed)
			it = m_String->begin() + m_next - iOrigLen;

			// calculate size difference
			std::ptrdiff_t iAdjust = iNewLen - iOrigLen;

			// adjust m_next to point after end of replaced sequence
			m_next += iAdjust;

			// and finally write the replacement sequence
			ToUTF8(m_Value, it);

			// no need to update m_OrigValue with m_Value as this
			// function is only called from the dtor or from operator++()
			// just before the values are updated anyway
		}
	}

	NarrowString* m_String { nullptr };
	size_type m_next;
	value_type m_Value { 0 };
	value_type m_OrigValue { 0 };

};

} // namespace Unicode

#ifdef DEKAF2
DEKAF2_NAMESPACE_END
#endif
