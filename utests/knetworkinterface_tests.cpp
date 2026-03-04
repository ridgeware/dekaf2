#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/knetworkinterface.h>
#include <dekaf2/klog.h>

using namespace dekaf2;

TEST_CASE("KNetworkInterface")
{
	SECTION("kGetNetworkInterfaces")
	{
		auto Interfaces = kGetNetworkInterfaces();
		bool bHadLoopback = false;

		for (auto& i : Interfaces)
		{
			if (i.HasFlags(KNetworkInterface::Loopback))
			{
				bHadLoopback = true;
				bool bHadv4Loopback = false;
				bool bHadv6Loopback = false;

				// there should not be a MAC address on the loopback interface
				CHECK ( i.GetMAC().IsValid() == false );
				CHECK ( i.HasFlags(KNetworkInterface::Up | KNetworkInterface::Running | KNetworkInterface::Loopback) );

				for (const auto& net : i.GetNetworks())
				{
					if (net.Is4())
					{
						bHadv4Loopback = true;
						CHECK ( net == KIPNetwork("127.0.0.1/8") );
					}
					else if (net.Is6())
					{
						bHadv6Loopback = true;
						CHECK ( net == KIPNetwork("::1/128") );
					}
				}

				CHECK ( bHadv4Loopback );
				CHECK ( bHadv6Loopback );
			}
#if 0
			kDebug(0, "{}: {}", i.GetName(), i.PrintFlags());
			if (i.GetMAC().IsValid())
			{
				kDebug(0, "  MAC: {}", i.GetMAC().ToHex());
			}

			if (i.GetBroadcast().IsValid())
			{
				kDebug(0, "  Broadcast: {}", i.GetBroadcast());
			}

			for (auto& n : i.GetNetworks())
			{
				kDebug(0, "  inet{}: {}", n.Is4() ? 4 : 6, n);
			}
#endif
		}

		CHECK ( bHadLoopback );
	}
}
