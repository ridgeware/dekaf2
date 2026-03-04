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
#include "klog.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <ws2tcpip.h>
#else
	#include <unistd.h>        // for sysconf()
	#include <arpa/inet.h>
	#include <sys/ioctl.h>     // for ioctl()
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
		#include <sys/sysinfo.h> // for sysinfo()
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KMACAddress::MAC KMACAddress::ReadFromInterface(KStringViewZ sInterfaceName, int sock) noexcept
//-----------------------------------------------------------------------------
{
	MAC  mac;
	bool bClear { true  };
	bool bClose { false };

	for (;;)
	{
#if DEKAF2_IS_MACOS

		std::array<int, 6> mib { CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST };

		if ((mib[5] = ::if_nametoindex(sInterfaceName.c_str())) == 0)
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

		strcpy(ifr.ifr_name, sInterfaceName.c_str());

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

		// TODO
		break;

#endif
		bClear = false;
		break;
	}

	if (bClose)
	{
		::close(sock);
	}

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

//-----------------------------------------------------------------------------
KNetworkInterface::Interfaces KNetworkInterface::GetAllInterfaces()
//-----------------------------------------------------------------------------
{
	std::vector<KNetworkInterface> Interfaces;

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

		auto family = it->ifa_addr->sa_family;

		if (family == AF_INET || family == AF_INET6)
		{
			KStringView sName(it->ifa_name);

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
				ni.m_sName = sName;
				ni.m_MAC   = KMACAddress(ni.m_sName, KMACAddress::FromInterface, sock);
				ni.m_Flags = CalcFlags(it->ifa_flags);
				Interfaces.push_back(ni);
				iface = Interfaces.begin() + (Interfaces.size() - 1);
			}

			KIPNetwork net;

			if (it->ifa_addr && it->ifa_netmask)
			{
				if (family == AF_INET)
				{
					net = KIPNetwork4(KIPAddress4(*(struct sockaddr_in*)(it->ifa_addr)),
					                  KIPAddress4(*(struct sockaddr_in*)(it->ifa_netmask)));

					if (it->ifa_dstaddr && iface->HasFlags(Broadcast))
					{
						iface->m_Broadcast = KIPAddress4(*(struct sockaddr_in*)(it->ifa_dstaddr));
					}
				}
				else
				{
					net = KIPNetwork6(KIPAddress6(*(struct sockaddr_in6*)(it->ifa_addr)),
					                  KIPAddress6(*(struct sockaddr_in6*)(it->ifa_netmask)));
				}
			}

			iface->m_Networks.push_back(std::move(net));
		}
	}

	if (sock >= 0)
	{
		::close(sock);
	}

	// Free memory
	freeifaddrs(ifaces);

#elif DEKAF2_IS_WINDOWS

	// TODO

#endif

	return Interfaces;

} // KNetworkInterface::GetAllInterfaces

DEKAF2_NAMESPACE_END
