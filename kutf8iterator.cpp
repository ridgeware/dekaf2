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

#include "kutf8iterator.h"

namespace dekaf2 {
namespace Unicode {

//-----------------------------------------------------------------------------
UTF8ConstIterator::UTF8ConstIterator(const iterator_base& it, bool bToEnd)
//-----------------------------------------------------------------------------
    : m_next(bToEnd ? it.end() : it.begin())
	, m_end(it.end())
{
	operator++();

} // ctor

//-----------------------------------------------------------------------------
UTF8ConstIterator::self_type& UTF8ConstIterator::operator++()
//-----------------------------------------------------------------------------
{
	m_Value = NextCodepointFromUTF8<iterator_base>(m_next, m_end);
	return *this;

} // prefix

//-----------------------------------------------------------------------------
UTF8ConstIterator::self_type UTF8ConstIterator::operator++(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;
	operator++();
	return i;

} // postfix

//-----------------------------------------------------------------------------
UTF8Iterator::UTF8Iterator(iterator_base& it, bool bToEnd)
//-----------------------------------------------------------------------------
: m_base(&it)
, m_next(bToEnd ? it.end() : it.begin())
{
	operator++();

} // ctor

//-----------------------------------------------------------------------------
UTF8Iterator::self_type& UTF8Iterator::operator++()
//-----------------------------------------------------------------------------
{
	if (m_base)
	{
		SaveChangedValue();
		m_Value = m_OrigValue = NextCodepointFromUTF8<iterator_base>(m_next, m_base->end());
	}
	return *this;

} // prefix

//-----------------------------------------------------------------------------
UTF8Iterator::self_type UTF8Iterator::operator++(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;
	operator++();
	i.m_OrigValue = i.m_Value;
	i.m_postfix = this;
	return i;

} // postfix

//-----------------------------------------------------------------------------
UTF8Iterator::self_type& UTF8Iterator::operator--()
//-----------------------------------------------------------------------------
{
	if (m_base)
	{
		SaveChangedValue();
		// stay at .end() if value is invalid
		if (m_Value != INVALID_CODEPOINT)
		{
			m_next -= UTF8Bytes(m_Value);
		}
		iterator_base::const_iterator hit = m_next;
		m_Value = m_OrigValue = PrevCodepointFromUTF8<iterator_base>(m_next, m_base->begin(), m_base->end());
		m_next = hit;
	}
	return *this;

} // prefix

//-----------------------------------------------------------------------------
UTF8Iterator::self_type UTF8Iterator::operator--(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;
	operator--();
	i.m_OrigValue = i.m_Value;
	i.m_postfix = this;
	return i;

} // postfix

//-----------------------------------------------------------------------------
void UTF8Iterator::SaveChangedValue()
//-----------------------------------------------------------------------------
{
	if (m_Value != m_OrigValue && m_base)
	{
		size_t iOrigLen = UTF8Bytes(m_OrigValue);
		size_t iNewLen = UTF8Bytes(m_Value);

		// create an iterator pointing to the start of the current sequence
		iterator_base::iterator it = const_cast<iterator_base::iterator>(m_next) - iOrigLen;

		if (iOrigLen < iNewLen)
		{
			// make room in the string
			m_base->insert(it - m_base->begin(), iNewLen - iOrigLen, ' ');
		}
		else if (iOrigLen > iNewLen)
		{
			// shorten the string
			m_base->erase(it - m_base->begin(), iOrigLen - iNewLen);
		}

		// adjust m_next to point after end of replaced sequence
		m_next += iNewLen - iOrigLen;

		if (m_postfix)
		{
			// adjust the next iter in the original iterator
			m_postfix->m_next += iNewLen - iOrigLen;
			m_postfix = nullptr;
		}

		// and finally write the replacement sequence
		ToUTF8(m_Value, it);

		// no need to update m_OrigValue with m_Value as this
		// function is only called from the dtor or from operator++()
		// just before the values are updated anyway
	}
}

} // end of namespace Unicode
} // end of namespace dekaf2

