#include "catch.hpp"

#include <dekaf2/ktime.h>
#include <dekaf2/kduration.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kscopeguard.h>
#include <dekaf2/kstringutils.h>
#include <array>

using namespace dekaf2;

TEST_CASE("KTime") {

	SECTION("kParseHTTPTimestamp")
	{
		static constexpr KStringView sOrigTime     = "Tue, 03 Aug 2021 10:23:42 GMT";
		static constexpr KStringView sBadOrigTime1 = "Tue, 03 Aug 2021 10:23:42 CET";
		static constexpr KStringView sBadOrigTime2 = "Tue, 03 NaN 2021 10:23:42 GMT";
		static constexpr KStringView sBadOrigTime3 = "10:23:42";

		KUnixTime tTime = kParseHTTPTimestamp(sOrigTime);
		CHECK ( tTime != KUnixTime(0) );
		auto sTime = kFormHTTPTimestamp(tTime);
		CHECK ( sTime == sOrigTime );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime1).ok() == false );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime2).ok() == false );
		CHECK ( kParseHTTPTimestamp(sBadOrigTime3).ok() == false );
	}

	SECTION("kParseSMTPTimestamp")
	{
		static constexpr KStringView sOrigTime1 = "Tue, 03 Aug 2021 10:23:42 -0000";
		static constexpr KStringView sOrigTime2 = "Tue, 03 Aug 2021 10:23:42 -0330";
		static constexpr KStringView sOrigTime3 = "Tue, 03 Aug 2021 10:23:42 +1101";
		static constexpr KStringView sOrigTime4 = "Tue, 03 Aug 2021 10:23:42 CET";
		static constexpr KStringView sBadOrigTime1 = "Tue, 03 Aug 2021 10:23:42 XXX";
		static constexpr KStringView sBadOrigTime2 = "Tue, 03 Aug 2021 10:23:42 -00";

		auto tTime = kParseSMTPTimestamp(sOrigTime1);
		CHECK (tTime.ok());
		auto sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == sOrigTime1);

		tTime = kParseSMTPTimestamp(sOrigTime2);
		CHECK (tTime.ok());
		sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == "Tue, 03 Aug 2021 06:53:42 -0000");

		tTime = kParseSMTPTimestamp(sOrigTime3);
		CHECK (tTime.ok());
		sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == "Tue, 03 Aug 2021 21:24:42 -0000");

		tTime = kParseSMTPTimestamp(sOrigTime4);
		CHECK (tTime.ok());
		sTime = kFormSMTPTimestamp(tTime);
		CHECK (sTime == "Tue, 03 Aug 2021 09:23:42 -0000");

		CHECK ( kParseSMTPTimestamp(sBadOrigTime1).ok() == false );
		CHECK ( kParseSMTPTimestamp(sBadOrigTime2).ok() == false );
	}

	SECTION("kTranslateSeconds")
	{
		CHECK ( KDuration::max().seconds() == chrono::seconds(9223372036) );
		CHECK ( kTranslateSeconds(KDuration::max().seconds().count()
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
		CHECK ( kTranslateSeconds(KDuration::min().seconds().count()-1, false) == "a very long negative time" );
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
		CHECK ( kTranslateDuration(nanoseconds::min()  , true) == "a very long negative time" );
		CHECK ( kTranslateDuration(nanoseconds::min()
								   +nanoseconds(1)     , true) == "-292.5 yrs" );
	}

	SECTION("KBrokenDownTime")
	{
		CHECK ( sizeof(std::time_t) <=  8 );
		CHECK ( sizeof(KUnixTime)   <=  8 );

		if (sizeof(std::time_t) == sizeof(long long))
		{
			CHECK ( sizeof(KUnixTime)   == sizeof(std::time_t) );
		}

		CHECK ( sizeof(KUTCTime)    <= 56 );
		CHECK ( sizeof(KLocalTime)  <= 72 );
		CHECK ( sizeof(std::tm)     <= 56 );

		KUTCTime UTC1;
		CHECK ( UTC1.empty()                 );
		CHECK ( UTC1.to_unix()      == KUnixTime(0));
		UTC1 += chrono::seconds(1);
		UTC1 += chrono::seconds(86399);
		CHECK ( UTC1.empty()        == false );
		CHECK ( UTC1.Format()       == "1970-01-02 00:00:00" );
		KLocalTime Local1;
		CHECK ( Local1.empty()               );
		KUTCTime UTC2(KUnixTime::from_time_t(123545656));
		CHECK ( UTC2.empty()        == false );
		CHECK ( UTC2.to_unix()      == KUnixTime(123545656) );

		CHECK ( UTC2.days().count()     == 30    );
		CHECK ( UTC2.months().count()   == 11    );
		CHECK ( UTC2.years().count()    == 1973  );
		CHECK ( UTC2.hours().count()    == 22    );
		CHECK ( UTC2.minutes().count()  == 14    );
		CHECK ( UTC2.seconds().count()  == 16    );
		CHECK ( UTC2.hours12().count()  == 10    );
		CHECK ( UTC2.weekday()          == chrono::Friday );
		CHECK ( UTC2.month()            == chrono::November );
		CHECK ( UTC2.is_pm()            == true  );
		CHECK ( UTC2.to_string()        == "1973-11-30 22:14:16" );
		CHECK ( UTC2.to_unix()          == KUnixTime(123545656) );
		UTC1 = UTC2;
		CHECK ( UTC1.to_unix()          == KUnixTime(123545656) );
		UTC1 = KUnixTime(126230399);
		CHECK ( UTC1.Format()       == "1973-12-31 23:59:59" );
		CHECK ( UTC1.to_unix()      == KUnixTime(126230399) );
		UTC2 = UTC1.to_unix();
		CHECK ( (UTC2 == UTC1) );
		CHECK ( UTC2.weekday()          == chrono::Monday );
		CHECK ( UTC2.month()            == chrono::December );
		CHECK ( UTC2.is_pm()        == true  );
		UTC1 += chrono::seconds(2);
		CHECK ( UTC1.Format()       == "1974-01-01 00:00:01" );
		CHECK ( UTC1.to_unix()      == KUnixTime(126230401) );
		CHECK ( UTC1.month()        == chrono::January );
		CHECK ( UTC1.weekday()      == chrono::Tuesday );
		CHECK ( UTC1.is_pm()        == false  );
		UTC1 -= chrono::seconds(2);
		CHECK ( UTC1.Format()       == "1973-12-31 23:59:59" );
		CHECK ( UTC1.to_unix()      == KUnixTime(126230399) );
		CHECK ( UTC1.month()        == chrono::December );
		CHECK ( UTC1.weekday()      == chrono::Monday   );
		CHECK ( UTC1.is_pm()        == true  );

		auto SysTime = UTC1.to_sys();
		CHECK ( std::chrono::system_clock::to_time_t(SysTime) == UTC1.to_unix().to_time_t() );
		std::chrono::system_clock::time_point SysTime2 = UTC1.to_sys();
		CHECK ( SysTime == SysTime2 );

		UTC2 += std::chrono::minutes(34);
		CHECK ( UTC2.Format()       == "1974-01-01 00:33:59" );
		UTC2 += std::chrono::seconds(1 * 60 * 60 * 24 * 365UL);
		CHECK ( UTC2.Format()       == "1975-01-01 00:33:59" );
		UTC2 += chrono::days(365);
		CHECK ( UTC2.Format()       == "1976-01-01 00:33:59" );
		UTC2 += chrono::days(70 * 365);
		CHECK ( UTC2.Format()       == "2045-12-14 00:33:59" );
		UTC2 -= chrono::days(70 * 365);
		CHECK ( UTC2.Format()       == "1976-01-01 00:33:59" );
		UTC2 += time_t(1 * 60 * 60 * 24 * 365UL);
		CHECK ( UTC2.Format()       == "1976-12-31 00:33:59" );
		UTC2 += KDuration(std::chrono::seconds(2));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:01" );
		UTC2 += KDuration(std::chrono::microseconds(2));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:01" );
		UTC2 += KDuration(std::chrono::microseconds(1000123));
		CHECK ( UTC2.Format()       == "1976-12-31 00:34:02" );
		UTC2 -= KDuration(std::chrono::microseconds(2000126));
		CHECK ( UTC2.Format()       == "1976-12-31 00:33:59" ); // subseconds ..

		auto UTC3 = UTC2;
		UTC3 += std::chrono::seconds(61);
		auto tDiff1 = UTC3 - UTC2;
		CHECK ( tDiff1 == std::chrono::seconds(61) );
		auto tDiff2 = UTC3.to_unix() - UTC2;
		CHECK ( tDiff2 == std::chrono::seconds(61) );
		auto tDiff3 = UTC3 - UTC2.to_unix();
		CHECK ( tDiff3 == std::chrono::seconds(61) );
		auto dDiff4 = UTC3 - UTC2.to_sys();
		CHECK ( KDuration(dDiff4).seconds() == chrono::seconds(61) );
		auto dDiff5 = UTC3.to_sys() - UTC2;
		CHECK ( KDuration(dDiff5).seconds() == chrono::seconds(61) );

		auto tz = kFindTimezone("Europe/Paris");

		auto oldLocale = kGetGlobalLocale();

		if (kSetGlobalLocale("fr_FR.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { kSetGlobalLocale(oldLocale.name()); };

			KLocalTime Local1;
			Local1 = KLocalTime(UTC1);
			Local1 = KLocalTime(UTC1, tz);

			SysTime =  kFromLocalTime(Local1.to_local(), tz);
			CHECK ( std::chrono::system_clock::to_time_t(SysTime) == KUnixTime(Local1.to_sys()).to_time_t());

			if (Local1.get_utc_offset() == std::chrono::minutes(60))
			{
				// these checks only work correctly with timezone set to CET
				CHECK ( UTC1.Format()            == "1973-12-31 23:59:59" );
				CHECK ( Local1.Format()          == "1974-01-01 00:59:59" );
				CHECK ( Local1.get_utc_offset().count() == 3600 );
				CHECK ( Local1.days().count()    == 1     );
				CHECK ( Local1.months().count()  == 1     );
				CHECK ( Local1.years().count()   == 1974  );
				CHECK ( Local1.hours().count()   == 0     );
				CHECK ( Local1.minutes().count() == 59    );
				CHECK ( Local1.seconds().count() == 59    );
				CHECK ( Local1.hours12().count() == 12    );
				CHECK ( Local1.is_pm()           == false );
				CHECK ( Local1.get_zone_abbrev() == "CET" );
				CHECK ( Local1.get_zone_name()   == "Europe/Paris" );
				CHECK ( Local1.month()           == chrono::January );
				CHECK ( Local1.weekday()         == chrono::Tuesday );
				CHECK ( Local1.get_utc_offset() == chrono::minutes(60) );
				CHECK ( kFormTimestamp(std::locale(), KLocalTime(UTC1, tz), "%A %c") == "Mardi Mar  1 jan 00:59:59 1974" );
				CHECK ( kFormTimestamp(std::locale("de_DE.UTF-8"), KLocalTime(UTC1, kFindTimezone("America/Mexico_City")), "%A %c") == "Montag Mo 31 Dez 17:59:59 1973" );
			}
		}

		{
			// these checks do not rely on global environment settings

			bool bHasTimezone = false;

			const chrono::time_zone* tz = nullptr;
			try {
				tz = kFindTimezone("Asia/Tokyo");
				bHasTimezone = true;
			} catch (const std::exception& ex) {
				kPrintLine ( "cannot find timezone Asia/Tokyo" );
			}

			KLocalTime Local1(UTC1, tz);

			bool bHasLocale = false;

			std::locale loc;
			try {
				loc = std::locale("fr_FR");
				bHasLocale = true;
			} catch (const std::exception& ex) {
				kPrintLine ( "cannot get locale fr_FR" );
			}

			SysTime =  kFromLocalTime(Local1.to_local(), Local1.get_time_zone());
			CHECK ( std::chrono::system_clock::to_time_t(SysTime) == KUnixTime(Local1.to_sys()).to_time_t());
			SysTime =  Local1.to_sys();
			CHECK ( std::chrono::system_clock::to_time_t(SysTime) == KUnixTime(Local1.to_sys()).to_time_t());
			CHECK ( Local1.get_utc_offset()  == chrono::seconds(32400));
			CHECK ( UTC1.Format()            == "1973-12-31 23:59:59" );
			CHECK ( Local1.Format()          == "1974-01-01 08:59:59" );
			CHECK ( Local1.days().count()    == 1     );
			CHECK ( Local1.months().count()  == 1     );
			CHECK ( Local1.years().count()   == 1974  );
			CHECK ( Local1.hours().count()   == 8     );
			CHECK ( Local1.minutes().count() == 59    );
			CHECK ( Local1.seconds().count() == 59    );
			CHECK ( Local1.hours12().count() == 8     );
			CHECK ( Local1.is_pm()           == false );
			CHECK ( Local1.get_zone_abbrev() == "JST" );
			CHECK ( Local1.get_zone_name()   == "Asia/Tokyo" );
			if (bHasLocale)
			{
				CHECK ( Local1.month()       == chrono::January );
				CHECK ( Local1.weekday()     == chrono::Tuesday );
				if (bHasTimezone) {
					CHECK ( kFormTimestamp(std::locale("de_DE.UTF-8"), KLocalTime(UTC1, kFindTimezone("America/Mexico_City")), "%A %c") == "Montag Mo 31 Dez 17:59:59 1973" );
				}
			}
			// test for the day of year calculation
			CHECK ( Local1.to_tm().tm_yday   == 0     );
			Local1 = kParseLocalTimestamp("16.08.2011 12:00:00");
			CHECK ( Local1.yearday().count() == 228   );
			CHECK ( Local1.to_tm().tm_yday   == 227   );
			CHECK ( Local1.is_leap()         == false );
			Local1 = kParseLocalTimestamp("16.08.2012 12:00:00");
			CHECK ( Local1.yearday().count() == 229   );
			CHECK ( Local1.to_tm().tm_yday   == 228   );
			CHECK ( Local1.is_leap()         == true  );
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
		static constexpr std::array<std::pair<KStringView, KStringView>, 125> Timestamps
		{{
			{ "Tue, 16 Aug 2021 11:23:42 +0100", "Mon, 16 Aug 2021 10:23:42 GMT" },
			{ "Tue, 17 Aug 2021 12:23:42 CEST" , "Tue, 17 Aug 2021 10:23:42 GMT" },
			{ "Fri Oct 24 15:32:27 2014 +0400" , "Fri, 24 Oct 2014 11:32:27 GMT" },
			{ "Fri Oct 24 15:32:29 +0400 2006" , "Tue, 24 Oct 2006 11:32:29 GMT" },

			{ "Tue, 18 Aug 2021 11:23:42 CET"  , "Wed, 18 Aug 2021 10:23:42 GMT" },

			{ "2021-08-19 03:23:42.211 -0700"  , "Thu, 19 Aug 2021 10:23:42 GMT" },
			{ "2021-08-20 03:23:42,211 -0700"  , "Fri, 20 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 21 12:23:42.211 CEST"  , "Sat, 21 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 22 11:23:42.211 CET"   , "Sun, 22 Aug 2021 10:23:42 GMT" },
			{ "2021-08-23 02:53:42.211-0730"   , "Mon, 23 Aug 2021 10:23:42 GMT" },
			{ "2021-08-24 02:53:42,211-0730"   , "Tue, 24 Aug 2021 10:23:42 GMT" },
			{ "Fri Oct 24 15:32:27 EDT 2014"   , "Fri, 24 Oct 2014 19:32:27 GMT" },

			{ "25/Aug/2021:02:53:42 -0730"     , "Wed, 25 Aug 2021 10:23:42 GMT" },
			{ "26/Aug/2021 02:53:42 -0730"     , "Thu, 26 Aug 2021 10:23:42 GMT" },
			{ "Aug 27 02:53:42 -0730 2021"     , "Fri, 27 Aug 2021 10:23:42 GMT" },
			{ "2021-08-28 02:53:42 -0730"      , "Sat, 28 Aug 2021 10:23:42 GMT" },
			{ "Aug 29, 2021 10:23:42 AM"       , "Sun, 29 Aug 2021 10:23:42 GMT" },
			{ "Aug 30, 2021 10:23:42 PM"       , "Mon, 30 Aug 2021 22:23:42 GMT" },
			{ "2021-08-31 02:53:42-0730"       , "Tue, 31 Aug 2021 10:23:42 GMT" },
			{ "2021-08-10T02:53:42-0730"       , "Tue, 10 Aug 2021 10:23:42 GMT" },

			{ "2021-08-11T10:23:42.321Z"       , "Wed, 11 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 12 10:23:42.321"       , "Thu, 12 Aug 2021 10:23:42 GMT" },
			{ "13-Aug-2021 10:23:42.321"       , "Fri, 13 Aug 2021 10:23:42 GMT" },
			{ "2021 Aug 14 11:23:42 CET"       , "Sat, 14 Aug 2021 10:23:42 GMT" },
			{ "2021-08-15 11:23:42 CET"        , "Sun, 15 Aug 2021 10:23:42 GMT" },
			{ "2021-08-16 10:23:42.335"        , "Mon, 16 Aug 2021 10:23:42 GMT" },
			{ "2021-08-17T10:23:42.335"        , "Tue, 17 Aug 2021 10:23:42 GMT" },
			{ "2021-08-18 10:23:42,335"        , "Wed, 18 Aug 2021 10:23:42 GMT" },
			{ "2021-08-19*10:23:42:335"        , "Thu, 19 Aug 2021 10:23:42 GMT" },
			{ "20210820 10:23:42.345"          , "Fri, 20 Aug 2021 10:23:42 GMT" },
			{ "Aug 21 2021 10:23:42"           , "Sat, 21 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 Aug 22 2021"           , "Sun, 22 Aug 2021 10:23:42 GMT" },
			{ "Aug 23 10:23:42 2021"           , "Mon, 23 Aug 2021 10:23:42 GMT" },
			{ "2021-08-24T10:23:42Z"           , "Tue, 24 Aug 2021 10:23:42 GMT" },
			{ "25 Aug 2021 10:23:42"           , "Wed, 25 Aug 2021 10:23:42 GMT" },
			{ "26-Aug-2021 10:23:42"           , "Thu, 26 Aug 2021 10:23:42 GMT" },
			{ "27/Aug/2021 10:23:42"           , "Fri, 27 Aug 2021 10:23:42 GMT" },
			{ "28/Aug/2021:10:23:42"           , "Sat, 28 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 Aug 4 2021"            , "Wed, 04 Aug 2021 10:23:42 GMT" },
			{ "Aug 5 10:23:42 2021"            , "Thu, 05 Aug 2021 10:23:42 GMT" },
			{ "Aug 6 2021 10:23:42"            , "Fri, 06 Aug 2021 10:23:42 GMT" },
			{ "7 Aug 2021 10:23:42"            , "Sat, 07 Aug 2021 10:23:42 GMT" },
			{ "8-Aug-2021 10:23:42"            , "Sun, 08 Aug 2021 10:23:42 GMT" },
			{ "9/Aug/2021 10:23:42"            , "Mon, 09 Aug 2021 10:23:42 GMT" },
			{ "2021.8.10 10:23:42"             , "Tue, 10 Aug 2021 10:23:42 GMT" },
			{ "2021-8-11 10:23:42"             , "Wed, 11 Aug 2021 10:23:42 GMT" },
			{ "2021/8/12 10:23:42"             , "Thu, 12 Aug 2021 10:23:42 GMT" },
			{ "2021.10.2 10:23:42"             , "Sat, 02 Oct 2021 10:23:42 GMT" },
			{ "2021-10-3 10:23:42"             , "Sun, 03 Oct 2021 10:23:42 GMT" },
			{ "2021/10/4 10:23:42"             , "Mon, 04 Oct 2021 10:23:42 GMT" },
			{ "2021.8.1 10:23:42"              , "Sun, 01 Aug 2021 10:23:42 GMT" },
			{ "2021-8-2 10:23:42"              , "Mon, 02 Aug 2021 10:23:42 GMT" },
			{ "2021/8/3 10:23:42"              , "Tue, 03 Aug 2021 10:23:42 GMT" },
			{ "2021-08-29 10:23:42"            , "Sun, 29 Aug 2021 10:23:42 GMT" },
			{ "2021-08-30T10:23:42"            , "Mon, 30 Aug 2021 10:23:42 GMT" },
			{ "2021-08-31*10:23:42"            , "Tue, 31 Aug 2021 10:23:42 GMT" },
			{ "2021/08/10 10:23:42"            , "Tue, 10 Aug 2021 10:23:42 GMT" },
			{ "2021/08/11*10:23:42"            , "Wed, 11 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 12-08-2021"            , "Thu, 12 Aug 2021 10:23:42 GMT" },
			{ "13-08-2021 10:23:42"            , "Fri, 13 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 14.08.2021"            , "Sat, 14 Aug 2021 10:23:42 GMT" },
			{ "15.08.2021 10:23:42"            , "Sun, 15 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 16/08/2021"            , "Mon, 16 Aug 2021 10:23:42 GMT" },
			{ "17/08/2021 10:23:42"            , "Tue, 17 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 18-08-21"              , "Wed, 18 Aug 2021 10:23:42 GMT" },
			{ "19-08-21 10:23:42"              , "Thu, 19 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 20.08.21"              , "Fri, 20 Aug 2021 10:23:42 GMT" },
			{ "21.08.21 10:23:42"              , "Sat, 21 Aug 2021 10:23:42 GMT" },
			{ "10:23:42 22/08/21"              , "Sun, 22 Aug 2021 10:23:42 GMT" },
			{ "23/08/21 10:23:42"              , "Mon, 23 Aug 2021 10:23:42 GMT" },
			{ "23.10.2021 10:23"               , "Sat, 23 Oct 2021 10:23:00 GMT" },
			{ "24/10/2021 10:23"               , "Sun, 24 Oct 2021 10:23:00 GMT" },
			{ "25-10-2021 10:23"               , "Mon, 25 Oct 2021 10:23:00 GMT" },
			{ "20210824T102342Z"               , "Tue, 24 Aug 2021 10:23:42 GMT" },
			{ "23.1.2021 10:23"                , "Sat, 23 Jan 2021 10:23:00 GMT" },
			{ "24/1/2021 10:23"                , "Sun, 24 Jan 2021 10:23:00 GMT" },
			{ "25-1-2021 10:23"                , "Mon, 25 Jan 2021 10:23:00 GMT" },
			{ "3.10.2021 10:23"                , "Sun, 03 Oct 2021 10:23:00 GMT" },
			{ "4/10/2021 10:23"                , "Mon, 04 Oct 2021 10:23:00 GMT" },
			{ "5-10-2021 10:23"                , "Tue, 05 Oct 2021 10:23:00 GMT" },
			{ "210825 10:23:42"                , "Wed, 25 Aug 2021 10:23:42 GMT" },
			{ "3.1.2021 10:23"                 , "Sun, 03 Jan 2021 10:23:00 GMT" },
			{ "4/1/2021 10:23"                 , "Mon, 04 Jan 2021 10:23:00 GMT" },
			{ "5-1-2021 10:23"                 , "Tue, 05 Jan 2021 10:23:00 GMT" },
			{ "20210826102342"                 , "Thu, 26 Aug 2021 10:23:42 GMT" },
			{ "Aug 27, 2021"                   , "Fri, 27 Aug 2021 00:00:00 GMT" },
			{ "Aug 3, 2021"                    , "Tue, 03 Aug 2021 00:00:00 GMT" },
			{ "2021 Aug 16"                    , "Mon, 16 Aug 2021 00:00:00 GMT" },
			{ "17-Aug-2021"                    , "Tue, 17 Aug 2021 00:00:00 GMT" },
			{ "Aug 18 2021"                    , "Wed, 18 Aug 2021 00:00:00 GMT" },
			{ "19 Aug 2021"                    , "Thu, 19 Aug 2021 00:00:00 GMT" },
			{ "20/Aug/2021"                    , "Fri, 20 Aug 2021 00:00:00 GMT" },
			{ "21/08/2021"                     , "Sat, 21 Aug 2021 00:00:00 GMT" },
			{ "22-08-2021"                     , "Sun, 22 Aug 2021 00:00:00 GMT" },
			{ "23.08.2021"                     , "Mon, 23 Aug 2021 00:00:00 GMT" },
			{ "2021/08/24"                     , "Tue, 24 Aug 2021 00:00:00 GMT" },
			{ "2021-08-25"                     , "Wed, 25 Aug 2021 00:00:00 GMT" },
			{ "2021.08.26"                     , "Thu, 26 Aug 2021 00:00:00 GMT" },
			{ "3.12.2021"                      , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "30.8.2021"                      , "Mon, 30 Aug 2021 00:00:00 GMT" },
			{ "3/12/2021"                      , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "30/8/2021"                      , "Mon, 30 Aug 2021 00:00:00 GMT" },
			{ "2021/12/3"                      , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "2021-12-3"                      , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "2021/1/12"                      , "Tue, 12 Jan 2021 00:00:00 GMT" },
			{ "2021-1-13"                      , "Wed, 13 Jan 2021 00:00:00 GMT" },
			{ "1.12.2021"                      , "Wed, 01 Dec 2021 00:00:00 GMT" },
			{ "2/12/2021"                      , "Thu, 02 Dec 2021 00:00:00 GMT" },
			{ "3-12-2021"                      , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "13-1-2021"                      , "Wed, 13 Jan 2021 00:00:00 GMT" },
			{ "2021/1/3"                       , "Sun, 03 Jan 2021 00:00:00 GMT" },
			{ "2021.1.4"                       , "Mon, 04 Jan 2021 00:00:00 GMT" },
			{ "2021-1-5"                       , "Tue, 05 Jan 2021 00:00:00 GMT" },
			{ "27/08/21"                       , "Fri, 27 Aug 2021 00:00:00 GMT" },
			{ "28-08-21"                       , "Sat, 28 Aug 2021 00:00:00 GMT" },
			{ "29.08.21"                       , "Sun, 29 Aug 2021 00:00:00 GMT" },
			{ "20210830"                       , "Mon, 30 Aug 2021 00:00:00 GMT" },
			{ "2.12.21"                        , "Thu, 02 Dec 2021 00:00:00 GMT" },
			{ "12.1.21"                        , "Tue, 12 Jan 2021 00:00:00 GMT" },
			{ "3/12/21"                        , "Fri, 03 Dec 2021 00:00:00 GMT" },
			{ "13/1/21"                        , "Wed, 13 Jan 2021 00:00:00 GMT" },
			{ "2.3.21"                         , "Tue, 02 Mar 2021 00:00:00 GMT" },
			{ "3/4/21"                         , "Sat, 03 Apr 2021 00:00:00 GMT" },
			{ "4/5/21"                         , "Tue, 04 May 2021 00:00:00 GMT" },
			{ "210831"                         , "Tue, 31 Aug 2021 00:00:00 GMT" },
		}};

		for (auto& Timestamp : Timestamps)
		{
			auto tTime = kParseTimestamp(Timestamp.first);
			INFO  ( Timestamp.first );
			CHECK ( tTime.ok() );
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
		CHECK ( kGetTimezoneOffset("XYZ" ) == chrono::minutes(-1)  );
		CHECK ( kGetTimezoneOffset("GMT" ) == std::chrono::seconds(0)   );
		CHECK ( kGetTimezoneOffset("CET" ) == std::chrono::seconds(1 * 60 * 60) );
		CHECK ( kGetTimezoneOffset("NPT" ) == std::chrono::seconds((5 * 60 + 45) * 60) );
		CHECK ( kGetTimezoneOffset("NZST") == std::chrono::seconds(12 * 60 * 60) );
		CHECK ( kGetTimezoneOffset("COST") == std::chrono::seconds(-4 * 60 * 60) );
		CHECK ( kGetTimezoneOffset("HST" ) == std::chrono::seconds(-10 * 60 * 60) );
	}

	SECTION("comparison")
	{
		KUTCTime a = KUnixTime(123456);
		KUTCTime b = KUnixTime(12345);
		if (a > b)
		{
			CHECK ( true );
		}
		else
		{
			CHECK ( false );
		}
		CHECK ( (a >  b) );
		CHECK ( (a >= b) );
		CHECK ( (a != b) );
		CHECK ( (b <  a) );
		CHECK ( (b <= a) );

		KLocalTime c = KLocalTime(b);
		CHECK       ( (a >  c) );
		CHECK       ( (a >= c) );
		CHECK_FALSE ( (a == c) );
		CHECK       ( (a != c) );
		CHECK       ( (c <  a) );
		CHECK       ( (c <= a) );

		CHECK_FALSE ( (c >  a) );
		CHECK_FALSE ( (c >= a) );
		CHECK       ( (c != a) );
		CHECK_FALSE ( (a <  c) );
		CHECK_FALSE ( (a <= c) );
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

	SECTION("KUnixTime")
	{
		// check that our own constexpr from_time_t() works correctly (take care to keep time_t small enough
		// for 32 bit time_t ..)
		CHECK ( KUnixTime::from_time_t(2445823474) == std::chrono::system_clock::from_time_t(2445823474) );
		auto tp = std::chrono::system_clock::now();
		KUnixTime UT = tp;
		// check that our own constexpr to_time_t() works correctly
		CHECK ( KUnixTime::to_time_t(UT) == std::chrono::system_clock::to_time_t(tp) );

		// check that time conversions don't have overflows
		KUnixTime UMax = KUnixTime::max();
		KUnixTime UMin = KUnixTime::min();

		if (KUnixTime::duration(1) == chrono::microseconds(1))
		{
			CHECK ( UMax.to_string() == "32103-01-10 04:00:54"  );
			CHECK ( UMin.to_string() == "-28164-12-21 04:00:54" );
		}
		else if (KUnixTime::duration(1) == chrono::nanoseconds(1))
		{
			CHECK ( UMax.to_string() == "2262-04-11 23:47:16"  );
			CHECK ( UMin.to_string() == "1677-09-21 23:47:16" );
		}
		else
		{
			INFO("bad system clock duration type of larger than microseconds");
			CHECK ( false );
		}
	}

	SECTION("diff")
	{
		{
			KUTCTime Date1("1.5.2006 12:00");
			KUTCTime Date2("3.5.2007 11:00");
			auto d = Date2 - Date1;
			d.days();
			time_t t = d;
			time_t diff = Date2 - Date1;
			CHECK ( diff == 367 * 86400 - 3600 );
			KDuration duration = diff;
			CHECK ( duration.days() == chrono::days(366) );
		}
		{
			KUTCTime Date1(KUnixTime::from_time_t(3298462375));
			KUTCTime Date2(KUnixTime::from_time_t(3298462342));
			auto d = Date1 - Date2;
			auto days = d.days();
		}
	}

#if 0
	SECTION("KTimeOfDay")
	{
		KUnixTime U = kParseTimestamp("20.12.2022 14:37:56");
		//			auto d = chrono::floor<chrono::days>(U);
		//			chrono::hh_mm_ss hms = chrono::make_time(U - d);
		KTimeOfDay TD = U;
		CHECK ( TD.hours().count()   == 14 );
		CHECK ( TD.minutes().count() == 37 );
		CHECK ( TD.seconds().count() == 56 );
		CHECK ( TD.is_negative()     == false );
		CHECK ( TD.subseconds().count() == 0 );

		TD = kParseTimestamp("2022-12-20 14:37:56.789");
		CHECK ( TD.hours().count()   == 14 );
		CHECK ( TD.minutes().count() == 37 );
		CHECK ( TD.seconds().count() == 56 );
		CHECK ( TD.is_negative()     == false );
		CHECK ( TD.subseconds() == chrono::milliseconds(789) );

		TD = KUnixTime(1671547076);
		CHECK ( TD.hours().count()   == 14 );
		CHECK ( TD.minutes().count() == 37 );
		CHECK ( TD.seconds().count() == 56 );
		CHECK ( TD.is_negative()     == false );
		CHECK ( TD.subseconds() == chrono::milliseconds(0) );
	}
#endif

	SECTION("kFormTimeStamp")
	{
		auto now = kNow();
		auto sNow = kFormTimestamp();
		auto now2 = kParseTimestamp(sNow);
		auto diff = KUnixTime(now2) - now;
		CHECK ( chrono::duration_cast<chrono::seconds>(diff).count() < 3 );
		auto tz = kFindTimezone("Asia/Tokyo");
		KUnixTime U("12:34:56 16.08.2022");
		CHECK ( kFormTimestamp(U) == "2022-08-16 12:34:56" );
		CHECK ( kFormTimestamp(KLocalTime(U, tz), "%Y-%m-%d %H:%M:%S") == "2022-08-16 21:34:56" );
	}

	SECTION("custom formatters")
	{
		KUnixTime U = kParseTimestamp("12:34:56 16.08.2022");
		CHECK ( kFormat("{:%F %T}", U) == "2022-08-16 12:34:56" );
		KUTCTime UTC("12:34:56 16.08.2022");
		auto tz = kFindTimezone("Asia/Tokyo");
		KLocalTime Local(UTC, tz);
		CHECK ( kFormat("{:%Z: %F %T}, {:%Z: %F %T}", UTC, Local) == "UTC: 2022-08-16 12:34:56, JST: 2022-08-16 21:34:56" );
		CHECK ( kFormat("{}", UTC.hours()) == "12h" );
		//			CHECK ( kFormat("{}", UTC.days())  == "16d" );
		// mind you that days/months/years do not yet work.. (but would output e.g. "16[86400]s" for 16 days)
	}

	SECTION("utc dates")
	{
		KUTCTime Date0;
		CHECK ( Date0.ok() == false );
		Date0.trunc();
		CHECK ( Date0.ok() == true );
		CHECK ( Date0.month() == chrono::January    );
		CHECK ( Date0.day()   == chrono::day(1)     );
		CHECK ( Date0.last_day() == chrono::day(31) );
		CHECK ( Date0.year()  == chrono::year(0)    );
		CHECK ( Date0.is_leap()== true              );

		KUTCTime Date1;
		Date1.year(2024);
		CHECK ( Date1.ok() == false );
		CHECK ( Date1 == false );
		Date1.month(8);
		CHECK ( Date1.ok() == false );
		Date1.day(16);
		CHECK ( Date1.ok() == true );
		CHECK ( Date1.month() == chrono::August     );
		CHECK ( Date1.day()   == chrono::day(16)    );
		CHECK ( Date1.last_day() == chrono::day(31) );
		CHECK ( Date1.year()  == chrono::year(2024) );
		CHECK ( Date1.is_leap()== true              );
		CHECK ( Date1.to_string() == "2024-08-16 00:00:00"   );
		CHECK ( kFormat("{:%d/%m/%Y %H:%M:%S}", Date1) == "16/08/2024 00:00:00" );

		Date1 = KUTCTime(chrono::year(2023)/3/8);
		Date1 += chrono::months(1);
		CHECK ( Date1.month() == chrono::April      );
		CHECK ( Date1.day()   == chrono::day(8)     );
		CHECK ( Date1.last_day() == chrono::day(30) );
		CHECK ( Date1.year()  == chrono::year(2023) );
		CHECK ( Date1.is_leap()== false             );

		Date1 = KUTCTime(chrono::year(2023)/2/28);
		Date1 += chrono::days(1);
		CHECK ( Date1.month() == chrono::March      );
		CHECK ( Date1.day()   == chrono::day(1)     );
		CHECK ( Date1.last_day() == chrono::day(31) );
		CHECK ( Date1.year()  == chrono::year(2023) );
		CHECK ( Date1.is_leap()== false             );

		Date1 -= chrono::days(1);
		CHECK ( Date1.month() == chrono::February   );
		CHECK ( Date1.day()   == chrono::day(28)    );
		CHECK ( Date1.last_day() == chrono::day(28) );
		CHECK ( Date1.year()  == chrono::year(2023) );
		CHECK ( Date1.is_leap()== false             );

		Date1 = KUTCTime(chrono::year(2024)/2/28);
		Date1 += chrono::days(1);
		CHECK ( Date1.month() == chrono::February   );
		CHECK ( Date1.day()   == chrono::day(29)    );
		CHECK ( Date1.last_day() == chrono::day(29) );
		CHECK ( Date1.year()  == chrono::year(2024) );
		CHECK ( Date1.is_leap()== true              );

		KUTCTime Date2(chrono::year(2018)/8/16);
		CHECK_FALSE ( Date2 == Date1 );
		CHECK       ( Date2 != Date1 );
		CHECK_FALSE ( Date2 >  Date1 );
		CHECK_FALSE ( Date2 >= Date1 );
		CHECK       ( Date2 <  Date1 );
		CHECK       ( Date2 <= Date1 );

		Date2 = Date1;
		CHECK       ( Date2 == Date1 );
		CHECK_FALSE ( Date2 != Date1 );
		CHECK_FALSE ( Date2 >  Date1 );
		CHECK       ( Date2 >= Date1 );
		CHECK_FALSE ( Date2 <  Date1 );
		CHECK       ( Date2 <= Date1 );

		Date2 = Date1 + chrono::years(2);
		CHECK ( Date2.ok() == false ); // 2026-02-29 ..
		Date2.floor();
		CHECK ( Date2.ok() == true  ); // 2026-02-28 ..
		CHECK ( Date2.month() == chrono::February   );
		CHECK ( Date2.day()   == chrono::day(28)    );
		CHECK ( Date2.last_day() == chrono::day(28) );
		CHECK ( Date2.year()  == chrono::year(2026) );
		CHECK ( Date2.is_leap()== false             );

		Date2 = Date1 + chrono::years(2);
		CHECK ( Date2.ok() == false ); // 2026-02-29 ..
		Date2.ceil();
		CHECK ( Date2.ok() == true  ); // 2026-03-01 ..
		CHECK ( Date2.month() == chrono::March   );
		CHECK ( Date2.day()   == chrono::day(1)    );
		CHECK ( Date2.last_day() == chrono::day(31) );
		CHECK ( Date2.year()  == chrono::year(2026) );
		CHECK ( Date2.is_leap()== false             );

		Date2 = Date1 + chrono::months(8);
		Date2 = Date1 + chrono::days(60);
	}

	SECTION("local dates")
	{
		KLocalTime Date0;
		CHECK ( Date0.ok() == false );

		auto Date1 = KLocalTime(chrono::year(2023)/3/8);
		CHECK ( Date1.month() == chrono::March       );
		CHECK ( Date1.day()   == chrono::day(8)      );
		CHECK ( Date1.last_day() == chrono::day(31)  );
		CHECK ( Date1.year()   == chrono::year(2023) );
		CHECK ( Date1.is_leap()== false              );

		Date1 = KLocalTime(chrono::year(2024)/2/29);
		CHECK ( Date1.month() == chrono::February    );
		CHECK ( Date1.day()   == chrono::day(29)     );
		CHECK ( Date1.last_day() == chrono::day(29)  );
		CHECK ( Date1.year()   == chrono::year(2024) );
		CHECK ( Date1.is_leap()== true               );
		// do not check on local time conversion - it may differ
//		CHECK ( Date1.to_string() == "2024-02-29 00:00:00"   );
//		CHECK ( kFormat("{:%d/%m/%Y %H:%M:%S}", Date1) == "29/02/2024 00:00:00" );

		KLocalTime Date2(chrono::year(2018)/8/16);
		CHECK_FALSE ( ( Date2 == Date1 ) );
		CHECK       ( ( Date2 != Date1 ) );
		CHECK_FALSE ( ( Date2 >  Date1 ) );
		CHECK_FALSE ( ( Date2 >= Date1 ) );
		CHECK       ( ( Date2 <  Date1 ) );
		CHECK       ( ( Date2 <= Date1 ) );

		Date2 = Date1;
		CHECK       ( ( Date2 == Date1 ) );
		CHECK_FALSE ( ( Date2 != Date1 ) );
		CHECK_FALSE ( ( Date2 >  Date1 ) );
		CHECK       ( ( Date2 >= Date1 ) );
		CHECK_FALSE ( ( Date2 <  Date1 ) );
		CHECK       ( ( Date2 <= Date1 ) );
	}

	SECTION("kFormCommonLogTimestamp")
	{
		bool bHasLocale = false;

		KUnixTime U = kParseTimestamp("12:34:56 16.08.2022");
		CHECK ( kFormCommonLogTimestamp(U) == "[16/Aug/2022:12:34:56 +0000]" );
	}

	SECTION("detail::KParsedTimestamp")
	{
		auto t1 = kParseTimestamp("12:34:56 16.08.2022");
		KUnixTime t2 = kParseTimestamp("13:34:56 16.08.2022");
		auto diff1 = t2 - t1;
		auto diff2 = t1 - t2;
		auto diff3 = t1 - t1;
		CHECK ( diff1 == chrono::hours(1)  );
		CHECK ( diff2 == chrono::hours(-1) );
		CHECK ( diff3 == KDuration::zero() );

		CHECK_FALSE ( ( t1 == t2 ) );
		CHECK       ( ( t1 != t2 ) );
		CHECK       ( ( t1 <  t2 ) );
		CHECK       ( ( t1 <= t2 ) );
		CHECK_FALSE ( ( t1 >  t2 ) );
		CHECK_FALSE ( ( t1 >= t2 ) );

		CHECK_FALSE ( ( t2 == t1 ) );
		CHECK       ( ( t2 != t1 ) );
		CHECK_FALSE ( ( t2 <  t1 ) );
		CHECK_FALSE ( ( t2 <= t1 ) );
		CHECK       ( ( t2 >  t1 ) );
		CHECK       ( ( t2 >= t1 ) );
	}
}
