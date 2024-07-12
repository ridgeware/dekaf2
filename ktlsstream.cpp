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
#include "kfrozen.h"
#include <openssl/opensslv.h>

DEKAF2_NAMESPACE_BEGIN

static KTLSContext s_KTLSClientContext { false };

//-----------------------------------------------------------------------------
bool KTLSIOStream::handshake(KAsioTLSStream<asiostream>* stream)
//-----------------------------------------------------------------------------
{
	if (!stream->bNeedHandshake)
	{
		return true;
	}

	stream->bNeedHandshake = false;
	
	stream->Socket.async_handshake(stream->GetContext().GetRole(),
								   [&](const boost::system::error_code& ec)
	{
		stream->ec = ec;
	});

	stream->RunTimed();

	if (stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
	{
		kDebug(1, "TLS handshake failed with {}: {}",
			   stream->sEndpoint,
			   stream->ec.message());
		return false;
	}

#if OPENSSL_VERSION_NUMBER <= 0x10002000L || defined(DEKAF2_WITH_KLOG)
	auto ssl = stream->Socket.native_handle();
#endif

#if OPENSSL_VERSION_NUMBER <= 0x10002000L
	// OpenSSL <= 1.0.2 did not do hostname validation - let's do it manually

	if (stream->GetContext().GetVerify())
	{
		// look for server certificate
		auto* cert = SSL_get_peer_certificate(ssl);

		if (cert)
		{
			// have one, free it immediately
			X509_free(cert);
		}
		else
		{
			kDebug(1, "server did not present a certificate");
			return false;
		}

		// check chain verification
		if (SSL_get_verify_result(ssl) != X509_V_OK)
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
		kDebug(2, "TLS handshake successful, rx/tx {}/{} bytes",
			   BIO_number_read(SSL_get_rbio(ssl)),
			   BIO_number_written(SSL_get_wbio(ssl)));

		auto cipher = SSL_get_current_cipher(ssl);
		kDebug(2, "TLS version: {}, cipher: {}",
			   SSL_CIPHER_get_version(cipher),
			   SSL_CIPHER_get_name(cipher));

		auto compress  = SSL_get_current_compression(ssl);
		auto expansion = SSL_get_current_expansion(ssl);

		if (compress || expansion)
		{
			kDebug(2, "TLS compression: {}, expansion: {}",
				   compress  ? SSL_COMP_get_name(compress ) : "NONE",
				   expansion ? SSL_COMP_get_name(expansion) : "NONE");
		}
	}
#endif

	return true;

} // handshake

//-----------------------------------------------------------------------------
bool KTLSIOStream::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	// check if this stream was constructed with the manual handshake flag
	if (m_Stream.bManualHandshake)
	{
		m_Stream.bManualHandshake = false;
	}

	return handshake(&m_Stream);

} // StartManualTLSHandshake

//-----------------------------------------------------------------------------
bool KTLSIOStream::SetManualTLSHandshake(bool bYesno)
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
bool KTLSIOStream::SetRequestHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2
	// allow ALPN negotiation for HTTP/2 if this is a client
	if (m_Stream.GetContext().GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto sProto = bAlsoAllowHTTP1 ? "\x02h2\0x08http/1.1" : "\x02h2";
		auto iResult = SSL_set_alpn_protos(m_Stream.Socket.native_handle(),
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
KStringView KTLSIOStream::GetALPN()
//-----------------------------------------------------------------------------
{
	const unsigned char* alpn { nullptr };
	unsigned int alpnlen { 0 };
	SSL_get0_alpn_selected(m_Stream.Socket.native_handle(), &alpn, &alpnlen);
	return { reinterpret_cast<const char*>(alpn), alpnlen };

} // GetALPN

//-----------------------------------------------------------------------------
std::streamsize KTLSIOStream::direct_read_some(void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (!m_Stream.bManualHandshake)
	{
		if (!handshake(&m_Stream))
		{
			return -1;
		}
	}

	std::streamsize iRead { 0 };

	if (!m_Stream.bManualHandshake)
	{
		m_Stream.Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			m_Stream.ec = ec;
			iRead = bytes_transferred;
		});
	}
	else
	{
		m_Stream.Socket.next_layer().async_read_some(boost::asio::buffer(sBuffer, iCount),
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
std::streamsize KTLSIOStream::TLSStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KTLSIOStream*>(stream_);

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
std::streamsize KTLSIOStream::TLSStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we need to loop the writer, as write_some() has an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs

	std::streamsize iWrote { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KTLSIOStream*>(stream_);

		if (!TLSStream.m_Stream.bManualHandshake)
		{
			if (!handshake(&TLSStream.m_Stream))
			{
				return -1;
			}
		}

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart{0};

			if (!TLSStream.m_Stream.bManualHandshake)
			{
				TLSStream.m_Stream.Socket.async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					TLSStream.m_Stream.ec = ec;
					iWrotePart = bytes_transferred;
				});
			}
			else
			{
				TLSStream.m_Stream.Socket.next_layer().async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
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
KTLSIOStream::KTLSIOStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
: KTLSIOStream(s_KTLSClientContext, iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KTLSIOStream::KTLSIOStream(KTLSContext& Context,
                           int iSecondsTimeout)
//-----------------------------------------------------------------------------
	: base_type(&m_TLSStreamBuf)
	, m_Stream(Context, iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KTLSIOStream::KTLSIOStream(KTLSContext& Context, 
                           const KTCPEndPoint& Endpoint,
                           TLSOptions Options,
                           int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : KTLSIOStream(Context, iSecondsTimeout)
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KTLSIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KTLSIOStream::Connect(const KTCPEndPoint& Endpoint, TLSOptions Options)
//-----------------------------------------------------------------------------
{
	m_Stream.bNeedHandshake = true;

	kDebug(2, "resolving domain {}", Endpoint.Domain.get());

	boost::asio::ip::tcp::resolver Resolver(m_Stream.IOService);
	boost::asio::ip::tcp::resolver::query query(Endpoint.Domain.get().c_str(), Endpoint.Port.Serialize().c_str());
	auto hosts = Resolver.resolve(query, m_Stream.ec);

	if (Good())
	{
#ifdef DEKAF2_WITH_KLOG
		if (kWouldLog(2))
		{
#if (BOOST_VERSION < 106600)
			auto it = hosts;
			decltype(it) ie;
#else
			auto it = hosts.begin();
			auto ie = hosts.end();
#endif
			for (; it != ie; ++it)
			{
				kDebug(2, "resolved to: {}", it->endpoint().address().to_string());
			}
		}
#endif

		if ((Options & TLSOptions::VerifyCert) != 0)
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_peer
										  | boost::asio::ssl::verify_fail_if_no_peer_cert);
		}
		else
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_none);
		}

		SetManualTLSHandshake((Options & TLSOptions::ManualHandshake) != 0);

		if (m_Stream.GetContext().GetRole() == boost::asio::ssl::stream_base::client 
			&& (Options & TLSOptions::RequestHTTP2) != 0)
		{
			SetRequestHTTP2((Options & TLSOptions::FallBackToHTTP1) != 0);
		}

		// make sure client side SNI works..
		SSL_set_tlsext_host_name(m_Stream.Socket.native_handle(), Endpoint.Domain.get().c_str());

		boost::asio::async_connect(m_Stream.Socket.lowest_layer(),
								   hosts,
								   [&](const boost::system::error_code& ec,
#if (BOOST_VERSION < 106600)
                                       boost::asio::ip::tcp::resolver::iterator endpoint)
#else
                                       const boost::asio::ip::tcp::endpoint& endpoint)
#endif
		{
			if (endpoint.address().is_v6())
			{
				m_Stream.sEndpoint.Format("[{}]:{}",
#if (BOOST_VERSION < 106600)
					endpoint->endpoint().address().to_string(),
					endpoint->endpoint().port());
#else
					endpoint.address().to_string(),
					endpoint.port());
#endif
			}
			else
			{
				m_Stream.sEndpoint.Format("{}:{}",
#if (BOOST_VERSION < 106600)
					endpoint->endpoint().address().to_string(),
					endpoint->endpoint().port());
#else
					endpoint.address().to_string(),
					endpoint.port());
#endif
			}
			m_Stream.ec = ec;
		});

		kDebug(2, "trying to connect to {} {}", "endpoint", Endpoint.Serialize());

		m_Stream.RunTimed();
	}

	if (!Good() || !m_Stream.Socket.lowest_layer().is_open())
	{
		kDebug(1, "{}: {}", Endpoint.Serialize(), Error());
		return false;
	}

	kDebug(2, "connected to {} {}", "endpoint", m_Stream.sEndpoint);

	return true;

} // connect


//-----------------------------------------------------------------------------
std::unique_ptr<KTLSStream> CreateKTLSServer(KTLSContext& Context)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTLSStream>(Context);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTLSClient> CreateKTLSClient(int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTLSClient>(s_KTLSClientContext, iSecondsTimeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTLSClient> CreateKTLSClient(const KTCPEndPoint& EndPoint,
											 TLSOptions Options,
											 int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKTLSClient(iSecondsTimeout);

	Client->Connect(EndPoint, Options);

	return Client;

} // CreateKTLSClient

DEKAF2_NAMESPACE_END

