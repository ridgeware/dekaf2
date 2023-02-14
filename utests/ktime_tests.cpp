#include "catch.hpp"

#include <dekaf2/ktime.h>
#include <dekaf2/kduration.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kscopeguard.h>
#include <array>

using namespace dekaf2;

TEST_CASE("KTime") {

	SECTION("kParseHTTPTimestamp")
	{
		static constexpr KStringView sOrigTime     = "Tue, 03 Aug 2021 10:23:42 GMT";
		static constexpr KStringView sBadOrigTime1 = "Tue, 03 Aug 2021 10:23:42 CET";
		static constexpr KStringView sBadOrigTime2 = "Tue, 03 NaN 2021 10:23:42 GMT";
		static constexpr KStringView sBadOrigTime3 = "10:23:42";

		auto tTime = kParseHTTPTimestamp(sOrigTime);
		CHECK ( tTime != 0 );
		auto sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == sOrigTime );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime1) == 0 );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime2) == 0 );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime3) == 0 );
	}

	SECTION("kParseSMTPTimestamp")
	{
		static constexpr KStringView sOrigTime1 = "Tue, 03 Aug 2021 10:23:42 -0000";
		static constexpr KStringView sOrigTime2 = "Tue, 03 Aug 2021 10:23:42 -0330";
		static constexpr KStringView sOrigTime3 = "Tue, 03 Aug 2021 10:23:42 +1101";
		static constexpr KStringView sBadOrigTime1 = "Tue, 03 Aug 2021 10:23:42 CET";
		static constexpr KStringView sBadOrigTime2 = "Tue, 03 Aug 2021 10:23:42 -00";

		auto tTime = kParseSMTPTimestamp(sOrigTime1);
		CHECK (tTime != 0);
		auto sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == sOrigTime1);

		tTime = kParseSMTPTimestamp(sOrigTime2);
		CHECK (tTime != 0);
		sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == "Tue, 03 Aug 2021 06:53:42 -0000");

		tTime = kParseSMTPTimestamp(sOrigTime3);
		CHECK (tTime != 0);
		sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == "Tue, 03 Aug 2021 21:24:42 -0000");

		CHECK ( kParseSMTPTimestamp(sBadOrigTime1) == 0 );
		CHECK ( kParseSMTPTimestamp(sBadOrigTime2) == 0 );
	}

	SECTION("kTranslateSeconds")
	{
		CHECK ( KDuration::max().seconds() == 9223372036 );
		CHECK ( kTranslateSeconds(KDuration::max().seconds()
								               , true ) == "292 yrs, 24 wks, 3 days, 23 hrs, 47 mins, 16 secs" );
		CHECK ( kTranslateSeconds(0            , false) == "less than a second" );
		CHECK ( kTranslateSeconds(1            , false) == "1 sec" );
		CHECK ( kTranslateSeconds(2            , false) == "2 secs" );
		CHECK ( kTranslateSeconds(1326257      , false) == "2.2 wks" );
		CHECK ( kTranslateSeconds(6348747235   , false) == "201.3 yrs" );
		CHECK ( kTranslateSeconds(2376         , false) == "39 mins, 36 secs" );
		CHECK ( kTranslateSeconds(23872        , false) == "6 hrs, 37 mins" );
		CHECK ( kTranslateSeconds(23           , false) == "23 secs" );
		CHECK ( kTranslateSeconds(123          , false) == "2 mins, 3 secs" );
		CHECK ( kTranslateSeconds(-1           , false) == "-1 sec" );
		CHECK ( kTranslateSeconds(-80          , false) == "-1.3 mins" );
		CHECK ( kTranslateSeconds(-3*60*60-15  , false) == "-3.0 hours" );
		CHECK ( kTranslateSeconds(9223372036+1 , false) == "a very long time" );
		CHECK ( kTranslateSeconds(KDuration::min().seconds()-1, false) == "a very short time" );
		CHECK ( kTranslateSeconds(120          , true ) == "2 mins" );
		CHECK ( kTranslateSeconds(0            , true ) == "less than a second" );
		CHECK ( kTranslateSeconds(1            , true ) == "1 sec" );
		CHECK ( kTranslateSeconds(2            , true ) == "2 secs" );
		CHECK ( kTranslateSeconds(1326257      , true ) == "2 wks, 1 day, 8 hrs, 24 mins, 17 secs" );
		CHECK ( kTranslateSeconds(-1326257     , true ) == "-2.2 wks" );
		CHECK ( kTranslateSeconds(6348747235   , true ) == "201 yrs, 16 wks, 3 days, 20 hrs, 53 mins, 55 secs" );
		CHECK ( kTranslateSeconds(23872        , true ) == "6 hrs, 37 mins, 52 secs" );
	}

	SECTION("kTranslateDuration")
	{
		using namespace std::chrono;

		CHECK ( kTranslateDuration(milliseconds(-1)    , false) == "-1 millisec" );
		CHECK ( kTranslateDuration(milliseconds(1001)  , false) == "1.001 secs" );
		CHECK ( kTranslateDuration(milliseconds(2002)  , false) == "2.002 secs" );
		CHECK ( kTranslateDuration(milliseconds(1000)  , false) == "1 sec" );
		CHECK ( kTranslateDuration(milliseconds(980)   , false) == "980 millisecs" );
		CHECK ( kTranslateDuration(microseconds(12345) , false) == "12.345 millisecs" );
		CHECK ( kTranslateDuration(microseconds(-12345), false) == "-12.345 millisecs" );
		CHECK ( kTranslateDuration(microseconds(345)   , false) == "345 microsecs" );
		CHECK ( kTranslateDuration(microseconds(1)     , false) == "1 microsec" );
		CHECK ( kTranslateDuration(nanoseconds(34567)  , false) == "34.567 microsecs" );
		CHECK ( kTranslateDuration(nanoseconds(54)     , false) == "54 nanosecs" );
		CHECK ( kTranslateDuration(nanoseconds(1)      , false) == "1 nanosec" );
		CHECK ( kTranslateDuration(nanoseconds::max()  , false) == "292.5 yrs" );
		CHECK ( kTranslateDuration(nanoseconds::max()  , true) == "292 yrs, 24 wks, 3 days, 23 hrs, 47 mins, 16 secs, 854 msecs, 775 usecs, 807 nsecs" );
		CHECK ( kTranslateDuration(nanoseconds::min()  , true) == "a very short time" );
		CHECK ( kTranslateDuration(nanoseconds::min()
								   +nanoseconds(1)     , true) == "-292.5 yrs" );
	}

	SECTION("KBrokenDownTime")
	{
		KUTCTime UTC1;
		CHECK ( UTC1.empty()                 );
		CHECK ( UTC1.ToTimeT()      == 0     );
		UTC1.AddSeconds(1);
//		CHECK ( UTC1.empty()                 );
//		CHECK ( UTC1.Format()       == ""    );
		UTC1.AddSeconds(86399);
#ifdef DEKAF2_IS_MACOS
		// this is admittedly an edge case ..
		CHECK ( UTC1.empty()        == false );
		CHECK ( UTC1.Format()       == "1900-01-01 00:00:00" );
#endif
		KLocalTime Local1;
		CHECK ( Local1.empty()               );
		CHECK ( Local1.ToTimeT()    == 0     );
		KUTCTime UTC2(123545656);
		CHECK ( UTC2.empty()        == false );
		CHECK ( UTC2.ToTimeT()      == 123545656 );

		CHECK ( UTC2.GetDay()       == 30    );
		CHECK ( UTC2.GetMonth()     == 11    );
		CHECK ( UTC2.GetYear()      == 1973  );
		CHECK ( UTC2.GetHour()      == 22    );
		CHECK ( UTC2.GetMinute()    == 14    );
		CHECK ( UTC2.GetSecond()    == 16    );
		CHECK ( UTC2.GetHour12()    == 10    );
		CHECK ( UTC2.GetDayName(true)   == "Fri" );
		CHECK ( UTC2.GetMonthName(true) == "Nov" );
		CHECK ( UTC2.IsPM()         == true  );
		CHECK ( UTC2.Format()       == "1973-11-30 22:14:16" );
		CHECK ( UTC2.ToTimeT()      == 123545656 );
		UTC1 = UTC2;
		CHECK ( UTC1.ToTimeT()      == 123545656 );
		UTC1.SetDay(31);
		UTC1.SetMonth(12);
		UTC1.SetHour(23);
		UTC1.SetMinute(59);
		UTC1.SetSecond(59);
		CHECK ( UTC1.Format()       == "1973-12-31 23:59:59" );
		CHECK ( UTC1.ToTimeT()      == 126230399 );
		UTC2 = UTC1.ToTimeT();
		CHECK ( (UTC2 == UTC1) );
		CHECK ( UTC2.GetMonthName(true) == "Dec" );
		CHECK ( UTC2.GetDayName(true)   == "Mon" );
		CHECK ( UTC2.IsPM()         == true  );
		UTC1.AddSeconds(2);
		CHECK ( UTC1.Format()       == "1974-01-01 00:00:01" );
		CHECK ( UTC1.ToTimeT()      == 126230401 );
		CHECK ( UTC1.GetMonthName(true) == "Jan" );
		CHECK ( UTC1.GetDayName(true)   == "Tue" );
		CHECK ( UTC1.IsPM()         == false  );
		UTC1.AddSeconds(-2);
		CHECK ( UTC1.Format()       == "1973-12-31 23:59:59" );
		CHECK ( UTC1.ToTimeT()      == 126230399 );
		CHECK ( UTC1.GetMonthName(true) == "Dec" );
		CHECK ( UTC1.GetDayName(true)   == "Mon" );
		CHECK ( UTC1.IsPM()         == true  );

		auto SysTime = UTC1.ToTimePoint();
		CHECK ( std::chrono::system_clock::to_time_t(SysTime) == UTC1.ToTimeT() );
		std::chrono::system_clock::time_point SysTime2 = UTC1;
		CHECK ( SysTime == SysTime2 );

		UTC2 += std::chrono::minutes(34);
		CHECK ( UTC2.Format()       == "1974-01-01 00:33:59" );
		UTC2 += std::chrono::seconds(1 * 60 * 60 * 24 * 365UL);
		CHECK ( UTC2.Format()       == "1975-01-01 00:33:59" );
#ifdef DEKAF2_HAS_CHRONO_WEEKDAY
		UTC2 += std::chrono::days(365);
#else
		UTC2 += std::chrono::seconds(1 * 60 * 60 * 24 * 365UL);
#endif
		CHECK ( UTC2.Format()       == "1976-01-01 00:33:59" );
#ifdef DEKAF2_HAS_CHRONO_WEEKDAY
		UTC2 += std::chrono::days(70 * 365);
#else
		UTC2 += std::chrono::seconds(70 * 60 * 60 * 24 * 365UL);
#endif
		CHECK ( UTC2.Format()       == "2045-12-14 00:33:59" );
#ifdef DEKAF2_HAS_CHRONO_WEEKDAY
		UTC2 -= std::chrono::days(70 * 365);
#else
		UTC2 -= std::chrono::seconds(70 * 60 * 60 * 24 * 365UL);
#endif
		CHECK ( UTC2.Format()       == "1976-01-01 00:33:59" );
		UTC2 += time_t(1 * 60 * 60 * 24 * 365UL);
		CHECK ( UTC2.Format()       == "1976-12-31 00:33:59" );
		UTC2 += KDuration(std::chrono::seconds(2));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:01" );
		UTC2 += KDuration(std::chrono::microseconds(2));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:01" );
		UTC2 += KDuration(std::chrono::microseconds(1000123));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:02" );
		UTC2 -= KDuration(std::chrono::microseconds(2000123));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:00" );

		auto UTC3 = UTC2 + std::chrono::seconds(61);
		auto tDiff = UTC3 - UTC2;
		CHECK ( tDiff == 61 );
		tDiff = UTC3.ToTimeT() - UTC2;
		CHECK ( tDiff == 61 );
		tDiff = UTC3 - UTC2.ToTimeT();
		CHECK ( tDiff == 61 );
		auto dDiff = UTC3 - UTC2.ToTimePoint();
		CHECK ( KDuration(dDiff).seconds() == 61 );
		dDiff = UTC3.ToTimePoint() - UTC2;
		CHECK ( KDuration(dDiff).seconds() == 61 );

		auto oldLocale = kGetGlobalLocale();
		if (kSetGlobalLocale("fr_FR.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			KLocalTime Local1;
			Local1 = UTC1;

			SysTime = Local1.ToTimePoint();
			CHECK ( std::chrono::system_clock::to_time_t(SysTime) == Local1.ToTimeT() );
			SysTime2 = Local1;
			CHECK ( SysTime == SysTime2 );

			if (Local1.GetUTCOffset() == 60 * 60)
			{
				// these checks only work correctly with timezone set to CET
				CHECK ( Local1.Format()       == "1974-01-01 00:59:59" );
				CHECK ( Local1.ToTimeT()      == 126230399 );
				CHECK ( Local1.GetDay()       == 1     );
				CHECK ( Local1.GetMonth()     == 1     );
				CHECK ( Local1.GetYear()      == 1974  );
				CHECK ( Local1.GetHour()      == 0     );
				CHECK ( Local1.GetMinute()    == 59    );
				CHECK ( Local1.GetSecond()    == 59    );
				CHECK ( Local1.GetHour12()    == 0     );
				CHECK ( Local1.IsPM()         == false );
#ifdef DEKAF2_IS_OSX
				CHECK ( Local1.GetMonthName( true, true) == "jan" );
				CHECK ( Local1.GetMonthName(false, true) == "janvier" );
				CHECK ( Local1.GetDayName  ( true, true) == "Mar" );
				CHECK ( Local1.GetDayName  (false, true) == "Mardi" );
#endif
				CHECK ( Local1.GetUTCOffset() == 3600  );
#ifdef DEKAF2_IS_OSX
				CHECK ( kFormTimestamp(UTC1.ToTimeT(), "%A %c", true) == "Mardi Mar  1 jan 00:59:59 1974" );
#endif
			}
		}

	}

	SECTION("kParseTimestamp")
	{
		auto tTime = kParseTimestamp("???, DD NNN YYYY hh:mm:ss ???", "Tue, 03 Aug 2021 10:23:42 GMT");
		auto sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 10:23:42 GMT" );

		tTime = kParseTimestamp("???, DD NNN YYYY hh:mm:ss zzz", "Tue, 03 Aug 2021 10:23:42 GMT");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 10:23:42 GMT" );

		tTime = kParseTimestamp("???, DD NNN YYYY hh:mm:ss ZZZZZ", "Tue, 03 Aug 2021 10:23:42 -0430");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 14:53:42 GMT" );

		tTime = kParseTimestamp("???, DD NNN YYYY hh:mm:ss ZZZZZ", "Tue,  3 Aug 2021 10:23:42 -0430");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 14:53:42 GMT" );

		tTime = kParseTimestamp("???, DD NNN YY hh:mm:ss ZZZZZ", "Tue, 03 Aug 21 10:23:42 -0430");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 14:53:42 GMT" );

		tTime = kParseTimestamp("???, DD NNN YYYY hh:mm:ss zzz", "Tue, 03 Aug 2021 10:23:42 PDT");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 17:23:42 GMT" );

		tTime = kParseTimestamp("Tue, 03 Aug 2021 10:23:42 GMT");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 10:23:42 GMT" );

		tTime = kParseTimestamp("Tue, 03 Aug 2021 10:23:42 -0430");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 14:53:42 GMT" );

		tTime = kParseTimestamp("Tue,  3 Aug 2021 10:23:42 -0430");
		sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == "Tue, 03 Aug 2021 14:53:42 GMT" );
	}

	SECTION("kParseTimestamp 2")
	{
		static constexpr std::array<std::pair<KStringView, KStringView>, 55> Timestamps
		{{
			{ "Tue, 03 Aug 2021 11:23:42 +0100", "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Tue, 03 Aug 2021 12:23:42 CEST" , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Tue, 03 Aug 2021 11:23:42 CET"  , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 03:23:42.211 -0700"  , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 03:23:42,211 -0700"  , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 03 12:23:42.211 CEST"  , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 03 11:23:42.211 CET"   , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 02:53:42.211-0730"   , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 02:53:42,211-0730"   , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/Aug/2021:02:53:42 -0730"     , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/Aug/2021 02:53:42 -0730"     , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Aug 03 02:53:42 -0730 2021"     , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 02:53:42 -0730"      , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Aug 03, 2021 10:23:42 AM"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Aug 03, 2021 10:23:42 PM"       , "Tue, 03 Aug 2021 22:23:42 GMT" },
			{ "2021-08-03 02:53:42-0730"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03T02:53:42-0730"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03T10:23:42.321Z"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 03 10:23:42.321"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03-Aug-2021 10:23:42.321"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 03 11:23:42 CET"       , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 11:23:42 CET"        , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 10:23:42.335"        , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03T10:23:42.335"        , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 10:23:42,335"        , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03*10:23:42:335"        , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "20210803 10:23:42.345"          , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Aug 03 2021 10:23:42"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 Aug 03 2021"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "Aug 03 10:23:42 2021"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03T10:23:42Z"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03 Aug 2021 10:23:42"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03-Aug-2021 10:23:42"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/Aug/2021 10:23:42"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/Aug/2021:10:23:42"           , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03 10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03T10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-03*10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021/08/03 10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021/08/03*10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03-08-2021"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03-08-2021 10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03.08.2021"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03.08.2021 10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03/08/2021"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/08/2021 10:23:42"            , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03-08-21"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03-08-21 10:23:42"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03.08.21"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03.08.21 10:23:42"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 03/08/21"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "03/08/21 10:23:42"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "20210803T102342Z"               , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "210803 10:23:42"                , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "20210803102342"                 , "Tue, 03 Aug 2021 10:23:42 GMT" }
		}};

		for (auto& Timestamp : Timestamps)
		{
			auto tTime = kParseTimestamp(Timestamp.first);
			auto sTime = kFormHTTPTimestamp(tTime);
			INFO  ( Timestamp.first );
			CHECK ( sTime == Timestamp.second );
		}
	}

#ifdef DEKAF2_IS_OSX
	// windows and linux have differing month and day names in e.g. French than MacOS,
	// and linux distributions use different ones over the years, too
	SECTION("kParseTimestamp localized")
	{
		static constexpr std::array<std::pair<KStringView, KStringView>, 19> Timestamps
		{{
			{ "Mer, 03 fév 2021 11:23:42 +0100", "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "Mer, 03 fév 2021 12:23:42 CEST" , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "Mer, 03 fév 2021 11:23:42 CET"  , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "2021 fév 03 12:23:42.211 CEST"  , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "2021 fév 03 11:23:42.211 CET"   , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03/fév/2021:02:53:42 -0730"     , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03/fév/2021 02:53:42 -0730"     , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "fév 03 02:53:42 -0730 2021"     , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "fév 03, 2021 10:23:42 AM"       , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "fév 03, 2021 10:23:42 PM"       , "Wed, 03 Feb 2021 22:23:42 GMT" },
			{ "2021 fév 03 10:23:42.321"       , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03-fév-2021 10:23:42.321"       , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "2021 fév 03 11:23:42 CET"       , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "fév 03 2021 10:23:42"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "10:23:42 fév 03 2021"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03 fév 2021 10:23:42"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03-fév-2021 10:23:42"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03/fév/2021 10:23:42"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
			{ "03/fév/2021:10:23:42"           , "Wed, 03 Feb 2021 10:23:42 GMT" },
		}};

		auto oldLocale = kGetGlobalLocale();

		if (kSetGlobalLocale("fr_FR.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			for (auto& Timestamp : Timestamps)
			{
				auto tTime = kParseTimestamp(Timestamp.first);
				auto sTime = kFormHTTPTimestamp(tTime);
				INFO  ( Timestamp.first );
				CHECK ( sTime == Timestamp.second );
			}
		}
	}
#endif

	SECTION("kGetTimezoneOffset")
	{
		CHECK ( kGetTimezoneOffset("XYZ" ) == -1  );
		CHECK ( kGetTimezoneOffset("GMT" ) ==  0  );
		CHECK ( kGetTimezoneOffset("CET" ) ==  1 * 60 * 60 );
		CHECK ( kGetTimezoneOffset("NPT" ) == (5 * 60 + 45) * 60 );
		CHECK ( kGetTimezoneOffset("NZST") == 12 * 60 * 60 );
		CHECK ( kGetTimezoneOffset("COST") == -4 * 60 * 60 );
		CHECK ( kGetTimezoneOffset("HST" ) == -10 * 60 * 60 );
	}

	SECTION("comparison")
	{
		KUTCTime a = 123456;
		KUTCTime b = 12345;
		CHECK ( (a >  b) );
		CHECK ( (a >= b) );
		CHECK ( (a != b) );
		CHECK ( (b <  a) );
		CHECK ( (b <= a) );
	}

#ifdef DEKAF2_IS_OSX
	// windows has differing month and day names in e.g. French than MacOS,
	// and linux distributions use different ones over the years, too
	SECTION("kGetLocalizedMonthName")
	{
		auto oldLocale = kGetGlobalLocale();

		if (kSetGlobalLocale("fr_FR.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			CHECK ( kGetMonthName( 1,  true, true) == "fév"      );
			CHECK ( kGetMonthName( 7,  true, true) == "aoû"      );
			CHECK ( kGetMonthName( 0, false, true) == "janvier"  );
			CHECK ( kGetMonthName(11, false, true) == "décembre" );

			CHECK ( kGetDayName( 1,  true, true) == "Lun"       );
			CHECK ( kGetDayName( 3,  true, true) == "Mer"       );
			CHECK ( kGetDayName( 1, false, true) == "Lundi"    );
			CHECK ( kGetDayName( 3, false, true) == "Mercredi" );
		}
/*

 With the current implementation, month and day names will stick
 to the first locale they were used with - see comment in ktime.h

		if (kSetGlobalLocale("de_DE.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			CHECK ( kGetMonthName( 0,  true, true) == "Jan"      );
			CHECK ( kGetMonthName( 2,  true, true) == "Mär"      );
			CHECK ( kGetMonthName( 0, false, true) == "Januar"   );
			CHECK ( kGetMonthName(11, false, true) == "Dezember" );

			CHECK ( kGetDayName( 1,  true, true) == "Mo"       );
			CHECK ( kGetDayName( 3,  true, true) == "Mi"       );
			CHECK ( kGetDayName( 1, false, true) == "Montag"   );
			CHECK ( kGetDayName( 3, false, true) == "Mittwoch" );
		}

		if (kSetGlobalLocale("en_US.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			CHECK ( kGetMonthName( 0,  true, true) == "Jan"      );
			CHECK ( kGetMonthName( 2,  true, true) == "Mar"      );
			CHECK ( kGetMonthName( 0, false, true) == "January"   );
			CHECK ( kGetMonthName(11, false, true) == "December" );

			CHECK ( kGetDayName( 1,  true, true) == "Mon"       );
			CHECK ( kGetDayName( 3,  true, true) == "Wed"       );
			CHECK ( kGetDayName( 1, false, true) == "Monday"   );
			CHECK ( kGetDayName( 3, false, true) == "Wednesday" );
		}
 */
	}
#endif
}
