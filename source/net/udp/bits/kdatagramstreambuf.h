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

#pragma once

/// @file bits/kdatagramstreambuf.h
/// provides a generic std::streambuf adapter for datagram sockets with automatic fragmentation

#include <dekaf2/core/init/kdefinitions.h>
#include <streambuf>
#include <vector>
#include <cstring>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A std::streambuf that wraps a connected datagram socket (KUDPSocket or
/// KDTLSSocket). Writes are automatically split into datagrams of at most
/// MaxDatagramSize bytes. Reads receive one datagram per underflow() call.
///
/// The Socket type must provide:
///   - std::streamsize Send(const void*, std::streamsize)
///   - std::streamsize Receive(void*, std::streamsize)
///   - KStringView Error() const
///   - static constexpr std::size_t s_iDefaultMaxDatagramSize
template<class Socket>
class DEKAF2_PUBLIC KDatagramStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construct a datagram streambuf around an existing connected socket.
	/// @param Socket the connected datagram socket (must outlive this streambuf)
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KDatagramStreamBuf(Socket& socket, std::size_t iMaxDatagramSize = Socket::s_iDefaultMaxDatagramSize)
	//-----------------------------------------------------------------------------
	: m_Socket(socket)
	, m_iMaxDatagramSize(iMaxDatagramSize)
	, m_WriteBuf(iMaxDatagramSize)
	, m_ReadBuf(65507)  // max UDP payload
	{
		SetupWriteBuffer();
		// read buffer starts empty — underflow() will fill it
		setg(m_ReadBuf.data(), m_ReadBuf.data(), m_ReadBuf.data());
	}

	//-----------------------------------------------------------------------------
	~KDatagramStreamBuf() override
	//-----------------------------------------------------------------------------
	{
		// flush any remaining write data
		sync();
	}

	//-----------------------------------------------------------------------------
	/// Change the maximum datagram size. This flushes any pending write data first.
	/// @param iMaxDatagramSize the new maximum datagram size
	void SetMaxDatagramSize(std::size_t iMaxDatagramSize)
	//-----------------------------------------------------------------------------
	{
		// flush first
		sync();

		m_iMaxDatagramSize = iMaxDatagramSize;
		m_WriteBuf.resize(iMaxDatagramSize);
		SetupWriteBuffer();
	}

	//-----------------------------------------------------------------------------
	/// Get the current maximum datagram size.
	std::size_t GetMaxDatagramSize() const { return m_iMaxDatagramSize; }
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// Write a sequence of characters. Automatically splits into datagrams.
	std::streamsize xsputn(const char_type* s, std::streamsize n) override
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
	}

	//-----------------------------------------------------------------------------
	/// Handle write buffer overflow — sends the current buffer as a datagram.
	int_type overflow(int_type ch) override
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
	}

	//-----------------------------------------------------------------------------
	/// Flush the write buffer — sends remaining data as a datagram.
	int sync() override
	//-----------------------------------------------------------------------------
	{
		return FlushWriteBuffer() ? 0 : -1;
	}

	//-----------------------------------------------------------------------------
	/// Read a sequence of characters from the next datagram.
	std::streamsize xsgetn(char_type* s, std::streamsize n) override
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
	}

	//-----------------------------------------------------------------------------
	/// Handle read buffer underflow — receives the next datagram.
	int_type underflow() override
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
	}

//----------
private:
//----------

	bool FlushWriteBuffer()
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
			return false;
		}

		return true;
	}

	void SetupWriteBuffer()
	{
		setp(m_WriteBuf.data(), m_WriteBuf.data() + m_iMaxDatagramSize);
	}

	Socket&              m_Socket;
	std::size_t          m_iMaxDatagramSize;
	std::vector<char>    m_WriteBuf;
	std::vector<char>    m_ReadBuf;

}; // KDatagramStreamBuf

/// @}

DEKAF2_NAMESPACE_END
