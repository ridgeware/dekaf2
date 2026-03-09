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

/// @file knetworkinterface.h
/// gathering information about system network interfaces

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kipaddress.h"
#include "kipnetwork.h"
#include "khex.h"
#include <array>

#ifndef DEKAF2_HAS_CPP_17
	#include <boost/container/vector.hpp>
#else
	#include <vector>
#endif

#if DEKAF2_IS_UNIX
struct ifaddrs;
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a MAC address container
class DEKAF2_PUBLIC KMACAddress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// parsing mode from string MAC address representation
	enum Mode { Relaxed, Strict, FromInterface };

	using MAC = std::array<uint8_t, 6>;

	                   KMACAddress() = default;

	/// constexpr construct from bytes in network order
	         constexpr KMACAddress(MAC mac) noexcept
	                   : m_MAC(mac)
	                   {}

	/// constexpr construct from string representation
	/// @param sMac the string representation
	/// @param mode either Relaxed, which takes strings with or without separators,
	/// or Strict, which only accepts the colon separated format like "01:02:03:04:05:06",
	/// or FromInterface, which reads the address of the given named interface
	/// -  defaults to Relaxed
	explicit constexpr KMACAddress(KStringView sMac, Mode mode = Mode::Relaxed) noexcept
	                   : m_MAC(FromString(sMac, mode))
	                   {}

	/// construct from interface with given name
	/// @param sInterface the name of the interface to be read
	/// @param mode set to Mode::FromInterface - otherwise the interface name will be interpreted as input string
	/// @param sock if set to a positive value will be used as socket handle for interface queries, else an internal
	/// socket will be opened - defaults to -1
	                   KMACAddress(KStringViewZ sInterface, Mode mode, int sock = -1) noexcept
	                   : m_MAC(mode == Mode::FromInterface ? ReadFromInterface(sInterface, sock)
	                                                       : FromString(sInterface, mode))
	                   {}

	/// return the bytes in network order
	DEKAF2_NODISCARD
	constexpr const MAC& ToBytes() const      { return m_MAC; }

	/// return as a 64 bit integer in host order
	DEKAF2_NODISCARD
	constexpr uint64_t ToUInt() const
	{
		return (static_cast<uint64_t>(m_MAC[0]) << 40)
		     | (static_cast<uint64_t>(m_MAC[1]) << 32)
		     | (static_cast<uint64_t>(m_MAC[2]) << 24)
		     | (static_cast<uint64_t>(m_MAC[3]) << 16)
		     | (static_cast<uint64_t>(m_MAC[4]) <<  8)
		     |  static_cast<uint64_t>(m_MAC[5]);
	}

	/// returns true if this is a MAC address with local scope, returns true for globally unique address.
	DEKAF2_NODISCARD
	constexpr bool IsLocal() const noexcept { return (m_MAC[0] & 2) != 0; }

	/// returns true if this is a multicast address, false for individual address
	DEKAF2_NODISCARD
	constexpr bool IsMulticast() const noexcept { return (m_MAC[0] & 1) != 0; }

	/// returns true if this address is valid (not empty)
	DEKAF2_NODISCARD
	constexpr bool IsValid() const noexcept { return !IsEqual(m_MAC, Null()); }

	/// returns a string representation of the MAC address
	/// @param chSeparator the character to be used as separator between byte pairs, defaults to ':'. A \0 inserts no separator at all.
	DEKAF2_NODISCARD
	KString ToHex(char chSeparator = ':') const;

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const KMACAddress& a1, const KMACAddress& a2) noexcept
	{
		return IsEqual(a1.m_MAC, a2.m_MAC);
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator<(const KMACAddress& a1, const KMACAddress& a2) noexcept
	{
		return a1.ToUInt() < a2.ToUInt();
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator==(const MAC& m1, const MAC& m2) noexcept
	{
		return IsEqual(m1, m2);
	}

	DEKAF2_NODISCARD
	friend constexpr bool operator!=(const MAC& m1, const MAC& m2) noexcept
	{
		return !IsEqual(m1, m2);
	}

	/// the empty MAC address
	static constexpr MAC Null() noexcept
	{
		return MAC { 0, 0, 0, 0, 0, 0 };
	}

//------
private:
//------

	static constexpr bool IsEqual(const MAC& m1, const MAC& m2) noexcept
	{
		return m1[0] == m2[0] && m1[1] == m2[1] && m1[2] == m2[2] &&
		       m1[3] == m2[3] && m1[4] == m2[4] && m1[5] == m2[5];
	}

	static MAC ReadFromInterface(KStringViewZ sInterface, int sock = -1) noexcept;

	static constexpr MAC FromString(KStringView sMac, Mode mode) noexcept
	{
		switch (mode)
		{
			default:
			case Relaxed:
				return kBytesFromHex<MAC>(sMac);

			case Strict:
				return kBytesFromHex<MAC, 1, 2>(sMac, ":");

			case FromInterface:
				return ReadFromInterface(KString(sMac));
		}
	}

	MAC m_MAC { Null() };

}; // KMACAddress

DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(constexpr, KMACAddress)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// available information about a network interface
class DEKAF2_PUBLIC KNetworkInterface
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using Networks   = std::vector<KIPNetwork>;

#ifndef DEKAF2_HAS_CPP_17
	// boost::container allows use of incomplete types
	using Interfaces = boost::container::vector<KNetworkInterface>;
#else
	// std::vector beginning with C++17 allows incomplete types in the
	// template instantiation
	using Interfaces = std::vector<KNetworkInterface>;
#endif

	/// link stats for an interface
	struct LinkStats
	{
		std::size_t   m_iTXPackets { 0 }; ///< sent packets
		std::size_t   m_iRXPackets { 0 }; ///< received packets
		std::size_t   m_iTXBytes   { 0 }; ///< sent bytes
		std::size_t   m_iRXBytes   { 0 }; ///< received bytes
	};

	/// network interface flags
	enum IFFlags
	{
		None         = 0,       ///< not set
		Up           = 1 <<  0, ///< Interface is up
		Broadcast    = 1 <<  1, ///< Valid broadcast address set
		Debug        = 1 <<  2, ///< Internal debugging flag
		Loopback     = 1 <<  3, ///< Interface is a loopback interface
		PointToPoint = 1 <<  4, ///< Interface is a point-to-point link
		Running      = 1 <<  5, ///< Resources allocated
		NoARP        = 1 <<  6, ///< No arp protocol, L2 destination address not set
		Promisc      = 1 <<  7, ///< Interface is in promiscuous mode
		NoTrailers   = 1 <<  8, ///< Avoid use of trailers
		AllMulti     = 1 <<  9, ///< Receive all multicast packets
		OActive      = 1 << 10, ///< Transmission in progress (BSD)
		Simplex      = 1 << 11, ///< Can't hear own transmissions (BSD)
		Master       = 1 << 12, ///< Master of a load balancing bundle
		Slave        = 1 << 13, ///< Slave of a load balancing bundle
		MultiCast    = 1 << 14, ///< Supports multicast
		PortSel      = 1 << 15, ///< Is able to select media type via ifmap
		AutoMedia    = 1 << 16, ///< Auto media selection active
		Dynamic      = 1 << 17, ///< The addresses are lost when the interface goes down
		LowerUp      = 1 << 18, ///< Driver signals L1 up
		Dormant      = 1 << 19, ///< Driver signals dormant
		Echo         = 1 << 10, ///< Echo sent packets
	};

	KNetworkInterface() = default;

	/// construct for existing interface
	/// @param sInterface a interface name in the list of operating system network interfaces
	KNetworkInterface(KStringViewZ sInterface);

	/// construct from full set of elements
	KNetworkInterface(KString sName, IFFlags Flags, KMACAddress MAC, Networks networks)
	: m_sName    (std::move(sName))
	, m_MAC      (std::move(MAC))
	, m_Networks (std::move(networks))
	, m_Flags    (Flags)
	{}

	/// get the interface name
	DEKAF2_NODISCARD
	const KString&     GetName     () const noexcept { return m_sName;     }

	/// get the MAC address of the interface
	DEKAF2_NODISCARD
	const KMACAddress& GetMAC      () const noexcept { return m_MAC;       }

	/// get the broadcast addres of the interface, if any
	DEKAF2_NODISCARD
	const KIPAddress&  GetBroadcast() const noexcept { return m_Broadcast; }

	/// get the list of assigned networks of the interface
	DEKAF2_NODISCARD
	const Networks&    GetNetworks () const noexcept { return m_Networks;  }

	/// get the interface flags
	DEKAF2_NODISCARD
	IFFlags            GetFlags    () const noexcept { return m_Flags;     }

	/// get the link stats for the interface (empty on MacOS)
	DEKAF2_NODISCARD
	const LinkStats&   GetLinkStats() const noexcept { return m_LinkStats; }

	/// get the interface flags converted into a string
	DEKAF2_NODISCARD
	KString            PrintFlags  () const
	{
		return PrintFlags(GetFlags());
	}

	/// checks if the given flags are set for the interface
	DEKAF2_NODISCARD
	bool               HasFlags    (IFFlags flags) const noexcept
	{
		return (m_Flags & flags) == flags;
	}

	/// returns true if this is a wireless interface
	DEKAF2_NODISCARD
	bool               IsWLAN      () const noexcept
	{
		return !m_sWirelessProtocol.empty();
	}

	/// returns wireless protocoll if this is a wireless interface, typically starts with "IEEE 802.11"
	DEKAF2_NODISCARD
	const KString&     GetWirelessProtocol() const noexcept
	{
		return m_sWirelessProtocol;
	}

	/// returns (connected) SSID if this is a wireless interface
	DEKAF2_NODISCARD
	const KString&     GetSSID     () const noexcept
	{
		return m_sSSID;
	}

	/// static method: return a vector of all network interfaces of the sytem
	/// @param sStartsWith start of interface name(s) to be looked up, defaults to empty string == all interfaces
	DEKAF2_NODISCARD
	static Interfaces  GetAllInterfaces(KStringView sStartsWith = {});

	/// helper method: convert operation specific numeric flags into the class enum values
	DEKAF2_NODISCARD
	static IFFlags     CalcFlags   (uint32_t iFlags) noexcept;

	/// helper method: print enum flags into a string
	DEKAF2_NODISCARD
	static KString     PrintFlags  (IFFlags Flags) noexcept;

//------
private:
//------

#if DEKAF2_IS_UNIX
	/// append more data from an ifaddrs struct
	bool AppendInterfaceData(const ifaddrs& iface, int sock) noexcept;
#endif
	/// add WLAN information, if any
	void CheckWLANStatus(int sock) noexcept;

	KString       m_sName;
	KString       m_sWirelessProtocol;
	KString       m_sSSID;
	KMACAddress   m_MAC;
	KIPAddress    m_Broadcast;
	Networks      m_Networks;
	IFFlags       m_Flags           { IFFlags::None };
	LinkStats     m_LinkStats;

}; // KNetworkInterface

DEKAF2_ENUM_IS_FLAG(KNetworkInterface::IFFlags)

/// return a vector of all network interfaces of the sytem
/// @param sStartsWith start of interface name(s) to be looked up, defaults to empty string == all interfaces
DEKAF2_NODISCARD DEKAF2_PUBLIC inline
KNetworkInterface::Interfaces kGetNetworkInterfaces(KStringView sStartsWith = {})
{
	return KNetworkInterface::GetAllInterfaces(sStartsWith);
}

DEKAF2_NAMESPACE_END
