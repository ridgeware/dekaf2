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

		using reference = value_type&;
		using pointer = value_type*;
		using difference_type = std::ptrdiff_t;
		using self_type = UTF8ConstIterator;
		using iterator_base = KStringView;

		//-----------------------------------------------------------------------------
		/// standalone ctor
		inline UTF8ConstIterator()
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
		inline UTF8ConstIterator(const self_type& other)
		//-----------------------------------------------------------------------------
		: m_base(other.m_base)
		, m_next(other.m_next)
		, m_Value(other.m_Value)
		{
		}

		//-----------------------------------------------------------------------------
		/// move constructor
		inline UTF8ConstIterator(self_type&& other)
		//-----------------------------------------------------------------------------
		: m_base(std::move(other.m_base))
		, m_next(std::move(other.m_next))
		, m_Value(std::move(other.m_Value))
		{
		}

		//-----------------------------------------------------------------------------
		/// copy assignment
		inline self_type& operator=(const self_type& other)
		//-----------------------------------------------------------------------------
		{
			m_base = other.m_base;
			m_next = other.m_next;
			m_Value = other.m_Value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		/// move assignment
		inline self_type& operator=(self_type&& other)
		//-----------------------------------------------------------------------------
		{
			m_base = std::move(other.m_base);
			m_next = std::move(other.m_next);
			m_Value = std::move(other.m_Value);
			return *this;
		}

		//-----------------------------------------------------------------------------
		/// postfix increment
		self_type& operator++();
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// prefix increment
		self_type operator++(int);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// returns the current value
		inline reference operator*()
		//-----------------------------------------------------------------------------
		{
			return m_Value;
		}

		//-----------------------------------------------------------------------------
		/// returns the current value
		inline pointer operator->()
		//-----------------------------------------------------------------------------
		{
			return &m_Value;
		}

		//-----------------------------------------------------------------------------
		/// equality operator
		inline bool operator==(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_base == rhs.m_base && m_next == rhs.m_next;
		}

		//-----------------------------------------------------------------------------
		/// inequality operator
		inline bool operator!=(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return !operator==(rhs);
		}

		//-------
	protected:
		//-------

		const iterator_base* m_base { nullptr };
		iterator_base::const_iterator m_next;
		value_type m_Value;

	};

} // namespace Unicode

} // of namespace dekaf2








