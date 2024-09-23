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
#include "ksystem.h"
#include "kurlencode.h"
#include "kunixstream.h"
#include "ktcpstream.h"
#include "ktlsstream.h"
#include "kquicstream.h"
#include "kpoll.h"
#include <openssl/ssl.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

std::unordered_map<KString, std::pair<KString, bool>> g_KnownHosts;

boost::asio::ip::tcp::resolver::query
CreateTCPQuery(KStringViewZ sHostname, KStringViewZ sPort, KStreamOptions::Family Family)
{
	sHostname = KIOStreamSocket::GetKnownHostAddress(sHostname, Family);

	if (Family == KStreamOptions::Family::Any)
	{
		return boost::asio::ip::tcp::resolver::query(
			sHostname.c_str(),
			sPort.c_str(),
			boost::asio::ip::tcp::resolver::query::numeric_service
		);
	}
	else
	{
		return boost::asio::ip::tcp::resolver::query(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(),
			sHostname.c_str(),
			sPort.c_str(),
			boost::asio::ip::tcp::resolver::query::numeric_service
		);
	}
}

boost::asio::ip::udp::resolver::query 
CreateUDPQuery(KStringViewZ sHostname, KStringViewZ sPort, KStreamOptions::Family Family)
{
	sHostname = KIOStreamSocket::GetKnownHostAddress(sHostname, Family);

	if (Family == KStreamOptions::Family::Any)
	{
		return boost::asio::ip::udp::resolver::query(
			sHostname.c_str(),
			sPort.c_str(),
			boost::asio::ip::tcp::resolver::query::numeric_service
		);
	}
	else
	{
		return boost::asio::ip::udp::resolver::query(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6(),
			sHostname.c_str(),
			sPort.c_str(),
			boost::asio::ip::tcp::resolver::query::numeric_service
		);
	}
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KIOStreamSocket::resolver_results_tcp_type
KIOStreamSocket::ResolveTCP(
	KStringViewZ sHostname,
	uint16_t iPort,
	KStreamOptions::Family Family,
	boost::asio::io_service& IOService,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	kDebug (3, "resolving domain {}", sHostname);

	boost::asio::ip::tcp::resolver Resolver(IOService);

	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	//	auto query = CreateQuery<boost::asio::ip::tcp>(sHostname, KString::to_string(iPort).c_str(), iFamily);
	auto Hosts = Resolver.resolve(CreateTCPQuery(sIPAddress.empty() ? sHostname.c_str() : sIPAddress.c_str(), KString::to_string(iPort).c_str(), Family), ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (DEKAF2_CLASSIC_ASIO)
		auto it = Hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		if (it == ie)
		{
			kDebug(2, "{}: {}", sHostname, ec.message());
		}

		for (; it != ie; ++it)
		{
			kDebug (3, "resolved to: {}", it->endpoint().address().to_string());
		}
	}
#endif

	return Hosts;

} // kResolveTCP

//-----------------------------------------------------------------------------
KIOStreamSocket::resolver_results_udp_type
KIOStreamSocket::ResolveUDP(
	KStringViewZ sHostname,
	uint16_t iPort,
	KStreamOptions::Family Family,
	boost::asio::io_service& IOService,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	kDebug (3, "resolving domain {}", sHostname);

	boost::asio::ip::udp::resolver Resolver(IOService);

	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}


	//	auto Hosts = Resolver.resolve(query, ec);
	auto Hosts = Resolver.resolve(CreateUDPQuery(sIPAddress.empty() ? sHostname.c_str() : sIPAddress.c_str(), KString::to_string(iPort).c_str(), Family), ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (DEKAF2_CLASSIC_ASIO)
		auto it = Hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		if (it == ie)
		{
			kDebug(2, "{}: {}", sHostname, ec.message());
		}

		for (; it != ie; ++it)
		{
			kDebug (3, "resolved to: {}", it->endpoint().address().to_string());
		}
	}
#endif

	return Hosts;

} // kResolveUDP

//-----------------------------------------------------------------------------
KIOStreamSocket::resolver_results_tcp_type
KIOStreamSocket::ReverseLookup(
	KStringViewZ sIPAddr,
	boost::asio::io_service& IOService,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	kDebug(2, "reverse lookup for IP {}", sIPAddr);

	boost::asio::ip::tcp::endpoint endpoint;

	if (kIsIPv6Address(sIPAddr, true))
	{
		// have [] around the IP as in URLs
		endpoint.address (boost::asio::ip::address_v6::from_string (KString(sIPAddr.substr(1, sIPAddr.size() - 2))));
	}
	else if (kIsValidIPv4 (sIPAddr))
	{
		endpoint.address (boost::asio::ip::address_v4::from_string (sIPAddr.c_str(), ec));
	}
	else if (kIsValidIPv6 (sIPAddr))
	{
		endpoint.address (boost::asio::ip::address_v6::from_string (sIPAddr.c_str(), ec));
	}
	else
	{
		kDebug(1, "Invalid address specified: {} --> FAILED", sIPAddr);
		return {};
	}

	if (ec)
	{
		kDebug(2, "{}: {}", sIPAddr, ec.message());
		return {};
	}

	boost::asio::ip::tcp::resolver Resolver (IOService);
	auto Hosts = Resolver.resolve(endpoint, ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (DEKAF2_CLASSIC_ASIO)
		auto it = Hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		for (; it != ie; ++it)
		{
			kDebug (3, "resolved to: {}", it->host_name());
		}
	}
#endif

	return Hosts;

} // ReverseLookup

#if (DEKAF2_CLASSIC_ASIO)
//-----------------------------------------------------------------------------
KString KIOStreamSocket::PrintResolvedAddress(resolver_endpoint_tcp_type endpoint)
//-----------------------------------------------------------------------------
{
	if (endpoint->endpoint().address().is_v6())
	{
		return kFormat("[{}]:{}",
			endpoint->endpoint().address().to_string(),
			endpoint->endpoint().port()
		);
	}
	else
	{
		return kFormat("{}:{}",
			endpoint->endpoint().address().to_string(),
			endpoint->endpoint().port()
		);
	}

} // PrintResolvedAddress
#else
//-----------------------------------------------------------------------------
KString KIOStreamSocket::PrintResolvedAddress(const resolver_endpoint_tcp_type& endpoint)
//-----------------------------------------------------------------------------
{
	if (endpoint.address().is_v6())
	{
		return kFormat("[{}]:{}",
			endpoint.address().to_string(),
			endpoint.port()
		);
	}
	else
	{
		return kFormat("{}:{}",
			endpoint.address().to_string(),
			endpoint.port()
		);
	}

} // PrintResolvedAddress
#endif // of DEKAF2_CLASSIC_ASIO

//-----------------------------------------------------------------------------
void KIOStreamSocket::AddKnownHostAddress(KStringView sHostname, KStringView sIPAddress)
//-----------------------------------------------------------------------------
{
	bool bIsIPv6 { false };

	if (kIsIPv6Address(sIPAddress, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sIPAddress.ToView(1, sIPAddress.size() - 2);
		bIsIPv6 = true;
	}
	else if (kIsIPv6Address(sIPAddress, false))
	{
		bIsIPv6 = true;
	}

	auto p = g_KnownHosts.insert({ sHostname, { sIPAddress, bIsIPv6 } });

	if (!p.second)
	{
		p.first->second = { sIPAddress, bIsIPv6 };
	}

	kDebug(2, "added known host: {} -> {}", sHostname, sIPAddress);

} // AddKnownHostAddress

//-----------------------------------------------------------------------------
KStringViewZ KIOStreamSocket::GetKnownHostAddress(KStringViewZ sHostname, KStreamOptions::Family Family)
//-----------------------------------------------------------------------------
{
	auto it = g_KnownHosts.find(sHostname);

	if (it != g_KnownHosts.end())
	{
		if ( Family == KStreamOptions::Family::Any ||
			(Family == KStreamOptions::Family::IPv4 && !it->second.second) ||
			(Family == KStreamOptions::Family::IPv4 &&  it->second.second))
		{
			kDebug(2, "resolving from known hosts: {} -> {}", sHostname, it->second.first);
			return it->second.first;
		}
	}

	return sHostname;

} // GetKnownHostAddress

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
bool KIOStreamSocket::CheckIfReady(int what, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (what & POLLIN)
	{
		auto SSL = GetNativeTLSHandle();

		if (SSL && ::SSL_pending(SSL) > 0)
		{
			return true;
		}
	}

	auto iResult = kPoll(GetNativeSocket(), what, Timeout);

	if (iResult == 0)
	{
		// timed out, no events
		return SetError(kFormat("connection with {} timed out after {}", GetEndPoint(), Timeout));
	}
	else if (iResult < 0)
	{
		return SetErrnoError("error during poll: ");
	}

	// data available
	return true;

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

	return false;

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
