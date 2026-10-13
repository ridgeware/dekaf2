#include "catch.hpp"

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystemstats.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpclient.h>

using namespace dekaf2;

namespace {

// holds each connection open until the client sends a line
class KHoldingServer : public KTCPServer
{

public:

	KHoldingServer(uint16_t iPort)
	: KTCPServer(iPort, false, 5)
	{
	}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		KString sLine;
		stream->ReadLine(sLine);
	}

}; // KHoldingServer

} // end of anonymous namespace

TEST_CASE("KSystemStats")
{
	SECTION("GatherAll")
	{
		KSystemStats Stats;

		// a source may be missing, like netstat in a container - but the gathering
		// must not throw
		CHECK_NOTHROW ( Stats.GatherAll() );

		const auto& Values = Stats.GetStats();

		CHECK ( Values.Get("cpuinfo_num_cores").sValue.Int64()   > 0 );
		CHECK ( Values.Get("meminfo_memtotal_kb").sValue.Int64() > 0 );
		CHECK ( Values.Get("disk_root_total_kb").sValue.Int64()  > 0 );
		CHECK ( Values.Get("boot_time_unix").sValue.Int64()      > 0 );
		CHECK ( Values.Get("hostname").sValue.empty() == false );

		// no entries without a value, for values a platform does not have
		for (const auto& Value : Values)
		{
			INFO ( Value.first );
			CHECK ( Value.second.sValue.empty() == false );
		}
	}

	SECTION("GatherNetstat")
	{
		KHoldingServer Server(7658);
		Server.Start(chrono::seconds(5), false);

		KTCPClient Client("127.0.0.1:7658", chrono::seconds(5));
		REQUIRE ( Client.is_open() );

		KSystemStats Stats;
		CHECK ( Stats.GatherNetstat() );

		auto iEstablished = Stats.GetStats().Get("netstat_tcp_established").sValue.Int64();

#ifdef DEKAF2_IS_OSX
		// since macOS 27 a program that does not come with the system sees no TCP
		// sockets of the system - neither netstat started by it nor the sysctl that
		// netstat reads list any, so there is nothing to check
		if (iEstablished != 0)
#endif
		{
			// both ends of the connection on this host
			CHECK ( iEstablished >= 2 );
		}

		Client.WriteLine("done").Flush();
	}

	SECTION("GatherProcs")
	{
		KSystemStats Stats;

		CHECK ( Stats.GatherProcs() > 0 );

		// this process itself is left out
		CHECK ( Stats.GetProcs().contains(KString::to_string(kGetPid())) == false );
	}

	SECTION("Backtrace")
	{
		auto sChain = KSystemStats::Backtrace(kGetPid());

		CHECK ( sChain.starts_with(kFormat("{}:", kGetPid())) );
	}
}
