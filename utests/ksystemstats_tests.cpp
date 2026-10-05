#include "catch.hpp"

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystemstats.h>
#include <dekaf2/system/os/ksystem.h>

using namespace dekaf2;

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
