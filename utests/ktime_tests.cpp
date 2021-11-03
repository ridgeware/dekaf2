#include "catch.hpp"

#include <dekaf2/ktime.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kscopeguard.h>

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
 * The local time zone tests depend on too many moving parts to
 * have them executed per default, particularly due to differences
 * across Linux distributions (not all switch on TZ settings).
 * We leave the code in, but comment it out, so that it can be
 * used for manual tests.
 */

/*
		KString sOldTZ = kGetEnv("TZ");
		KScopeGuard TZGuard = [&sOldTZ] { kSetEnv("TZ", sOldTZ); };

#ifdef DEKAF2_IS_WINDOWS
		kSetEnv("TZ", "GST-1GDT"); // "German Standard Time -1 German Daylight Time (note both timezone names are unknown in Germany..)
#else
		kSetEnv("TZ", "CET");      // set Central European Time as timezone
#endif

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
 */

	}

}
