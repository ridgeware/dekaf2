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

/// @file kipaddress.h
/// ip address checks

#include "kdefinitions.h"
#include "kstringview.h"
#include "kformat.h"
#include "khash.h"
#include <array>

DEKAF2_NAMESPACE_BEGIN

/// checks if an IP address is a IPv6 address like '[a0:ef::c425:12]'
/// @param sAddress the string to test
/// @param bNeedsBraces if true, address has to be in square braces [ ], if false they must not be present
/// @return true if sAddress holds an IPv6 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv6Address(KStringView sAddress, bool bNeedsBraces);

/// checks if an IP address is a IPv4 address like '1.2.3.4'
/// @param sAddress the string to test
/// @return true if sAddress holds an IPv4 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv4Address(KStringView sAddress);

/// Return true if the string represents a private IP address, both IPv4 and IPv6
/// @param sIP the IP address
/// @param bExcludeDocker if set to true, addresses of the 172.x.x.x range
/// are not treated as private IPs, as these are often the result of docker TCP proxying
/// @return true if sIP is a private IP
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsPrivateIP(KStringView sIP, bool bExcludeDocker = true);

class KIPAddress6;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPAddress4
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KIPAddress6;

//----------
public:
//----------

	using BytesT = std::array<uint8_t, 4>;

	constexpr          KIPAddress4() noexcept = default;

	constexpr explicit KIPAddress4(const BytesT& IP) noexcept
	                   : m_IP(IP)
	                   {}

	constexpr explicit KIPAddress4(uint32_t IP) noexcept
	                   : m_IP { static_cast<uint8_t>(IP >> 24), static_cast<uint8_t>(IP >> 16),
	                            static_cast<uint8_t>(IP >> 8), static_cast<uint8_t>(IP) }
	                   {}

	constexpr explicit KIPAddress4(const KIPAddress6& IPv6) noexcept
	                   : m_IP(FromIPv6(IPv6))
	                   {}

	          explicit KIPAddress4(KStringView sAddress) noexcept
	                   : m_IP(FromString(sAddress))
	                   {}

	/// get address as string
	          KString  ToString () const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// get address in host byte order
	DEKAF2_NODISCARD
	constexpr uint32_t  ToUInt   () const noexcept
	{
		return (static_cast<uint32_t>(m_IP[0]) << 24)
			 | (static_cast<uint32_t>(m_IP[1]) << 16)
			 | (static_cast<uint32_t>(m_IP[2]) << 8)
			 |  static_cast<uint32_t>(m_IP[3]);
	}

	/// get address in network byte order
	DEKAF2_NODISCARD
	constexpr BytesT    ToBytes  () const noexcept
	{
		return m_IP;
	}

	/// is address unspecified?
	DEKAF2_NODISCARD
	constexpr bool      IsUnspecified() const noexcept
	{
		return m_IP == BytesT { 0, 0, 0, 0 };
	}

	/// is address unspecifed?
	DEKAF2_NODISCARD
	constexpr bool      IsValid  () const noexcept
	{
		return !IsUnspecified();
	}

	/// is address a class A address?
	DEKAF2_NODISCARD
	constexpr bool      IsClassA() const noexcept
	{
	  return (ToUInt() & 0x80000000) == 0;
	}

	/// is address a class B address?
	DEKAF2_NODISCARD
	constexpr bool      IsClassB() const noexcept
	{
	  return (ToUInt() & 0xC0000000) == 0x80000000;
	}

	/// is address a class C address?
	DEKAF2_NODISCARD
	constexpr bool      IsClassC() const noexcept
	{
	  return (ToUInt() & 0xE0000000) == 0xC0000000;
	}

	/// is address a loopback address
	DEKAF2_NODISCARD
	constexpr bool      IsLoopback() const noexcept
	{
	  return (ToUInt() & 0xFF000000) == 0x7F000000;
	}

	/// is address a multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticast() const noexcept
	{
	  return (ToUInt() & 0xF0000000) == 0xE0000000;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPAddress4& a1,
	                                 const KIPAddress4& a2) noexcept
	{
	  return a1.m_IP == a2.m_IP;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator<(const KIPAddress4& a1,
	                                const KIPAddress4& a2) noexcept
	{
	  return a1.ToUInt() < a2.ToUInt();
	}

	/// get any address (= empty address)
	DEKAF2_NODISCARD
	static constexpr KIPAddress4 Any() noexcept
	{
	  return KIPAddress4();
	}

	/// get loopback address
	DEKAF2_NODISCARD
	static constexpr KIPAddress4 Loopback() noexcept
	{
	  return KIPAddress4(0x7F000001);
	}

	/// get broadcast address
	DEKAF2_NODISCARD
	static constexpr KIPAddress4 Broadcast() noexcept
	{
	  return KIPAddress4(0xFFFFFFFF);
	}

	/// get broadcast address with specified address and netmask
	DEKAF2_NODISCARD
	static constexpr KIPAddress4 Broadcast(const KIPAddress4& addr,
		const KIPAddress4& mask) noexcept
	{
	  return KIPAddress4(addr.ToUInt() | (mask.ToUInt() ^ 0xFFFFFFFF));
	}

//----------
private:
//----------

	static BytesT FromString(KStringView sAddress) noexcept;

	static constexpr BytesT FromIPv6(const KIPAddress6& IPv6) noexcept;

	BytesT m_IP { 0, 0, 0, 0 };

}; // KIPAddress4

DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(constexpr, KIPAddress4)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPAddress6
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KIPAddress4;

//----------
public:
//----------

	using BytesT = std::array<uint8_t, 16>;
	using ScopeT = uint32_t;

	constexpr          KIPAddress6() noexcept = default;

	constexpr explicit KIPAddress6(const BytesT& IP, ScopeT Scope = 0) noexcept
	                   : m_IP(IP)
	                   , m_Scope(Scope)
	                   {}

	constexpr explicit KIPAddress6(const KIPAddress4& IPv4) noexcept
	                   : m_IP(FromIPv4(IPv4))
	                   {}

	          explicit KIPAddress6(KStringView sAddress, ScopeT Scope = 0) noexcept
	                   : m_IP(FromString(sAddress))
	                   , m_Scope(Scope)
	                   {}

	/// get address as string
	DEKAF2_NODISCARD
	          KString   ToString (bool bWithBraces = false, bool bUnabridged = false) const noexcept;

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// get address in network byte order
	DEKAF2_NODISCARD
	constexpr BytesT    ToBytes  () const noexcept
	{
		return m_IP;
	}

	constexpr void      Scope(ScopeT Scope) noexcept
	{
	  m_Scope = Scope;
	}

	DEKAF2_NODISCARD
	constexpr ScopeT    Scope() const noexcept
	{
	  return m_Scope;
	}

	/// is address the loopback address?
	DEKAF2_NODISCARD
	constexpr bool      IsLoopback() const noexcept
	{
		return m_IP == BytesT { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	}

	/// is address unspecified?
	DEKAF2_NODISCARD
	constexpr bool      IsUnspecified() const noexcept
	{
		return m_IP == BytesT { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	}

	/// is address unspecifed?
	DEKAF2_NODISCARD
	constexpr bool      IsValid  () const noexcept
	{
		return !IsUnspecified();
	}

	/// is address link local?
	DEKAF2_NODISCARD
	constexpr bool      IsLinkLocal() const noexcept
	{
		return ((m_IP[0] == 0xfe) && ((m_IP[1] & 0xc0) == 0x80));
	}

	/// is address site local?
	DEKAF2_NODISCARD
	constexpr bool      IsSiteLocal() const noexcept
	{
		return ((m_IP[0] == 0xfe) && ((m_IP[1] & 0xc0) == 0xc0));
	}

	/// is address a mapped IPv4 address?
	DEKAF2_NODISCARD
	constexpr bool      IsV4Mapped() const noexcept
	{
		return (   (m_IP[0]  == 0)    && (m_IP[1]  == 0)
				&& (m_IP[2]  == 0)    && (m_IP[3]  == 0)
				&& (m_IP[4]  == 0)    && (m_IP[5]  == 0)
				&& (m_IP[6]  == 0)    && (m_IP[7]  == 0)
				&& (m_IP[8]  == 0)    && (m_IP[9]  == 0)
				&& (m_IP[10] == 0xff) && (m_IP[11] == 0xff));
	}

	/// is address a multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticast() const noexcept
	{
		return (m_IP[0] == 0xff);
	}

	/// is address aglobal multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticastGlobal() const noexcept
	{
		return ((m_IP[0] == 0xff) && ((m_IP[1] & 0x0f) == 0x0e));
	}

	/// is address a link-local multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticastLinkLocal() const noexcept
	{
		return ((m_IP[0] == 0xff) && ((m_IP[1] & 0x0f) == 0x02));
	}

	/// is address a node-local multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticastNodeLocal() const noexcept
	{
		return ((m_IP[0] == 0xff) && ((m_IP[1] & 0x0f) == 0x01));
	}

	/// is address a org-local multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticastOrgLocal() const noexcept
	{
		return ((m_IP[0] == 0xff) && ((m_IP[1] & 0x0f) == 0x08));
	}

	/// is address a site-local multicast address?
	DEKAF2_NODISCARD
	constexpr bool      IsMulticastSiteLocal() const noexcept
	{
		return ((m_IP[0] == 0xff) && ((m_IP[1] & 0x0f) == 0x05));
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPAddress6& a1,
									 const KIPAddress6& a2) noexcept
	{
		return a1.m_IP == a2.m_IP && a1.m_Scope == a2.m_Scope;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator<(const KIPAddress6& a1,
									const KIPAddress6& a2) noexcept
	{
		if (a1.m_IP < a2.m_IP) return true;
		if (a1.m_IP > a2.m_IP) return false;
		return a1.m_Scope < a2.m_Scope;
	}

	/// get any address (= empty address)
	DEKAF2_NODISCARD
	static constexpr KIPAddress6 Any() noexcept
	{
		return KIPAddress6();
	}

	/// get loopback address
	DEKAF2_NODISCARD
	static constexpr KIPAddress6 Loopback() noexcept
	{
		return KIPAddress6(BytesT { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });
	}

//----------
private:
//----------

	static BytesT FromString(KStringView sAddress) noexcept;

	static constexpr BytesT FromIPv4(const KIPAddress4& IPv4) noexcept
	{
		BytesT IP { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
		IP[12] = IPv4.m_IP[0];
		IP[13] = IPv4.m_IP[1];
		IP[14] = IPv4.m_IP[2];
		IP[15] = IPv4.m_IP[3];
		return IP;
	}

	BytesT m_IP    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	ScopeT m_Scope { 0 };

}; // KIPAddress6

DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(constexpr, KIPAddress6)

//-----------------------------------------------------------------------------
constexpr KIPAddress4::BytesT KIPAddress4::FromIPv6(const KIPAddress6& IPv6) noexcept
//-----------------------------------------------------------------------------
{
	BytesT IP;

	if (IPv6.IsV4Mapped())
	{
		IP = BytesT { IPv6.m_IP[12], IPv6.m_IP[13], IPv6.m_IP[14], IPv6.m_IP[15] };
	}
	else
	{
		IP = BytesT { 0, 0, 0, 0 };
	}

	return IP;

} // KIPAddress4::FromIPv6

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KIPAddress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	constexpr          KIPAddress() noexcept = default;

	constexpr          KIPAddress(KIPAddress4 IPAddress4) noexcept
	                   : m_v4(std::move(IPAddress4))
	                   , m_v6(KIPAddress6::BytesT{0})
	                   , m_Type(IPv4)
	                   {}

	constexpr          KIPAddress(KIPAddress6 IPAddress6) noexcept
	                   : m_v4(KIPAddress4::BytesT{0})
	                   , m_v6(std::move(IPAddress6))
	                   , m_Type(IPv6)
	                   {}

	          explicit KIPAddress(KStringView sAddress) noexcept
	                   : KIPAddress(FromString(sAddress))
	                   {}

	/// is address an IPv4 address?.
	DEKAF2_NODISCARD
	constexpr bool Is4() const noexcept
	{
	  return m_Type == AddressType::IPv4;
	}

	/// is address an IPv6 address?.
	DEKAF2_NODISCARD
	constexpr bool Is6() const noexcept
	{
	  return m_Type == AddressType::IPv6;
	}

	/// get address as string
	DEKAF2_NODISCARD
	KString ToString() const noexcept
	{
		if (Is4()) return m_v4.ToString();
		if (Is6()) return m_v6.ToString();
		return {};
	}

	explicit operator KString() const noexcept
	{
		return ToString();
	}

	/// get address as KIPAddress4 if possible, else as unspecified address
	DEKAF2_NODISCARD
	constexpr KIPAddress4 To4() const noexcept
	{
		switch (m_Type)
		{
			case AddressType::Invalid:
				return {};
			case AddressType::IPv4:
				return m_v4;
			case AddressType::IPv6:
				return KIPAddress4(m_v6);
		}
	}

	/// get address as KIPAddress6 if possible, else as unspecified address
	DEKAF2_NODISCARD
	constexpr KIPAddress6 To6() const noexcept
	{
		switch (m_Type)
		{
			case AddressType::Invalid:
				return {};
			case AddressType::IPv4:
				return KIPAddress6(m_v4);
			case AddressType::IPv6:
				return m_v6;
		}
	}

	/// returns true if the address either is an IPv4 address or can be converted into one
	DEKAF2_NODISCARD
	constexpr bool IsConvertibleTo4() const noexcept
	{
		if (!Is6()) return true;
		return m_v6.IsV4Mapped();
	}

	/// always returns true
	DEKAF2_NODISCARD
	constexpr bool IsConvertibleTo6() const noexcept
	{
		return true;
	}

	/// is address a loopback address?
	DEKAF2_NODISCARD
	constexpr bool IsLoopback() const noexcept
	{
	  return Is4() ? m_v4.IsLoopback()
		           : Is6() ? m_v6.IsLoopback() : false;
	}

	/// is address unspecifed?
	DEKAF2_NODISCARD
	constexpr bool IsUnspecified() const noexcept
	{
		return Is4() ? m_v4.IsUnspecified()
					 : Is6() ? m_v6.IsUnspecified() : true;
	}

	/// is address valid?
	DEKAF2_NODISCARD
	constexpr bool IsValid() const noexcept
	{
		return !IsUnspecified();
	}

	/// is address a multicast address?
	DEKAF2_NODISCARD
	constexpr bool IsMulticast() const noexcept
	{
		return Is4() ? m_v4.IsMulticast()
					 : Is6() ? m_v6.IsMulticast() : false;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KIPAddress& a1,
									 const KIPAddress& a2) noexcept
	{
	  if (a1.m_Type != a2.m_Type) return false;
	  if (a1.Is4()) return a1.m_v4 == a2.m_v4;
	  if (a1.Is6()) return a1.m_v4 == a2.m_v4;
	  return true;
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator<(const KIPAddress& a1,
									const KIPAddress& a2) noexcept
	{
	  if (a1.m_Type < a2.m_Type) return true;
	  if (a1.m_Type > a2.m_Type) return false;
	  if (a1.Is4()) return a1.m_v4 < a2.m_v4;
	  if (a1.Is6()) return a1.m_v6 < a2.m_v6;
	  return false;
	}

//----------
private:
//----------

	static KIPAddress FromString(KStringView sAddress) noexcept;

	KIPAddress4 m_v4;
	KIPAddress6 m_v6;

	enum AddressType : uint8_t { Invalid, IPv4, IPv6 } m_Type;

}; // KIPAddress

DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(constexpr, KIPAddress)

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPAddress4 IP)
{
	auto s = IP.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPAddress6 IP)
{
	auto s = IP.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KIPAddress IP)
{
	auto s = IP.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KIPAddress4> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPAddress4& IP, FormatContext& ctx) const
	{
		return formatter<string_view>::format(IP.ToString(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KIPAddress6> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPAddress6& IP, FormatContext& ctx) const
	{
		return formatter<string_view>::format(IP.ToString(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KIPAddress> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KIPAddress& IP, FormatContext& ctx) const
	{
		return formatter<string_view>::format(IP.ToString(), ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE

namespace std
{

template<>
struct hash<DEKAF2_PREFIX KIPAddress4>
{
	DEKAF2_CONSTEXPR_14 std::size_t operator()(const DEKAF2_PREFIX KIPAddress4& IP) const noexcept
	{
		auto Bytes = IP.ToBytes();
		return DEKAF2_PREFIX kHash(Bytes.data(), Bytes.size());
	}
};

template<>
struct hash<DEKAF2_PREFIX KIPAddress6>
{
	DEKAF2_CONSTEXPR_14 std::size_t operator()(const DEKAF2_PREFIX KIPAddress6& IP) const noexcept
	{
		auto Bytes = IP.ToBytes();
		return DEKAF2_PREFIX kHash(Bytes.data(), Bytes.size());
	}
};

template<>
struct hash<DEKAF2_PREFIX KIPAddress>
{
	DEKAF2_CONSTEXPR_14 std::size_t operator()(const DEKAF2_PREFIX KIPAddress& IP) const noexcept
	{
		auto IP6 = IP.To6();
		auto Bytes = IP6.ToBytes();
		return DEKAF2_PREFIX kHash(Bytes.data(), Bytes.size());
	}
};

} // end of namespace std
