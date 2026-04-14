/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/net/udp/kudpstream.h>
#include <dekaf2/core/logging/klog.h>
#include <cstring>

DEKAF2_NAMESPACE_BEGIN

// ====================== KUDPStreamBuf ======================

//-----------------------------------------------------------------------------
KUDPStreamBuf::KUDPStreamBuf(KUDPSocket& Socket, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: m_Socket(Socket)
, m_iMaxDatagramSize(iMaxDatagramSize)
, m_WriteBuf(iMaxDatagramSize)
, m_ReadBuf(65507)  // max UDP payload
{
	SetupWriteBuffer();
	// read buffer starts empty — underflow() will fill it
	setg(m_ReadBuf.data(), m_ReadBuf.data(), m_ReadBuf.data());
}

//-----------------------------------------------------------------------------
KUDPStreamBuf::~KUDPStreamBuf()
//-----------------------------------------------------------------------------
{
	// flush any remaining write data
	sync();
}

//-----------------------------------------------------------------------------
void KUDPStreamBuf::SetMaxDatagramSize(std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	// flush first
	sync();

	m_iMaxDatagramSize = iMaxDatagramSize;
	m_WriteBuf.resize(iMaxDatagramSize);
	SetupWriteBuffer();

} // SetMaxDatagramSize

//-----------------------------------------------------------------------------
void KUDPStreamBuf::SetupWriteBuffer()
//-----------------------------------------------------------------------------
{
	setp(m_WriteBuf.data(), m_WriteBuf.data() + m_iMaxDatagramSize);

} // SetupWriteBuffer

//-----------------------------------------------------------------------------
bool KUDPStreamBuf::FlushWriteBuffer()
//-----------------------------------------------------------------------------
{
	auto iSize = static_cast<std::streamsize>(pptr() - pbase());

	if (iSize <= 0)
	{
		return true;
	}

	auto iSent = m_Socket.Send(pbase(), iSize);

	// reset the write pointer regardless of success
	SetupWriteBuffer();

	if (iSent < 0)
	{
		kDebug(2, "UDP write failed: {}", m_Socket.Error());
		return false;
	}

	if (iSent != iSize)
	{
		kDebug(2, "UDP short write: sent {} of {} bytes", iSent, iSize);
	}

	return true;

} // FlushWriteBuffer

//-----------------------------------------------------------------------------
std::streamsize KUDPStreamBuf::xsputn(const char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iWritten = 0;

	while (iWritten < n)
	{
		auto iRemaining = static_cast<std::streamsize>(epptr() - pptr());
		auto iChunk     = std::min(n - iWritten, iRemaining);

		if (iChunk > 0)
		{
			std::memcpy(pptr(), s + iWritten, static_cast<std::size_t>(iChunk));
			pbump(static_cast<int>(iChunk));
			iWritten += iChunk;
		}

		// if the write buffer is full, send it as a datagram
		if (pptr() >= epptr())
		{
			if (!FlushWriteBuffer())
			{
				break;
			}
		}
	}

	return iWritten;

} // xsputn

//-----------------------------------------------------------------------------
KUDPStreamBuf::int_type KUDPStreamBuf::overflow(int_type ch)
//-----------------------------------------------------------------------------
{
	// buffer is full — flush as datagram
	if (!FlushWriteBuffer())
	{
		return traits_type::eof();
	}

	if (!traits_type::eq_int_type(ch, traits_type::eof()))
	{
		// put the overflow character into the fresh buffer
		*pptr() = traits_type::to_char_type(ch);
		pbump(1);
	}

	return ch;

} // overflow

//-----------------------------------------------------------------------------
int KUDPStreamBuf::sync()
//-----------------------------------------------------------------------------
{
	return FlushWriteBuffer() ? 0 : -1;

} // sync

//-----------------------------------------------------------------------------
std::streamsize KUDPStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead = 0;

	while (iRead < n)
	{
		auto iAvailable = static_cast<std::streamsize>(egptr() - gptr());

		if (iAvailable <= 0)
		{
			// need more data
			if (underflow() == traits_type::eof())
			{
				break;
			}
			iAvailable = static_cast<std::streamsize>(egptr() - gptr());
		}

		auto iChunk = std::min(n - iRead, iAvailable);

		std::memcpy(s + iRead, gptr(), static_cast<std::size_t>(iChunk));
		gbump(static_cast<int>(iChunk));
		iRead += iChunk;
	}

	return iRead;

} // xsgetn

//-----------------------------------------------------------------------------
KUDPStreamBuf::int_type KUDPStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	// receive the next datagram
	auto iReceived = m_Socket.Receive(m_ReadBuf.data(), static_cast<std::streamsize>(m_ReadBuf.size()));

	if (iReceived <= 0)
	{
		setg(m_ReadBuf.data(), m_ReadBuf.data(), m_ReadBuf.data());
		return traits_type::eof();
	}

	setg(m_ReadBuf.data(), m_ReadBuf.data(), m_ReadBuf.data() + iReceived);

	return traits_type::to_int_type(*gptr());

} // underflow


// ====================== KUDPStream ======================

//-----------------------------------------------------------------------------
KUDPStream::KUDPStream(KDuration Timeout, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Timeout)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
}

//-----------------------------------------------------------------------------
KUDPStream::KUDPStream(const KTCPEndPoint& Endpoint, KStreamOptions Options, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Endpoint, Options)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
	if (!m_Socket.Good())
	{
		SetError(m_Socket.Error());
	}
}

//-----------------------------------------------------------------------------
KUDPStream::~KUDPStream()
//-----------------------------------------------------------------------------
{
	// flush remaining data
	flush();
}

//-----------------------------------------------------------------------------
bool KUDPStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	if (!m_Socket.Connect(Endpoint, Options))
	{
		return SetError(m_Socket.Error());
	}

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KUDPStream::Disconnect()
//-----------------------------------------------------------------------------
{
	flush();
	return m_Socket.Disconnect();

} // Disconnect

//-----------------------------------------------------------------------------
void KUDPStream::SetMaxDatagramSize(std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	m_StreamBuf.SetMaxDatagramSize(iMaxDatagramSize);

} // SetMaxDatagramSize

//-----------------------------------------------------------------------------
std::unique_ptr<KUDPStream> CreateKUDPStream(KDuration Timeout, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUDPStream>(Timeout, iMaxDatagramSize);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KUDPStream> CreateKUDPStream(const KTCPEndPoint& EndPoint, KStreamOptions Options, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUDPStream>(EndPoint, Options, iMaxDatagramSize);
}

DEKAF2_NAMESPACE_END
