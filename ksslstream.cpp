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

namespace dekaf2 {

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
KSSLContext::KSSLContext(bool bIsServer, bool bVerifyCerts)
//-----------------------------------------------------------------------------
#if (BOOST_VERSION < 106600)
	: m_Context(s_IO_Service, boost::asio::ssl::context::sslv23)
#else
	: m_Context(bIsServer ? boost::asio::ssl::context::tls_server : boost::asio::ssl::context::tls_client)
#endif
	, m_Role(bIsServer ? boost::asio::ssl::stream_base::server : boost::asio::ssl::stream_base::client)
	, m_bVerify(bVerifyCerts)
 {
	 boost::asio::ssl::context::options options
	 	= boost::asio::ssl::context::default_workarounds
	 	| boost::asio::ssl::context::single_dh_use
	 	| boost::asio::ssl::context::no_sslv2
	 	| boost::asio::ssl::context::no_sslv3
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

	if (bVerifyCerts)
	{
		m_Context.set_default_verify_paths();
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

	kDebug(2, "TLS certificates successfully loaded");
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

	kDebug(2, "TLS certificates successfully set");
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
			kDebug(2, "set TLSv1.2 cipher suites {}", sCiphers);
			if (SSL_CTX_set_cipher_list (m_Context.native_handle(), sCiphers.c_str()))
			{
				bSuccess = true;
			}
			else
			{
				SetError(kFormat("setting TLSv1.2 cipher suites failed: {}", sCiphers));
			}
		}
	}

	if (!CipherV13.empty())
	{
		auto sCiphers = kJoined(CipherV13, ":");
#ifdef DEKAF2_HAS_TLSv13
		kDebug(2, "set TLSv1.3 cipher suites {}", sCiphers);
		if (SSL_CTX_set_ciphersuites(m_Context.native_handle(), sCiphers.c_str()))
		{
			bSuccess = true;
		}
		else
		{
			SetError(kFormat("setting TLSv1.3 cipher suites failed: {}", sCiphers));
		}
#else
		kDebug(1, "TLSv1.3 is not supported by the linked SSL library\ncannot set TLSv1.3 cipher suites {}", sCiphers);
#endif
	}

	return bSuccess;

} // SetAllowedCipherSuites

//-----------------------------------------------------------------------------
bool KSSLContext::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

static KSSLContext s_KSSLContextNoVerification   { false, false };
static KSSLContext s_KSSLContextWithVerification { false, true  };


//-----------------------------------------------------------------------------
bool KSSLIOStream::handshake(KAsioSSLStream<asiostream>* stream)
//-----------------------------------------------------------------------------
{
	if (!stream->bNeedHandshake)
	{
		return true;
	}

	stream->bNeedHandshake = false;
	
	stream->Socket.async_handshake(stream->SSLContext.GetRole(),
								   [&](const boost::system::error_code& ec)
	{
		stream->ec = ec;
	});

	stream->RunTimed();

	if (stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
	{
		kDebug(1, "ssl handshake failed with {}: {}",
			   stream->sEndpoint,
			   stream->ec.message());
		return false;
	}

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
std::streamsize KSSLIOStream::SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioSSLStream<asiostream>*>(stream_);

		if (!stream->bManualHandshake)
		{
			if (!handshake(stream))
			{
				return -1;
			}
		}

		if (!stream->bManualHandshake)
		{
			stream->Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				stream->ec = ec;
				iRead = bytes_transferred;
			});
		}
		else
		{
			stream->Socket.next_layer().async_read_some(boost::asio::buffer(sBuffer, iCount),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				stream->ec = ec;
				iRead = bytes_transferred;
			});
		}

		stream->RunTimed();

		if (iRead == 0 || stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
		{
			if (stream->ec.value() == boost::asio::error::eof)
			{
				kDebug(2, "input stream got closed by endpoint {}", stream->sEndpoint);
			}
			else
			{
				kDebug(1, "cannot read from tls stream with endpoint {}: {}",
					   stream->sEndpoint,
					   stream->ec.message());
			}
		}
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

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioSSLStream<asiostream>*>(stream_);

		if (!stream->bManualHandshake)
		{
			if (!handshake(stream))
			{
				return -1;
			}
		}

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart{0};

			if (!stream->bManualHandshake)
			{
				stream->Socket.async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					stream->ec = ec;
					iWrotePart = bytes_transferred;
				});
			}
			else
			{
				stream->Socket.next_layer().async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					stream->ec = ec;
					iWrotePart = bytes_transferred;
				});
			}

			stream->RunTimed();

			iWrote += iWrotePart;

			if (iWrotePart == 0 || stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
			{
				if (stream->ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "output stream got closed by endpoint {}", stream->sEndpoint);
				}
				else
				{
					kDebug(1, "cannot write to tls stream with endpoint {}: {}",
						   stream->sEndpoint,
						   stream->ec.message());
				}
				break;
			}
		}
	}

	return iWrote;

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(int iSecondsTimeout, bool bManualHandshake)
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
	, m_Stream(s_KSSLContextNoVerification, iSecondsTimeout, bManualHandshake)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(KSSLContext& Context, int iSecondsTimeout, bool bManualHandshake)
//-----------------------------------------------------------------------------
	: base_type(&m_SSLStreamBuf)
	, m_Stream(Context, iSecondsTimeout, bManualHandshake)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const KTCPEndPoint& Endpoint, int iSecondsTimeout, bool bManualHandshake)
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
	, m_Stream(s_KSSLContextNoVerification, iSecondsTimeout, bManualHandshake)
{
	Connect(Endpoint);
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	m_Stream.bNeedHandshake = true;

	kDebug(2, "resolving domain {}", Endpoint.Domain.get());

	boost::asio::ip::tcp::resolver Resolver(m_Stream.IOService);
	boost::asio::ip::tcp::resolver::query query(Endpoint.Domain.get().c_str(), Endpoint.Port.Serialize().c_str());
	auto hosts = Resolver.resolve(query, m_Stream.ec);

	if (Good())
	{
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

		if (m_Stream.SSLContext.GetVerify())
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_peer
										  | boost::asio::ssl::verify_fail_if_no_peer_cert);
		}
		else
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_none);
		}

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

		kDebug(2, "trying to connect to endpoint {}", Endpoint.Serialize());

		m_Stream.RunTimed();
	}

	if (!Good() || !m_Stream.Socket.lowest_layer().is_open())
	{
		kDebug(1, "{}: {}", Endpoint.Serialize(), Error());
		return false;
	}

	// make sure client side SNI works..
	SSL_set_tlsext_host_name(m_Stream.Socket.native_handle(), Endpoint.Domain.get().c_str());

	kDebug(2, "connected to endpoint {}", Endpoint.Serialize());

	return true;

} // connect


//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLServer(KSSLContext& Context)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>(Context);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(bool bVerifyCerts, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLClient>(bVerifyCerts ? s_KSSLContextWithVerification : s_KSSLContextNoVerification, iSecondsTimeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(const KTCPEndPoint& EndPoint, bool bVerifyCerts, bool bManualHandshake, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKSSLClient(bVerifyCerts, iSecondsTimeout);

	if (bManualHandshake)
	{
		Client->SetManualTLSHandshake(true);
	}

	Client->Connect(EndPoint);

	return Client;

} // CreateKSSLClient

} // end of namespace dekaf2

