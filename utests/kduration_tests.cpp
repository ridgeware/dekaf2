
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

	SECTION("timespec")
	{
		struct timespec ts;
		ts.tv_sec = 16;
		ts.tv_nsec = 234465478;
		KDuration d(ts);
		CHECK ( d.nanoseconds().count() == 16234465478 );
		auto ts2 = d.to_timespec();
		CHECK ( ts2.tv_sec  == 16 );
		CHECK ( ts2.tv_nsec == 234465478 );
		struct timespec ts3(d);
		CHECK ( ts3.tv_sec  == 16 );
		CHECK ( ts3.tv_nsec == 234465478 );
	}

	SECTION("timeval")
	{
		struct timeval tv;
		tv.tv_sec = 16;
		tv.tv_usec = 234465;
		KDuration d(tv);
		CHECK ( d.microseconds().count() == 16234465 );
		auto tv2 = d.to_timeval();
		CHECK ( tv2.tv_sec  == 16 );
		CHECK ( tv2.tv_usec == 234465 );
		struct timeval tv3(d);
		CHECK ( tv3.tv_sec  == 16 );
		CHECK ( tv3.tv_usec == 234465 );
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

	SECTION("ToString")
	{
		KDuration D(chrono::nanoseconds(2387464723123874534));
		CHECK ( D.ToString(KDuration::Smart    ) == "75.7 yrs" );
		CHECK ( D.ToString(KDuration::Long     ) == "75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs" );
		CHECK ( D.ToString(KDuration::Brief    ) == "76 y" );
		CHECK ( D.ToString(KDuration::Condensed) == "3947w3d16h38m43s123ms874µs534ns" );
		D = chrono::seconds(23482);
		CHECK ( D.ToString(KDuration::Condensed) == "6h31m22s" );
		D = chrono::seconds(1);
		CHECK ( D.ToString(KDuration::Condensed) == "1s" );
		D = chrono::milliseconds(734);
		CHECK ( D.ToString(KDuration::Condensed) == "734ms" );
		D = chrono::milliseconds(734);
		CHECK ( D.ToString(KDuration::Brief) == "734 ms" );
		D = chrono::milliseconds(1055);
		CHECK ( D.ToString(KDuration::Brief) == "1.1 s" );
	}

	SECTION("FromString")
	{
		KDuration D("75.7 yrs");
		CHECK ( D.ToString(KDuration::Smart    ) == "75.7 yrs" );
		D = KDuration("75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs");
		CHECK ( D.ToString(KDuration::Long     ) == "75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs" );
		D = KDuration("-75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs");
		CHECK ( D.ToString(KDuration::Long     ) == "-75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs" );
		D = KDuration("76 y");
		CHECK ( D.ToString(KDuration::Brief    ) == "76 y" );
		D = KDuration("-76 y");
		CHECK ( D.ToString(KDuration::Brief    ) == "-76 y" );
		D = KDuration("3947w3d16h38m43s123ms874µs534ns");
		CHECK ( D.ToString(KDuration::Condensed) == "3947w3d16h38m43s123ms874µs534ns" );
		D = KDuration("-3947w3d16h38m43s123ms874µs534ns");
		CHECK ( D.ToString(KDuration::Condensed) == "-3947w3d16h38m43s123ms874µs534ns" );
		D = KDuration("6h31m22s");
		CHECK ( D.ToString(KDuration::Condensed) == "6h31m22s" );
		D = KDuration("1s");
		CHECK ( D.ToString(KDuration::Condensed) == "1s" );
		D = KDuration("734ms");
		CHECK ( D.ToString(KDuration::Condensed) == "734ms" );
		D = KDuration("+734ms");
		CHECK ( D.ToString(KDuration::Condensed) == "734ms" );
		D = KDuration("-734ms");
		CHECK ( D.ToString(KDuration::Condensed) == "-734ms" );
		D = KDuration("734 ms");
		CHECK ( D.ToString(KDuration::Brief) == "734 ms" );
		D = KDuration("1.1 s");
		CHECK ( D.ToString(KDuration::Brief) == "1.1 s" );
		D = KDuration("874µs");
		CHECK ( D.ToString(KDuration::Brief) == "874 µs" );
		D = KDuration("874us");
		CHECK ( D.ToString(KDuration::Brief) == "874 µs" );
		D = KDuration("874 microseconds");
		CHECK ( D.ToString(KDuration::Brief) == "874 µs" );
		D = KDuration("874µ");
		CHECK ( D.ToString(KDuration::Brief) == "874 µs" );

		// bad format

		D = KDuration("1.1 ");
		CHECK ( D.ToString(KDuration::Brief) == "0 ns" );
		D = KDuration("+-100s");
		CHECK ( D.ToString(KDuration::Brief) == "0 ns" );
		D = KDuration("17k");
		D = KDuration("ns");
		CHECK ( D.ToString(KDuration::Brief) == "0 ns" );
		CHECK ( D.ToString(KDuration::Brief) == "0 ns" );
		D = KDuration("3947w3d16h38m-43s123ms874µs534ns");
		CHECK ( D.ToString(KDuration::Condensed) == "0ns" );
		D = KDuration("75 yrs, -36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs");
		CHECK ( D.ToString(KDuration::Long     ) == "less than a nanosecond" );
	}

	SECTION("format")
	{
		std::vector<std::pair<KStringView, KDuration>> svector1 {
			{    "4.1 y",   chrono::seconds(127834275)  },
#if DEKAF2_HAS_NANOSECONDS_SYS_CLOCK
			{   "1.2 µs",   chrono::nanoseconds(1235)   },
			{   "235 ns",   chrono::nanoseconds(235)    },
#else
			{     "1 µs",   chrono::nanoseconds(1235)   },
			{     "0 µs",   chrono::nanoseconds(235)    },
#endif
			{     "10 s",   chrono::milliseconds(10123) },
			{     "14 y",   chrono::hours(123453)       },
			{    "12 µs",   chrono::microseconds(12)    }
		};

		for (auto& p : svector1)
		{
			KString s;
			s = kFormat("{}", p.second);
			CHECK ( s == p.first );
		}
	}

	SECTION("KStopTime")
	{
		KStopTime Stop(KStopTime::Halted);
		kSleep(chrono::milliseconds(20));
		Stop.clear();
		kSleep(chrono::milliseconds(50));
		auto took = Stop.elapsed();
		CHECK ( took > chrono::milliseconds(40) );
		CHECK ( took < chrono::milliseconds(60) );
	}
}
