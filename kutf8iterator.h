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

namespace dekaf2 {
namespace Unicode {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class UTF8ConstIterator
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//-------
public:
//-------
	using iterator_category = std::input_iterator_tag;
	using value_type = codepoint_t;

	using reference = const value_type&;
	using pointer = const value_type*;
	using difference_type = std::ptrdiff_t;
	using self_type = UTF8ConstIterator;
	using iterator_base = KStringView;

	//-----------------------------------------------------------------------------
	/// standalone ctor
	UTF8ConstIterator()
	//-----------------------------------------------------------------------------
	{
		// beware, m_it is a nullptr now
	}

	//-----------------------------------------------------------------------------
	/// ctor for const strings
	UTF8ConstIterator(const iterator_base& it, bool bToEnd);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy constructor
	UTF8ConstIterator(const self_type& other)
	//-----------------------------------------------------------------------------
	: m_next(other.m_next)
	, m_end(other.m_end)
	, m_Value(other.m_Value)
	{
	}

	//-----------------------------------------------------------------------------
	/// move constructor
	UTF8ConstIterator(self_type&& other)
	//-----------------------------------------------------------------------------
	: m_next(std::move(other.m_next))
	, m_end(std::move(other.m_end))
	, m_Value(std::move(other.m_Value))
	{
	}

	//-----------------------------------------------------------------------------
	/// copy assignment
	self_type& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		m_next = other.m_next;
		m_end = other.m_end;
		m_Value = other.m_Value;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		m_next = std::move(other.m_next);
		m_end = std::move(other.m_end);
		m_Value = std::move(other.m_Value);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// prefix increment
	self_type& operator++();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// postfix increment
	self_type operator++(int);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the current value
	reference operator*() const
	//-----------------------------------------------------------------------------
	{
		return m_Value;
	}

	//-----------------------------------------------------------------------------
	/// returns the current value
	pointer operator->() const
	//-----------------------------------------------------------------------------
	{
		return &m_Value;
	}

	//-----------------------------------------------------------------------------
	/// equality operator
	bool operator==(const self_type& rhs) const
	//-----------------------------------------------------------------------------
	{
		// need to check for same value as well, as the end iterator points
		// to the same address, but has an invalid value (-1)
		return m_next == rhs.m_next && m_Value == rhs.m_Value;
	}

	//-----------------------------------------------------------------------------
	/// inequality operator
	bool operator!=(const self_type& rhs) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(rhs);
	}

//-------
protected:
//-------

	iterator_base::const_iterator m_next;
	iterator_base::const_iterator m_end;
	value_type m_Value;

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class UTF8Iterator
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//-------
public:
//-------
	using iterator_category = std::forward_iterator_tag;
	using value_type = codepoint_t;

	using reference = value_type&;
	using pointer = value_type*;
	using difference_type = std::ptrdiff_t;
	using self_type = UTF8Iterator;
	using iterator_base = KString;

	//-----------------------------------------------------------------------------
	/// standalone ctor
	UTF8Iterator()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// ctor for strings
	UTF8Iterator(iterator_base& it, bool bToEnd);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy constructor
	UTF8Iterator(const self_type& other)
	//-----------------------------------------------------------------------------
	: m_base(other.m_base)
	, m_next(other.m_next)
	, m_Value(other.m_Value)
	, m_OrigValue(other.m_OrigValue)
	{
	}

	//-----------------------------------------------------------------------------
	/// move constructor
	UTF8Iterator(self_type&& other)
	//-----------------------------------------------------------------------------
	: m_base(std::move(other.m_base))
	, m_next(std::move(other.m_next))
	, m_Value(std::move(other.m_Value))
	, m_OrigValue(std::move(other.m_OrigValue))
	{
	}

	//-----------------------------------------------------------------------------
	/// dtor
	~UTF8Iterator()
	//-----------------------------------------------------------------------------
	{
		SaveChangedValue();
	}

	//-----------------------------------------------------------------------------
	/// copy assignment
	self_type& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		m_base = other.m_base;
		m_next = other.m_next;
		m_Value = other.m_Value;
		m_OrigValue = other.m_OrigValue;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		m_base = std::move(other.m_base);
		m_next = std::move(other.m_next);
		m_Value = std::move(other.m_Value);
		m_OrigValue = std::move(other.m_OrigValue);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// prefix increment
	self_type& operator++();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// postfix increment
	self_type operator++(int);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// prefix decrement
	self_type& operator--();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// postfix decrement
	self_type operator--(int);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the current value
	reference operator*()
	//-----------------------------------------------------------------------------
	{
		return m_Value;
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
	bool operator==(const self_type& rhs) const
	//-----------------------------------------------------------------------------
	{
		// need to check for same value as well, as the end iterator points
		// to the same address, but has an invalid value (-1)
		return m_next == rhs.m_next && m_Value == rhs.m_Value;
	}

	//-----------------------------------------------------------------------------
	/// inequality operator
	bool operator!=(const self_type& rhs) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(rhs);
	}

//-------
protected:
//-------

	void SaveChangedValue();

	iterator_base* m_base { nullptr };
	iterator_base::const_iterator m_next;
	self_type* m_postfix { 0 };
	value_type m_Value { 0 };
	value_type m_OrigValue { 0 };

};

} // namespace Unicode

} // of namespace dekaf2

