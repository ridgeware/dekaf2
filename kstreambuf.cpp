/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
//
*/

#include <cstring>
#include "kstreambuf.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KInStreamBuf::~KInStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KInStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iExtracted = 0;

	{
		// read as many chars as possible directly from the stream buffer
		std::streamsize iReadInStreamBuf = std::min(n, in_avail());
		if (iReadInStreamBuf > 0)
		{
			std::memcpy(s, gptr(), static_cast<size_t>(iReadInStreamBuf));
			s += iReadInStreamBuf;
			n -= iReadInStreamBuf;
			iExtracted = iReadInStreamBuf;
			// adjust stream buffer pointers
			setg(eback(), gptr()+iReadInStreamBuf, egptr());
		}
	}

	if (n > 0)
	{
		// read remaining chars directly from the callbacḱ function
		iExtracted += m_CallbackR(s, n, m_CustomPointerR);
	}

	return iExtracted;
}

//-----------------------------------------------------------------------------
KInStreamBuf::int_type KInStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	std::streamsize rb = m_CallbackR(m_buf, STREAMBUFSIZE, m_CustomPointerR);
	if (rb > 0)
	{
		setg(m_buf, m_buf, m_buf+rb);
		return traits_type::to_int_type(m_buf[0]);
	}
	else
	{
		return traits_type::eof();
	}
}

//-----------------------------------------------------------------------------
KOutStreamBuf::~KOutStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KOutStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	return m_Callback(s, n, m_CustomPointer);
}

//-----------------------------------------------------------------------------
KOutStreamBuf::int_type KOutStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	return static_cast<int_type>(m_Callback(&ch, 1, m_CustomPointer));
}

//-----------------------------------------------------------------------------
KStreamBuf::~KStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	return m_CallbackW(s, n, m_CustomPointerW);
}

//-----------------------------------------------------------------------------
KStreamBuf::int_type KStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	return static_cast<int_type>(m_CallbackW(&ch, 1, m_CustomPointerW));
}





//-----------------------------------------------------------------------------
KBufferedOutStreamBuf::~KBufferedOutStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KBufferedOutStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote { 0 };

	while (n)
	{
		std::streamsize iWriteInStreamBuf = std::min(n, epptr() - pptr());
		if (iWriteInStreamBuf > 0)
		{
			std::memcpy(pptr(), s, static_cast<size_t>(iWriteInStreamBuf));
			s += iWriteInStreamBuf;
			n -= iWriteInStreamBuf;
			iWrote += iWriteInStreamBuf;
			// adjust stream buffer pointers
			setp(pptr()+iWriteInStreamBuf, egptr());
		}

		if (epptr() - pptr() == 0)
		{
			// flush buffer
			if (sync())
			{
				// error
				return 0;
			}
		}
	}

	return iWrote;

} // xsputn

//-----------------------------------------------------------------------------
KBufferedOutStreamBuf::int_type KBufferedOutStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	if (!traits_type::eq_int_type(ch, traits_type::eof()))
	{
		char_type cch = ch;
		return xsputn(&cch, 1);
	}
	else
	{
		sync();
		return 0;
	}

} // overflow

//-----------------------------------------------------------------------------
int KBufferedOutStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	std::streamsize iToWrite = pptr() - m_buf;
	std::streamsize iWrote { 0 };
	if (iToWrite)
	{
		iWrote = base_type::xsputn(m_buf, pptr() - m_buf);
		setp(m_buf, m_buf+STREAMBUFSIZE);
	}

	return (iWrote == iToWrite) ? 0 : -1;

} // sync




//-----------------------------------------------------------------------------
KBufferedStreamBuf::~KBufferedStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KBufferedStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote { 0 };

	while (n)
	{
		std::streamsize iAvail = epptr() - pptr();
		std::streamsize iWriteInStreamBuf = std::min(n, iAvail);
		if (iWriteInStreamBuf > 0)
		{
			std::memcpy(pptr(), s, static_cast<size_t>(iWriteInStreamBuf));
			s += iWriteInStreamBuf;
			n -= iWriteInStreamBuf;
			iWrote += iWriteInStreamBuf;
			// adjust stream buffer pointers
			setp(pptr()+iWriteInStreamBuf, epptr());
		}

		if (epptr() - pptr() == 0)
		{
			// flush buffer
			if (sync())
			{
				// error
				return 0;
			}
		}
	}

	return iWrote;

} // xsputn

//-----------------------------------------------------------------------------
KBufferedStreamBuf::int_type KBufferedStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	if (!traits_type::eq_int_type(ch, traits_type::eof()))
	{
		char_type cch = ch;
		return xsputn(&cch, 1);
	}
	else
	{
		sync();
		return 0;
	}

} // overflow

//-----------------------------------------------------------------------------
int KBufferedStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	std::streamsize iToWrite = pptr() - m_buf;
	std::streamsize iWrote { 0 };
	if (iToWrite)
	{
		iWrote = base_type::xsputn(m_buf, iToWrite);
		setp(m_buf, m_buf+STREAMBUFSIZE);
	}

	return (iWrote == iToWrite) ? 0 : -1;

} // sync


}
