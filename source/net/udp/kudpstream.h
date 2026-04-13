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

/// @file kudpstream.h
/// provides a std::iostream adapter for UDP with automatic datagram fragmentation

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/net/udp/kudpsocket.h>
#include <dekaf2/core/errors/kerror.h>
#include <streambuf>
#include <iostream>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A std::streambuf that wraps a connected KUDPSocket. Writes are automatically
/// split into datagrams of at most MaxDatagramSize bytes. Reads receive one
/// datagram per underflow() call.
class DEKAF2_PUBLIC KUDPStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construct a UDP streambuf around an existing connected KUDPSocket.
	/// @param Socket the connected UDP socket (must outlive this streambuf)
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KUDPStreamBuf(KUDPSocket& Socket, std::size_t iMaxDatagramSize = KUDPSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KUDPStreamBuf() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Change the maximum datagram size. This flushes any pending write data first.
	/// @param iMaxDatagramSize the new maximum datagram size
	void SetMaxDatagramSize(std::size_t iMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the current maximum datagram size.
	std::size_t GetMaxDatagramSize() const { return m_iMaxDatagramSize; }
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// Write a sequence of characters. Automatically splits into datagrams.
	std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Handle write buffer overflow — sends the current buffer as a datagram.
	int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Flush the write buffer — sends remaining data as a datagram.
	int sync() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a sequence of characters from the next datagram.
	std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Handle read buffer underflow — receives the next datagram.
	int_type underflow() override;
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	bool FlushWriteBuffer();
	void SetupWriteBuffer();

	KUDPSocket&          m_Socket;
	std::size_t          m_iMaxDatagramSize;
	std::vector<char>    m_WriteBuf;
	std::vector<char>    m_ReadBuf;

}; // KUDPStreamBuf


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A std::iostream that wraps a connected KUDPSocket, providing stream-based
/// I/O with automatic datagram fragmentation. Writes are automatically split
/// into datagrams of at most MaxDatagramSize bytes. Each read underflow
/// receives one datagram.
class DEKAF2_PUBLIC KUDPStream : public std::iostream, public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construct an unconnected UDP stream.
	/// @param Timeout I/O timeout
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KUDPStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout(),
	           std::size_t iMaxDatagramSize = KUDPSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a connected UDP stream.
	/// @param Endpoint the remote endpoint to connect to
	/// @param Options stream options (timeout, address family, ..)
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KUDPStream(const KTCPEndPoint& Endpoint,
	           KStreamOptions Options = KStreamOptions{},
	           std::size_t iMaxDatagramSize = KUDPSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KUDPStream();
	//-----------------------------------------------------------------------------

	KUDPStream(const KUDPStream&) = delete;
	KUDPStream& operator=(const KUDPStream&) = delete;

	//-----------------------------------------------------------------------------
	/// Connect to a remote endpoint.
	/// @param Endpoint the remote endpoint
	/// @param Options stream options
	/// @return true on success
	bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect the stream.
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is the stream connected?
	bool is_open() const { return m_Socket.is_open(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is the stream in a good state?
	bool Good() const { return m_Socket.Good(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the underlying KUDPSocket.
	KUDPSocket& GetSocket() { return m_Socket; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the underlying KUDPSocket (const).
	const KUDPSocket& GetSocket() const { return m_Socket; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the maximum datagram size.
	/// @param iMaxDatagramSize the new maximum datagram size
	void SetMaxDatagramSize(std::size_t iMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the current maximum datagram size.
	std::size_t GetMaxDatagramSize() const { return m_StreamBuf.GetMaxDatagramSize(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the I/O timeout.
	void SetTimeout(KDuration Timeout) { m_Socket.SetTimeout(Timeout); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the I/O timeout.
	KDuration GetTimeout() const { return m_Socket.GetTimeout(); }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	KUDPSocket    m_Socket;
	KUDPStreamBuf m_StreamBuf;

}; // KUDPStream


//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KUDPStream> CreateKUDPStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout(),
                                             std::size_t iMaxDatagramSize = KUDPSocket::s_iDefaultMaxDatagramSize);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KUDPStream> CreateKUDPStream(const KTCPEndPoint& EndPoint,
                                             KStreamOptions Options = KStreamOptions{},
                                             std::size_t iMaxDatagramSize = KUDPSocket::s_iDefaultMaxDatagramSize);
//-----------------------------------------------------------------------------


/// @}

DEKAF2_NAMESPACE_END
