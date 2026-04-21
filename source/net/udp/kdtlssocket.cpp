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

#include <dekaf2/net/udp/kdtlssocket.h>
#include <dekaf2/net/address/kresolve.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/time/duration/kduration.h>

#if !DEKAF2_IS_WINDOWS
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <poll.h>
#else
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KDTLSSocket::KDTLSSocket(KDuration Timeout)
//-----------------------------------------------------------------------------
: m_pContext(&m_OwnContext)
, m_Timeout(Timeout)
{
}

//-----------------------------------------------------------------------------
KDTLSSocket::KDTLSSocket(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
: m_pContext(&Context)
, m_Timeout(Timeout)
{
}

//-----------------------------------------------------------------------------
KDTLSSocket::KDTLSSocket(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: m_pContext(&m_OwnContext)
, m_Timeout(Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KDTLSSocket::KDTLSSocket(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: m_pContext(&Context)
, m_Timeout(Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KDTLSSocket::~KDTLSSocket()
//-----------------------------------------------------------------------------
{
	Disconnect();
}

//-----------------------------------------------------------------------------
void KDTLSSocket::Cleanup()
//-----------------------------------------------------------------------------
{
	if (m_SSL)
	{
		m_SSL.reset(); // SSL_free also frees the BIO
		m_BIO = nullptr;
	}

	if (m_Socket >= 0)
	{
#if DEKAF2_IS_WINDOWS
		::closesocket(m_Socket);
#else
		::close(m_Socket);
#endif
		m_Socket = -1;
	}

} // Cleanup

//-----------------------------------------------------------------------------
bool KDTLSSocket::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	Cleanup();

	m_Timeout  = Options.GetTimeout();
	m_Endpoint = Endpoint;

	auto& sHostname = Endpoint.Domain.get();
	auto& sPort     = Endpoint.Port.get();

	// resolve the hostname
	boost::asio::io_service IOService(1);
	boost::system::error_code ec;

	auto hosts = KResolve::ResolveUDP(sHostname, sPort, Options.GetFamily(), IOService, ec);

	if (ec)
	{
		return SetError(kFormat("cannot resolve {}: {}", Endpoint, ec.message()));
	}

#if (DEKAF2_CLASSIC_ASIO)
	// Boost < 1.66: resolve() returns a forward iterator directly; the range
	// is terminated by a default-constructed iterator.
	auto it = hosts;
	decltype(it) ie;
#else
	auto it = hosts.begin();
	auto ie = hosts.end();
#endif

	if (it == ie)
	{
		return SetError(kFormat("no addresses found for {}", Endpoint));
	}

	// basic_resolver_entry::endpoint() exists in both classic (Boost < 1.66)
	// and modern asio; dereferencing the iterator directly yields the entry,
	// not the endpoint.
	auto resolved_endpoint = it->endpoint();

	// create the native socket
	int family = (resolved_endpoint.protocol() == boost::asio::ip::udp::v4()) ? AF_INET : AF_INET6;

	m_Socket = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);

	if (m_Socket < 0)
	{
		return SetError(kFormat("cannot create UDP socket for DTLS: {}", strerror(errno)));
	}

	// connect the UDP socket to the remote endpoint (the asio endpoint provides the sockaddr)
	if (::connect(m_Socket, resolved_endpoint.data(), static_cast<socklen_t>(resolved_endpoint.size())) < 0)
	{
		Cleanup();
		return SetError(kFormat("cannot connect DTLS to {}: {}", Endpoint, strerror(errno)));
	}

	// create SSL object from the DTLS context
	m_SSL.reset(::SSL_new(m_pContext->GetContext().native_handle()));

	if (!m_SSL)
	{
		Cleanup();
		return SetError("cannot create DTLS SSL object");
	}

	// create a datagram BIO from the connected socket
	m_BIO = ::BIO_new_dgram(m_Socket, BIO_NOCLOSE);

	if (!m_BIO)
	{
		Cleanup();
		return SetError("cannot create DTLS BIO");
	}

	// set the timeout on the BIO
	auto tv = m_Timeout.to_timeval();
	::BIO_ctrl(m_BIO, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &tv);
	::BIO_ctrl(m_BIO, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &tv);

	::SSL_set_bio(m_SSL.get(), m_BIO, m_BIO);

	// perform the DTLS handshake
	if (!DoHandshake())
	{
		Cleanup();
		return false;
	}

	kDebug(2, "DTLS connected to {}", Endpoint);

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KDTLSSocket::DoHandshake()
//-----------------------------------------------------------------------------
{
	// Retry SSL_connect until it completes, fails with a real error, or the
	// overall connect timeout elapses. The datagram BIO has its own per-read
	// timeout equal to m_Timeout, but a single flight lost in the DTLS
	// handshake can cause BIO_read to return -1 with SSL_ERROR_WANT_READ;
	// in that case we must call DTLSv1_handle_timeout() so OpenSSL can
	// retransmit the previous flight, then call SSL_connect() again.
	// This matches the OpenSSL-documented DTLS client handshake pattern and
	// is required on some older OpenSSL versions (e.g. 1.1.0) where the
	// single-shot variant returned early.
	const auto tDeadline = KSteadyTime::now() + m_Timeout;

	for (;;)
	{
		auto iResult = ::SSL_connect(m_SSL.get());

		if (iResult == 1)
		{
			kDebug(2, "DTLS handshake completed with {}", m_Endpoint);
			return true;
		}

		auto iError = ::SSL_get_error(m_SSL.get(), iResult);

		if (iError == SSL_ERROR_WANT_READ || iError == SSL_ERROR_WANT_WRITE)
		{
			if (KSteadyTime::now() >= tDeadline)
			{
				return SetError(kFormat("DTLS handshake timed out with {} after {}", m_Endpoint, m_Timeout));
			}

			// Poke the DTLS retransmit timer. Safe to call unconditionally —
			// it is a no-op when no retransmit is pending.
			::DTLSv1_handle_timeout(m_SSL.get());
			continue;
		}

		// Real error — extract OpenSSL's error string so the caller gets
		// something actionable instead of a bare "SSL error N".
		char sOpenSSL[256] = {};
		auto ulErr = ::ERR_peek_last_error();

		if (ulErr)
		{
			::ERR_error_string_n(ulErr, sOpenSSL, sizeof(sOpenSSL));
		}

		return SetError(kFormat("DTLS handshake failed with {}: SSL error {}{}{}",
		                        m_Endpoint, iError,
		                        ulErr ? " - " : "",
		                        ulErr ? sOpenSSL : ""));
	}

} // DoHandshake

//-----------------------------------------------------------------------------
bool KDTLSSocket::Disconnect()
//-----------------------------------------------------------------------------
{
	if (m_SSL)
	{
		::SSL_shutdown(m_SSL.get());
		kDebug(2, "DTLS disconnected from {}", m_Endpoint);
	}

	Cleanup();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
std::streamsize KDTLSSocket::Send(const void* pBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (!m_SSL)
	{
		SetError("DTLS socket not connected");
		return -1;
	}

	auto iSent = ::SSL_write(m_SSL.get(), pBuffer, static_cast<int>(iCount));

	if (iSent <= 0)
	{
		auto iError = ::SSL_get_error(m_SSL.get(), iSent);
		SetError(kFormat("DTLS send failed: SSL error {}", iError));
		return -1;
	}

	return static_cast<std::streamsize>(iSent);

} // Send

//-----------------------------------------------------------------------------
std::streamsize KDTLSSocket::Send(KStringView sData)
//-----------------------------------------------------------------------------
{
	return Send(sData.data(), static_cast<std::streamsize>(sData.size()));

} // Send

//-----------------------------------------------------------------------------
std::streamsize KDTLSSocket::Receive(void* pBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (!m_SSL)
	{
		SetError("DTLS socket not connected");
		return -1;
	}

	auto iReceived = ::SSL_read(m_SSL.get(), pBuffer, static_cast<int>(iCount));

	if (iReceived <= 0)
	{
		auto iError = ::SSL_get_error(m_SSL.get(), iReceived);

		if (iError == SSL_ERROR_ZERO_RETURN)
		{
			// peer closed
			return 0;
		}

		if (iError == SSL_ERROR_WANT_READ || iError == SSL_ERROR_WANT_WRITE)
		{
			// timeout
			SetError(kFormat("DTLS receive timed out after {}", m_Timeout));
		}
		else
		{
			SetError(kFormat("DTLS receive failed: SSL error {}", iError));
		}

		return -1;
	}

	return static_cast<std::streamsize>(iReceived);

} // Receive

//-----------------------------------------------------------------------------
KString KDTLSSocket::Receive(std::size_t iMaxSize)
//-----------------------------------------------------------------------------
{
	KString sBuffer;
	sBuffer.resize_uninitialized(iMaxSize);

	auto iReceived = Receive(sBuffer.data(), static_cast<std::streamsize>(iMaxSize));

	if (iReceived <= 0)
	{
		return {};
	}

	sBuffer.resize(static_cast<std::size_t>(iReceived));

	return sBuffer;

} // Receive

//-----------------------------------------------------------------------------
bool KDTLSSocket::is_open() const
//-----------------------------------------------------------------------------
{
	return m_SSL.get() != nullptr && m_Socket >= 0;

} // is_open

//-----------------------------------------------------------------------------
bool KDTLSSocket::Good() const
//-----------------------------------------------------------------------------
{
	return is_open() && !HasError();

} // Good

//-----------------------------------------------------------------------------
int KDTLSSocket::GetNativeSocket() const
//-----------------------------------------------------------------------------
{
	return m_Socket;

} // GetNativeSocket

DEKAF2_NAMESPACE_END
