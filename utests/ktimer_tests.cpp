
#include "catch.hpp"
#include <dekaf2/ktimer.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/klog.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KTimer")
//-----------------------------------------------------------------------------
{
	SECTION("KTimer")
	{
		std::size_t iCalled = 0;

		KTimer Timer(chrono::milliseconds(50));
		Timer.DestructWithJoin();

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
