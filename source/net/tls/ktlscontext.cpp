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

#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/kfrozen.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <dekaf2/net/address/kipaddress.h>

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
	#define DEKAF2_HAS_TLS_CLIENT_HELLO_CB 1
#else
	#define DEKAF2_HAS_TLS_CLIENT_HELLO_CB 0
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
// index for the ALPN preference list stored as SSL_CTX ex_data - the free
// function ties the string's lifetime to that of the SSL_CTX, which in-flight
// handshakes keep alive after an SNI context switch
int GetALPNExDataIndex()
//-----------------------------------------------------------------------------
{
	static int s_iIndex = ::SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr,
	[](void*, void* ptr, CRYPTO_EX_DATA*, int, long, void*)
	{
		delete static_cast<KString*>(ptr);
	});

	return s_iIndex;

} // GetALPNExDataIndex

//-----------------------------------------------------------------------------
// SNI hostnames are case insensitive, and may carry a trailing root dot
KString NormalizeSNIName(KStringView sHostname)
//-----------------------------------------------------------------------------
{
	sHostname.remove_suffix('.');
	return sHostname.ToLowerASCII();

} // NormalizeSNIName

#if DEKAF2_HAS_TLS_CLIENT_HELLO_CB

//-----------------------------------------------------------------------------
// extract the first host_name entry from a raw server_name extension (RFC 6066):
// uint16 list size, then entries of uint8 name type (0 = host_name), uint16 size, name
KStringView ParseSNIHostName(const unsigned char* pData, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	if (iSize < 2)
	{
		return {};
	}

	std::size_t iList = (pData[0] << 8) | pData[1];
	pData += 2;
	iSize -= 2;

	if (iList < iSize)
	{
		iSize = iList;
	}

	while (iSize >= 3)
	{
		auto iType        = pData[0];
		std::size_t iName = (pData[1] << 8) | pData[2];
		pData += 3;
		iSize -= 3;

		if (iName > iSize)
		{
			break;
		}

		if (iType == 0)
		{
			return { reinterpret_cast<const char*>(pData), iName };
		}

		pData += iName;
		iSize -= iName;
	}

	return {};

} // ParseSNIHostName

//-----------------------------------------------------------------------------
// extract the protocol names from a raw ALPN extension (RFC 7301):
// uint16 list size, then entries of uint8 size, name
std::vector<KStringView> ParseALPNProtocols(const unsigned char* pData, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	std::vector<KStringView> Protocols;

	if (iSize < 2)
	{
		return Protocols;
	}

	std::size_t iList = (pData[0] << 8) | pData[1];
	pData += 2;
	iSize -= 2;

	if (iList < iSize)
	{
		iSize = iList;
	}

	while (iSize >= 1)
	{
		std::size_t iProto = pData[0];
		pData += 1;
		iSize -= 1;

		if (!iProto || iProto > iSize)
		{
			break;
		}

		Protocols.push_back({ reinterpret_cast<const char*>(pData), iProto });
		pData += iProto;
		iSize -= iProto;
	}

	return Protocols;

} // ParseALPNProtocols

#endif // of DEKAF2_HAS_TLS_CLIENT_HELLO_CB

} // end of anonymous namespace

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
#if (DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT && OPENSSL_VERSION_NUMBER >= 0x10100000L)
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

		case Transport::DTls:
			if (bIsServer)
			{
				return ::SSL_CTX_new(::DTLS_server_method());
			}
			else
			{
				return ::SSL_CTX_new(::DTLS_client_method());
			}
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
#elif (!DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT || OPENSSL_VERSION_NUMBER < 0x10100000L)
: m_Context(bIsServer ? boost::asio::ssl::context::tls_server : boost::asio::ssl::context::tls_client)
#else
: m_Context(CreateContext(bIsServer, transport))
#endif
, m_Role(bIsServer ? boost::asio::ssl::stream_base::server : boost::asio::ssl::stream_base::client)
{
#if (!DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT || OPENSSL_VERSION_NUMBER < 0x10100000L)
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
		return SetError(kFormat("error setting TLS verify path: {}", ec.message()));
	}

#ifdef DEKAF2_SYSTEM_CERTIFICATE_PATH

	KString sVerifyPath = DEKAF2_SYSTEM_CERTIFICATE_PATH;

	if (!sVerifyPath.empty())
	{
		return SetAdditionalTLSVerifyPath(sVerifyPath);
	}

#endif
	// when adding more code after the endif here,
	// modify the return condition above to only return on false
	return true;

} // SetDefaults

//-----------------------------------------------------------------------------
bool KTLSContext::SetAdditionalTLSVerifyPath(KStringView sVerifyPath)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

	kDebug(2, "setting additional TLS verify path: {}", sVerifyPath);
//	kPrintLine("setting additional TLS verify path: {}", sVerifyPath);

	// set the system default cert paths, but do not yet switch verify mode on
	// (we do that individually for each connect on request)
	m_Context.add_verify_path(sVerifyPath, ec);

	if (ec)
	{
		return SetError(kFormat("error setting TLS verify path {}: {}", sVerifyPath, ec.message()));
	}

	return true;

} // SetAdditionalTLSVerifyPath

//-----------------------------------------------------------------------------
bool KTLSContext::ClientHello::HasALPN(KStringView sProtocol) const
//-----------------------------------------------------------------------------
{
	for (auto sALPN : ALPNs)
	{
		if (sALPN == sProtocol)
		{
			return true;
		}
	}

	return false;

} // ClientHello::HasALPN

//-----------------------------------------------------------------------------
bool KTLSContext::InstallSNIDispatch()
//-----------------------------------------------------------------------------
{
	auto SNI = m_SNIDispatch.unique();

	if (!SNI->bInstalled)
	{
#if DEKAF2_HAS_TLS_CLIENT_HELLO_CB
		::SSL_CTX_set_client_hello_cb(m_Context.native_handle(), &ClientHelloCallback, this);
#else
		SSL_CTX_set_tlsext_servername_callback(m_Context.native_handle(), &ServerNameCallback);
		SSL_CTX_set_tlsext_servername_arg(m_Context.native_handle(), this);
#endif
		SNI->bInstalled = true;
	}

	return true;

} // InstallSNIDispatch

//-----------------------------------------------------------------------------
bool KTLSContext::AddSNIContext(KStringView sHostname, std::shared_ptr<KTLSContext> Context)
//-----------------------------------------------------------------------------
{
	if (GetRole() != boost::asio::ssl::stream_base::server)
	{
		return SetError("SNI dispatch requires a server context");
	}

	if (!Context || Context->GetRole() != boost::asio::ssl::stream_base::server)
	{
		return SetError("the SNI context must be a server context");
	}

	if (Context.get() == this)
	{
		return SetError("cannot add a context to itself");
	}

	auto sName = NormalizeSNIName(sHostname);

	if (sName.empty())
	{
		return SetError("empty SNI hostname");
	}

	if (!InstallSNIDispatch())
	{
		return false;
	}

	kDebug(2, "adding SNI context for {}", sName);

	m_SNIDispatch.unique()->Hosts[std::move(sName)] = std::move(Context);

	return true;

} // AddSNIContext

//-----------------------------------------------------------------------------
bool KTLSContext::RemoveSNIContext(KStringView sHostname)
//-----------------------------------------------------------------------------
{
	return m_SNIDispatch.unique()->Hosts.erase(NormalizeSNIName(sHostname)) > 0;

} // RemoveSNIContext

//-----------------------------------------------------------------------------
std::shared_ptr<KTLSContext> KTLSContext::GetSNIContext(KStringView sHostname) const
//-----------------------------------------------------------------------------
{
	auto sName = NormalizeSNIName(sHostname);

	if (!sName.empty())
	{
		auto SNI = m_SNIDispatch.shared();

		if (!SNI->Hosts.empty())
		{
			auto it = SNI->Hosts.find(sName);

			if (it != SNI->Hosts.end())
			{
				return it->second;
			}

			// no exact match - a wildcard entry may match the leftmost label
			auto iDot = sName.find('.');

			if (iDot != KString::npos && iDot > 0 && iDot + 1 < sName.size())
			{
				KString sWildcard("*");
				sWildcard += sName.ToView(iDot);

				it = SNI->Hosts.find(sWildcard);

				if (it != SNI->Hosts.end())
				{
					return it->second;
				}
			}
		}
	}

	return nullptr;

} // GetSNIContext

//-----------------------------------------------------------------------------
bool KTLSContext::SetSNICallback(SNICallback Callback)
//-----------------------------------------------------------------------------
{
	if (GetRole() != boost::asio::ssl::stream_base::server)
	{
		return SetError("SNI dispatch requires a server context");
	}

	if (!InstallSNIDispatch())
	{
		return false;
	}

	m_SNIDispatch.unique()->Callback = std::move(Callback);

	return true;

} // SetSNICallback

//-----------------------------------------------------------------------------
std::shared_ptr<KTLSContext> KTLSContext::ResolveSNI(const ClientHello& Hello) const
//-----------------------------------------------------------------------------
{
	SNICallback Callback;

	{
		// copy the callback out of the lock - it may itself call GetSNIContext()
		Callback = m_SNIDispatch.shared()->Callback;
	}

	if (Callback)
	{
		auto Context = Callback(Hello);

		if (Context)
		{
			if (Context->GetRole() == boost::asio::ssl::stream_base::server)
			{
				return Context;
			}

			kDebug(1, "SNI callback returned a client context - ignored");
		}
	}

	if (!Hello.sServerName.empty())
	{
		return GetSNIContext(Hello.sServerName);
	}

	return nullptr;

} // ResolveSNI

#if DEKAF2_HAS_TLS_CLIENT_HELLO_CB

//-----------------------------------------------------------------------------
int KTLSContext::ClientHelloCallback(ssl_st* ssl, int* al, void* arg)
//-----------------------------------------------------------------------------
{
	auto* self = static_cast<KTLSContext*>(arg);

	if (self)
	{
		ClientHello Hello;

		const unsigned char* pExt;
		std::size_t iExt;

		if (::SSL_client_hello_get0_ext(ssl, TLSEXT_TYPE_server_name, &pExt, &iExt))
		{
			Hello.sServerName = ParseSNIHostName(pExt, iExt);
		}

		if (::SSL_client_hello_get0_ext(ssl, TLSEXT_TYPE_application_layer_protocol_negotiation, &pExt, &iExt))
		{
			Hello.ALPNs = ParseALPNProtocols(pExt, iExt);
		}

		auto Context = self->ResolveSNI(Hello);

		if (Context)
		{
			kDebug(3, "switching TLS context for SNI host {}", Hello.sServerName);
			::SSL_set_SSL_CTX(ssl, Context->m_Context.native_handle());
		}
	}

	return SSL_CLIENT_HELLO_SUCCESS;

} // ClientHelloCallback

#else // of DEKAF2_HAS_TLS_CLIENT_HELLO_CB

//-----------------------------------------------------------------------------
int KTLSContext::ServerNameCallback(ssl_st* ssl, int* al, void* arg)
//-----------------------------------------------------------------------------
{
	auto* self = static_cast<KTLSContext*>(arg);

	if (self)
	{
		ClientHello Hello;

		auto* sName = ::SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

		if (sName)
		{
			Hello.sServerName = sName;
		}

		auto Context = self->ResolveSNI(Hello);

		if (Context)
		{
			kDebug(3, "switching TLS context for SNI host {}", Hello.sServerName);
			::SSL_set_SSL_CTX(ssl, Context->m_Context.native_handle());
		}
	}

	return SSL_TLSEXT_ERR_OK;

} // ServerNameCallback

#endif // of DEKAF2_HAS_TLS_CLIENT_HELLO_CB

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
		static const char sH2[]   = "\x02h2";
		static const char sH2H1[] = "\x02h2\x08http/1.1";
		auto  sProto  = bAlsoAllowHTTP1 ? sH2H1 : sH2;
		auto  iResult = ::SSL_CTX_set_alpn_protos(m_Context.native_handle(),
		                                         reinterpret_cast<const unsigned char*>(sProto),
		                                         static_cast<unsigned int>(bAlsoAllowHTTP1 ? sizeof(sH2H1) - 1 : sizeof(sH2) - 1));
		if (iResult == 0)
		{
			return true;
		}
		return SetError(kFormat("failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sProto), iResult));
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
int KTLSContext::ALPNSelectCallback(ssl_st* ssl, const unsigned char** out, unsigned char* outlen,
                                    const unsigned char* in, unsigned int inlen, void* arg)
//-----------------------------------------------------------------------------
{
	// read the preference list from the current SSL_CTX - after an SNI dispatch this
	// is the selected context, not the one the connection was accepted with
	auto* Ctx   = ::SSL_get_SSL_CTX(ssl);
	auto* sALPN = Ctx ? static_cast<const KString*>(::SSL_CTX_get_ex_data(Ctx, GetALPNExDataIndex())) : nullptr;

	if (sALPN && !sALPN->empty())
	{
		unsigned char* pSelected;
		unsigned char  iSelected;

		if (::SSL_select_next_proto(&pSelected, &iSelected,
		                            reinterpret_cast<const unsigned char*>(sALPN->data()),
		                            static_cast<unsigned int>(sALPN->size()),
		                            in, inlen) == OPENSSL_NPN_NEGOTIATED)
		{
			*out    = pSelected;
			*outlen = iSelected;
			return SSL_TLSEXT_ERR_OK;
		}
	}

	// RFC 7301: no overlap is answered with a fatal no_application_protocol alert
	return SSL_TLSEXT_ERR_ALERT_FATAL;

} // ALPNSelectCallback

//-----------------------------------------------------------------------------
bool KTLSContext::SetALPNRaw(KStringView sALPN)
//-----------------------------------------------------------------------------
{
	if (GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto iResult = ::SSL_CTX_set_alpn_protos(m_Context.native_handle(),
		                                         reinterpret_cast<const unsigned char*>(sALPN.data()),
		                                         static_cast<unsigned int>(sALPN.size()));

		if (iResult == 0)
		{
			return true;
		}

		return SetError(kFormat("failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sALPN), iResult));
	}

	// server mode: store the preference list in the SSL_CTX itself (see ALPNSelectCallback)
	auto* Ctx        = m_Context.native_handle();
	auto* sProtocols = new KString(sALPN);
	auto* sOld       = static_cast<KString*>(::SSL_CTX_get_ex_data(Ctx, GetALPNExDataIndex()));

	if (!::SSL_CTX_set_ex_data(Ctx, GetALPNExDataIndex(), sProtocols))
	{
		delete sProtocols;
		return SetError("cannot store ALPN protocol list");
	}

	delete sOld;

	::SSL_CTX_set_alpn_select_cb(Ctx, &ALPNSelectCallback, nullptr);

	return true;

} // SetALPNRaw

//-----------------------------------------------------------------------------
KString KTLSContext::SetClientIdentity(ssl_st* ssl, KStringView sHostname, bool bVerifyCert)
//-----------------------------------------------------------------------------
{
	// an IPv6 address may come in the brackets of a URL
	if (sHostname.size() > 2 && sHostname.front() == '[' && sHostname.back() == ']')
	{
		sHostname.remove_prefix(1);
		sHostname.remove_suffix(1);
	}

	// the OpenSSL calls need a C string
	KString sHost(sHostname);

	bool bIsIP = kIsIPv4Address(sHost) || kIsIPv6Address(sHost, false);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	if (bVerifyCert)
	{
		auto* Param = ::SSL_get0_param(ssl);

		if (bIsIP)
		{
			// clear a name set earlier - both would have to match otherwise
			::X509_VERIFY_PARAM_set1_host(Param, nullptr, 0);

			if (!::X509_VERIFY_PARAM_set1_ip_asc(Param, sHost.c_str()))
			{
				return kFormat("failed to set the certificate verification address: {}", sHost);
			}
		}
		else
		{
			::X509_VERIFY_PARAM_set1_ip(Param, nullptr, 0);

			if (!::SSL_set1_host(ssl, sHost.c_str()))
			{
				return kFormat("failed to set the certificate verification hostname: {}", sHost);
			}
		}
	}
#endif

	// RFC 6066 does not permit IP literals in SNI - send none, and drop one set earlier
	// (a separate variable, the OpenSSL macro does not parenthesize its argument)
	const char* szSNI = bIsIP ? nullptr : sHost.c_str();

	if (!::SSL_set_tlsext_host_name(ssl, szSNI))
	{
		return kFormat("failed to set SNI hostname: {}", sHost);
	}

	return KString{};

} // SetClientIdentity

DEKAF2_NAMESPACE_END

