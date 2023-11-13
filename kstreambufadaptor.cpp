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

#include "kstreambufadaptor.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KOutStreamBufAdaptor::KOutStreamBufAdaptor(std::ostream& ostream)
//-----------------------------------------------------------------------------
: m_ostream(&ostream)
, m_ParentStreamBuf(m_ostream->rdbuf(this))
{
}

//-----------------------------------------------------------------------------
KOutStreamBufAdaptor::~KOutStreamBufAdaptor()
//-----------------------------------------------------------------------------
{
	if (m_ostream != nullptr)
	{
		m_ostream->rdbuf(m_ParentStreamBuf);
	}

} // dtor

//-----------------------------------------------------------------------------
std::streambuf::int_type KOutStreamBufAdaptor::overflow(int_type c)
//-----------------------------------------------------------------------------
{
	if (!traits_type::eq_int_type(traits_type::eof(), c))
	{
		if (Output(c)) return c;
		return traits_type::eof();
	}
	return c;

} // overflow

//-----------------------------------------------------------------------------
std::streamsize KOutStreamBufAdaptor::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	if (Output(KStringView(s, n))) return n;
	return 0;

} // xsputn

//-----------------------------------------------------------------------------
int KOutStreamBufAdaptor::sync()
//-----------------------------------------------------------------------------
{
	return m_ParentStreamBuf->pubsync();

} // sync

//-----------------------------------------------------------------------------
bool KOutStreamBufAdaptor::Output(KStringView sOut)
//-----------------------------------------------------------------------------
{
	for (auto ch : sOut)
	{
		if (!Output(ch))
		{
			return false;
		}
	}
	return true;

} // Output

std::size_t KOutStreamBufAdaptor::Write(KStringView sWrite)
//-----------------------------------------------------------------------------
{
	auto iWrote = m_ParentStreamBuf->sputn(sWrite.data(), sWrite.size());

	if (iWrote < 0)
	{
		return 0;
	}
	else
	{
		return static_cast<std::size_t>(iWrote);
	}

} // Write



//-----------------------------------------------------------------------------
KInStreamBufAdaptor::KInStreamBufAdaptor(std::istream& istream)
//-----------------------------------------------------------------------------
: m_istream(&istream)
, m_ParentStreamBuf(m_istream->rdbuf(this))
{
}

//-----------------------------------------------------------------------------
KInStreamBufAdaptor::~KInStreamBufAdaptor()
//-----------------------------------------------------------------------------
{
	if (m_istream != nullptr)
	{
		m_istream->rdbuf(m_ParentStreamBuf);
	}

} // dtor

//-----------------------------------------------------------------------------
std::streambuf::int_type KInStreamBufAdaptor::underflow()
//-----------------------------------------------------------------------------
{
	auto ch = m_ParentStreamBuf->sbumpc();

	if (!traits_type::eq_int_type(traits_type::eof(), ch))
	{
		ch = Inspect(ch);
		m_chBuf = ch;
		setg(&m_chBuf, &m_chBuf, &m_chBuf+1);
	}

	return ch;

} // underflow

//-----------------------------------------------------------------------------
std::streamsize KInStreamBufAdaptor::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iExtracted = 0;

	// save the buffer start
	char_type* sBufferStart = s;

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
		auto iRead = m_ParentStreamBuf->sgetn(s, n);
		// iRead is -1 on error
		if (iRead > 0)
		{
			iExtracted += iRead;
		}
	}

	auto iNewSize = Inspect(sBufferStart, iExtracted);

	if (iNewSize > n)
	{
		return n;
	}

	return iNewSize;

} // xsgetn

//-----------------------------------------------------------------------------
std::streambuf::pos_type KInStreamBufAdaptor::seekoff(off_type off,
													  std::ios_base::seekdir dir,
													  std::ios_base::openmode which)
//-----------------------------------------------------------------------------
{
	// invalidate read buffer, if any
	setg(0, 0, 0);
	return m_ParentStreamBuf->pubseekoff(off, dir, which);
}

//-----------------------------------------------------------------------------
std::streamsize KInStreamBufAdaptor::Inspect(char* sBuffer, std::streamsize iSize)
//-----------------------------------------------------------------------------
{
	char* it = sBuffer;
	char* ie = sBuffer + iSize;

	for (; it < ie; ++it)
	{
		*it = Inspect(*it);

		if (traits_type::eq_int_type(traits_type::eof(), *it))
		{
			break;
		}
	}

	return it - sBuffer;
}


} // end of namespace dekaf2

