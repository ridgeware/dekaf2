/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include "kcountingstreambuf.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KCountingOutputStreamBuf::~KCountingOutputStreamBuf()
//-----------------------------------------------------------------------------
{
	m_ostream.rdbuf(m_SBuf);
}

//-----------------------------------------------------------------------------
std::streambuf::int_type KCountingOutputStreamBuf::overflow(int_type c)
//-----------------------------------------------------------------------------
{
	if (!traits_type::eq_int_type(traits_type::eof(), c))
	{
		auto ch = m_SBuf->sputc(c);

		if (!traits_type::eq_int_type(traits_type::eof(), ch))
		{
			++m_iCount;
		}

		return ch;
	}
	return c;

} // overflow

//-----------------------------------------------------------------------------
std::streamsize KCountingOutputStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	auto iWrote = m_SBuf->sputn(s, n);
	m_iCount += iWrote;
	return iWrote;

} // xsputn

//-----------------------------------------------------------------------------
int KCountingOutputStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	return m_SBuf->pubsync();

} // sync

} // end of namespace dekaf2

