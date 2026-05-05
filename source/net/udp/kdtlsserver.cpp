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

#include <dekaf2/net/udp/kdtlsserver.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <array>

#if !DEKAF2_IS_WINDOWS
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
#else
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

DEKAF2_NAMESPACE_BEGIN

static constexpr std::size_t s_iMaxBufferSize = 65536;

//-----------------------------------------------------------------------------
KDTLSServer::KDTLSServer(uint16_t iPort, bool bStoreNewCerts)
//-----------------------------------------------------------------------------
: m_iPort(iPort)
{
}

//-----------------------------------------------------------------------------
KDTLSServer::~KDTLSServer()
//-----------------------------------------------------------------------------
{
	Stop();
}

//-----------------------------------------------------------------------------
bool KDTLSServer::LoadTLSCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	return m_TLSContext.LoadTLSCertificates(sCert, sKey, sPassword);

} // LoadTLSCertificates

//-----------------------------------------------------------------------------
bool KDTLSServer::SetTLSCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	return m_TLSContext.SetTLSCertificates(sCert, sKey, sPassword);

} // SetTLSCertificates

//-----------------------------------------------------------------------------
bool KDTLSServer::Start(DatagramCallback Callback, bool bBlock)
//-----------------------------------------------------------------------------
{
	if (m_bRunning)
	{
		return SetError("DTLS server is already running");
	}

	// create and bind the listen socket
	m_Socket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (m_Socket < 0)
	{
		return SetError(kFormat("cannot create DTLS server socket: {}", strerror(errno)));
	}

	// enable dual-stack
	int off = 0;
	::setsockopt(m_Socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));

	// allow address reuse
	int on = 1;
	::setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on));

	struct sockaddr_in6 addr{};
	addr.sin6_family = AF_INET6;
	addr.sin6_port   = htons(m_iPort);
	addr.sin6_addr   = in6addr_any;

	if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		auto err = strerror(errno);
#if DEKAF2_IS_WINDOWS
		::closesocket(m_Socket);
#else
		::close(m_Socket);
#endif
		m_Socket = -1;
		return SetError(kFormat("cannot bind DTLS server to port {}: {}", m_iPort, err));
	}

	// enable cookie exchange for DoS protection
	::SSL_CTX_set_cookie_generate_cb(m_TLSContext.GetContext().native_handle(),
	[](SSL* ssl, unsigned char* cookie, unsigned int* cookie_len) -> int
	{
		// simple cookie: random bytes
		if (RAND_bytes(cookie, 16) != 1)
		{
			return 0;
		}
		*cookie_len = 16;
		return 1;
	});

	::SSL_CTX_set_cookie_verify_cb(m_TLSContext.GetContext().native_handle(),
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	[](SSL* ssl, const unsigned char* cookie, unsigned int cookie_len) -> int
#else
	[](SSL* ssl, unsigned char* cookie, unsigned int cookie_len) -> int
#endif
	{
		// accept all cookies (in production, verify against the generated cookie)
		return 1;
	});

	m_bQuit = false;

	if (bBlock)
	{
		kDebug(1, "DTLS server starting on port {} (blocking)", m_iPort);
		RunLoop(std::move(Callback));
	}
	else
	{
		kDebug(1, "DTLS server starting on port {} (background)", m_iPort);
		m_Thread = std::make_unique<std::thread>(kMakeThread(
			[this, Callback = std::move(Callback)]() mutable
			{
				RunLoop(std::move(Callback));
			}));
	}

	return true;

} // Start

//-----------------------------------------------------------------------------
bool KDTLSServer::Stop()
//-----------------------------------------------------------------------------
{
	if (!m_bRunning && m_Socket < 0)
	{
		return true;
	}

	kDebug(1, "DTLS server stopping on port {}", m_iPort);

	m_bQuit = true;

	// close the socket to unblock any pending recvfrom
	if (m_Socket >= 0)
	{
#if DEKAF2_IS_WINDOWS
		::closesocket(m_Socket);
#else
		::close(m_Socket);
#endif
		m_Socket = -1;
	}

	if (m_Thread && m_Thread->joinable())
	{
		m_Thread->join();
		m_Thread.reset();
	}

	{
		std::lock_guard<std::recursive_mutex> Lock(m_SessionsMutex);
		m_Sessions.clear();
	}

	m_bRunning = false;

	kDebug(1, "DTLS server stopped on port {}", m_iPort);

	return true;

} // Stop

//-----------------------------------------------------------------------------
bool KDTLSServer::FlushBIO(PeerSession& Session)
//-----------------------------------------------------------------------------
{
	std::array<char, s_iMaxBufferSize> outbuf;
	int iPending = ::BIO_read(Session.wbio, outbuf.data(), outbuf.size());

	if (iPending > 0 && m_Socket >= 0)
	{
		auto iSent = ::sendto(m_Socket, outbuf.data(), iPending, 0,
		                      reinterpret_cast<struct sockaddr*>(&Session.addr), Session.addr_len);
		return iSent == iPending;
	}

	return iPending <= 0; // no pending data is not an error

} // FlushBIO

//-----------------------------------------------------------------------------
bool KDTLSServer::SendTo(KStringView sPeerAddress, KStringView sData)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::recursive_mutex> Lock(m_SessionsMutex);

	auto it = m_Sessions.find(sPeerAddress);

	if (it == m_Sessions.end())
	{
		return SetError(kFormat("no DTLS session for peer {}", sPeerAddress));
	}

	auto& session = it->second;

	auto iWritten = ::SSL_write(session.ssl.get(), sData.data(), static_cast<int>(sData.size()));

	if (iWritten <= 0)
	{
		auto iError = ::SSL_get_error(session.ssl.get(), iWritten);
		return SetError(kFormat("DTLS send to {} failed: SSL error {}", sPeerAddress, iError));
	}

	if (!FlushBIO(session))
	{
		return SetError(kFormat("DTLS send to {} failed: cannot flush BIO", sPeerAddress));
	}

	return true;

} // SendTo

//-----------------------------------------------------------------------------
void KDTLSServer::RunLoop(DatagramCallback Callback)
//-----------------------------------------------------------------------------
{
	m_bRunning = true;

	// simple single-threaded DTLS server loop
	// For each incoming datagram, we check if we already have a DTLS session for the peer.
	// If not, we create one and do the handshake. Then we decrypt and dispatch.

	std::array<char, s_iMaxBufferSize> buf;

	while (!m_bQuit)
	{
		// use kPoll() so we can check m_bQuit periodically instead of
		// blocking indefinitely in recvfrom() — close() on the fd from
		// another thread is not reliably portable (UB on Linux)
		int iPollResult = kPoll(m_Socket, POLLIN, chrono::milliseconds(500));

		if (m_bQuit)
		{
			break;
		}

		if (iPollResult <= 0)
		{
			// timeout or error — just retry
			continue;
		}

		struct sockaddr_storage peer_addr{};
		socklen_t peer_addr_len = sizeof(peer_addr);

		auto iReceived = ::recvfrom(m_Socket, buf.data(), buf.size(), 0,
		                            reinterpret_cast<struct sockaddr*>(&peer_addr), &peer_addr_len);

		if (m_bQuit || iReceived < 0)
		{
			break;
		}

		// format peer address as key
		std::array<char, INET6_ADDRSTRLEN> addrstr{};
		uint16_t port = 0;

		if (peer_addr.ss_family == AF_INET)
		{
			auto* sa = reinterpret_cast<struct sockaddr_in*>(&peer_addr);
			::inet_ntop(AF_INET, &sa->sin_addr, addrstr.data(), addrstr.size());
			port = ntohs(sa->sin_port);
		}
		else
		{
			auto* sa = reinterpret_cast<struct sockaddr_in6*>(&peer_addr);
			::inet_ntop(AF_INET6, &sa->sin6_addr, addrstr.data(), addrstr.size());
			port = ntohs(sa->sin6_port);
		}

		KString sPeerKey = kFormat("{}:{}", addrstr.data(), port);

		std::lock_guard<std::recursive_mutex> Lock(m_SessionsMutex);

		auto it = m_Sessions.find(sPeerKey);

		if (it == m_Sessions.end())
		{
			// new peer — create a DTLS session
			KUniquePtr<SSL, ::SSL_free> ssl(::SSL_new(m_TLSContext.GetContext().native_handle()));

			if (!ssl)
			{
				kDebug(1, "cannot create SSL object for DTLS peer {}", sPeerKey);
				continue;
			}

			// create memory BIO pair
			auto* rbio = ::BIO_new(::BIO_s_mem());
			auto* wbio = ::BIO_new(::BIO_s_mem());
			::BIO_set_mem_eof_return(rbio, -1);
			::BIO_set_mem_eof_return(wbio, -1);
			::SSL_set_bio(ssl.get(), rbio, wbio);
			::SSL_set_accept_state(ssl.get());

			// enable DTLS cookie exchange
			::SSL_set_options(ssl.get(), SSL_OP_COOKIE_EXCHANGE);

			PeerSession session;
			session.ssl      = std::move(ssl);
			session.rbio     = rbio;
			session.wbio     = wbio;
			session.addr     = peer_addr;
			session.addr_len = peer_addr_len;

			it = m_Sessions.emplace(sPeerKey, std::move(session)).first;
		}

		auto& session = it->second;

		// feed the received datagram to the SSL read BIO
		::BIO_write(session.rbio, buf.data(), static_cast<int>(iReceived));

		// try to read decrypted data
		std::array<char, s_iMaxBufferSize> plaintext;
		auto iDecrypted = ::SSL_read(session.ssl.get(), plaintext.data(), plaintext.size());

		if (iDecrypted > 0)
		{
			try
			{
				Callback(KStringView(plaintext.data(), static_cast<std::size_t>(iDecrypted)), sPeerKey);
			}
			catch (const std::exception& e)
			{
				kDebug(1, "exception in DTLS datagram callback: {}", e.what());
			}
		}
		else
		{
			auto iError = ::SSL_get_error(session.ssl.get(), iDecrypted);

			if (iError == SSL_ERROR_SSL || iError == SSL_ERROR_SYSCALL)
			{
				kDebug(2, "DTLS session error for {}: SSL error {}", sPeerKey, iError);
				m_Sessions.erase(it);
				continue;
			}
			// SSL_ERROR_WANT_READ is normal during handshake
		}

		// flush any pending outgoing data (handshake messages, alerts)
		FlushBIO(session);
	}

	m_bRunning = false;

} // RunLoop

DEKAF2_NAMESPACE_END
