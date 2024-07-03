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

#include "ksslstream.h"
#include "klog.h"
#include "kfrozen.h"
#include <openssl/opensslv.h>

DEKAF2_NAMESPACE_BEGIN

#if (BOOST_VERSION < 106600)
boost::asio::io_service KSSLContext::s_IO_Service;
#endif

//-----------------------------------------------------------------------------
std::string KSSLContext::PasswordCallback(std::size_t max_length,
						  boost::asio::ssl::context::password_purpose purpose) const
//-----------------------------------------------------------------------------
{
	return m_sPassword;
}

//-----------------------------------------------------------------------------
KSSLContext::KSSLContext(bool bIsServer)
//-----------------------------------------------------------------------------
#if (BOOST_VERSION < 106600)
	: m_Context(s_IO_Service, boost::asio::ssl::context::sslv23)
#else
	: m_Context(bIsServer ? boost::asio::ssl::context::tls_server : boost::asio::ssl::context::tls_client)
#endif
	, m_Role(bIsServer ? boost::asio::ssl::stream_base::server : boost::asio::ssl::stream_base::client)
 {
	 boost::asio::ssl::context::options options
	 	= boost::asio::ssl::context::default_workarounds
	 	| boost::asio::ssl::context::single_dh_use
	 	| boost::asio::ssl::context::no_sslv2
	 	| boost::asio::ssl::context::no_sslv3
#ifdef SSL_OP_NO_COMPRESSION
		| boost::asio::ssl::context::no_compression
#endif
#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
		| SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
#endif
#if (BOOST_VERSION >= 106600)
	 	| boost::asio::ssl::context::no_tlsv1_1
#endif
	 	| boost::asio::ssl::context::no_tlsv1;

	boost::system::error_code ec;
	m_Context.set_options(options, ec);

	if (ec)
	{
		SetError(kFormat("error setting SSL options {}: {}", options, ec.message()));
	}

	// set the system default cert paths, but do not yet switch verify mode on
	// (we do that individually for each connect on request)
	m_Context.set_default_verify_paths(ec);

	if (ec)
	{
		SetError(kFormat("error setting SSL verify paths: {}", ec.message()));
	}

} // ctor

//-----------------------------------------------------------------------------
bool KSSLContext::LoadSSLCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KSSLContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set password callback: {}", ec.message()));
	}

	m_Context.use_certificate_chain_file(sCert.c_str(), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set certificate file {}: {}", sCert, ec.message()));
	}

	if (sKey.empty())
	{
		// maybe the cert has both, the cert and the key
		sKey = sCert;
	}

	m_Context.use_private_key_file(sKey.c_str(), boost::asio::ssl::context::pem, ec);

	if (ec)
	{
		return SetError(kFormat("cannot set key file {}: {}", sKey, ec.message()));
	}

	kDebug(2, "TLS certificates successfully {}", "loaded");
	return true;

} // LoadSSLCertificates

//-----------------------------------------------------------------------------
bool KSSLContext::SetSSLCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
#if (BOOST_VERSION < 105400)

	kDebug(1, "you need to link against at least boost 1.54 for buffered SSL certificates");
	return false;

#else

	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KSSLContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set password callback: {}", ec.message()));
	}

	m_Context.use_certificate_chain(boost::asio::const_buffer(sCert.data(), sCert.size()), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set certificate: {}", ec.message()));
	}

	if (sKey.empty())
	{
		// maybe the cert has both, the cert and the key
		sKey = sCert;
	}

	m_Context.use_private_key(boost::asio::const_buffer(sKey.data(), sKey.size()), boost::asio::ssl::context::pem, ec);

	if (ec)
	{
		return SetError(kFormat("cannot set key: {}", ec.message()));
	}

	kDebug(2, "TLS certificates successfully {}", "set");
	return true;

#endif

} // SetSSLCertificates

//-----------------------------------------------------------------------------
bool KSSLContext::SetDHPrimes(KStringView sDHPrimes)
//-----------------------------------------------------------------------------
{
	if (sDHPrimes.empty())
	{
		return true;
	}

	boost::system::error_code ec;

	m_Context.use_tmp_dh(boost::asio::const_buffer(sDHPrimes.data(), sDHPrimes.size()), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set DH primes: {}", ec.message()));
	}

	kDebug(2, "DH primes successfully set, server will use perfect forward secrecy");
	return true;

} // SetDHPrime

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	#define DEKAF2_HAS_TLSv13
#endif

//-----------------------------------------------------------------------------
bool KSSLContext::SetAllowedCipherSuites(KStringView sCipherSuites)
//-----------------------------------------------------------------------------
{
	// the TLS 1.3 cipher suites are only supported since OpenSSL 1.1.1,
	// and they use a different interface to be set ...
	//
	// check if we have some

	if (sCipherSuites.empty())
	{
		return true;
	}

#ifdef DEKAF2_HAS_FROZEN
	// this set is created at compile time
	static constexpr auto s_TLSv13Ciphers {frozen::make_unordered_set( {
#else
	// this set is created at run time
	static const std::unordered_set<KStringView> s_TLSv13Ciphers {
#endif
		"TLS_AES_128_GCM_SHA256"_ksv,
		"TLS_AES_256_GCM_SHA384"_ksv,
		"TLS_CHACHA20_POLY1305_SHA256"_ksv,
		"TLS_AES_128_CCM_SHA256"_ksv,
		"TLS_AES_128_CCM_8_SHA256"_ksv
#ifdef DEKAF2_HAS_FROZEN
	})};
#else
	};
#endif

	if (sCipherSuites == "PFS")
	{
		// only allow perfect forward secrecy, and GCM or POLY1305
		sCipherSuites =
		"TLS_AES_256_GCM_SHA384"
		":TLS_CHACHA20_POLY1305_SHA256"
		":TLS_AES_128_GCM_SHA256"
		":ECDHE-ECDSA-AES256-GCM-SHA384"
		":ECDHE-ECDSA-CHACHA20-POLY1305"
		":ECDHE-ECDSA-AES128-GCM-SHA256"
		":ECDHE-RSA-AES256-GCM-SHA384"
		":ECDHE-RSA-CHACHA20-POLY1305"
		":ECDHE-RSA-AES128-GCM-SHA256";
	}

	std::vector<KStringView> CipherV12;
	std::vector<KStringView> CipherV13;

	for (const auto sCipher : sCipherSuites.Split(":, "))
	{
		if (!sCipher.empty())
		{
			if (s_TLSv13Ciphers.find(sCipher) == s_TLSv13Ciphers.end())
			{
				CipherV12.push_back(sCipher);
			}
			else
			{
				CipherV13.push_back(sCipher);
			}
		}
	}

	bool bSuccess { false };

	if (!CipherV13.empty() || !CipherV12.empty())
	{
		if (CipherV12.empty())
		{
			kDebug(2, "disable TLSv1.2 (no cipher selected)");
			SSL_CTX_set_cipher_list(m_Context.native_handle(), "");
		}
		else
		{
			auto sCiphers = kJoined(CipherV12, ":");
			kDebug(2, "set TLSv{} cipher suites {}", "1.2", sCiphers);
			if (SSL_CTX_set_cipher_list (m_Context.native_handle(), sCiphers.c_str()))
			{
				bSuccess = true;
			}
			else
			{
				SetError(kFormat("setting TLSv{} cipher suites failed: {}", "1.2", sCiphers));
			}
		}
	}

	if (!CipherV13.empty())
	{
		auto sCiphers = kJoined(CipherV13, ":");
#ifdef DEKAF2_HAS_TLSv13
		kDebug(2, "set TLSv{} cipher suites {}", "1.3", sCiphers);
		if (SSL_CTX_set_ciphersuites(m_Context.native_handle(), sCiphers.c_str()))
		{
			bSuccess = true;
		}
		else
		{
			SetError(kFormat("setting TLSv{} cipher suites failed: {}", "1.3", sCiphers));
		}
#else
		kDebug(1, "TLSv1.3 is not supported by the linked SSL library\ncannot set TLSv1.3 cipher suites {}", sCiphers);
#endif
	}

	return bSuccess;

} // SetAllowedCipherSuites

#if DEKAF2_HAS_NGHTTP2 && OPENSSL_VERSION_NUMBER >= 0x10200000L

#define DEKAF2_ALLOW_HTTP2_SERVER_MODE 0 // set to 1 to allow http2 server mode

#if DEKAF2_ALLOW_HTTP2_SERVER_MODE

namespace {

//-----------------------------------------------------------------------------
// the string compare in the server side ALPN negotiation
int select_alpn(const unsigned char** out, unsigned char* outlen,
                const unsigned char* in, unsigned int inlen,
                const char* key, unsigned int keylen)
//-----------------------------------------------------------------------------
{
	for (unsigned int i = 0; i + keylen <= inlen; i += (unsigned int)(in[i] + 1))
	{
		if (memcmp(&in[i], key, keylen) == 0)
		{
			*out = (unsigned char *)&in[i + 1];
			*outlen = in[i];
			return 0;
		}
	}

	return -1;

} // select_alpn

//-----------------------------------------------------------------------------
// the openssl proto selection callback in server context - we use the
// arg pointer as a flag for http/1.1 being allowed, too
int alpn_select_proto_cb(SSL* ssl, const unsigned char** out,
                         unsigned char* outlen, const unsigned char* in,
                         unsigned int inlen, void* arg)
//-----------------------------------------------------------------------------
{
	int iResult { -1 };

	if (select_alpn(out, outlen, in, inlen, "\x02h2", 2) == 0)
	{
		iResult = 1;
	}
	else if (select_alpn(out, outlen, in, inlen, "\x08http/1.1", 8) == 0)
	{
		iResult = 0;
	}

	if (iResult != 1)
	{
		if (arg == nullptr || iResult != 0)
		{
			return SSL_TLSEXT_ERR_NOACK;
		}
	}

	return SSL_TLSEXT_ERR_OK;

} // alpn_select_proto_cb

} // end of anonymous namespace

#endif // of DEKAF2_ALLOW_HTTP2_SERVER_MODE
#endif // of DEKAF2_HAS_NGHTTP2

//-----------------------------------------------------------------------------
bool KSSLContext::SetAllowHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2 && OPENSSL_VERSION_NUMBER >= 0x10200000L
	// allow ALPN negotiation for HTTP/2 if this is a client
	if (GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto sProto = bAlsoAllowHTTP1 ? "\x02h2\0x08http/1.1" : "\x02h2";
		auto iResult = SSL_CTX_set_alpn_protos(m_Context.native_handle(),
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
#if DEKAF2_ALLOW_HTTP2_SERVER_MODE
		if (GetRole() == boost::asio::ssl::stream_base::server)
		{
			SSL_CTX_set_alpn_select_cb(m_Context.native_handle(),
			                           alpn_select_proto_cb,
			                           bAlsoAllowHTTP1 ? this : nullptr); // we use the user ptr as a flag
		}
#else
		kDebug(1, "HTTP2 is only supported in client mode");
#endif
	}
#else
		kDebug(2, "HTTP2 is not supported by this build");
#endif

	return false;

} // SetAllowHTTP2

//-----------------------------------------------------------------------------
bool KSSLContext::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

static KSSLContext s_KSSLClientContext { false };


//-----------------------------------------------------------------------------
bool KSSLIOStream::handshake(KAsioSSLStream<asiostream>* stream)
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
bool KSSLIOStream::StartManualTLSHandshake()
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
bool KSSLIOStream::SetManualTLSHandshake(bool bYesno)
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
bool KSSLIOStream::SetRequestHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2 && OPENSSL_VERSION_NUMBER >= 0x10200000L
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
		kDebug(1, "HTTP2 is only supported in client mode");
	}
#else
		kDebug(2, "HTTP2 is not supported by this build");
#endif

	return false;

} // SetRequestHTTP2

//-----------------------------------------------------------------------------
KStringView KSSLIOStream::GetALPN()
//-----------------------------------------------------------------------------
{
	const unsigned char* alpn { nullptr };
	unsigned int alpnlen { 0 };
	SSL_get0_alpn_selected(m_Stream.Socket.native_handle(), &alpn, &alpnlen);
	return { reinterpret_cast<const char*>(alpn), alpnlen };

} // GetALPN

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::direct_read_some(void* sBuffer, std::streamsize iCount)
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
std::streamsize KSSLIOStream::SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KSSLIOStream*>(stream_);

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

} // SSLStreamReader

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::SSLStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we need to loop the writer, as write_some() has an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs

	std::streamsize iWrote { 0 };

	if (stream_)
	{
		auto& TLSStream = *static_cast<KSSLIOStream*>(stream_);

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

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
: KSSLIOStream(s_KSSLClientContext, iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(KSSLContext& Context,
                           int iSecondsTimeout)
//-----------------------------------------------------------------------------
	: base_type(&m_SSLStreamBuf)
	, m_Stream(Context, iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(KSSLContext& Context, 
                           const KTCPEndPoint& Endpoint,
                           TLSOptions Options,
                           int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : KSSLIOStream(Context, iSecondsTimeout)
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Connect(const KTCPEndPoint& Endpoint, TLSOptions Options)
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
			m_Stream.sEndpoint.Format("{}:{}",
#if (BOOST_VERSION < 106600)
									  endpoint->endpoint().address().to_string(),
									  endpoint->endpoint().port());
#else
									  endpoint.address().to_string(),
									  endpoint.port());
#endif
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
std::unique_ptr<KSSLStream> CreateKSSLServer(KSSLContext& Context)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>(Context);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLClient>(s_KSSLClientContext, iSecondsTimeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(const KTCPEndPoint& EndPoint,
											 TLSOptions Options,
											 int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKSSLClient(iSecondsTimeout);

	Client->Connect(EndPoint, Options);

	return Client;

} // CreateKSSLClient

DEKAF2_NAMESPACE_END

