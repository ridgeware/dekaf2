#include "catch.hpp"

#include <dekaf2/core/logging/kdatedlogfile.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/time/clock/ktime.h>

using namespace dekaf2;

namespace {

// returns the time 2026-10-iDay iHour:iMinute:iSecond.iMilliseconds UTC
KUnixTime At(unsigned iDay, int iHour, int iMinute, int iSecond, int iMilliseconds = 0)
{
	auto Day = chrono::sys_days(chrono::year(2026) / chrono::month(10) / chrono::day(iDay));

	return KUnixTime(chrono::system_clock::time_point(Day
	                                                  + chrono::hours(iHour)
	                                                  + chrono::minutes(iMinute)
	                                                  + chrono::seconds(iSecond)
	                                                  + chrono::milliseconds(iMilliseconds)));
}

} // end of anonymous namespace

TEST_CASE("KDatedLogFile")
{
	KTempDir TempDir;
	auto sLog = kFormat("{}/dated.log", TempDir.Name());

	SECTION("cut at midnight UTC")
	{
		KDatedLogFile Log(sLog);
		CHECK ( Log.WriteLine("first"                , At(29, 12,  0,  0     )) );
		CHECK ( Log.WriteLine("last of the day"      , At(29, 23, 59, 59, 999)) );
		CHECK ( Log.WriteLine("first of the next day", At(30,  0,  0,  0     )) );
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261029") == "first\nlast of the day\n" );
		CHECK ( kReadAll(sLog)               == "first of the next day\n" );
	}

	SECTION("Write() takes the record as it is")
	{
		KDatedLogFile Log(sLog);
		CHECK ( Log.Write("one record\n", At(29, 12, 0, 0)) );
		CHECK ( Log.Write("next day\n"  , At(30, 12, 0, 0)) );
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261029") == "one record\n" );
		CHECK ( kReadAll(sLog)               == "next day\n" );
	}

	SECTION("late records within and after the grace period")
	{
		KDatedLogFile Log(sLog);
		Log.WriteLine("day 29"  , At(29, 23, 59, 50     ));
		Log.WriteLine("day 30 a", At(30,  0,  0,  1     ));
		// the newest record is 1 second after midnight, within the default grace of 60 seconds
		Log.WriteLine("late 1"  , At(29, 23, 59, 59, 998));
		// the newest record is 60 seconds after midnight, which ends the grace period
		Log.WriteLine("day 30 b", At(30,  0,  1,  0     ));
		Log.WriteLine("late 2"  , At(29, 23, 59, 59, 999));
		// a record of the day before the previous day goes into the current file as well
		Log.WriteLine("day 28"  , At(28, 12,  0,  0     ));
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261029") == "day 29\nlate 1\n" );
		CHECK ( kReadAll(sLog)               == "day 30 a\nday 30 b\nlate 2\nday 28\n" );
	}

	SECTION("size cuts within a day")
	{
		KDatedLogFile::Options Options;
		Options.iMaxSize = 20;
		KDatedLogFile Log(sLog, Options);
		// every line has 11 bytes with its line feed, two lines exceed the maximum size
		Log.WriteLine("0123456789", At(29, 10, 0, 0));
		Log.WriteLine("abcdefghij", At(29, 11, 0, 0));
		Log.WriteLine("ABCDEFGHIJ", At(29, 12, 0, 0));
		Log.WriteLine("next day"  , At(30,  1, 0, 0));
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261029"  ) == "0123456789\n" );
		CHECK ( kReadAll(sLog + "-20261029.1") == "abcdefghij\n" );
		CHECK ( kReadAll(sLog + "-20261029.2") == "ABCDEFGHIJ\n" );
		CHECK ( kReadAll(sLog)                 == "next day\n" );
	}

	SECTION("a name that exists as a compressed file is not used again")
	{
		REQUIRE ( kWriteFile(sLog + "-20261029.gz", "compressed") );
		KDatedLogFile Log(sLog);
		Log.WriteLine("day 29", At(29, 10, 0, 0));
		Log.WriteLine("day 30", At(30, 10, 0, 0));
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261029.gz") == "compressed" );
		CHECK ( kReadAll(sLog + "-20261029.1")  == "day 29\n" );
	}

	SECTION("an existing file of an earlier day is cut by the first record")
	{
		REQUIRE ( kWriteFile(sLog, "before the restart\n") );
		REQUIRE ( kSetLastMod(sLog, At(28, 18, 0, 0)) );
		KDatedLogFile Log(sLog);
		Log.WriteLine("after the restart", At(29, 9, 0, 0));
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog + "-20261028") == "before the restart\n" );
		CHECK ( kReadAll(sLog)               == "after the restart\n" );
	}

	SECTION("an existing file of the same day is continued")
	{
		REQUIRE ( kWriteFile(sLog, "earlier today\n") );
		REQUIRE ( kSetLastMod(sLog, At(29, 8, 0, 0)) );
		KDatedLogFile Log(sLog);
		Log.WriteLine("later today", At(29, 9, 0, 0));
		CHECK ( Log.Flush() );
		CHECK ( kReadAll(sLog) == "earlier today\nlater today\n" );
		CHECK ( kFileExists(sLog + "-20261029") == false );
	}

#if DEKAF2_HAS_TIMEZONES
	SECTION("the days of a time zone, with the end of summer time")
	{
		auto Zone = kFindTimezone("Europe/Berlin", false);

		if (Zone != nullptr)
		{
			KDatedLogFile::Options Options;
			Options.TimeZone = Zone;
			KDatedLogFile Log(sLog, Options);
			// in Berlin, 2026-10-25 has 25 hours: it starts at 2026-10-24 22:00 UTC (summer
			// time) and ends at 2026-10-25 23:00 UTC (winter time)
			Log.WriteLine("24th"      , At(24, 21, 59, 59));
			Log.WriteLine("25th first", At(24, 22,  0,  0));
			Log.WriteLine("25th last" , At(25, 22, 59, 59));
			Log.WriteLine("26th"      , At(25, 23,  0,  0));
			CHECK ( Log.Flush() );
			CHECK ( kReadAll(sLog + "-20261024") == "24th\n" );
			CHECK ( kReadAll(sLog + "-20261025") == "25th first\n25th last\n" );
			CHECK ( kReadAll(sLog)               == "26th\n" );
		}
	}
#endif
}
