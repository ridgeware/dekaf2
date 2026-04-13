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

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/net/address/kipaddress.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/crypto/hash/khash.h>
#include <dekaf2/core/errors/kexception.h>
#include <array>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_address
/// @{

class KIPNetwork;
class KIPNetwork6;

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPNetworkBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class DEKAF2_PREFIX KIPNetwork;

//----------
public:
//----------

	/// returns the prefix length of the network
	constexpr uint8_t  PrefixLength() const noexcept
	{
		return m_iPrefixLength;
	}

//----------
protected:
//----------

	constexpr          KIPNetworkBase() = default;
	constexpr          KIPNetworkBase(uint8_t iPrefixLength, uint8_t iPrefixMax, KError& ec)
	                   : m_iPrefixLength(std::min(iPrefixLength, iPrefixMax))
	                   { if (iPrefixLength > iPrefixMax && !ec) ec = KIPError("prefix too large"); }

	constexpr explicit KIPNetworkBase(uint8_t iPrefixLength, uint8_t iPrefixMax)
	                   : m_iPrefixLength(std::min(iPrefixLength, iPrefixMax))
	                   { if (iPrefixLength > iPrefixMax) throw KIPError("prefix too large"); }

	constexpr void     SetPrefixLength         (uint8_t iPrefixLength) { m_iPrefixLength = iPrefixLength; }
	static KStringView AddressStringFromString (KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;
	static uint8_t     PrefixLengthFromString  (KStringView sNetwork, uint8_t iMaxPrefixLength, bool bAcceptSingleHost, KIPError& ec) noexcept;

	static uint8_t     CalcPrefixFromMask      (const KIPAddress4& Mask, KIPError& ec) noexcept
	{
		return CalcPrefixFromMask(Mask.ToBytes().data(), Mask.ToBytes().size(), ec);
	}

	static uint8_t     CalcPrefixFromMask      (const KIPAddress6& Mask, KIPError& ec) noexcept
	{
		return CalcPrefixFromMask(Mask.ToBytes().data(), Mask.ToBytes().size(), ec);
	}

//----------
private:
//----------

	static uint8_t     CalcPrefixFromMask      (const detail::IP::value_type* Mask, uint16_t iMaskLen, KIPError& ec) noexcept;

	uint8_t m_iPrefixLength { 0 };

	constexpr void SetIPv4(bool bYesNo) noexcept
	{
		m_bIsIPv4 = bYesNo;
	}

	constexpr bool IsIPv4() const noexcept
	{
		return m_bIsIPv4;
	}

	bool m_bIsIPv4 { false };

}; // KNetworkBase

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One IPv4 network description, that is one IPv4 address and its net prefix
class DEKAF2_PUBLIC KIPNetwork4 : public detail::KIPNetworkBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base = detail::KIPNetworkBase;

	/// default constructor
	constexpr          KIPNetwork4() noexcept = default;

	/// construct from IPv4 and prefix length, not throwing but returning possible error in ec
	constexpr          KIPNetwork4(const KIPAddress4& IP, uint8_t iPrefixLength, KIPError& ec) noexcept
	                   : base(iPrefixLength, 32, ec)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv4 and prefix length, throws on error
	constexpr          KIPNetwork4(const KIPAddress4& IP, uint8_t iPrefixLength)
	                   : base(iPrefixLength, 32)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv4 address and netmask, not throwing but returning possible error in ec
	                   KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask, KIPError& ec) noexcept
	                   : base(CalcPrefixFromMask(Mask, ec), 32, ec)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv4 address and netmask, throws on error
	                   KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask);

	/// construct from IPv4 address and prefix length in string notation, not throwing but returning possible error in ec
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host
	                   KIPNetwork4(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;
	/// construct from IPv4 address and prefix length in string notation, not throwing but returning possible error in ec
	                   KIPNetwork4(KStringView sNetwork, KIPError& ec) noexcept
	                   : KIPNetwork4(sNetwork, false, ec)
	                   {}
	/// construct from IPv4 address and prefix length in string notation, throws on error
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host, defaults to false
	          explicit KIPNetwork4(KStringView sNetwork, bool bAcceptSingleHost = false);

	/// returns the IPv4 address of the network as used in construction
	DEKAF2_NODISCARD
	constexpr const KIPAddress4& Address() const noexcept { return m_IP; }

	/// get network as string
	DEKAF2_NODISCARD
	KString ToString () const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// is address unspecified?
	DEKAF2_NODISCARD
	constexpr bool IsUnspecified() const noexcept
	{
		return Address().IsUnspecified();
	}

	/// is address valid?
	DEKAF2_NODISCARD
	constexpr bool IsValid() const noexcept
	{
		return Address().IsValid();
	}

	/// return netmask
	DEKAF2_NODISCARD
	constexpr KIPAddress4 Netmask() const noexcept
	{
		return KIPAddress4(PrefixLength() == 0 ? 0 : 0xffffffff << (32 - PrefixLength()) );
	}

	/// return address of the network
	DEKAF2_NODISCARD
	constexpr KIPAddress4 Network() const noexcept
	{
		return KIPAddress4(m_IP.ToUInt() & Netmask().ToUInt());
	}

	/// return broadcast address for the network
	DEKAF2_NODISCARD
	constexpr KIPAddress4 Broadcast() const noexcept
	{
		return KIPAddress4(Network().ToUInt() | (Netmask().ToUInt() ^ 0xFFFFFFFF));
	}

	/// return host range
	DEKAF2_NODISCARD
	std::pair<KIPAddress4, KIPAddress4> Hosts() const noexcept;

	/// returns true if this network contains the given IPv4 address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress4& IP) const noexcept;

	/// returns true if this network contains the given IPv6 address (for mapped v6 addresses..)
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress6& IP) const noexcept;

	/// returns true if this network contains the given IPv4 or IPv6 address (for mapped v6 addresses..)
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress& IP) const noexcept;

	/// return network without any host bits set
	DEKAF2_NODISCARD
	constexpr KIPNetwork4 Canonical() const noexcept
	{
		return KIPNetwork4(Network(), PrefixLength());
	}

	/// is this indeed a host address and not a network?
	DEKAF2_NODISCARD
	constexpr bool IsHost() const noexcept
	{
		return PrefixLength() == 32;
	}

	/// is network a subnet of another network?
	DEKAF2_NODISCARD
	bool IsSubnetOf(const KIPNetwork4& other) const;

	/// is network a subnet of another network?
	DEKAF2_NODISCARD
	bool IsSubnetOf(const KIPNetwork6& other) const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPNetwork4& n1,
	                                 const KIPNetwork4& n2) noexcept
	{
		return n1.m_IP == n2.m_IP && n1.PrefixLength() == n2.PrefixLength();
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

	static KIPAddress4 AddressFromString  (KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;

	KIPAddress4 m_IP;

}; // KIPNetwork4

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One IPv6 network description, that is one IPv6 address and its net prefix
class DEKAF2_PUBLIC KIPNetwork6 : public detail::KIPNetworkBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base = detail::KIPNetworkBase;

	/// default constructor
	constexpr          KIPNetwork6() noexcept = default;

	/// construct from IPv6 and prefix length, not throwing but returning possible error in ec
	constexpr          KIPNetwork6(const KIPAddress6& IP, uint8_t iPrefixLength, KIPError& ec) noexcept
	                   : base(iPrefixLength, 128, ec)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv6 and prefix length, throws on error
	constexpr          KIPNetwork6(const KIPAddress6& IP, uint8_t iPrefixLength)
	                   : base(iPrefixLength, 128)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv6 and prefix length, not throwing but returning possible error in ec
	                   KIPNetwork6(const KIPAddress6& IP, const KIPAddress6& Mask, KIPError& ec) noexcept
	                   : base(CalcPrefixFromMask(Mask, ec), 128, ec)
	                   , m_IP(IP)
	                   {}

	/// construct from IPv6 and prefix length, throws on error
	                   KIPNetwork6(const KIPAddress6& IP, const KIPAddress6& Mask);

	/// construct from IPv6 address and prefix length in string notation, not throwing but returning possible error in ec
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host
	                   KIPNetwork6(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;
	/// construct from IPv6 address and prefix length in string notation, not throwing but returning possible error in ec
	                   KIPNetwork6(KStringView sNetwork, KIPError& ec) noexcept
	                   : KIPNetwork6(sNetwork, false, ec)
	                   {}
	/// construct from IPv6 address and prefix length in string notation, throws on error
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host, defaults to false
	          explicit KIPNetwork6(KStringView sNetwork, bool bAcceptSingleHost = false);

	/// construct from IPv4 network, never throws
	          explicit KIPNetwork6(const KIPNetwork4& Net4) noexcept;

	/// returns the IPv6 address of the network as used in construction
	DEKAF2_NODISCARD
	constexpr const KIPAddress6& Address() const noexcept { return m_IP; }

	/// is address unspecified?
	DEKAF2_NODISCARD
	constexpr bool IsUnspecified() const noexcept
	{
		return Address().IsUnspecified();
	}

	/// is address valid?
	DEKAF2_NODISCARD
	constexpr bool IsValid() const noexcept
	{
		return Address().IsValid();
	}

	/// get network as string
	DEKAF2_NODISCARD
	KString ToString () const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// return netmask
	DEKAF2_NODISCARD
	KIPAddress6 Netmask() const noexcept;

	/// return address of the network
	DEKAF2_NODISCARD
	KIPAddress6 Network() const noexcept;

	/// return host range
	DEKAF2_NODISCARD
	std::pair<KIPAddress6, KIPAddress6> Hosts() const noexcept;

	/// returns true if this network contains the given IPv4 address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress4& IP) const noexcept;

	/// returns true if this network contains the given IPv6 address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress6& IP) const noexcept;

	/// returns true if this network contains the given IPv4 or IPv6 address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress& IP) const noexcept;

	/// return network without any host bits set
	DEKAF2_NODISCARD
	KIPNetwork6 Canonical() const noexcept
	{
		return KIPNetwork6(Network(), PrefixLength());
	}

	/// is this indeed a host address and not a network?
	DEKAF2_NODISCARD
	constexpr bool IsHost() const noexcept
	{
		return PrefixLength() == 128;
	}

	/// is network a subnet of another network?
	DEKAF2_NODISCARD
	bool IsSubnetOf(const KIPNetwork4& other) const;

	/// is network a subnet of another network?
	DEKAF2_NODISCARD
	bool IsSubnetOf(const KIPNetwork6& other) const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPNetwork6& n1,
	                                 const KIPNetwork6& n2) noexcept
	{
		return n1.m_IP == n2.m_IP && n1.PrefixLength() == n2.PrefixLength();
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

	static KIPAddress6 AddressFromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;

	KIPAddress6 m_IP;

}; // KIPNetwork6

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One IP network description, that is one IP address and its net prefix, either IPv4 or IPv6
class DEKAF2_PUBLIC KIPNetwork
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor, does not throw
	constexpr          KIPNetwork() noexcept = default;

	/// construct from KIPNetwork4, does not throw
	constexpr          KIPNetwork(KIPNetwork4 Net4) noexcept
	                   : m_Net6(KIPAddress6(Net4.Address()), Net4.PrefixLength())
	                   { SetIPv4(true); }

	/// construct from KIPNetwork6, does not throw
	constexpr          KIPNetwork(KIPNetwork6 Net6) noexcept
	                   : m_Net6(std::move(Net6))
	                   { SetIPv4(false); }

	/// construct from address and prefix length in string notation, not throwing but returning possible error in ec
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host
	                   KIPNetwork(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
	                   : KIPNetwork(FromString(sNetwork, bAcceptSingleHost, ec))
	                   {}
	/// construct from address and prefix length in string notation, not throwing but returning possible error in ec
	                   KIPNetwork(KStringView sNetwork, KIPError& ec) noexcept
	                   : KIPNetwork(sNetwork, false, ec)
	                   {}
	/// construct from address and prefix length in string notation, throws on error
	/// @param bAcceptSingleHost if true accepting a network without prefix as single host, defaults to false
	          explicit KIPNetwork(KStringView sNetwork, bool bAcceptSingleHost = false)
	                   : KIPNetwork(FromString(sNetwork, bAcceptSingleHost))
	                   {}

	/// returns the prefix length of the network
	DEKAF2_NODISCARD
	constexpr uint8_t PrefixLength() const noexcept
	{
		return m_Net6.PrefixLength();
	}

	/// is network an IPv4 network?
	DEKAF2_NODISCARD
	constexpr bool Is4() const noexcept
	{
		return m_Net6.IsIPv4();
	}

	/// is network an IPv6 network?
	DEKAF2_NODISCARD
	constexpr bool Is6() const noexcept
	{
		return !m_Net6.IsIPv4();
	}

	/// get address in network byte order as 16 bytes (IPv6 representation)
	DEKAF2_NODISCARD
	constexpr const detail::IP::Bytes6& ToBytes6() const noexcept
	{
		return m_Net6.Address().ToBytes();
	}

	/// get address in network byte order as 4 bytes (IPv4 representation, valid only if Is4())
	DEKAF2_NODISCARD
	const detail::IP::Bytes4& ToBytes4() const noexcept
	{
		return *reinterpret_cast<const detail::IP::Bytes4*>(m_Net6.Address().ToBytes().data() + 12);
	}

	/// is address unspecified?
	DEKAF2_NODISCARD
	constexpr bool IsUnspecified() const noexcept
	{
		return m_Net6.IsUnspecified();
	}

	/// is address valid?
	DEKAF2_NODISCARD
	constexpr bool IsValid() const noexcept
	{
		return m_Net6.IsValid();
	}

	/// get address as string
	DEKAF2_NODISCARD
	KString ToString() const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// returns true if this network contains the given address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress4& IP) const noexcept;

	/// returns true if this network contains the given address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress6& IP) const noexcept;

	/// returns true if this network contains the given address
	DEKAF2_NODISCARD
	bool Contains(const KIPAddress& IP) const noexcept;

	/// is this indeed a host address and not a network?
	DEKAF2_NODISCARD
	constexpr bool IsHost() const noexcept
	{
		return Is4() ? PrefixLength() == 32 : PrefixLength() == 128;
	}

	/// is network a subnet of another network?
	DEKAF2_NODISCARD
	bool IsSubnetOf(const KIPNetwork& other) const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPNetwork& n1,
									 const KIPNetwork& n2) noexcept
	{
		return n1.Is4() == n2.Is4()
			&& n1.PrefixLength() == n2.PrefixLength()
			&& n1.m_Net6.Address() == n2.m_Net6.Address();
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator!=(const KIPNetwork& n1,
									 const KIPNetwork& n2) noexcept
	{
		return !operator==(n1, n2);
	}

//----------
private:
//----------

	constexpr void SetIPv4(bool bYesNo) { m_Net6.SetIPv4(bYesNo); }

	KIPNetwork4 ToNetwork4() const noexcept;

	static KIPNetwork FromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept;
	static KIPNetwork FromString(KStringView sNetwork, bool bAcceptSingleHost);

	KIPNetwork6 m_Net6;

}; // KIPNetwork

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

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPNetwork Net)
{
	auto s = Net.ToString();
	stream.write(s.data(), s.size());
	return stream;
}


/// @}

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

template <>
struct formatter<DEKAF2_PREFIX KIPNetwork> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPNetwork& Net, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Net.ToString(), ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE

namespace std
{

template<>
struct hash<DEKAF2_PREFIX KIPNetwork4>
{
	DEKAF2_CONSTEXPR_14
	std::size_t operator()(const DEKAF2_PREFIX KIPNetwork4& Net) const noexcept
	{
		auto& Bytes = Net.Address().ToBytes();
		return DEKAF2_PREFIX kHash(Net.PrefixLength(), DEKAF2_PREFIX kHash(&Bytes[0], Bytes.size()));
	}
};

template<>
struct hash<DEKAF2_PREFIX KIPNetwork6>
{
	DEKAF2_CONSTEXPR_14
	std::size_t operator()(const DEKAF2_PREFIX KIPNetwork6& Net) const noexcept
	{
		auto& Bytes = Net.Address().ToBytes();
		return DEKAF2_PREFIX kHash(Net.PrefixLength(), DEKAF2_PREFIX kHash(&Bytes[0], Bytes.size()));
	}
};

template<>
struct hash<DEKAF2_PREFIX KIPNetwork>
{
	DEKAF2_CONSTEXPR_14
	std::size_t operator()(const DEKAF2_PREFIX KIPNetwork& Net) const noexcept
	{
		auto& Bytes = Net.ToBytes6();
		return DEKAF2_PREFIX kHash(Net.PrefixLength(), DEKAF2_PREFIX kHash(&Bytes[0], Bytes.size()));
	}
};

} // end of namespace std
