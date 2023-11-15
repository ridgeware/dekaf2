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

#include "kstreambuf.h"
#include <cstring> // memcpy()..
#include <algorithm>

namespace dekaf2 {

//-----------------------------------------------------------------------------
KNullStreamBuf::~KNullStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streambuf::int_type KNullStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	return ch;
}

//-----------------------------------------------------------------------------
std::streamsize KNullStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	return n;
}

//-----------------------------------------------------------------------------
std::streambuf::int_type KNullStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	m_chBuf = 0;
	setg(&m_chBuf, &m_chBuf, &m_chBuf+1);
	return 0;
}

//-----------------------------------------------------------------------------
std::streamsize KNullStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::memset(s, 0, n);
	return n;
}

//-----------------------------------------------------------------------------
KNullStreamBuf& KNullStreamBuf::Create()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<KNullStreamBuf> NullBuf = std::make_unique<KNullStreamBuf>();
	return *NullBuf.get();
}

//-----------------------------------------------------------------------------
KInStreamBuf::~KInStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KInStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iTotal { 0 };

	// read as many chars as possible directly from the stream buffer
	auto iRead = std::min(n, in_avail());

	if (iRead > 0)
	{
		// copy directly from streambuf
		std::memcpy(s, gptr(), static_cast<size_t>(iRead));

		n     -= iRead;
		iTotal = iRead;

		// adjust the stream buffer pointers
		setg(eback(), gptr()+iRead, egptr());
	}

	while (n > 0)
	{
		// advance buffer by read chars
		s += iRead;
		// read remaining chars directly from the callbacḱ function
		iRead = m_CallbackR(s, n, m_CustomPointerR);

		// iRead is 0 on EOF or possibly < 0 on error
		if (iRead <= 0)
		{
			break;
		}

		iTotal += iRead;
		n      -= iRead;
	}

	return iTotal;
}

//-----------------------------------------------------------------------------
KInStreamBuf::int_type KInStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	// call the data provider
	auto rb = m_CallbackR(m_buf.data(), STREAMBUFSIZE, m_CustomPointerR);

	if (rb <= 0)
	{
		// no more characters
		return traits_type::eof();
	}

	// set new read arena
	setg(m_buf.data(), m_buf.data(), m_buf.data()+rb);

	// and return first char to indicate it is not EOF
	return traits_type::to_int_type(m_buf[0]);
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

	while (n >= DIRECTWRITE)
	{
		auto iFilled = FlushableSize();

		if (iFilled >= DIRECTWRITE)
		{
			if (sync())
			{
				// error
				return 0;
			}
		}
		else if (iFilled > 0)
		{
			auto iNeedToFill = DIRECTWRITE - iFilled;

			if (n - iNeedToFill >= DIRECTWRITE)
			{
				// fill buffer with iNeedToFill chars so that it reaches
				// the minimum flush length, then send the remainder of s
				// directly - we use a recursion to achieve this, n is
				// always guaranteed to be < DIRECTWRITE so it will not
				// take this branch
				iWrote = xsputn(s, iNeedToFill);
				if (iWrote != iNeedToFill)
				{
					// error
					return 0;
				}

				s += iNeedToFill;
				n -= iNeedToFill;

				if (sync())
				{
					// error
					return 0;
				}

				// and simply run into the call to base_type::xsputn below
			}
			else
			{
				// jump out of this loop and add s to the buffer.
				break;
			}
		}

		// buffer is empty, just write this fragment to the underlying stream
		return iWrote + base_type::xsputn(s, n);
	}

	while (n > 0)
	{
		auto iAvail = RemainingSize();
		auto iWriteInStreamBuf = std::min(n, iAvail);

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
KBufferedOutStreamBuf::int_type KBufferedOutStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	if (!traits_type::eq_int_type(ch, traits_type::eof()))
	{
		char_type cch = ch;
		return (xsputn(&cch, 1) == 1) ? 0 : traits_type::eof();
	}
	else
	{
		return (sync() == 0) ? 0 : traits_type::eof();
	}

} // overflow

//-----------------------------------------------------------------------------
int KBufferedOutStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	auto iToWrite = FlushableSize();
	decltype(iToWrite) iWrote { 0 };

	if (iToWrite)
	{
		iWrote = base_type::xsputn(m_buf.data(), pptr() - m_buf.data());
		setp(m_buf.data(), m_buf.data()+STREAMBUFSIZE);
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

	while (n >= DIRECTWRITE)
	{
		auto iFilled = FlushableSize();

		if (iFilled >= DIRECTWRITE)
		{
			if (sync())
			{
				// error
				return 0;
			}
		}
		else if (iFilled > 0)
		{
			auto iNeedToFill = DIRECTWRITE - iFilled;

			if (n - iNeedToFill >= DIRECTWRITE)
			{
				// fill buffer with iNeedToFill chars so that it reaches
				// the minimum flush length, then send the remainder of s
				// directly - we use a recursion to achieve this, n is
				// always guaranteed to be < DIRECTWRITE so it will not
				// take this branch
				iWrote = xsputn(s, iNeedToFill);
				if (iWrote != iNeedToFill)
				{
					// error
					return 0;
				}

				s += iNeedToFill;
				n -= iNeedToFill;

				if (sync())
				{
					// error
					return 0;
				}

				// and simply run into the call to base_type::xsputn below
			}
			else
			{
				// jump out of this loop and add s to the buffer.
				break;
			}
		}

		// buffer is empty, just write this fragment to the underlying stream
		return iWrote + base_type::xsputn(s, n);
	}

	while (n > 0)
	{
		auto iAvail = RemainingSize();
		auto iWriteInStreamBuf = std::min(n, iAvail);

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
		auto cch = traits_type::to_char_type(ch);
		return (xsputn(&cch, 1) == 1) ? 0 : traits_type::eof();
	}
	else
	{
		return (sync() == 0) ? 0 : traits_type::eof();
	}

} // overflow

//-----------------------------------------------------------------------------
int KBufferedStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	auto iToWrite = FlushableSize();
	decltype(iToWrite) iWrote { 0 };

	if (iToWrite)
	{
		iWrote = base_type::xsputn(m_buf.data(), iToWrite);
		setp(m_buf.data(), m_buf.data()+STREAMBUFSIZE);
	}

	return (iWrote == iToWrite) ? 0 : -1;

} // sync

}
