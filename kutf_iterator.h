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

/// @file kutf_iterator.h
/// provides iterator types for utf8/utf16/utf32 strings

#include "kutf.h"
#include <iterator>

#ifdef DEKAF2
DEKAF2_NAMESPACE_BEGIN
#endif

namespace KUTF_NAMESPACE {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A non-modifying bidirectional iterator over a utf string (of any bit width)
template<class UTFString>
class ConstIterator
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
	using self_type = ConstIterator;
	using orig_value_type = typename UTFString::value_type;

	//-----------------------------------------------------------------------------
	/// ctor for const string iterators
	ConstIterator(const typename UTFString::const_iterator it,
	              const typename UTFString::const_iterator ie,
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
	ConstIterator(const UTFString& String, bool bToEnd = false)
	//-----------------------------------------------------------------------------
	: ConstIterator(String.begin(), String.end(), bToEnd)
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
		if (KUTF_LIKELY(m_Value != INVALID_CODEPOINT))
		{
			m_next -= UTFChars<sizeof(orig_value_type)*8>(m_Value);
		}
		typename UTFString::const_iterator hit = m_next;
		m_Value = PrevCodepoint(m_begin, m_next);
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
	self_type& operator+=(std::size_t iIncrement)
	//-----------------------------------------------------------------------------
	{
		if (iIncrement > 0)
		{
			kutf::Increment(m_next, m_end, iIncrement - 1);
		}
		return operator++();
	}

	//-----------------------------------------------------------------------------
	self_type& operator-=(std::size_t iDecrement)
	//-----------------------------------------------------------------------------
	{
		if (iDecrement > 0)
		{
			kutf::Decrement(m_begin, m_next, iDecrement);
		}
		return operator--();
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
	/// lower than operator
	bool operator<(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return m_next < other.m_next;
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

	typename UTFString::const_iterator m_begin;
	typename UTFString::const_iterator m_end;
	typename UTFString::const_iterator m_next;
	value_type m_Value;

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A modifying bidirectional iterator over a utf8 string - new codepoints do not need to
/// have the same size in utf8-bytes as the codepoint they are replacing
// To adapt to changing string buffer addresses through inserts of UTF8 codepoints,
// this iterator has to work on character indexes, not on pointers to characters.
// Also, any string length manipulation outside of this iterator would invalidate it.
template<class UTFString>
class Iterator
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
	using self_type = Iterator;
	using orig_value_type = typename UTFString::value_type;

	//-----------------------------------------------------------------------------
	/// ctor for strings
	Iterator(UTFString& String, bool bToEnd = false)
	//-----------------------------------------------------------------------------
	: m_String(&String)
	, m_next(bToEnd ? UTFString::npos : 0)
	{
		operator++();
	}

	//-----------------------------------------------------------------------------
	/// copy constructor
	Iterator(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move constructor
	Iterator(self_type&& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor
	~Iterator()
	//-----------------------------------------------------------------------------
	{
		if (m_Value != m_OrigValue)
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
		if (KUTF_LIKELY(m_String != nullptr))
		{
			if (m_Value != m_OrigValue)
			{
				SaveChangedValue();
			}

			if (KUTF_UNLIKELY(m_next != UTFString::npos))
			{
				if (KUTF_UNLIKELY(m_next == m_String->size()))
				{
					m_next = UTFString::npos;
				}
				else
				{
					typename UTFString::iterator it = m_String->begin() + m_next;
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
		if (KUTF_LIKELY(m_String != nullptr))
		{
			if (KUTF_UNLIKELY(m_Value != m_OrigValue))
			{
				SaveChangedValue();
			}

			if (KUTF_UNLIKELY(m_next != UTFString::npos))
			{
				// stay at .end() if value is invalid
				if (KUTF_LIKELY(m_Value != INVALID_CODEPOINT))
				{
					m_next -= UTFChars<sizeof(orig_value_type)*8>(m_Value);
				}
			}
			else
			{
				m_next = m_String->size();
			}

			typename UTFString::iterator it = m_String->begin() + m_next;
			m_Value = m_OrigValue = PrevCodepoint(m_String->begin(), it);
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

	//-----------------------------------------------------------------------------
	self_type& operator+=(std::size_t iIncrement)
	//-----------------------------------------------------------------------------
	{
		if (iIncrement > 0)
		{
			auto it = m_String->begin() + m_next;
			kutf::Increment(it, m_String->end(), iIncrement - 1);
			m_next = it - m_String->begin();
		}
		return operator++();
	}

	//-----------------------------------------------------------------------------
	self_type& operator-=(std::size_t iDecrement)
	//-----------------------------------------------------------------------------
	{
		if (iDecrement > 0)
		{
			auto it = m_String->begin() + m_next;
			kutf::Decrement(m_String->begin(), it, iDecrement);
			m_next = it - m_String->begin();
		}
		return operator--();
	}

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	void SaveChangedValue()
	//-----------------------------------------------------------------------------
	{
		if (KUTF_LIKELY(m_String != nullptr))
		{
			std::size_t iOrigLen = UTFChars<sizeof(orig_value_type)*8>(m_OrigValue);
			std::size_t iNewLen = UTFChars<sizeof(orig_value_type)*8>(m_Value);

			// create an iterator pointing to the start of the current sequence
			typename UTFString::iterator it = m_String->begin() + m_next - iOrigLen;

			if (KUTF_UNLIKELY(iOrigLen < iNewLen))
			{
				// make room in the string
				m_String->insert(it - m_String->begin(), iNewLen - iOrigLen, ' ');
			}
			else if (KUTF_UNLIKELY(iOrigLen > iNewLen))
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
			ToUTF(m_Value, it);

			// no need to update m_OrigValue with m_Value as this
			// function is only called from the dtor or from operator++()
			// just before the values are updated anyway
		}
	}

	UTFString* m_String { nullptr };
	size_type  m_next;
	value_type m_Value { 0 };
	value_type m_OrigValue { 0 };

};

} // namespace KUTF_NAMESPACE

#ifdef DEKAF2
DEKAF2_NAMESPACE_END
#endif
