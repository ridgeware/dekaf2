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

// partly modeled after concepts and code parts of boost::asio::ip::network_4
// / network_6 which are
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2014 Oliver Kowalke (oliver dot kowalke at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// end of boost::asio copyright

#pragma once

/// @file kipnetwork.h
/// classes to store and check IPv4 and IPv6 networks

#include "kdefinitions.h"
#include "kipaddress.h"
#include "kstringview.h"
#include "kformat.h"
#include "khash.h"
#include "kexception.h"
#include <array>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPNetwork4
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor
	constexpr          KIPNetwork4() noexcept = default;

	/// construct from IPv4 and prefix length, not throwing but returning possible error in ec
	constexpr          KIPNetwork4(const KIPAddress4& IP, uint8_t iPrefixLength, KIPError& ec) noexcept
	                   : m_IP(IP)
	                   , m_iPrefixLength(std::min(uint8_t(32), iPrefixLength))
	{
		if (iPrefixLength > 32)
		{
			ec = KIPError("prefix length > 32");
		}
	}

	/// construct from IPv4 and prefix length, throws on error
	constexpr          KIPNetwork4(const KIPAddress4& IP, uint8_t iPrefixLength)
	                   : m_IP(IP)
	                   , m_iPrefixLength(iPrefixLength)
	{
		if (iPrefixLength > 32)
		{
			throw KIPError("prefix length > 32");
		}
	}

	/// construct from IPv4 address and netmask, not throwing but returning possible error in ec
	                   KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask, KIPError& ec) noexcept;
	/// construct from IPv4 address and netmask, throws on error
	                   KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask);

	/// construct from IPv4 address and prefix length in string notation, not throwing but returning possible error in ec
	                   KIPNetwork4(KStringView sNetwork, KIPError& ec) noexcept;
	/// construct from IPv4 address and prefix length in string notation, throws on error
	          explicit KIPNetwork4(KStringView sNetwork);

	/// returns the IPv4 address of the network as used in construction
	constexpr KIPAddress4 Address() const noexcept { return m_IP; }
	/// returns the prefix length of the network
	uint8_t PrefixLength() const noexcept { return m_iPrefixLength; }

	/// get network as string
	KString ToString () const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// return netmask
	KIPAddress4 Netmask() const noexcept;

	/// return address of the network
	constexpr KIPAddress4 Network() const noexcept
	{
		return KIPAddress4(m_IP.ToUInt() & Netmask().ToUInt());
	}

	/// return broadcast address for the network
	constexpr KIPAddress4 Broadcast() const noexcept
	{
		return KIPAddress4(Network().ToUInt() | (Netmask().ToUInt() ^ 0xFFFFFFFF));
	}

	/// return host range
	std::pair<KIPAddress4, KIPAddress4> Hosts() const noexcept;

	/// returns true if this network contains the given IPv4 address
	bool Contains(const KIPAddress4& IP) const noexcept;

	/// return network without any host bits set
	constexpr KIPNetwork4 Canonical() const noexcept
	{
		return KIPNetwork4(Network(), PrefixLength());
	}

	/// is this indeed a host address and not a network?
	constexpr bool IsHost() const noexcept
	{
		return PrefixLength() == 32;
	}

	/// is network a subnet of another network?
	bool IsSubnetOf(const KIPNetwork4& other) const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPNetwork4& n1,
	                                 const KIPNetwork4& n2) noexcept
	{
		return n1.m_IP == n2.m_IP && n1.m_iPrefixLength == n2.m_iPrefixLength;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator!=(const KIPNetwork4& n1,
	                                 const KIPNetwork4& n2) noexcept
	{
		return !operator==(n1, n2);
	}

//----------
private:
//----------

	static uint8_t     CalcPrefixFromMask     (const KIPAddress4& Mask, KIPError& ec) noexcept;
	static KIPAddress4 NetworkFromString      (KStringView sNetwork, KIPError& ec) noexcept;
	static uint8_t     PrefixLengthFromString (KStringView sNetwork, KIPError& ec) noexcept;

	KIPAddress4 m_IP;
	uint8_t     m_iPrefixLength { 0 };

}; // KIPNetwork4

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPNetwork6
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor
	constexpr          KIPNetwork6() noexcept = default;

	/// construct from IPv6 and prefix length, not throwing but returning possible error in ec
	constexpr explicit KIPNetwork6(const KIPAddress6& IP, uint8_t iPrefixLength, KIPError& ec) noexcept
	                   : m_IP(IP)
	                   , m_iPrefixLength(std::min(uint8_t(128), iPrefixLength))
	{
		if (iPrefixLength > 128)
		{
			ec = KIPError("prefix length > 128");
		}
	}

	/// construct from IPv6 and prefix length, throws on error
	constexpr explicit KIPNetwork6(const KIPAddress6& IP, uint8_t iPrefixLength)
	                   : m_IP(IP)
	                   , m_iPrefixLength(iPrefixLength)
	{
		if (iPrefixLength > 128)
		{
			throw KIPError("prefix length > 128");
		}
	}

	/// construct from IPv6 address and netmask, not throwing but returning possible error in ec
			  explicit KIPNetwork6(const KIPNetwork6& IP, const KIPNetwork6& Mask, KIPError& ec) noexcept;
	/// construct from IPv6 address and netmask, throws on error
			  explicit KIPNetwork6(const KIPNetwork6& IP, const KIPNetwork6& Mask);

	/// construct from IPv6 address and prefix length in string notation, not throwing but returning possible error in ec
			  explicit KIPNetwork6(KStringView sNetwork, KIPError& ec) noexcept;
	/// construct from IPv6 address and prefix length in string notation, throws on error
			  explicit KIPNetwork6(KStringView sNetwork);

	/// returns the IPv6 address of the network as used in construction
	constexpr KIPAddress6 Address() const noexcept { return m_IP; }
	/// returns the prefix length of the network
	uint8_t PrefixLength() const noexcept { return m_iPrefixLength; }

	/// get network as string
	KString ToString () const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// return netmask
	KIPAddress6 Netmask() const noexcept;

	/// return address of the network
	KIPAddress6 Network() const noexcept;

	/// return host range
	std::pair<KIPAddress6, KIPAddress6> Hosts() const noexcept;

	/// returns true if this network contains the given IPv6 address
	bool Contains(const KIPAddress6& IP) const noexcept;

	/// return network without any host bits set
	constexpr KIPNetwork6 Canonical() const noexcept
	{
		return KIPNetwork6(Network(), PrefixLength());
	}

	/// is this indeed a host address and not a network?
	constexpr bool IsHost() const noexcept
	{
		return PrefixLength() == 128;
	}

	/// is network a subnet of another network?
	bool IsSubnetOf(const KIPNetwork6& other) const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPNetwork6& n1,
	                                 const KIPNetwork6& n2) noexcept
	{
		return n1.m_IP == n2.m_IP && n1.m_iPrefixLength == n2.m_iPrefixLength;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator!=(const KIPNetwork6& n1,
	                                 const KIPNetwork6& n2) noexcept
	{
		return !operator==(n1, n2);
	}

//----------
private:
//----------

	static uint8_t     CalcPrefixFromMask     (const KIPAddress6& Mask, KIPError& ec) noexcept;
	static KIPNetwork6 NetworkFromString      (KStringView sNetwork, KIPError& ec) noexcept;
	static uint8_t     PrefixLengthFromString (KStringView sNetwork, KIPError& ec) noexcept;

	KIPAddress6 m_IP;
	uint8_t     m_iPrefixLength { 0 };

}; // KIPNetwork6

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPNetwork4 Net)
{
	auto s = Net.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPNetwork6 Net)
{
	auto s = Net.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KIPNetwork4> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPNetwork4& Net, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Net.ToString(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KIPNetwork6> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPNetwork6& Net, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Net.ToString(), ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE

namespace std
{

	// TODO add hash functions

} // end of namespace std
