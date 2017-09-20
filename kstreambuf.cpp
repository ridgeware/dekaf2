
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

}
