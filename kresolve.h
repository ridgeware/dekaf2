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

#pragma once

/// @file kresolve.h
/// host name resolution and reverse lookup

#include "kstring.h"
#include "kstringview.h"
#include "bits/kstringviewz.h"
#include "bits/kasio.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

namespace KResolve {

#if (DEKAF2_CLASSIC_ASIO)
	using resolver_results_tcp_type  = boost::asio::ip::tcp::resolver::iterator;
	using resolver_endpoint_tcp_type = resolver_results_tcp_type;
	using resolver_results_udp_type  = boost::asio::ip::udp::resolver::iterator;
	using resolver_endpoint_udp_type = resolver_results_udp_type;
#else
	using resolver_results_tcp_type  = boost::asio::ip::tcp::resolver::results_type;
	using resolver_endpoint_tcp_type = boost::asio::ip::tcp::resolver::endpoint_type;
	using resolver_results_udp_type  = boost::asio::ip::udp::resolver::results_type;
	using resolver_endpoint_udp_type = boost::asio::ip::udp::resolver::endpoint_type;
#endif

	/// lookup a hostname, returns list of found addresses
	resolver_results_tcp_type
	ResolveTCP(
		KStringView sHostname,
		uint16_t iPort,
		KStreamOptions::Family Family,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// lookup a hostname, returns list of found addresses
	resolver_results_udp_type
	ResolveUDP(
		KStringView sHostname,
		uint16_t iPort,
		KStreamOptions::Family Family,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// reverse lookup an IP address, returns list of found addresses
	resolver_results_tcp_type
	ReverseLookup(
		KStringView sIPAddr,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// print one endpoint address into a string
	KString
	PrintResolvedAddress(
#if (DEKAF2_CLASSIC_ASIO)
		resolver_endpoint_tcp_type endpoint
#else
		const resolver_endpoint_tcp_type& endpoint
#endif
	);

	/// add a hostname and its address to the list of known hosts (like /etc/hosts)
	/// -this function is not thread safe, use it at init of program
	void
	AddKnownHostAddress(
		KStringView sHostname,
		KStringView sIPAddress
	);

	/// search for a hostname and its address in the list of known hosts (like /etc/hosts)
	/// @param sHostname the hostname to look up
	/// @param Family the address family searched for
	/// @return the resolved IP address or the original hostname
	KStringView
	GetKnownHostAddress(
		KStringView sHostname,
		KStreamOptions::Family Family
	);

} // end of namespace KResolve

/// Resolve the given hostname into either/or IPv4 IP addresses or IPv6 addresses.
/// @param sHostname the hostname to resolve
/// @param bIPv4 try IPv4 resolver, default = true
/// @param bIPv6 try IPv6 resolver, default = true
/// @param iMax maximum count of returned addresses, default = unlimited
/// @return resolved IP addresses (as a std::vector<KString>), or if hostname fails to resolve, an empty vector.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::vector<KString> kResolveHostToList (KStringView sHostname, bool bIPv4 = true, bool bIPv6 = true, std::size_t iMax = npos);

/// Resolve the given hostname into either an IPv4 IP address or an IPv6 address. If both versions are checked, the first found takes precedence.
/// If multiple addresses of the same kind are found, the first one takes precedence.
/// @param sHostname the hostname to resolve
/// @param bIPv4 try IPv4 resolver, default = true
/// @param bIPv6 try IPv6 resolver, default = true
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kResolveHost (KStringView sHostname, bool bIPv4 = true, bool bIPv6 = true);

/// Resolve the given hostname into an IPv4 IP address, e.g. "50.1.2.3".
/// @param sHostname the hostname to resolve
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline KString kResolveHostIPV4 (KStringView sHostname)
{
	return kResolveHost (sHostname, true, false);
}

/// Resolve the given hostname into an IPv6 IP address, e.g. "fe80::be27:ebff:fad4:49e7".
/// @param sHostname the hostname to resolve
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline KString kResolveHostIPV6 (KStringView sHostname)
{
	return kResolveHost (sHostname, false, true);
}

/// Return all host names that map to the specified IP address
/// @param sIPAddr the IP address of the searched host
/// @param iMax maximum count of returned addresses, default = unlimited
/// @return resolved host names (as a std::vector<KString>), or if address fails to resolve, an empty vector.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::vector<KString> kReverseLookupToList (KStringView sIPAddr, std::size_t iMax = npos);

/// Return the first host name that maps to the specified IP address
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kReverseLookup (KStringView sIPAddr);

DEKAF2_NAMESPACE_END
