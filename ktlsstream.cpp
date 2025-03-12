/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "ktlsstream.h"
#include "klog.h"
#include <openssl/opensslv.h>

DEKAF2_NAMESPACE_BEGIN

static KTLSContext s_KTLSClientContext { false, KTLSContext::Transport::Tcp };

//-----------------------------------------------------------------------------
bool KTLSStream::Handshake()
//-----------------------------------------------------------------------------
{
	if (!m_Stream.bNeedHandshake)
	{
		return true;
	}

	m_Stream.bNeedHandshake = false;

	m_Stream.Socket.async_handshake(m_Stream.GetContext().GetRole(),
								    [&](const boost::system::error_code& ec)
	{
		m_Stream.ec = ec;
	});

	m_Stream.RunTimed();

	if (m_Stream.ec.value() != 0 || !m_Stream.Socket.lowest_layer().is_open())
	{
		auto sMessage = m_Stream.ec.message();
		kDebug(1, "TLS handshake failed with {}: {}",
			   m_Stream.sEndpoint,
			   sMessage);

		if ((m_StreamOptions & KStreamOptions::RequestHTTP2) == KStreamOptions::RequestHTTP2
			&& sMessage.find("bad extension") != std::string::npos)
		{
			kDebug(1, "the counterpart seems to not understand HTTP/2 ALPS properly");
			m_bRetryWithHTTP1 = (m_StreamOptions & KStreamOptions::FallBackToHTTP1) == KStreamOptions::FallBackToHTTP1;
		}
		return false;
	}

#if OPENSSL_VERSION_NUMBER <= 0x10002000L || defined(DEKAF2_WITH_KLOG)
	auto ssl = m_Stream.Socket.native_handle();
#endif

#if OPENSSL_VERSION_NUMBER <= 0x10002000L
	// OpenSSL <= 1.0.2 did not do hostname validation - let's do it manually

	if (m_Stream.GetContext().GetVerify())
	{
		// look for server certificate
		auto* cert = ::SSL_get_peer_certificate(ssl);

		if (cert)
		{
			// have one, free it immediately
			::X509_free(cert);
		}
		else
		{
			kDebug(1, "server did not present a certificate");
			return false;
		}

		// check chain verification
		if (::SSL_get_verify_result(ssl) != ::X509_V_OK)
		{
			kDebug(1, "certificate chain verification failed");
			return false;
		}
	}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	// in OpenSSL < 1.1.0 we need to manually check that the host name is
	// one of the certificate host names ..
#endif

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
		kDebug (3, "TLS handshake successful, rx/tx {}/{} bytes",
			   ::BIO_number_read(::SSL_get_rbio(ssl)),
			   ::BIO_number_written(::SSL_get_wbio(ssl)));

		auto cipher = ::SSL_get_current_cipher(ssl);
		kDebug(2, "TLS version: {}, cipher: {}",
			   ::SSL_CIPHER_get_version(cipher),
			   ::SSL_CIPHER_get_name(cipher));

		auto compress  = ::SSL_get_current_compression(ssl);
		auto expansion = ::SSL_get_current_expansion(ssl);

		if (compress || expansion)
		{
			kDebug(2, "TLS compression: {}, expansion: {}",
				   compress  ? ::SSL_COMP_get_name(compress ) : "NONE",
				   expansion ? ::SSL_COMP_get_name(expansion) : "NONE");
		}
	}
#endif

	return true;

} // Handshake

//-----------------------------------------------------------------------------
bool KTLSStream::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	// check if this stream was constructed with the manual handshake flag
	if (m_Stream.bManualHandshake)
	{
		m_Stream.bManualHandshake = false;
	}

	return Handshake();

} // StartManualTLSHandshake

//-----------------------------------------------------------------------------
bool KTLSStream::SetManualTLSHandshake(bool bYesno)
//-----------------------------------------------------------------------------
{
	if (m_Stream.bNeedHandshake)
	{
		m_Stream.bManualHandshake = bYesno;
		return true;
	}
	else
	{
		return false;
	}

} // SetManualTLSHandshake

//-----------------------------------------------------------------------------
bool KTLSStream::SetRequestHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2
	// allow ALPN negotiation for HTTP/2 if this is a client
	if (GetContext().GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto sProto = bAlsoAllowHTTP1 ? "\x02h2\0x08http/1.1" : "\x02h2";
		auto iResult = ::SSL_set_alpn_protos(GetNativeTLSHandle(),
		                                     reinterpret_cast<const unsigned char*>(sProto),
		                                     static_cast<unsigned int>(strlen(sProto)));
		if (iResult == 0)
		{
			return true;
		}

		kDebug(1, "failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sProto), iResult);
	}
	else
	{
		kDebug(1, "HTTP/2 is only supported in client mode");
	}
#else  // of DEKAF2_HAS_NGHTTP2
		kDebug(2, "HTTP/2 is not supported by this build");
#endif // of DEKAF2_HAS_NGHTTP2

	return false;

} // SetRequestHTTP2

//-----------------------------------------------------------------------------
std::streamsize KTLSStream::direct_read_some(void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (!m_Stream.bManualHandshake)
	{
		if (!Handshake())
		{
			return -1;
		}
	}

	std::streamsize iRead { 0 };

	if (!m_Stream.bManualHandshake)
	{
		GetAsioSocket().async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			m_Stream.ec = ec;
			iRead = bytes_transferred;
		});
	}
	else
	{
		GetAsioSocket().next_layer().async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			m_Stream.ec = ec;
			iRead = bytes_transferred;
		});
	}

	m_Stream.RunTimed();

	return iRead;

} // direct_read_some

//-----------------------------------------------------------------------------
std::streamsize KTLSStream::TLSStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KTLSStream*>(stream_);

		iRead = TLSStream.direct_read_some(sBuffer, iCount);

#ifdef DEKAF2_WITH_KLOG
		if (DEKAF2_UNLIKELY(kWouldLog(1)))
		{
			if (iRead == 0 || TLSStream.m_Stream.ec.value() != 0 || !TLSStream.m_Stream.Socket.lowest_layer().is_open())
			{
				if (TLSStream.m_Stream.ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "input stream got closed by endpoint {}", TLSStream.m_Stream.sEndpoint);
				}
				else
				{
					kDebug(1, "cannot read from {} stream with endpoint {}: {}",
						   "TLS",
						   TLSStream.m_Stream.sEndpoint,
						   TLSStream.m_Stream.ec.message());
				}
			}
		}
#endif
	}

	return iRead;

} // TLSStreamReader

//-----------------------------------------------------------------------------
std::streamsize KTLSStream::TLSStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we need to loop the writer, as write_some() has an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs

	std::streamsize iWrote { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KTLSStream*>(stream_);

		if (!TLSStream.m_Stream.bManualHandshake)
		{
			if (!TLSStream.Handshake())
			{
				return -1;
			}
		}

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart{0};

			if (!TLSStream.m_Stream.bManualHandshake)
			{
				TLSStream.GetAsioSocket().async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					TLSStream.m_Stream.ec = ec;
					iWrotePart = bytes_transferred;
				});
			}
			else
			{
				TLSStream.GetAsioSocket().next_layer().async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					TLSStream.m_Stream.ec = ec;
					iWrotePart = bytes_transferred;
				});
			}

			TLSStream.m_Stream.RunTimed();

			iWrote += iWrotePart;

			if (iWrotePart == 0 || TLSStream.m_Stream.ec.value() != 0 || !TLSStream.m_Stream.Socket.lowest_layer().is_open())
			{
#ifdef DEKAF2_WITH_KLOG
				if (TLSStream.m_Stream.ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "output stream got closed by endpoint {}", TLSStream.m_Stream.sEndpoint);
				}
				else
				{
					kDebug(1, "cannot write to {} stream with endpoint {}: {}",
						   "TLS",
						   TLSStream.m_Stream.sEndpoint,
						   TLSStream.m_Stream.ec.message());
				}
#endif
				break;
			}
		}
	}

	return iWrote;

} // TLSStreamWriter

//-----------------------------------------------------------------------------
KTLSStream::KTLSStream()
//-----------------------------------------------------------------------------
: KTLSStream(s_KTLSClientContext, KStreamOptions::GetDefaultTimeout())
{
}

//-----------------------------------------------------------------------------
KTLSStream::KTLSStream(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
: base_type(&m_TLSStreamBuf, Timeout)
, m_Stream(Context, Timeout)
{
}

//-----------------------------------------------------------------------------
KTLSStream::KTLSStream(KTLSContext& Context, 
                       const KTCPEndPoint& Endpoint,
                       KStreamOptions Options)
//-----------------------------------------------------------------------------
: KTLSStream(Context, Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KTLSStream::KTLSStream(const KTCPEndPoint& Endpoint,
                       KStreamOptions Options)
//-----------------------------------------------------------------------------
: KTLSStream(s_KTLSClientContext, Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KTLSStream::Timeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	m_Stream.Timeout = Timeout;
	return true;
}

//-----------------------------------------------------------------------------
bool KTLSStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	m_StreamOptions = Options;
	m_Stream.bNeedHandshake = true;
	m_bRetryWithHTTP1 = false;

	SetUnresolvedEndPoint(Endpoint);

	auto& sHostname = Endpoint.Domain.get();

	auto hosts = KIOStreamSocket::ResolveTCP(sHostname, Endpoint.Port.get(), m_StreamOptions.GetFamily(), m_Stream.IOService, m_Stream.ec);

	if (Good())
	{
#if DEKAF2_SSL_DEBUG
		SSL_set_msg_callback(GetNativeTLSHandle(), SSL_trace);
		SSL_set_msg_callback_arg(GetNativeTLSHandle(), BIO_new_fp(stderr, BIO_NOCLOSE));
#endif

		if ((m_StreamOptions & KStreamOptions::VerifyCert) != 0)
		{
			GetAsioSocket().set_verify_mode(boost::asio::ssl::verify_peer
			                              | boost::asio::ssl::verify_fail_if_no_peer_cert);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
			// looks as if asio is not setting the expected host name though? Let's do it manually.
			// that only works though with OpenSSL versions >= 1.1.0
			if (!::SSL_set1_host(GetNativeTLSHandle(), sHostname.c_str()))
			{
				return SetError(kFormat("Failed to set the certificate verification hostname: {}", sHostname));
			}
#endif
		}
		else
		{
			GetAsioSocket().set_verify_mode(boost::asio::ssl::verify_none);
		}

		if (GetContext().GetRole() == boost::asio::ssl::stream_base::client)
		{
			if (m_StreamOptions & KStreamOptions::ManualHandshake)
			{
				SetManualTLSHandshake(true);
			}

			if (m_StreamOptions & KStreamOptions::RequestHTTP2)
			{
				SetRequestHTTP2(m_StreamOptions & KStreamOptions::FallBackToHTTP1);
			}
		}

		// make sure client side SNI works..
		if (!::SSL_set_tlsext_host_name(GetNativeTLSHandle(), sHostname.c_str()))
		{
			return SetError(kFormat("failed to set SNI hostname: {}", sHostname));
		}

		boost::asio::async_connect(GetTCPSocket(), hosts,
		                           [&](const boost::system::error_code& ec,
#if (DEKAF2_CLASSIC_ASIO)
		                               resolver_endpoint_tcp_type endpoint)
#else
		                               const resolver_endpoint_tcp_type& endpoint)
#endif
		{
			m_Stream.sEndpoint  = PrintResolvedAddress(endpoint);
			m_Stream.ec         = ec;
			// parse the endpoint back into our basic KTCPEndpoint
			SetEndPointAddress(m_Stream.sEndpoint);
		});

		kDebug (2, "connecting to {}: {}", "endpoint", Endpoint);

		m_Stream.RunTimed();
	}

	if (!Good() || !GetTCPSocket().is_open())
	{
		return SetError(m_Stream.ec.message());
	}

	kDebug(2, "connected to {}: {}", "endpoint", GetEndPointAddress());

	return true;

} // Connect

//-----------------------------------------------------------------------------
void KTLSStream::SetConnectedEndPointAddress(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	// update stream
	m_Stream.sEndpoint = Endpoint.Serialize();
	// update base
	SetEndPointAddress(Endpoint);

} // SetConnectedEndPointAddress


//-----------------------------------------------------------------------------
std::unique_ptr<KTLSStream> CreateKTLSServer(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTLSStream>(Context, Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTLSClient> CreateKTLSClient()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTLSClient>(s_KTLSClientContext);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTLSClient> CreateKTLSClient(const KTCPEndPoint& EndPoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKTLSClient();

	Client->Connect(EndPoint, Options);

	return Client;

} // CreateKTLSClient

DEKAF2_NAMESPACE_END

