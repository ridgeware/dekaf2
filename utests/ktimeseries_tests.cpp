
#include "catch.hpp"
#include <dekaf2/ktimeseries.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/klog.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KTimeSeries")
//-----------------------------------------------------------------------------
{
	SECTION("KTimeSeries")
	{
		KTimeSeries<uint64_t> TS;

		TS += 123;
		auto tp1 = TS.begin()->first;
		TS.Add(tp1, 426);
		TS.Add(tp1, 332);

		CHECK ( TS.size() == 1 );
		CHECK ( TS.Get(tp1).Count() ==   3 );
		CHECK ( TS.Get(tp1).Min()   == 123 );
		CHECK ( TS.Get(tp1).Mean()  == 293 );
		CHECK ( TS.Get(tp1).Max()   == 426 );

		auto tp2 = tp1 + decltype(TS)::Duration(1);
		auto tp3 = tp1 + decltype(TS)::Duration(2);
		auto tp4 = tp1 + decltype(TS)::Duration(3);

		TS.Add(tp3,  23847);
		TS.Add(tp3, 489742);
		TS.Add(tp3, 239823);
		TS.Add(tp3,   2938);

		CHECK ( TS.size() == 2 );

		CHECK ( TS.Get(tp2).Count() == 0 );
		CHECK ( TS.Get(tp2).Min()   == 0 );
		CHECK ( TS.Get(tp2).Mean()  == 0 );
		CHECK ( TS.Get(tp2).Max()   == 0 );

		CHECK ( TS.Get(tp3).Count() ==      4 );
		CHECK ( TS.Get(tp3).Min()   ==   2938 );
		CHECK ( TS.Get(tp3).Mean()  == 189087 );
		CHECK ( TS.Get(tp3).Max()   == 489742 );

		CHECK ( TS.Sum().Mean()         == 108175 );
		CHECK ( TS.Sum(TS.Range(tp1, tp2)).Mean() ==    293 );
		CHECK ( TS.Sum(TS.Range(tp1, tp2)).Max()  ==    426 );
		CHECK ( TS.Sum(TS.Range(tp1, tp4)).Mean() == 108175 );
		CHECK ( TS.Sum(TS.Range(tp1, tp4)).Max()  == 489742 );

		CHECK ( TS.Median() == (189087 + 293) / 2 );
		CHECK ( TS.Median(TS.Range(TS.begin(), TS.end())) == (189087 + 293) / 2 );

		auto TS2 = TS;

		CHECK ( TS.size() == 2 );
		TS.EraseAfter(tp4);
		CHECK ( TS.size() == 2 );
		TS.EraseAfter(tp3);
		CHECK ( TS.size() == 1 );
		CHECK ( TS.Sum(TS.Range(tp1, tp4)).Mean() ==    293 );
		CHECK ( TS.Sum(TS.Range(tp1, tp4)).Max() ==    426 );

		TS2.Add(tp4, 23484);

		CHECK ( TS2.Median() == 23484 );
		CHECK ( TS2.Median(TS.Range(TS2.begin(), TS2.end())) == 23484 );

		auto TS_BAK = TS2;

		CHECK ( TS2.size() == 3 );
		auto TSCopy = TS2.Last(2);
		CHECK ( TSCopy.size() == 2 );
		TS2.ResizeToLast(2);
		CHECK ( TS2.size() == 2 );

		TS2 = TS_BAK;

		CHECK ( TS2.size() == 3 );
		TSCopy = TS2.First(2);
		CHECK ( TSCopy.size() == 2 );
		TS2.ResizeToFirst(2);
		CHECK ( TS2.size() == 2 );

		TS2 = TS_BAK;
		CHECK ( TS2.size() == 3 );
		TS2.MaxSize(3);
		CHECK ( TS2.size() == 3 );
		TS2.Add(tp4 + std::chrono::minutes(3), 2343);
		CHECK ( TS2.size() == 3 );

// gcc 6 has difficulties casting timepoints of different durations
#if !DEKAF2_IS_GCC || DEKAF2_GCC_VERSION_MAJOR > 6

		KTimeSeries<uint64_t, std::chrono::minutes> TS3;

		auto tp = std::chrono::system_clock::now();

		TS3.Add(tp,  2874732);
		TS3.Add(tp, 42623456);
		TS3.Add(tp,   332235);

		CHECK ( TS3.size() == 1 );

		CHECK ( TS3.Get(tp).Count() == 3 );
		CHECK ( TS3.Get(tp + std::chrono::milliseconds(100)).Min()   ==   332235 );
		CHECK ( TS3.Get(tp + std::chrono::microseconds(124)).Mean()   == 15276807 );
		CHECK ( TS3.Get(tp + std::chrono::nanoseconds(177) ).Max()   == 42623456 );
#endif
	}

#if !DEKAF2_IS_GCC || DEKAF2_GCC_VERSION_MAJOR > 6

	SECTION("odd interval")
	{
		using KTimeSeries5M = KTimeSeries<uint32_t, std::chrono::duration<long, std::ratio<5 * 60>>>;

		KTimeSeries5M TM5;

		TM5 += 234;
		auto tp1 = KTimeSeries5M::ToSystemClock(TM5.begin()->first);
		TM5.Add(tp1, 92342);
		TM5.Add(tp1,  9034);

		CHECK ( TM5.size() == 1 );
		CHECK ( TM5.Get(tp1).Count() ==     3 );
		CHECK ( TM5.Get(tp1).Min()   ==   234 );
		CHECK ( TM5.Get(tp1).Mean()  == 33870 );
		CHECK ( TM5.Get(tp1).Max()   == 92342 );
		CHECK ( TM5.Sum().Mean()     == 33870 );
		CHECK ( TM5.Interval().count() == 5UL * 60 * 1000 * 1000 * 1000 );
	}
#endif
}
