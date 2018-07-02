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
    : m_base(bToEnd ? nullptr : &it)
{
	if (m_base != nullptr)
	{
		m_next = it.begin();
		m_Value = CodepointFromUTF8<iterator_base>(m_next, m_base->end());
		if (m_Value == INVALID_CODEPOINT)
		{
			m_base = nullptr;
		}
	}
	else
	{
		m_next = it.end();
	}

} // ctor

//-----------------------------------------------------------------------------
UTF8ConstIterator::self_type& UTF8ConstIterator::operator++()
//-----------------------------------------------------------------------------
{
	if (m_base != nullptr)
	{
		m_Value = CodepointFromUTF8<iterator_base>(m_next, m_base->end());
		if (m_Value == INVALID_CODEPOINT)
		{
			m_base = nullptr;
		}
	}

	return *this;

} // prefix

//-----------------------------------------------------------------------------
UTF8ConstIterator::self_type UTF8ConstIterator::operator++(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;

	if (m_base != nullptr)
	{
		m_Value = CodepointFromUTF8<iterator_base>(m_next, m_base->end());
		if (m_Value == INVALID_CODEPOINT)
		{
			m_base = nullptr;
		}
	}

	return i;

} // postfix

} // end of namespace Unicode
} // end of namespace dekaf2

