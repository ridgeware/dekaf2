/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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


#include "kiostreamsocket.h"
#include <dekaf2/kurlencode.h>
#include <dekaf2/kunixstream.h>
#include <dekaf2/ktcpstream.h>
#include "ktlsstream.h"
#include <dekaf2/kquicstream.h>
#include <dekaf2/kpoll.h>
#include <openssl/ssl.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KIOStreamSocket::~KIOStreamSocket()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KIOStreamSocket::SetSSLError()
//-----------------------------------------------------------------------------
{
	auto SSL = GetNativeTLSHandle();

	if (SSL)
	{
		switch (::SSL_get_error(SSL, 0))
		{
			case SSL_ERROR_ZERO_RETURN:
				/* Normal completion of the stream */
				return SetError("");

			case SSL_ERROR_SSL:
				/*
				 * Some stream fatal error occurred. This could be because of a stream
				 * reset - or some failure occurred on the underlying connection.
				 */
#ifdef DEKAF2_HAS_OPENSSL_QUIC
				switch (::SSL_get_stream_read_state(SSL))
				{
					case SSL_STREAM_STATE_RESET_REMOTE:
						/* The stream has been reset but the connection is still healthy. */
						return SetError("Stream reset occurred");

					case SSL_STREAM_STATE_CONN_CLOSED:
						/* Connection is already closed. Skip SSL_shutdown() */
						return SetError("Connection closed");

					default:
						return SetError("Unknown stream failure");
				}
#else
				return SetError("Unknown stream failure");
#endif
				break;

			default:
				/* Some other unexpected error occurred */
				return SetError("Other OpenSSL error");
		}
	}

	return SetError("");

} // SetSSLError

//-----------------------------------------------------------------------------
int KIOStreamSocket::CheckIfReady(int what, KDuration Timeout, bool bTimeoutIsAnError)
//-----------------------------------------------------------------------------
{
	if (what & POLLIN)
	{
		if (rdbuf())
		{
			auto iInStreambuf = rdbuf()->in_avail();

			if (iInStreambuf > 0)
			{
				kDebug(3, "have bytes in streambuf: {}", iInStreambuf);
				return POLLIN;
			}
		}

		if (IsTLS())
		{
			auto SSL = GetNativeTLSHandle();

			if (SSL)
			{
				auto iReady = ::SSL_pending(SSL);

				if (iReady > 0)
				{
					kDebug(3, "have SSL bytes: {}", iReady);
					return POLLIN;
				}

				// SSL_pending() only reports already-decrypted application data.
				// SSL's internal read BIO may contain complete TLS records that
				// were read from the socket during a previous SSL_read() but have
				// not been decrypted yet. In that state SSL_pending() returns 0
				// and kPoll() also returns 0 (the socket buffer is empty), causing
				// a deadlock. Check for any buffered record data before falling
				// through to kPoll().
				//
				// In OpenSSL's BIO pair architecture, data can be buffered at
				// two levels below SSL_pending():
				// 1. The SSL record layer's internal read buffer (rbuf) - detected
				//    by SSL_has_pending() (OpenSSL >= 1.1.0)
				// 2. The BIO pair buffer (int_bio, accessed via SSL_get_rbio()) -
				//    data written by boost::asio from socket reads but not yet
				//    consumed by the record layer. Detected by BIO_ctrl_pending().
				// Both checks are needed because ssl3_read_n may not drain the
				// entire BIO into rbuf if rbuf already holds data from a previous
				// read-ahead.
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
				if (::SSL_has_pending(SSL) || (::SSL_get_rbio(SSL) && ::BIO_ctrl_pending(::SSL_get_rbio(SSL)) > 0))
				{
					kDebug(3, "have unprocessed SSL record or BIO data");
					return POLLIN;
				}
#else
				auto rbio = ::SSL_get_rbio(SSL);

				if (rbio && ::BIO_ctrl_pending(rbio) > 0)
				{
					kDebug(3, "have unprocessed SSL BIO data");
					return POLLIN;
				}
#endif
			}
		}
	}

	auto iResult = kPoll(GetNativeSocket(), what, Timeout);

	if (iResult == 0)
	{
		// timed out, no events
		if (bTimeoutIsAnError)
		{
			SetError(kFormat("connection with {} timed out after {}", GetEndPoint().empty() ? GetEndPointAddress() : GetEndPoint(), Timeout));
		}
		return 0;
	}
	else if (iResult < 0)
	{
		SetErrnoError("error during poll: ");
		return 0;
	}

	// event(s) triggered
	return iResult;

} // CheckIfReady

//-----------------------------------------------------------------------------
bool KIOStreamSocket::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	kDebug(2, "method not supported - no TLS stream type");
	return false;
}

//-----------------------------------------------------------------------------
bool KIOStreamSocket::SetManualTLSHandshake(bool bYes)
//-----------------------------------------------------------------------------
{
	kDebug(2, "method not supported - no TLS stream type");
	return false;
}

//-----------------------------------------------------------------------------
bool KIOStreamSocket::SetALPNRaw(KStringView sALPN)
//-----------------------------------------------------------------------------
{
	auto SSL = GetNativeTLSHandle();

	if (!SSL)
	{
		kDebug(2, "method not supported - no TLS stream type");
		return false;
	}

	auto iResult = ::SSL_set_alpn_protos(SSL,
	                                     reinterpret_cast<const unsigned char*>(sALPN.data()),
	                                     static_cast<unsigned int>(sALPN.size()));

	if (iResult == 0)
	{
		kDebug(2, "ALPN set to: {}", sALPN);
		return true;
	}

	return SetError(kFormat("failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sALPN), iResult));

} // SetALPNRaw

//-----------------------------------------------------------------------------
KStringView KIOStreamSocket::GetALPN()
//-----------------------------------------------------------------------------
{
	auto SSL = GetNativeTLSHandle();

	if (!SSL)
	{
		kDebug(2, "method not supported - no TLS stream type");
		return {};
	}

	const unsigned char* alpn { nullptr };
	unsigned int alpnlen { 0 };
	::SSL_get0_alpn_selected(SSL, &alpn, &alpnlen);
	return { reinterpret_cast<const char*>(alpn), alpnlen };

} // GetALPN

//-----------------------------------------------------------------------------
bool KIOStreamSocket::Timeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// value was already set in SetTimeout(), this is the virtual hook
	return true;
}

//-----------------------------------------------------------------------------
KIOStreamSocket::native_tls_handle_type KIOStreamSocket::GetNativeTLSHandle()
//-----------------------------------------------------------------------------
{
	kDebug(2, "method not supported - no TLS stream type");
	return nullptr;
}

//-----------------------------------------------------------------------------
void KIOStreamSocket::SetConnectedEndPointAddress(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	SetEndPointAddress(Endpoint);
}


//-----------------------------------------------------------------------------
std::unique_ptr<KIOStreamSocket> KIOStreamSocket::Create(const KURL& URL, bool bForceTLS, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (URL.Protocol == url::KProtocol::UNIX)
	{
		return std::make_unique<KUnixStream>(URL.Path.get(), Options);
	}
#endif

	url::KPort Port = URL.Port;

	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}

	if ((url::KProtocol::UNDEFINED && Port.get() == 443) || URL.Protocol == url::KProtocol::HTTPS || bForceTLS)
	{
#if DEKAF2_HAS_OPENSSL_QUIC
		if (Options.IsSet(KStreamOptions::RequestHTTP3))
		{
			return std::make_unique<KQuicStream>(KTCPEndPoint(URL.Domain, Port), Options);
		}
		else
#endif
		{
			return std::make_unique<KTLSStream>(KTCPEndPoint(URL.Domain, Port), Options);
		}
	}
	else // NOLINT: we want the else after return..
	{
		return std::make_unique<KTCPStream>(KTCPEndPoint(URL.Domain, Port), Options);
	}

} // Create

//-----------------------------------------------------------------------------
std::unique_ptr<KIOStreamSocket> KIOStreamSocket::Create(std::iostream& IOStream)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KIOStreamSocketAdaptor>(IOStream);

} // Create

DEKAF2_NAMESPACE_END
