
#include "catch.hpp"
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/dekaf2.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KTimer")
//-----------------------------------------------------------------------------
{
	SECTION("KTimer")
	{
		std::size_t iCalled = 0;

		KTimer Timer(chrono::milliseconds(50));

		auto ID = Timer.CallEvery(chrono::milliseconds(50), [&](KUnixTime tp)
		{
	//		kDebug(1, "the current timepoint is: {}", KTimer::ToTimeT(tp));
			++iCalled;

		}, false);

		kSleep(chrono::milliseconds(500));

		CHECK ( Timer.Cancel(ID) );
		CHECK ( iCalled >= 2 );
		CHECK ( iCalled <= 11 );
	}
}
