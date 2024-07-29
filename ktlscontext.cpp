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

#include "ktlscontext.h"
#include "klog.h"
#include "kfrozen.h"
#include <openssl/opensslv.h>

DEKAF2_NAMESPACE_BEGIN

#if (DEKAF2_CLASSIC_ASIO)
boost::asio::io_service KTLSContext::s_IO_Service;
#endif

//-----------------------------------------------------------------------------
std::string KTLSContext::PasswordCallback(std::size_t max_length,
						  boost::asio::ssl::context::password_purpose purpose) const
//-----------------------------------------------------------------------------
{
	return m_sPassword;
}

//-----------------------------------------------------------------------------
::SSL_CTX* KTLSContext::CreateContext(bool bIsServer, Transport transport)
//-----------------------------------------------------------------------------
{
#if (DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT)
	switch (transport)
	{
		case Transport::Tcp:
			if (bIsServer)
			{
				return ::SSL_CTX_new(::TLS_server_method());
			}
			else
			{
				return ::SSL_CTX_new(::TLS_client_method());
			}
			break;

		case Transport::Quic:
#if DEKAF2_HAS_OPENSSL_QUIC
			if (bIsServer)
			{
				// TODO !
				return nullptr;
			}
			else
			{
				return ::SSL_CTX_new(::OSSL_QUIC_client_method());
				// the following would spawn a thread to deal with the QUIC timing
				//return ::SSL_CTX_new(::OSSL_QUIC_client_thread_method();
			}
#else // of DEKAF2_HAS_OPENSSL_QUIC
			kDebug(1, "QUIC protocol not supported by this build");
#endif // of DEKAF2_HAS_OPENSSL_QUIC
			break;
	}
#endif // of DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT
	return nullptr;

} // CreateContext

//-----------------------------------------------------------------------------
KTLSContext::KTLSContext(bool bIsServer, Transport transport)
//-----------------------------------------------------------------------------
#if (DEKAF2_CLASSIC_ASIO)
: m_Context(s_IO_Service, boost::asio::ssl::context::sslv23)
#elif (!DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT)
: m_Context(bIsServer ? boost::asio::ssl::context::tls_server : boost::asio::ssl::context::tls_client)
#else
: m_Context(CreateContext(bIsServer, transport))
#endif
, m_Role(bIsServer ? boost::asio::ssl::stream_base::server : boost::asio::ssl::stream_base::client)
{
#if (!DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT)
	if (transport == Transport::Quic)
	{
		kDebug(1, "you need a boost version >= 1.73 to enable QUIC");
	}
#endif

	SetDefaults();
}

//-----------------------------------------------------------------------------
bool KTLSContext::SetDefaults()
//-----------------------------------------------------------------------------
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
#if (!DEKAF2_CLASSIC_ASIO)
		| boost::asio::ssl::context::no_tlsv1_1
#endif
		| boost::asio::ssl::context::no_tlsv1;
	
	boost::system::error_code ec;
	m_Context.set_options(options, ec);

	if (ec)
	{
		return SetError(kFormat("error setting TLS options {}: {}", options, ec.message()));
	}

	// set the system default cert paths, but do not yet switch verify mode on
	// (we do that individually for each connect on request)
	m_Context.set_default_verify_paths(ec);

	if (ec)
	{
		return SetError(kFormat("error setting TLS verify paths: {}", ec.message()));
	}

	return true;

} // SetDefaults

//-----------------------------------------------------------------------------
bool KTLSContext::LoadTLSCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KTLSContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

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

} // LoadTLSCertificates

//-----------------------------------------------------------------------------
bool KTLSContext::SetTLSCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
#if (BOOST_VERSION < 105400)

	kDebug(1, "you need to link against at least boost 1.54 for buffered TLS certificates");
	return false;

#else

	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KTLSContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

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

} // SetTLSCertificates

//-----------------------------------------------------------------------------
bool KTLSContext::SetDHPrimes(KStringView sDHPrimes)
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
bool KTLSContext::SetAllowedCipherSuites(KStringView sCipherSuites)
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
			::SSL_CTX_set_cipher_list(m_Context.native_handle(), "");
		}
		else
		{
			auto sCiphers = kJoined(CipherV12, ":");
			kDebug(2, "set TLSv{} cipher suites {}", "1.2", sCiphers);
			if (::SSL_CTX_set_cipher_list (m_Context.native_handle(), sCiphers.c_str()))
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
		if (::SSL_CTX_set_ciphersuites(m_Context.native_handle(), sCiphers.c_str()))
		{
			bSuccess = true;
		}
		else
		{
			SetError(kFormat("setting TLSv{} cipher suites failed: {}", "1.3", sCiphers));
		}
#else
		kDebug(1, "TLSv1.3 is not supported by the linked TLS library\ncannot set TLSv1.3 cipher suites {}", sCiphers);
#endif
	}

	return bSuccess;

} // SetAllowedCipherSuites

#if DEKAF2_HAS_NGHTTP2

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
bool KTLSContext::SetAllowHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2
	// allow ALPN negotiation for HTTP/2 if this is a client
	if (GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto sProto = bAlsoAllowHTTP1 ? "\x02h2\0x08http/1.1" : "\x02h2";
		auto iResult = ::SSL_CTX_set_alpn_protos(m_Context.native_handle(),
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
			::SSL_CTX_set_alpn_select_cb(m_Context.native_handle(),
			                             alpn_select_proto_cb,
			                             bAlsoAllowHTTP1 ? this : nullptr); // we use the user ptr as a flag
		}
#else  // of DEKAF2_ALLOW_HTTP2_SERVER_MODE
		kDebug(1, "HTTP/2 is only supported in client mode");
#endif // of DEKAF2_ALLOW_HTTP2_SERVER_MODE
	}
#else  // of DEKAF2_HAS_NGHTTP2
		kDebug(2, "HTTP/2 is not supported by this build");
#endif // of DEKAF2_HAS_NGHTTP2

	return false;

} // SetAllowHTTP2

//-----------------------------------------------------------------------------
bool KTLSContext::SetALPNRaw(KStringView sALPN)
//-----------------------------------------------------------------------------
{
	if (GetRole() != boost::asio::ssl::stream_base::client)
	{
		kDebug(1, "ALPN setup only supported in client mode");
		return false;
	}

	auto iResult = ::SSL_CTX_set_alpn_protos(m_Context.native_handle(),
	                                         reinterpret_cast<const unsigned char*>(sALPN.data()),
	                                         static_cast<unsigned int>(sALPN.size()));

	if (iResult == 0)
	{
		return true;
	}

	kDebug(1, "failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sALPN), iResult);

	return false;

} // SetALPNRaw

//-----------------------------------------------------------------------------
bool KTLSContext::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

DEKAF2_NAMESPACE_END

