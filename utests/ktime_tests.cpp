#include "catch.hpp"

#include <dekaf2/ktime.h>
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
		CHECK ( kTranslateSeconds(0            , false) == "less than a second" );
		CHECK ( kTranslateSeconds(1            , false) == "1 sec" );
		CHECK ( kTranslateSeconds(2            , false) == "2 secs" );
		CHECK ( kTranslateSeconds(1326257      , false) == "2.2 wks" );
		CHECK ( kTranslateSeconds(239874723651 , false) == "7606.4 yrs" );
		CHECK ( kTranslateSeconds(2376         , false) == "39 mins, 36 secs" );
		CHECK ( kTranslateSeconds(23872        , false) == "6 hrs, 37 mins" );
		CHECK ( kTranslateSeconds(23           , false) == "23 secs" );
		CHECK ( kTranslateSeconds(123          , false) == "2 mins, 3 secs" );
		CHECK ( kTranslateSeconds(120          , true ) == "2 mins" );
		CHECK ( kTranslateSeconds(0            , true ) == "less than a second" );
		CHECK ( kTranslateSeconds(1            , true ) == "1 sec" );
		CHECK ( kTranslateSeconds(2            , true ) == "2 secs" );
		CHECK ( kTranslateSeconds(1326257      , true ) == "2 wks, 1 day, 8 hrs, 24 mins, 17 secs" );
		CHECK ( kTranslateSeconds(239874723651 , true ) == "7606 yrs, 19 wks, 4 days, 19 hrs, 40 mins, 51 secs" );
		CHECK ( kTranslateSeconds(23872        , true ) == "6 hrs, 37 mins, 52 secs" );
	}

	SECTION("KBrokenDownTime")
	{
		KUTCTime UTC1;
		KUTCTime UTC2(123545656);

		CHECK ( UTC2.GetDay()       == 30    );
		CHECK ( UTC2.GetMonth()     == 11    );
		CHECK ( UTC2.GetYear()      == 1973  );
		CHECK ( UTC2.GetHour()      == 22    );
		CHECK ( UTC2.GetMinute()    == 14    );
		CHECK ( UTC2.GetSecond()    == 16    );
		CHECK ( UTC2.GetHour12()    == 10    );
		CHECK ( UTC2.GetDayName()   == "Fri" );
		CHECK ( UTC2.GetMonthName() == "Nov" );
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
		CHECK ( UTC2.GetMonthName() == "Dec" );
		CHECK ( UTC2.GetDayName()   == "Mon" );
		CHECK ( UTC2.IsPM()         == true  );
/*
		auto oldLocale = kGetGlobalLocale();
		if (kSetGlobalLocale("de_DE.UTF-8"))
		{
			KScopeGuard TZGuard = [&oldLocale] { std::locale::global(oldLocale); };

			KLocalTime Local1;
			Local1 = UTC1;
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
			CHECK ( Local1.GetMonthName() == "Jan" );
			CHECK ( Local1.GetDayName()   == "Tue" );
			CHECK ( Local1.GetUTCOffset() == 3600  );
			CHECK ( kFormTimestamp(UTC1.ToTimeT(), "%A %c", true) == "Dienstag Di  1 Jan 00:59:59 1974" );
		}
 */
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

}
