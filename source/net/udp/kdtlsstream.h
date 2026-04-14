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

/// @file kdtlsstream.h
/// provides a std::iostream adapter for DTLS with automatic datagram fragmentation

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/net/udp/kdtlssocket.h>
#include <dekaf2/net/udp/bits/kdatagramstreambuf.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/io/streams/kstream.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

/// A streambuf for DTLS sockets with automatic datagram fragmentation.
using KDTLSStreamBuf = KDatagramStreamBuf<KDTLSSocket>;


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A std::iostream that wraps a connected KDTLSSocket, providing stream-based
/// I/O with automatic datagram fragmentation and DTLS encryption. Writes are
/// automatically split into datagrams of at most MaxDatagramSize bytes. Each
/// read underflow receives one datagram.
class DEKAF2_PUBLIC KDTLSStream : public KReaderWriter<std::iostream>, public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construct an unconnected DTLS stream with a default client context.
	/// @param Timeout I/O timeout
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KDTLSStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout(),
	            std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct an unconnected DTLS stream with an explicit context.
	/// @param Context a KTLSContext created with Transport::DTls
	/// @param Timeout I/O timeout
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KDTLSStream(KTLSContext& Context,
	            KDuration Timeout = KStreamOptions::GetDefaultTimeout(),
	            std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a connected DTLS stream with a default client context.
	/// @param Endpoint the remote endpoint to connect to
	/// @param Options stream options (timeout, address family, ..)
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KDTLSStream(const KTCPEndPoint& Endpoint,
	            KStreamOptions Options = KStreamOptions{},
	            std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a connected DTLS stream with an explicit context.
	/// @param Context a KTLSContext created with Transport::DTls
	/// @param Endpoint the remote endpoint to connect to
	/// @param Options stream options (timeout, address family, ..)
	/// @param iMaxDatagramSize the maximum payload size per datagram
	KDTLSStream(KTLSContext& Context,
	            const KTCPEndPoint& Endpoint,
	            KStreamOptions Options = KStreamOptions{},
	            std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KDTLSStream();
	//-----------------------------------------------------------------------------

	KDTLSStream(const KDTLSStream&) = delete;
	KDTLSStream& operator=(const KDTLSStream&) = delete;

	//-----------------------------------------------------------------------------
	/// Connect to a remote endpoint and perform the DTLS handshake.
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
	/// Get the underlying KDTLSSocket.
	KDTLSSocket& GetSocket() { return m_Socket; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the underlying KDTLSSocket (const).
	const KDTLSSocket& GetSocket() const { return m_Socket; }
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

	KDTLSSocket                       m_Socket;
	KDatagramStreamBuf<KDTLSSocket>   m_StreamBuf;

}; // KDTLSStream


//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KDTLSStream> CreateKDTLSStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout(),
                                                std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KDTLSStream> CreateKDTLSStream(const KTCPEndPoint& EndPoint,
                                                KStreamOptions Options = KStreamOptions{},
                                                std::size_t iMaxDatagramSize = KDTLSSocket::s_iDefaultMaxDatagramSize);
//-----------------------------------------------------------------------------


/// @}

DEKAF2_NAMESPACE_END
