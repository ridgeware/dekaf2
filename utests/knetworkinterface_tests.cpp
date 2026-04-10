#include "catch.hpp"

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/core/logging/klog.h>

using namespace dekaf2;

TEST_CASE("KNetworkInterface")
{
	SECTION("kGetNetworkInterfaces")
	{
		auto Interfaces = kGetNetworkInterfaces();
		bool bHadLoopback = false;

		for (auto& iface : Interfaces)
		{
			if (iface.HasFlags(KNetworkInterface::Loopback))
			{
				bHadLoopback = true;
				bool bHadv4Loopback = false;
				bool bHadv6Loopback = false;

				// there should not be a MAC address on the loopback interface
				CHECK ( iface.GetMAC().IsValid() == false );
				CHECK ( iface.HasFlags(KNetworkInterface::Up | KNetworkInterface::Running | KNetworkInterface::Loopback) );

				for (const auto& net : iface.GetNetworks())
				{
					if (net.Is4())
					{
						bHadv4Loopback = true;
						CHECK ( net == KIPNetwork("127.0.0.1/8") );
					}
					else if (net.Is6())
					{
						bHadv6Loopback = true;
						auto sNet = net.ToString();
						auto iScope = sNet.find('%');
						if (iScope != npos)
						{
							auto iPrefix = sNet.find('/');
							if (iPrefix != npos)
							{
								sNet.erase(iScope, iPrefix-iScope);
							}
						}
						INFO ( sNet );
						CHECK ( (sNet == "::1/128" || sNet == "fe80::1/64") );
					}
				}

				CHECK ( bHadv4Loopback );
				CHECK ( bHadv6Loopback );
			}
#if 0
			kDebug(0, "{}: {}", iface.GetName(), iface.PrintFlags());

			if (iface.GetMAC().IsValid())
			{
				kDebug(0, "  MAC: {}", iface.GetMAC().ToHex());
			}

			if (iface.IsWLAN())
			{
				kDebug(0, "  {}: {}", iface.GetWirelessProtocol(), iface.GetSSID());
			}

			if (iface.GetBroadcast().IsValid())
			{
				kDebug(0, "  Broadcast: {}", iface.GetBroadcast());
			}

			for (auto& n : iface.GetNetworks())
			{
				kDebug(0, "  inet{}: {}", n.Is4() ? 4 : 6, n);
			}
#if !DEKAF2_IS_MACOS
			kDebug(0, "  rx packets: {}, tx packets: {}, rx bytes: {}, tx bytes: {}",
				   iface.GetLinkStats().m_iRXPackets, iface.GetLinkStats().m_iTXPackets,
				   iface.GetLinkStats().m_iRXBytes  , iface.GetLinkStats().m_iTXBytes   );
#endif
#endif
		}

		CHECK ( bHadLoopback );
	}

	SECTION("KNetworkInterface")
	{
#if !DEKAF2_IS_MACOS
		KNetworkInterface iface("eth0");
#else
		KNetworkInterface iface("en0");
#endif
#if 0
		kDebug(0, "{}: {}", iface.GetName(), iface.PrintFlags());

		if (iface.GetMAC().IsValid())
		{
			kDebug(0, "  MAC: {}", iface.GetMAC().ToHex());
		}

		if (iface.IsWLAN())
		{
			kDebug(0, "  {}: {}", iface.GetWirelessProtocol(), iface.GetSSID());
		}

		if (iface.GetBroadcast().IsValid())
		{
			kDebug(0, "  Broadcast: {}", iface.GetBroadcast());
		}

		for (auto& n : iface.GetNetworks())
		{
			kDebug(0, "  inet{}: {}", n.Is4() ? 4 : 6, n);
		}
#if !DEKAF2_IS_MACOS
		kDebug(0, "  rx packets: {}, tx packets: {}, rx bytes: {}, tx bytes: {}",
			   iface.GetLinkStats().m_iRXPackets, iface.GetLinkStats().m_iTXPackets,
			   iface.GetLinkStats().m_iRXBytes  , iface.GetLinkStats().m_iTXBytes   );
#endif
#endif
	}
}
