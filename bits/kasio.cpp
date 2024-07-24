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


#include "kasio.h"
#include "../ksystem.h"
#include "../kurlencode.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
boost::asio::ip::tcp::resolver::results_type
kResolveTCP(KStringViewZ sHostname,
			uint16_t iPort,
			boost::asio::io_service& IOService,
			boost::system::error_code& ec)
//-----------------------------------------------------------------------------
{
	kDebug(2, "resolving domain {}", sHostname);

	boost::asio::ip::tcp::resolver Resolver(IOService);

	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	boost::asio::ip::tcp::resolver::query query(sIPAddress.empty() ? sHostname.c_str()
												: sIPAddress.c_str(),
												KString::to_string(iPort).c_str(),
												boost::asio::ip::tcp::resolver::query::numeric_service);
	auto Hosts = Resolver.resolve(query, ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (BOOST_VERSION < 106600)
		auto it = hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		Hosts.empty();
		for (; it != ie; ++it)
		{
			kDebug(2, "resolved to: {}", it->endpoint().address().to_string());
		}
	}
#endif

	return Hosts;

} // kResolveTCP

//-----------------------------------------------------------------------------
boost::asio::ip::udp::resolver::results_type
kResolveUDP(KStringViewZ sHostname,
			uint16_t iPort,
			boost::asio::io_service& IOService,
			boost::system::error_code& ec)
//-----------------------------------------------------------------------------
{
	kDebug(2, "resolving domain {}", sHostname);

	boost::asio::ip::udp::resolver Resolver(IOService);

	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numeric address - get rid of the []
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}

	boost::asio::ip::udp::resolver::query query(sIPAddress.empty() ? sHostname.c_str()
												: sIPAddress.c_str(),
												KString::to_string(iPort).c_str(),
												boost::asio::ip::udp::resolver::query::numeric_service);
	auto Hosts = Resolver.resolve(query, ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (BOOST_VERSION < 106600)
		auto it = Hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		for (; it != ie; ++it)
		{
			kDebug(2, "resolved to: {}", it->endpoint().address().to_string());
		}
	}
#endif

	return Hosts;

} // kResolveUDP

//-----------------------------------------------------------------------------
boost::asio::ip::tcp::resolver::results_type
kReverseLookup(KStringViewZ sIPAddr,
			   boost::asio::io_service& IOService,
			   boost::system::error_code& ec)
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
		return {};
	}

	boost::asio::ip::tcp::resolver Resolver (IOService);
	auto Hosts = Resolver.resolve(endpoint, ec);

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
#if (BOOST_VERSION < 106600)
		auto it = Hosts;
		decltype(it) ie;
#else
		auto it = Hosts.begin();
		auto ie = Hosts.end();
#endif
		for (; it != ie; ++it)
		{
			kDebug(2, "resolved to: {}", it->host_name());
		}
	}
#endif

	return Hosts;

} // kReverseLookup

} // of namespace detail

DEKAF2_NAMESPACE_END
