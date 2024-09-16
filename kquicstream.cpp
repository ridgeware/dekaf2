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

#include "kquicstream.h"

#if DEKAF2_HAS_OPENSSL_QUIC

#include "klog.h"
#include "kscopeguard.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>

DEKAF2_NAMESPACE_BEGIN

static KTLSContext s_KQuicClientContext { false, KTLSContext::Transport::Quic };

//-----------------------------------------------------------------------------
bool KQuicStream::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	// QUIC handshakes immediately - we just return if the stream is good (no errors)
	return Good();
}

//-----------------------------------------------------------------------------
bool KQuicStream::Handshake()
//-----------------------------------------------------------------------------
{
	if (!m_bNeedHandshake)
	{
		return true;
	}

	m_bNeedHandshake = false;

	kDebug(3, "trying to connect");

	// switch to non-blocking mode for the handshake

	auto ssl = GetNativeTLSHandle();

	auto oldBlockingMode = ::SSL_get_blocking_mode(ssl);

	if (oldBlockingMode)
	{
		// this sets (temporarily) non-blocking mode to the QUIC stack
		if (!  ::SSL_set_blocking_mode(ssl, 0)
			|| ::SSL_get_blocking_mode(ssl))
		{
			return SetError("cannot switch to non-blocking mode");
		}
	}

	for (;;)
	{
		auto ec = ::SSL_connect(ssl);

		if (ec < 1)
		{
			auto iError = ::SSL_get_error(ssl, ec);

			if (iError == SSL_ERROR_WANT_WRITE)
			{
				if (!IsWriteReady()) return false; // error is already set
			}
			else if (iError == SSL_ERROR_WANT_READ)
			{
				if (!IsReadReady()) return false; // error is already set
			}
			else
			{
				KString sWhat;

				if (::SSL_get_verify_result(ssl) != X509_V_OK)
				{
					sWhat.Format("Verify error: {}", ::X509_verify_cert_error_string(::SSL_get_verify_result(GetNativeTLSHandle())));
				}

				return SetError(kFormat("Quic handshake failed: {}", sWhat));
			}
		}
		else
		{
			break;
		}
	}

	// reinstate the old blocking mode
	if (oldBlockingMode)
	{
		::SSL_set_blocking_mode(ssl, 1);
		if (!::SSL_get_blocking_mode(ssl))
		{
			return SetError("cannot switch back to blocking mode");
		}
	}

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
		kDebug(2, "Quic handshake successful");

		auto cipher = ::SSL_get_current_cipher(ssl);
		kDebug(2, "Quic version: {}, cipher: {}",
			   ::SSL_CIPHER_get_version(cipher),
			   ::SSL_CIPHER_get_name(cipher));

		auto compress  = ::SSL_get_current_compression(ssl);
		auto expansion = ::SSL_get_current_expansion(ssl);

		if (compress || expansion)
		{
			kDebug(2, "Quic compression: {}, expansion: {}",
				   compress  ? ::SSL_COMP_get_name(compress ) : "NONE",
				   expansion ? ::SSL_COMP_get_name(expansion) : "NONE");
		}
	}
#endif

	return true;

} // handshake

//-----------------------------------------------------------------------------
bool KQuicStream::SetRequestHTTP3()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP3
	// allow ALPN negotiation for HTTP/3 if this is a client
	if (GetContext().GetRole() == boost::asio::ssl::stream_base::client)
	{
		auto sALPN = "\x02h3";
		auto iResult = SSL_set_alpn_protos(GetNativeTLSHandle(),
		                                   reinterpret_cast<const unsigned char*>(sALPN),
		                                   static_cast<unsigned int>(strlen(sALPN)));
		if (iResult == 0)
		{
			kDebug(2, "successfully initialized ALPN to h3");
			return true;
		}
		return SetError(kFormat("failed to set ALPN protocol: '{}' - error {}", kEscapeForLogging(sALPN), iResult));
	}
	else
	{
		kDebug(1, "HTTP/3 is only supported in client mode");
	}
#else  // of DEKAF2_HAS_NGHTTP3
	kDebug(2, "HTTP/3 is not supported by this build");
#endif // of DEKAF2_HAS_NGHTTP3

	return false;

} // SetRequestHTTP3

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::direct_read_some(void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	if (IsReadReady())
	{
		if (!::SSL_read_ex(GetNativeTLSHandle(), sBuffer, iCount, reinterpret_cast<size_t*>(&iRead)))
		{
			SetSSLError();
		}
	}

	return iRead;

} // direct_read_some

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::QuicStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead { 0 };

	if (stream_)
	{
		auto& Quic = *static_cast<KQuicStream*>(stream_);

		iRead = Quic.direct_read_some(sBuffer, iCount);
	}

	return iRead;

} // QuicStreamReader

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::QuicStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we need to loop the writer, as write_some() has an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs

	std::streamsize iWrote { 0 };

	if (stream_)
	{
		auto& Quic = *static_cast<KQuicStream*>(stream_);

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart{0};

			if (Quic.IsWriteReady())
			{
				if (!::SSL_write_ex(Quic.GetNativeTLSHandle(), static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote, &iWrotePart))
				{
					Quic.SetSSLError();
				}
			}

			iWrote += iWrotePart;
		}
	}

	return iWrote;

} // QuicStreamWriter

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream()
//-----------------------------------------------------------------------------
: KQuicStream(s_KQuicClientContext, KStreamOptions::GetDefaultTimeout())
{
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
: base_type(&m_QuicStreamBuf, Timeout)
, m_TLSContext(Context)
, m_SSL(SSL_new(Context.GetContext().native_handle()))
{
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(KTLSContext& Context, 
                         const KTCPEndPoint& Endpoint,
                         KStreamOptions Options)
//-----------------------------------------------------------------------------
: KQuicStream(Context, Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: KQuicStream(s_KQuicClientContext, Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

namespace {
	// OPENSSL_free is a macro (and has thus no type). Create a wrapper that
	// can be used for the deleter of std::unique_ptr<>
	void MyOPENSSL_free(char* address)
	{
		OPENSSL_free(address);
	}
}

//-----------------------------------------------------------------------------
bool KQuicStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	if (!GetNativeTLSHandle())
	{
		return SetError("no OpenSSL object");
	}

	SetTimeout(Options.GetTimeout());

	SetUnresolvedEndPoint(Endpoint);

#if DEKAF2_QUIC_DEBUG
	::SSL_set_msg_callback(GetNativeTLSHandle(), SSL_trace);
	::SSL_set_msg_callback_arg(GetNativeTLSHandle(), BIO_new_fp(stderr, BIO_NOCLOSE));
#endif

	m_bNeedHandshake = true;

	if (m_NativeSocket >= 0)
	{
		::BIO_closesocket(m_NativeSocket);
		m_NativeSocket = -1;
	}

	auto& sHostname = Endpoint.Domain.get();
	kDebug(2, "resolving domain {}", sHostname);

	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is a ip v6 numerical address
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	if (sIPAddress.empty())
	{
		// check the known hostnames
		sIPAddress = KIOStreamSocket::GetKnownHostAddress(sHostname, Options.GetFamily());
		// sIPAddress now either contains a known IP address, or the original hostname..
	}

	::BIO_ADDRINFO *res_local;
	// lookup the ip address
	if (!::BIO_lookup_ex(sIPAddress.c_str(),
	                     Endpoint.Port.Serialize().c_str(),
	                     BIO_LOOKUP_CLIENT,
	                     Options.GetNativeFamily(),
	                     SOCK_DGRAM, 0,
	                     &res_local))
	{
		return SetError("error in address lookup");
	}

	KUniquePtr<::BIO_ADDRINFO, ::BIO_ADDRINFO_free> Hosts(res_local);

	kDebug(2, "trying to connect to {} {}", "endpoint", Endpoint);

	const ::BIO_ADDRINFO *ai;

	for (ai = Hosts.get(); ai != nullptr; ai = ::BIO_ADDRINFO_next(ai))
	{
		// create the socket
		m_NativeSocket = ::BIO_socket(::BIO_ADDRINFO_family(ai), SOCK_DGRAM, 0, 0);

		if (m_NativeSocket == -1)
		{
			continue;
		}

#if 0
		// this is currently bugged at least with OpenSSL/MacOS 3.2/3 - simply do not
		// connect here, will happen later at handshake (using the address set
		// with ::SSL_set1_initial_peer_addr() below
		// connect to server
		if (!::BIO_connect(m_NativeSocket, ::BIO_ADDRINFO_address(ai), 0))
		{
			::BIO_closesocket(m_NativeSocket);
			m_NativeSocket = -1;
			continue;
		}
#endif
		// set to non-blocking
		if (!::BIO_socket_nbio(m_NativeSocket, 1))
		{
			kDebug(1, "{}: cannot switch to non-blocking mode", Endpoint);
			::BIO_closesocket(m_NativeSocket);
			m_NativeSocket = -1;
			continue;
		}

		break;
	}

	if (m_NativeSocket < 0)
	{
		return SetError("cannot connect");
	}

	{
		// get a pointer on the peer address - we do not need to wrap this,
		// it is a pointer into a struct that is already wrapped in a std::unique_ptr
		auto peer_addr = ::BIO_ADDRINFO_address(ai);

		if (!peer_addr)
		{
			return SetError("cannot get peer address");
		}

		KUniquePtr<char, MyOPENSSL_free> ipaddress(::BIO_ADDR_hostname_string(peer_addr, 1)); // 1 means numeric
		KUniquePtr<char, MyOPENSSL_free> service  (::BIO_ADDR_service_string (peer_addr, 1)); // 1 means numeric

		if (::BIO_ADDR_family(peer_addr) == AF_INET6)
		{
			SetEndPointAddress(kFormat("[{}]:{}", ipaddress.get(), service.get()));
		}
		else
		{
			SetEndPointAddress(kFormat("{}:{}", ipaddress.get(), service.get()));
		}

		if (!::SSL_set1_initial_peer_addr(GetNativeTLSHandle(), peer_addr))
		{
			return SetError(kFormat("failed to set initial peer address: {}", ipaddress.get()));
		}
	}

	{
		// create a BIO to wrap the socket
		::BIO* bio = ::BIO_new(::BIO_s_datagram());

		if (bio == nullptr)
		{
			return SetError("cannot create a BIO context");
		}

		/*
		 * Associate the newly created BIO with the underlying socket. By
		 * passing BIO_CLOSE here the socket will be automatically closed when
		 * the BIO is freed. Alternatively you can use BIO_NOCLOSE, in which
		 * case you must close the socket explicitly when it is no longer
		 * needed.
		 */
		::BIO_set_fd(bio, m_NativeSocket, BIO_CLOSE);

		// and tell OpenSSL to use this BIO - it takes ownership
		::SSL_set_bio(GetNativeTLSHandle(), bio, bio);
	}

	if (!::SSL_set1_host(GetNativeTLSHandle(), sHostname.c_str()))
	{
		return SetError(kFormat("Failed to set the certificate verification hostname: {}", sHostname));
	}

	// QUIC mandates peer verification, but we still allow to switch it off
	// for testing
	if (Options.IsSet(KStreamOptions::VerifyCert))
	{
		// returns void
		::SSL_set_verify(GetNativeTLSHandle(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
	}
	else
	{
		// returns void
		::SSL_set_verify(GetNativeTLSHandle(), SSL_VERIFY_NONE, nullptr);
	}

	// make sure client side SNI works..
	if (!::SSL_set_tlsext_host_name(GetNativeTLSHandle(), sHostname.c_str()))
	{
		return SetError(kFormat("failed to set SNI hostname: {}", sHostname));
	}

	if (!Good() || GetNativeSocket() < 0)
	{
		return false;
	}

	if (GetContext().GetRole() == boost::asio::ssl::stream_base::client)
	{
#if 0
		SetALPN("http/1.0");
#else
		if (Options & KStreamOptions::RequestHTTP3)
		{
			SetRequestHTTP3();
		}
#endif
		// start an immediate handshake here
		if (!Handshake())
		{
			// error is already set
			return false;
		}
	}

	kDebug(2, "connected to {} {}", "endpoint", GetEndPointAddress());

	return true;

} // connect


//-----------------------------------------------------------------------------
std::unique_ptr<KQuicStream> CreateKQuicServer(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KQuicStream>(Context, Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KQuicClient> CreateKQuicClient()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KQuicClient>(s_KQuicClientContext, KStreamOptions::GetDefaultTimeout());
}

//-----------------------------------------------------------------------------
std::unique_ptr<KQuicClient> CreateKQuicClient(const KTCPEndPoint& EndPoint,
											   KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKQuicClient();

	Client->Connect(EndPoint, Options);

	return Client;

} // CreateKQuicClient

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_OPENSSL_QUIC
