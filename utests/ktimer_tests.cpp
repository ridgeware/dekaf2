
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

		KTimer Timer(std::chrono::milliseconds(50));

		auto ID = Timer.CallEvery(std::chrono::milliseconds(50), [&](KTimer::Timepoint tp)
		{
	//		kDebug(1, "the current timepoint is: {}", KTimer::ToTimeT(tp));
			++iCalled;
		});

		kMilliSleep(500);

		CHECK ( Timer.Cancel(ID) );
		CHECK ( iCalled >= 2 );
		CHECK ( iCalled <= 11 );
	}

	SECTION("KDuration")
	{
		KStopDuration StopD;

		for (int i = 0; i < 9; ++i)
		{
			StopD.Start();
			kMilliSleep(10);
			StopD.Stop();
			StopD.Start();
			kMilliSleep(5);
			StopD.Stop();
			StopD.Start();
			kMilliSleep(15);
			StopD.Stop();
		}

		CHECK ( StopD.Rounds() == 9 * 3 );
		auto ms = StopD.milliseconds();
		CHECK ( ms >= 270 );
		CHECK ( ms <= 270 + 100 );

		ms = StopD.millisecondsAverage();
		CHECK ( ms >= 10 );
		CHECK ( ms <= 15 );
	}

	SECTION("KDurations")
	{
		KStopDurations StopD;

		for (int i = 0; i < 9; ++i)
		{
			kMilliSleep(10);
			StopD.StoreInterval(0);
			kMilliSleep(5);
			StopD.StoreInterval(1);
			kMilliSleep(15);
			StopD.StoreInterval(2);
		}

		CHECK ( StopD.Rounds(0) == 9 );
		KDurations Stats;
		Stats += StopD;
		Stats += StopD;
		auto ms = Stats.milliseconds();

		CHECK ( Stats.size()    ==  3 );
		CHECK ( Stats.Rounds(0) == 18 );
		CHECK ( ms >= 270 * 2 );
		CHECK ( ms <= 270 * 3 );
	}
}
