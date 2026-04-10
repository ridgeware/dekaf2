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

#include "knetworkinterface.h"
#include <dekaf2/kcompatibility.h>
#include <dekaf2/krandom.h>
#include <dekaf2/kutf.h>
#include <dekaf2/klog.h>

#ifdef DEKAF2_IS_WINDOWS
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#include <io.h>
#else
	#include <unistd.h>        // for sysconf()
	#include <arpa/inet.h>
	#include <net/if.h>        // KNetworkInterface
	#include <netinet/in.h>    // KNetworkInterface
	#include <sys/socket.h>    // KNetworkInterface
	#include <ifaddrs.h>       // KNetworkInterface
	#ifdef DEKAF2_IS_MACOS
		// MacOS
		#include <sys/sysctl.h>    // for sysctl()
		#include <net/if_dl.h>     // KNetworkInterface
	#else
		// Unix
		#include <sys/ioctl.h>     // for ioctl()
		#if DEKAF2_HAS_INCLUDE(<linux/if_link.h>) || \
			(DEKAF2_GCC_VERSION_MAJOR > 5 && DEKAF2_GCC_VERSION_MINOR < 8)
			#define DEKAF2_HAS_IF_LINK_H 1
			#include <linux/if_link.h> // for rtnl_link_stats
		#endif
		#if DEKAF2_HAS_INCLUDE(<linux/wireless.h>) || \
			(DEKAF2_GCC_VERSION_MAJOR > 5 && DEKAF2_GCC_VERSION_MINOR < 8)
			#define DEKAF2_HAS_LINUX_WIRELESS 1
			#include <linux/wireless.h> // for Linux WE ioctls
		#endif
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_IS_WINDOWS

namespace {

//-----------------------------------------------------------------------------
KString ToUTF8(const wchar_t* wstr)
//-----------------------------------------------------------------------------
{
	if (!wstr || !wstr[0]) return {};
	return kutf::Convert<KString>(wstr);
}

//-----------------------------------------------------------------------------
bool MatchAdapterName(const IP_ADAPTER_ADDRESSES& adapter, KStringViewZ sName)
//-----------------------------------------------------------------------------
{
	if (sName == adapter.AdapterName) return true;

	if (adapter.FriendlyName)
	{
		return sName == ToUTF8(adapter.FriendlyName);
	}

	return false;
}

//-----------------------------------------------------------------------------
KNetworkInterface::IFFlags CalcWindowsFlags(const IP_ADAPTER_ADDRESSES& adapter)
//-----------------------------------------------------------------------------
{
	auto Flags = KNetworkInterface::IFFlags::None;

	if (adapter.OperStatus == IfOperStatusUp)
	{
		Flags |= KNetworkInterface::IFFlags::Up;
		Flags |= KNetworkInterface::IFFlags::Running;
	}

	if (adapter.IfType == IF_TYPE_SOFTWARE_LOOPBACK)
	{
		Flags |= KNetworkInterface::IFFlags::Loopback;
	}

	if (adapter.IfType == IF_TYPE_PPP || adapter.IfType == IF_TYPE_TUNNEL)
	{
		Flags |= KNetworkInterface::IFFlags::PointToPoint;
	}

	if (!(adapter.Flags & IP_ADAPTER_NO_MULTICAST))
	{
		Flags |= KNetworkInterface::IFFlags::MultiCast;
	}

	if (adapter.IfType == IF_TYPE_ETHERNET_CSMACD || adapter.IfType == IF_TYPE_IEEE80211)
	{
		Flags |= KNetworkInterface::IFFlags::Broadcast;
	}

	return Flags;
}

//-----------------------------------------------------------------------------
KString GetAdapterAddressesBuffer()
//-----------------------------------------------------------------------------
{
	KString buf;
	ULONG   bufLen = 15000;
	ULONG   ret;

	do
	{
		buf.resize(bufLen, '\0');
		ret = ::GetAdaptersAddresses(AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX,
			nullptr,
			reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()),
			&bufLen);
	}
	while (ret == ERROR_BUFFER_OVERFLOW);

	if (ret != ERROR_SUCCESS)
	{
		kDebug(1, "GetAdaptersAddresses failed with error {}", ret);
		buf.clear();
	}

	return buf;
}

//-----------------------------------------------------------------------------
void AppendAdapterNetworks(const IP_ADAPTER_ADDRESSES& adapter,
                           std::vector<KIPNetwork>& Networks)
//-----------------------------------------------------------------------------
{
	for (auto* ua = adapter.FirstUnicastAddress; ua != nullptr; ua = ua->Next)
	{
		auto* sa = ua->Address.lpSockaddr;

		KIPNetwork net;

		if (sa->sa_family == AF_INET)
		{
			auto* sin = reinterpret_cast<const sockaddr_in*>(sa);
			net = KIPNetwork4(KIPAddress4(*sin),
			                  static_cast<uint8_t>(ua->OnLinkPrefixLength));
		}
		else if (sa->sa_family == AF_INET6)
		{
			auto* sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
			net = KIPNetwork6(KIPAddress6(*sin6),
			                  static_cast<uint8_t>(ua->OnLinkPrefixLength));
		}

		if (net.IsValid())
		{
			Networks.push_back(std::move(net));
		}
	}
}

} // anonymous namespace

#endif // DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
KMACAddress::MAC KMACAddress::ReadFromInterface(KStringViewZ sInterfaceName, int sock) noexcept
//-----------------------------------------------------------------------------
{
	MAC  mac;
	bool bClear { true  };
#if DEKAF2_IS_UNIX && !DEKAF2_IS_MACOS
	bool bClose { false };
#endif

	for (;;)
	{
#if DEKAF2_IS_MACOS

		std::array<int, 6> mib { CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST };

		if ((mib[5] = static_cast<int>(::if_nametoindex(sInterfaceName.c_str()))) == 0)
		{
			kDebug(1, ::strerror(errno));
			break;
		}

		std::size_t len;

		if (::sysctl(mib.data(), 6, nullptr, &len, nullptr, 0) < 0)
		{
			kDebug(1, ::strerror(errno));
			break;
		}

		KString sBuf(len+1, '\0');

		if (::sysctl(mib.data(), 6, &sBuf[0], &len, nullptr, 0) < 0)
		{
			kDebug(1, ::strerror(errno));
			break;
		}

		auto* ifm = (struct if_msghdr *)sBuf.data();
		auto* sdl = (struct sockaddr_dl *)(ifm + 1);
		auto* ptr = (unsigned char *)LLADDR(sdl);

		mac = MAC { ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5] };

#elif DEKAF2_IS_UNIX

		if (sock < 0)
		{
			sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

			if (sock < 0)
			{
				kDebug(1, strerror(errno));
				break;
			}

			bClose = true;
		}

		struct ifreq ifr;

		strcpy_n(ifr.ifr_name, sInterfaceName.c_str(), IFNAMSIZ);

		if (::ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
		{
			kDebug(1, strerror(errno));
			break;
		}

		if (::ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
		{
			kDebug(1, strerror(errno));
			break;
		}

		auto* ptr = (unsigned char*)(ifr.ifr_hwaddr.sa_data);

		mac = MAC { ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5] };

#elif DEKAF2_IS_WINDOWS

		{
			auto buf = GetAdapterAddressesBuffer();

			if (buf.empty()) break;

			bool bFound = false;

			for (auto* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buf.data());
			     adapter != nullptr; adapter = adapter->Next)
			{
				if (MatchAdapterName(*adapter, sInterfaceName) && adapter->PhysicalAddressLength >= 6)
				{
					mac = MAC { adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
					            adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
					            adapter->PhysicalAddress[4], adapter->PhysicalAddress[5] };
					bFound = true;
					break;
				}
			}
			// jump out of the outer loop if not found, to skip bClear being set to false
			if (!bFound) break;
		}

#endif
		bClear = false;
		break;
	}

#if DEKAF2_IS_UNIX && !DEKAF2_IS_MACOS
	if (bClose)
	{
		::close(sock);
	}
#endif

	if (bClear)
	{
		mac.fill(0);
	}

	return mac;

} // KMACAddress::ReadFromInterface

//-----------------------------------------------------------------------------
KString KMACAddress::ToHex(char chSeparator) const
//-----------------------------------------------------------------------------
{
	if (chSeparator == '\0')
	{
		return kFormat("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
					   m_MAC[0], m_MAC[1], m_MAC[2], m_MAC[3], m_MAC[4], m_MAC[5]);
	}
	else
	{
		return kFormat("{1:02x}{0}{2:02x}{0}{3:02x}{0}{4:02x}{0}{5:02x}{0}{6:02x}",
					   chSeparator, m_MAC[0], m_MAC[1], m_MAC[2], m_MAC[3], m_MAC[4], m_MAC[5]);
	}

} // KMACAddress::Hex

//-----------------------------------------------------------------------------
KMACAddress KMACAddress::Random(bool bSetMultiCast) noexcept
//-----------------------------------------------------------------------------
{
	MAC m;
	kGetRandom(m.data(), m.size());

	if (bSetMultiCast)
	{
		m[0] |= 0x01;
	}
	else
	{
		// remove the multicast bit
		m[0] &= 0xfe;
	}

	// set the local bit
	m[0] |= 0x02;

	return m;

} // KMACAddress::Random

//-----------------------------------------------------------------------------
void KNetworkInterface::CheckWLANStatus(int sock) noexcept
//-----------------------------------------------------------------------------
{

#if DEKAF2_HAS_LINUX_WIRELESS

	m_sWirelessProtocol.clear();
	m_sSSID.clear();

	struct iwreq wrq;
	char essid[IW_ESSID_MAX_SIZE + 1] = {0};
	::memset(&wrq, 0, sizeof(struct iwreq));
	strcpy_n(wrq.ifr_name, m_sName.c_str(), IFNAMSIZ);

	if (::ioctl(sock, SIOCGIWNAME, &wrq) < 0)
	{
		// this is most probably simply no wireless interface, which would trigger ENOTTY
		if (errno != ENOTTY && errno != EOPNOTSUPP)
		{
			kDebug(0,"errno {}: {}", errno, strerror(errno));
		}
		return;
	}

	m_sWirelessProtocol = wrq.u.name;

	wrq.u.data.pointer = essid;
	if (ioctl(sock, SIOCGIWESSID, &wrq) < 0)
	{
		kDebug(1, strerror(errno));
		return;
	}

	m_sSSID = (char *)wrq.u.essid.pointer;
//	kDebug(0, "{}: wireless protocol: {}, SSID: {}", m_sName, m_sWirelessProtocol, m_sSSID);

#endif

} // KNetworkInterface::CheckWLANStatus

//-----------------------------------------------------------------------------
KNetworkInterface::IFFlags KNetworkInterface::CalcFlags(uint32_t iFlags) noexcept
//-----------------------------------------------------------------------------
{
	IFFlags Flags = IFFlags::None;

#if DEKAF2_IS_UNIX
#if defined(IFF_UP)
	if (iFlags & IFF_UP         ) Flags |= IFFlags::Up;
#endif
#if defined(IFF_BROADCAST)
	if (iFlags & IFF_BROADCAST  ) Flags |= IFFlags::Broadcast;
#endif
#if defined(IFF_DEBUG)
	if (iFlags & IFF_DEBUG      ) Flags |= IFFlags::Debug;
#endif
#if defined(IFF_LOOPBACK)
	if (iFlags & IFF_LOOPBACK   ) Flags |= IFFlags::Loopback;
#endif
#if defined(IFF_POINTOPOINT)
	if (iFlags & IFF_POINTOPOINT) Flags |= IFFlags::PointToPoint;
#endif
#if defined(IFF_RUNNING)
	if (iFlags & IFF_RUNNING    ) Flags |= IFFlags::Running;
#endif
#if defined(IFF_NOARP)
	if (iFlags & IFF_NOARP      ) Flags |= IFFlags::NoARP;
#endif
#if defined(IFF_PROMISC)
	if (iFlags & IFF_PROMISC    ) Flags |= IFFlags::Promisc;
#endif
#if defined(IFF_NOTRAILERS)
	if (iFlags & IFF_NOTRAILERS ) Flags |= IFFlags::NoTrailers;
#endif
#if defined(IFF_ALLMULTI)
	if (iFlags & IFF_ALLMULTI   ) Flags |= IFFlags::AllMulti;
#endif
#if defined(IFF_MULTICAST)
	if (iFlags & IFF_MULTICAST  ) Flags |= IFFlags::MultiCast;
#endif
#if defined(IFF_OACTIVE)
	if (iFlags & IFF_OACTIVE    ) Flags |= IFFlags::OActive;
#endif
#if defined(IFF_SIMPLEX)
	if (iFlags & IFF_SIMPLEX    ) Flags |= IFFlags::Simplex;
#endif
#if defined(IFF_MASTER)
	if (iFlags & IFF_MASTER     ) Flags |= IFFlags::Master;
#endif
#if defined(IFF_SLAVE)
	if (iFlags & IFF_SLAVE      ) Flags |= IFFlags::Slave;
#endif
#if defined(IFF_PORTSEL)
	if (iFlags & IFF_PORTSEL    ) Flags |= IFFlags::PortSel;
#endif
#if defined(IFF_AUTOMEDIA)
	if (iFlags & IFF_AUTOMEDIA  ) Flags |= IFFlags::AutoMedia;
#endif
#if defined(IFF_DYNAMIC)
	if (iFlags & IFF_DYNAMIC    ) Flags |= IFFlags::Dynamic;
#endif
#if defined(IFF_LOWERUP)
	if (iFlags & IFF_LOWERUP    ) Flags |= IFFlags::LowerUp;
#endif
#if defined(IFF_DORMANT)
	if (iFlags & IFF_DORMANT    ) Flags |= IFFlags::Dormant;
#endif
#if defined(IFF_ECHO)
	if (iFlags & IFF_ECHO       ) Flags |= IFFlags::Echo;
#endif
#endif // DEKAF2_IS_UNIX

	return Flags;

} // KNetworkInterface::CalcFlags

//-----------------------------------------------------------------------------
KString KNetworkInterface::PrintFlags  (IFFlags Flags) noexcept
//-----------------------------------------------------------------------------
{
	KString sFlags;

	constexpr std::array<std::pair<IFFlags, KStringView>, 21> FTable
	{{
		{ IFFlags::Up          , "Up"           },
		{ IFFlags::Broadcast   , "Broadcast"    },
		{ IFFlags::Debug       , "Debug"        },
		{ IFFlags::Loopback    , "Loopback"     },
		{ IFFlags::PointToPoint, "PointToPoint" },
		{ IFFlags::Running     , "Running"      },
		{ IFFlags::NoARP       , "NoARP"        },
		{ IFFlags::Promisc     , "Promisc"      },
		{ IFFlags::NoTrailers  , "NoTrailers"   },
		{ IFFlags::AllMulti    , "AllMulti"     },
		{ IFFlags::OActive     , "OActive"      },
		{ IFFlags::Simplex     , "Simplex"      },
		{ IFFlags::Master      , "Master"       },
		{ IFFlags::Slave       , "Slave"        },
		{ IFFlags::MultiCast   , "MultiCast"    },
		{ IFFlags::PortSel     , "PortSel"      },
		{ IFFlags::AutoMedia   , "AutoMedia"    },
		{ IFFlags::Dynamic     , "Dynamic"      },
		{ IFFlags::LowerUp     , "LowerUp"      },
		{ IFFlags::Dormant     , "Dormant"      },
		{ IFFlags::Echo        , "Echo"         },
	}};

	for (auto& f : FTable)
	{
		if ((Flags & f.first) == f.first)
		{
			if (!sFlags.empty())
			{
				sFlags += ',';
			}

			sFlags += f.second;
		}
	}

	return sFlags;

} // KNetworkInterface::PrintFlags

#if DEKAF2_IS_UNIX
//-----------------------------------------------------------------------------
bool KNetworkInterface::AppendInterfaceData(const ifaddrs& iface, int sock) noexcept
//-----------------------------------------------------------------------------
{
	KStringView sName(iface.ifa_name);

	if (m_sName.empty())
	{
		m_sName = sName;
		m_MAC   = KMACAddress(m_sName, KMACAddress::FromInterface, sock);
		m_Flags = CalcFlags(iface.ifa_flags);
		CheckWLANStatus(sock);
	}
	else if (m_sName != sName)
	{
		// names don't match ..
		return false;
	}

	auto family = iface.ifa_addr->sa_family;

	if (family != AF_INET && family != AF_INET6
#if DEKAF2_HAS_IF_LINK_H
												&& family != AF_PACKET
#endif
		)
	{
		// wrong address family
		return false;
	}

	KIPNetwork net;

	if (iface.ifa_addr && iface.ifa_netmask)
	{
		if (family == AF_INET)
		{
			net = KIPNetwork4(KIPAddress4(*(struct sockaddr_in*)(iface.ifa_addr)),
							  KIPAddress4(*(struct sockaddr_in*)(iface.ifa_netmask)));

			if (iface.ifa_dstaddr && HasFlags(Broadcast))
			{
				m_Broadcast = KIPAddress4(*(struct sockaddr_in*)(iface.ifa_dstaddr));
			}
		}
		else if (family == AF_INET6)
		{
			net = KIPNetwork6(KIPAddress6(*(struct sockaddr_in6*)(iface.ifa_addr)),
							  KIPAddress6(*(struct sockaddr_in6*)(iface.ifa_netmask)));
		}
#if DEKAF2_HAS_IF_LINK_H
		else if (family == AF_PACKET)
		{
			if (iface.ifa_data != nullptr)
			{
				auto* stats = (struct rtnl_link_stats*)(iface.ifa_data);

				m_LinkStats.m_iTXPackets = stats->tx_packets;
				m_LinkStats.m_iRXPackets = stats->rx_packets;
				m_LinkStats.m_iTXBytes   = stats->tx_bytes;
				m_LinkStats.m_iRXBytes   = stats->rx_bytes;
			}
		}
#endif
	}

	if (net.IsValid())
	{
		m_Networks.push_back(std::move(net));
	}

	return true;

} // KNetworkInterface::AppendInterfaceData

#endif // DEKAF2_IS_UNIX

//-----------------------------------------------------------------------------
KNetworkInterface::KNetworkInterface(KStringViewZ sInterface)
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_UNIX

	struct ifaddrs* ifaces = nullptr;

	// retrieve the current interfaces - returns 0 on success
	if (::getifaddrs(&ifaces) < 0)
	{
		kDebug (1, strerror(errno));
		return;
	}

#if DEKAF2_IS_MACOS

	int sock = -1;

#else

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sock < 0)
	{
		kDebug(1, strerror(errno));
		return;
	}

#endif

	for (auto it = ifaces; it != nullptr; it = it->ifa_next)
	{
		KStringView sName(it->ifa_name);

		if (sInterface == sName)
		{
			AppendInterfaceData(*it, sock);
		}
	}

	if (sock >= 0)
	{
		::close(sock);
	}

	// Free memory
	::freeifaddrs(ifaces);

#elif DEKAF2_IS_WINDOWS

	auto buf = GetAdapterAddressesBuffer();

	if (buf.empty()) return;

	for (auto* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buf.data());
	     adapter != nullptr; adapter = adapter->Next)
	{
		if (!MatchAdapterName(*adapter, sInterface)) continue;

		m_sName = adapter->FriendlyName
		        ? ToUTF8(adapter->FriendlyName)
		        : KString(adapter->AdapterName);

		if (adapter->PhysicalAddressLength >= 6)
		{
			m_MAC = KMACAddress(KMACAddress::MAC {
				adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
				adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
				adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]
			});
		}

		m_Flags = CalcWindowsFlags(*adapter);

		if (adapter->IfType == IF_TYPE_IEEE80211)
		{
			m_sWirelessProtocol = "IEEE 802.11";
		}

		AppendAdapterNetworks(*adapter, m_Networks);
		break;
	}

#endif

} // KNetworkInterface::KNetworkInterface

//-----------------------------------------------------------------------------
KNetworkInterface::Interfaces KNetworkInterface::GetAllInterfaces(KStringView sStartsWith)
//-----------------------------------------------------------------------------
{
	Interfaces Interfaces;

#if DEKAF2_IS_UNIX

	struct ifaddrs* ifaces = nullptr;

	// retrieve the current interfaces - returns 0 on success
	if (::getifaddrs(&ifaces) < 0)
	{
		kDebug (1, strerror(errno));
		return {};
	}

#if DEKAF2_IS_MACOS

	int sock = -1;

#else

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sock < 0)
	{
		kDebug(1, strerror(errno));
		return Interfaces;
	}

#endif

	auto iface = Interfaces.end();

	for (auto it = ifaces; it != nullptr; it = it->ifa_next)
	{
		if ((it->ifa_flags & IFF_UP) == 0)
		{
			// we only want to list the running interfaces
			continue;
		}

		KStringView sName(it->ifa_name);

		if (!sName.starts_with(sStartsWith))
		{
			// wrong start of interface name
			continue;
		}

		// on MacOS the interface address list is grouped by name, therefore
		// let's first check if the last iface iterator points already to the
		// right entry
		if (iface == Interfaces.end() || iface->GetName() != sName)
		{
			// no, search through the whole vector - this is probably Linux, or the next interface
			iface = std::find_if(Interfaces.begin(), Interfaces.end(), [sName](const KNetworkInterface& ifa)
			{
				return ifa.GetName() == sName;
			});
		}

		if (iface == Interfaces.end())
		{
			KNetworkInterface ni;

			if (ni.AppendInterfaceData(*it, sock))
			{
				Interfaces.push_back(ni);
				iface = Interfaces.begin() + (Interfaces.size() - 1);
			}
			else
			{
				// skip this data block
				continue;
			}
		}
		else
		{
			iface->AppendInterfaceData(*it, sock);
		}
	}

	if (sock >= 0)
	{
		::close(sock);
	}

	// Free memory
	::freeifaddrs(ifaces);

#elif DEKAF2_IS_WINDOWS

	auto buf = GetAdapterAddressesBuffer();

	if (buf.empty()) return Interfaces;

	for (auto* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buf.data());
	     adapter != nullptr; adapter = adapter->Next)
	{
		if (adapter->OperStatus != IfOperStatusUp)
		{
			continue;
		}

		KString sName = adapter->FriendlyName
		              ? ToUTF8(adapter->FriendlyName)
		              : KString(adapter->AdapterName);

		if (!sName.starts_with(sStartsWith))
		{
			continue;
		}

		KNetworkInterface ni;
		ni.m_sName = std::move(sName);

		if (adapter->PhysicalAddressLength >= 6)
		{
			ni.m_MAC = KMACAddress(KMACAddress::MAC {
				adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
				adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
				adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]
			});
		}

		ni.m_Flags = CalcWindowsFlags(*adapter);

		if (adapter->IfType == IF_TYPE_IEEE80211)
		{
			ni.m_sWirelessProtocol = "IEEE 802.11";
		}

		AppendAdapterNetworks(*adapter, ni.m_Networks);

		Interfaces.push_back(std::move(ni));
	}

#endif

	return Interfaces;

} // KNetworkInterface::GetAllInterfaces

DEKAF2_NAMESPACE_END
