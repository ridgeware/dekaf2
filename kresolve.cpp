/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "kresolve.h"
#include "kipaddress.h"
#include "bits/kasio.h"
#include "kiostreamsocket.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

namespace KResolve {

namespace {

std::unordered_map<KString, std::pair<KString, bool>> g_KnownHosts;

//-----------------------------------------------------------------------------
resolver_results_tcp_type
CreateTCPQuery(
	boost::asio::io_service& IOService,
	KStringView sHostname,
	KStringView sPort,
	KStreamOptions::Family Family,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	boost::asio::ip::tcp::resolver Resolver(IOService);

	sHostname = GetKnownHostAddress(sHostname, Family);

	if (Family == KStreamOptions::Family::Any)
	{
#ifdef DEKAF2_CLASSIC_ASIO
		return Resolver.resolve(
			boost::asio::ip::tcp::resolver::query(
				std::string(sHostname),
				std::string(sPort),
				boost::asio::ip::tcp::resolver::query::numeric_service
			),
			ec
		);
#else
	#if BOOST_ASIO_HAS_STRING_VIEW
		return Resolver.resolve(
			std::string_view(sHostname),
			std::string_view(sPort),
			boost::asio::ip::tcp::resolver::numeric_service,
			ec
		);
	#else
		return Resolver.resolve(
			std::string(sHostname),
			std::string(sPort),
			boost::asio::ip::tcp::resolver::numeric_service,
			ec
		);
	#endif
#endif
	}
	else
	{
#ifdef DEKAF2_CLASSIC_ASIO
		return Resolver.resolve(
			boost::asio::ip::tcp::resolver::query(
				Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(),
				std::string(sHostname),
				std::string(sPort),
				boost::asio::ip::tcp::resolver::query::numeric_service
			),
			ec
		);
#else
	#if BOOST_ASIO_HAS_STRING_VIEW
		return Resolver.resolve(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(),
			std::string_view(sHostname),
			std::string_view(sPort),
			boost::asio::ip::tcp::resolver::numeric_service,
			ec
		);
	#else
		return Resolver.resolve(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(),
			std::string(sHostname),
			std::string(sPort),
			boost::asio::ip::tcp::resolver::numeric_service,
			ec
		);
	#endif
#endif
	}

} // CreateTCPQuery

//-----------------------------------------------------------------------------
resolver_results_udp_type
CreateUDPQuery(
	boost::asio::io_service& IOService,
	KStringView sHostname,
	KStringView sPort,
	KStreamOptions::Family Family,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	boost::asio::ip::udp::resolver Resolver(IOService);

	sHostname = GetKnownHostAddress(sHostname, Family);

	if (Family == KStreamOptions::Family::Any)
	{
#ifdef DEKAF2_CLASSIC_ASIO
		return Resolver.resolve(
			boost::asio::ip::udp::resolver::query(
				std::string(sHostname),
				std::string(sPort),
				boost::asio::ip::udp::resolver::query::numeric_service
			),
			ec
		);
#else
	#if BOOST_ASIO_HAS_STRING_VIEW
		return Resolver.resolve(
			std::string_view(sHostname),
			std::string_view(sPort),
			boost::asio::ip::udp::resolver::numeric_service,
			ec
		);
	#else
		return Resolver.resolve(
			std::string(sHostname),
			std::string(sPort),
			boost::asio::ip::udp::resolver::numeric_service,
			ec
		);
	#endif
#endif
	}
	else
	{
#ifdef DEKAF2_CLASSIC_ASIO
		return Resolver.resolve(
			boost::asio::ip::udp::resolver::query(
				Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6(),
				std::string(sHostname),
				std::string(sPort),
				boost::asio::ip::udp::resolver::query::numeric_service
			),
			ec
		);
#else
	#if BOOST_ASIO_HAS_STRING_VIEW
		return Resolver.resolve(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6(),
			std::string_view(sHostname),
			std::string_view(sPort),
			boost::asio::ip::udp::resolver::numeric_service,
			ec
		);
	#else
		return Resolver.resolve(
			Family == KStreamOptions::Family::IPv4 ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6(),
			std::string(sHostname),
			std::string(sPort),
			boost::asio::ip::udp::resolver::numeric_service,
			ec
		);
	#endif
#endif
	}

} // 

} // end of anonymous namespace

//-----------------------------------------------------------------------------
resolver_results_tcp_type
ResolveTCP(
	KStringView sHostname,
	uint16_t iPort,
	KStreamOptions::Family Family,
	boost::asio::io_service& IOService,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	kDebug (3, "resolving domain {}", sHostname);

	KStringView sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	auto Hosts = CreateTCPQuery(IOService, sIPAddress.empty() ? sHostname : sIPAddress, KString::to_string(iPort), Family, ec);

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
resolver_results_udp_type
ResolveUDP(
	KStringView sHostname,
	uint16_t iPort,
	KStreamOptions::Family Family,
	boost::asio::io_service& IOService,
	boost::system::error_code& ec
)
//-----------------------------------------------------------------------------
{
	kDebug (3, "resolving domain {}", sHostname);

	KStringView sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	auto Hosts = CreateUDPQuery(IOService, sIPAddress.empty() ? sHostname : sIPAddress, KString::to_string(iPort), Family, ec);

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
resolver_results_tcp_type
ReverseLookup(
	KStringView sIPAddr,
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
#ifdef DEKAF2_CLASSIC_ASIO
		endpoint.address (boost::asio::ip::address_v6::from_string (std::string(sIPAddr.substr(1, sIPAddr.size() - 2)), ec));
#else
	#if defined(BOOST_ASIO_HAS_STRING_VIEW)
		endpoint.address (boost::asio::ip::make_address_v6 (std::string_view(sIPAddr.substr(1, sIPAddr.size() - 2)), ec));
	#else
		endpoint.address (boost::asio::ip::make_address_v6 (std::string(sIPAddr.substr(1, sIPAddr.size() - 2)), ec));
	#endif
#endif
	}
	else if (kIsIPv4Address(sIPAddr))
	{
#ifdef DEKAF2_CLASSIC_ASIO
		endpoint.address (boost::asio::ip::address_v4::from_string (std::string(sIPAddr), ec));
#else
	#if defined(BOOST_ASIO_HAS_STRING_VIEW)
		endpoint.address (boost::asio::ip::make_address_v4 (std::string_view(sIPAddr), ec));
	#else
		endpoint.address (boost::asio::ip::make_address_v4 (std::string(sIPAddr), ec));
	#endif
#endif
	}
	else if (kIsIPv6Address(sIPAddr, false))
	{
		// no [] around the IP
#ifdef DEKAF2_CLASSIC_ASIO
		endpoint.address (boost::asio::ip::address_v6::from_string (std::string(sIPAddr), ec));
#else
	#if defined(BOOST_ASIO_HAS_STRING_VIEW)
		endpoint.address (boost::asio::ip::make_address_v6 (std::string_view(sIPAddr), ec));
	#else
		endpoint.address (boost::asio::ip::make_address_v6 (std::string(sIPAddr), ec));
	#endif
#endif
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
KString PrintResolvedAddress(resolver_endpoint_tcp_type endpoint)
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
KString PrintResolvedAddress(const resolver_endpoint_tcp_type& endpoint)
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
void AddKnownHostAddress(KStringView sHostname, KStringView sIPAddress)
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
KStringView GetKnownHostAddress(KStringView sHostname, KStreamOptions::Family Family)
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

} // end of namespace KResolve

//-----------------------------------------------------------------------------
std::vector<KString> kResolveHostToList (KStringView sHostname, bool bIPv4, bool bIPv6, std::size_t iMax)
//-----------------------------------------------------------------------------
{
	KString sIPV4;
	KString sIPV6;
	std::vector<KString> Hosts;

	boost::asio::io_service IOService;
	boost::system::error_code ec;
	auto hosts = KResolve::ResolveTCP(sHostname, 80, KStreamOptions::Family::Any, IOService, ec);

	if (ec)
	{
		kDebug (1, "{} --> FAILED : {}", sHostname, ec.message());
	}
	else
	{
#if (DEKAF2_CLASSIC_ASIO)
		auto it = hosts;
		decltype(it) ie;
#else
		auto it = hosts.begin();
		auto ie = hosts.end();
#endif
		for (; it != ie && Hosts.size() < iMax; ++it)
		{
			if (it->endpoint().protocol() == boost::asio::ip::tcp::v4())
			{
				if (bIPv4)
				{
					Hosts.push_back(it->endpoint().address().to_string());
				}
				else if (sIPV4.empty())
				{
					sIPV4 = it->endpoint().address().to_string();
				}
			}
			else
			{
				if (bIPv6)
				{
					Hosts.push_back(it->endpoint().address().to_string());
				}
				else if (sIPV6.empty())
				{
					sIPV6 = it->endpoint().address().to_string();
				}
			}
		}

		if (!Hosts.empty())
		{
			// success
			kDebug (2, "{} --> {}", sHostname, Hosts.front());
		}
		else if (!sIPV6.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV6 {}", sHostname, sIPV6);
		}
		else if (!sIPV4.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV4 {}", sHostname, sIPV4);
		}
		else
		{
			// unknown..
			kDebug(1, "{} --> FAILED", sHostname);
		}
	}

	return Hosts;

} // kResolveHostToList

//-----------------------------------------------------------------------------
KString kResolveHost (KStringView sHostname, bool bIPv4, bool bIPv6)
//-----------------------------------------------------------------------------
{
	auto List = kResolveHostToList(sHostname, bIPv4, bIPv6, 1);

	if (!List.empty()) 
	{
		return KString{std::move(List.front())};
	}
	else
	{
		return KString{};
	}

} // kResolveHost

/*
//-----------------------------------------------------------------------------
bool kIsValidIPv4 (KStringViewZ sIPAddr)
//-----------------------------------------------------------------------------
{
	struct sockaddr_in sockAddr {};
	return inet_pton(AF_INET, sIPAddr.c_str(), &(sockAddr.sin_addr)) != 0;

} // kIsValidIPv4

//-----------------------------------------------------------------------------
bool kIsValidIPv6 (KStringViewZ sIPAddr)
//-----------------------------------------------------------------------------
{
	struct sockaddr_in6 sockAddr {};
	return inet_pton(AF_INET6, sIPAddr.c_str(), &(sockAddr.sin6_addr)) != 0;

} // kIsValidIPv6
*/

//-----------------------------------------------------------------------------
std::vector<KString> kReverseLookupToList (KStringView sIPAddr, std::size_t iMax)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Hosts;
	boost::system::error_code ec;
	boost::asio::io_service IOService;

	auto hosts = KResolve::ReverseLookup(sIPAddr, IOService, ec);

#if (DEKAF2_CLASSIC_ASIO)
	auto it = hosts;
	decltype(it) ie;
#else
	auto it = hosts.begin();
	auto ie = hosts.end();
#endif

	if (ec || it == ie)
	{
		kDebug (1, "{} --> FAILED : {}", sIPAddr, ec.message());
	}
	else
	{
		for (;it != ie; ++it)
		{
			Hosts.push_back(it->host_name());
			
			if (Hosts.size() >= iMax)
			{
				break;
			}
		}
	}

	return Hosts;

} // kReverseLookupToList

//-----------------------------------------------------------------------------
KString kReverseLookup (KStringView sIPAddr)
//-----------------------------------------------------------------------------
{
	KString sFoundHost;

	auto List = kReverseLookupToList(sIPAddr, 1);

	if (!List.empty())
	{
		return KString{std::move(List.front())};
	}
	else
	{
		return KString{};
	}

} // kReverseLookup

DEKAF2_NAMESPACE_END
