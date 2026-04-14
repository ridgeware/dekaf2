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

/// @file kdtlssocket.h
/// provides a DTLS-encrypted UDP socket with timeout support

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/time/duration/kduration.h>

typedef struct ssl_st SSL;
typedef struct bio_st BIO;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A DTLS-encrypted UDP socket. Uses OpenSSL's DTLS implementation directly
/// (not through boost::asio's SSL layer, which does not support DTLS).
/// The socket handles DTLS handshake, send, and receive with timeout support.
class DEKAF2_PUBLIC KDTLSSocket : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr std::size_t s_iDefaultMaxDatagramSize = 1472; // 1500 MTU - 20 IP - 8 UDP

	//-----------------------------------------------------------------------------
	/// Construct a DTLS client socket with a default client context.
	/// @param Timeout I/O timeout
	KDTLSSocket(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a DTLS socket with an explicit context.
	/// @param Context a KTLSContext created with Transport::DTls
	/// @param Timeout I/O timeout
	KDTLSSocket(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a connected DTLS client socket.
	/// @param Endpoint the remote endpoint to connect to
	/// @param Options stream options (timeout, address family, ..)
	KDTLSSocket(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a connected DTLS client socket with an explicit context.
	/// @param Context a KTLSContext created with Transport::DTls
	/// @param Endpoint the remote endpoint to connect to
	/// @param Options stream options (timeout, address family, ..)
	KDTLSSocket(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KDTLSSocket();
	//-----------------------------------------------------------------------------

	KDTLSSocket(const KDTLSSocket&) = delete;
	KDTLSSocket& operator=(const KDTLSSocket&) = delete;
	KDTLSSocket(KDTLSSocket&&) = delete;
	KDTLSSocket& operator=(KDTLSSocket&&) = delete;

	//-----------------------------------------------------------------------------
	/// Connect to a remote endpoint and perform the DTLS handshake.
	/// @param Endpoint the remote endpoint
	/// @param Options stream options
	/// @return true on success
	bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect and shut down DTLS.
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send encrypted data.
	/// @param sData the data to send
	/// @return the number of bytes sent, or -1 on error
	std::streamsize Send(KStringView sData);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send encrypted data.
	/// @param pBuffer pointer to the data buffer
	/// @param iCount number of bytes to send
	/// @return the number of bytes sent, or -1 on error
	std::streamsize Send(const void* pBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive and decrypt a datagram.
	/// @param pBuffer pointer to the receive buffer
	/// @param iCount maximum bytes to receive
	/// @return the number of bytes received, or -1 on error
	std::streamsize Receive(void* pBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive and decrypt a datagram into a string.
	/// @param iMaxSize maximum datagram size
	/// @return the received data (empty on error)
	KString Receive(std::size_t iMaxSize = 65507);
	//-----------------------------------------------------------------------------

	// --- state and options ---

	//-----------------------------------------------------------------------------
	/// Is the socket connected and the DTLS handshake complete?
	bool is_open() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is the socket in a good state?
	bool Good() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout.
	void SetTimeout(KDuration Timeout) { m_Timeout = Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the current I/O timeout.
	KDuration GetTimeout() const { return m_Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the maximum datagram size.
	std::size_t GetMaxDatagramSize() const { return m_iMaxDatagramSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the maximum datagram size.
	void SetMaxDatagramSize(std::size_t iSize) { m_iMaxDatagramSize = iSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the endpoint.
	const KTCPEndPoint& GetEndPoint() const { return m_Endpoint; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the underlying native socket file descriptor (or -1 if not connected).
	int GetNativeSocket() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the native OpenSSL SSL handle.
	SSL* GetNativeSSLHandle() { return m_SSL.get(); }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	bool DoHandshake();
	void Cleanup();

	KTLSContext*                m_pContext         { nullptr };
	KUniquePtr<SSL, ::SSL_free> m_SSL;
	BIO*                        m_BIO              { nullptr };
	int                         m_Socket           { -1      };
	KTLSContext                 m_OwnContext       { false, KTLSContext::Transport::DTls };
	KTCPEndPoint                m_Endpoint;
	KDuration                   m_Timeout;
	std::size_t                 m_iMaxDatagramSize { s_iDefaultMaxDatagramSize };

}; // KDTLSSocket

/// @}

DEKAF2_NAMESPACE_END
