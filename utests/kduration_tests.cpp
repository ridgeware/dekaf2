
#include "catch.hpp"
#include <dekaf2/kduration.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/klog.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KDuration")
//-----------------------------------------------------------------------------
{
	SECTION("KDuration")
	{
		KStopDuration StopD;

		for (int i = 0; i < 9; ++i)
		{
			StopD.Start();
			kSleep(chrono::milliseconds(10));
			StopD.Stop();
			StopD.Start();
			kSleep(chrono::milliseconds(5));
			StopD.Stop();
			StopD.Start();
			kSleep(chrono::milliseconds(15));
			StopD.Stop();
		}

		CHECK ( StopD.Rounds() == 9 * 3 );
		auto ms = StopD.milliseconds();
		CHECK ( ms >= chrono::milliseconds(270) );
		CHECK ( ms <= chrono::milliseconds(270 * 2) );

		ms = StopD.average().milliseconds();
		CHECK ( ms >= chrono::milliseconds(10) );
		CHECK ( ms <= chrono::milliseconds(15) );
	}

	SECTION("KDurations")
	{
		KStopDurations StopD;

		for (int i = 0; i < 9; ++i)
		{
			kSleep(chrono::milliseconds(10));
			StopD.StoreInterval(0);
			kSleep(chrono::milliseconds(5));
			StopD.StoreInterval(1);
			kSleep(chrono::milliseconds(15));
			StopD.StoreInterval(2);
		}

		CHECK ( StopD.Rounds(0) == 9 );
		KDurations Stats;
		Stats += StopD;
		Stats += StopD;
		auto ms = Stats.duration().milliseconds();

		CHECK ( Stats.size()    ==  3 );
		CHECK ( Stats.Rounds(0) == 18 );
		CHECK ( ms >= chrono::milliseconds(270 * 2) );
		CHECK ( ms <= chrono::milliseconds(270 * 4) );
	}

	SECTION("comparison")
	{
		KDuration a = std::chrono::milliseconds(700);
		KDuration b = std::chrono::milliseconds(500);
		CHECK ( a >  b );
		CHECK ( a >= b );
		CHECK ( a != b );
		CHECK ( b <  a );
		CHECK ( b <= a );
	}
}
