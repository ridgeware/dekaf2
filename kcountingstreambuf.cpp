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

//-----------------------------------------------------------------------------
KCountingInputStreamBuf::~KCountingInputStreamBuf()
//-----------------------------------------------------------------------------
{
	m_istream.rdbuf(m_SBuf);
}

//-----------------------------------------------------------------------------
std::streambuf::int_type KCountingInputStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	auto ch = m_SBuf->sbumpc();

	if (!traits_type::eq_int_type(traits_type::eof(), ch))
	{
		m_chBuf = ch;
		setg(&m_chBuf, &m_chBuf, &m_chBuf+1);
		++m_iCount;
	}

	return ch;

} // underflow

//-----------------------------------------------------------------------------
std::streamsize KCountingInputStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iExtracted = 0;

	{
		// read as many chars as possible directly from the stream buffer
		iExtracted = std::min(n, in_avail());

		if (iExtracted > 0)
		{
			std::memcpy(s, gptr(), static_cast<size_t>(iExtracted));
			// adjust stream buffer pointers
			setg(eback(), gptr() + iExtracted, egptr());
			// and substract already copied bytes
			n -= iExtracted;
		}
	}

	if (n > 0)
	{
		// advance s by the already copied bytes above (or 0)
		s += iExtracted;
		// read remaining chars directly from the callback function
		auto iRead = m_SBuf->sgetn(s, n);
		// iRead is -1 on error
		if (iRead > 0)
		{
			iExtracted += iRead;
		}
	}

	m_iCount += iExtracted;

	return iExtracted;

} // xsgetn

//-----------------------------------------------------------------------------
std::streambuf::pos_type KCountingInputStreamBuf::seekoff(off_type off,
														  std::ios_base::seekdir dir,
														  std::ios_base::openmode which)
//-----------------------------------------------------------------------------
{
	// invalidate read buffer, if any
	setg(0, 0, 0);
	return m_SBuf->pubseekoff(off, dir, which);
}

} // end of namespace dekaf2

