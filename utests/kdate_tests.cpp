#include "catch.hpp"

#include <dekaf2/kdate.h>
#include <dekaf2/ktime.h>
#include <dekaf2/kformat.h>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KDate")
{
	SECTION("generic")
	{
		auto now = chrono::system_clock::now();

		// Get a local time_point with days precision
		auto day = chrono::floor<chrono::days>(now);

		// Convert local days-precision time_point to a local {y, m, d} calendar
		chrono::year_month_day ymd{day};

		// Split time since local midnight into {h, m, s, subseconds}
		chrono::hh_mm_ss<chrono::system_clock::duration> hms{now - day};

		using namespace chrono::literals;

#if DEKAF2_HAS_MONTH_AND_YEAR_LITERALS
		auto yy     = 2000y;
		auto dd     = 12d;
#else
		auto yy     = chrono::year(2000);
		auto dd     = chrono::day(12);
#endif
		auto hh     = 12h;
		auto mm     = 12min;
		auto ss     = 12s;
		auto ms     = 12ms;
		auto us     = 12us;
		auto ns     = 12ns;
		auto tp     = chrono::sys_days{dd/10/yy} + hh + mm + ss + ms + us + ns;
#if DEKAF2_HAS_MONTH_AND_YEAR_LITERALS
		auto t      = chrono::sys_days{10d/10/2012} + 12h + 38min + 40s + 123456us;
#else
		auto t      = chrono::sys_days{chrono::day(10)/10/2012} + 12h + 38min + 40s + 123456us;
#endif
		CHECK ( tp < t );

		auto x      = chrono::April;
		auto y      = chrono::Monday;

		CHECK ( x > chrono::February );
		CHECK ( y != chrono::Friday  );
	}

	SECTION("KDate")
	{
		KDate Date0;
		CHECK ( Date0.ok() == false );
		Date0.to_trunc();
		CHECK ( Date0.ok() == true );
		CHECK ( Date0.month() == chrono::January    );
		CHECK ( Date0.day()   == chrono::day(1)     );
		CHECK ( Date0.last_day() == chrono::day(31) );
		CHECK ( Date0.year()  == chrono::year(0)    );
		CHECK ( Date0.is_leap()== true              );

		KDate Date1;
		Date1.year(2024);
		CHECK ( Date1.ok() == false );
		Date1.month(8);
		CHECK ( Date1.ok() == false );
		Date1.day(16);
		CHECK ( Date1.ok() == true );
		CHECK ( Date1.month() == chrono::August     );
		CHECK ( Date1.day()   == chrono::day(16)    );
		CHECK ( Date1.last_day() == chrono::day(31) );
		CHECK ( Date1.year()  == chrono::year(2024) );
		CHECK ( Date1.is_leap()== true              );
		CHECK ( Date1.to_string() == "2024-08-16"   );
		CHECK ( kFormat("{:%d/%m/%Y}", Date1) == "16/08/2024" );

		Date1 = KDate(chrono::year(2023)/3/8);
		Date1 += chrono::months(1);
		CHECK ( Date1.month() == chrono::April      );
		CHECK ( Date1.day()   == chrono::day(8)     );
		CHECK ( Date1.last_day() == chrono::day(30) );
		CHECK ( Date1.year()  == chrono::year(2023) );
		CHECK ( Date1.is_leap()== false             );

		Date1 = KDate(chrono::year(2023)/2/28);
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

		Date1 = KDate(chrono::year(2024)/2/28);
		Date1 += chrono::days(1);
		CHECK ( Date1.month() == chrono::February   );
		CHECK ( Date1.day()   == chrono::day(29)    );
		CHECK ( Date1.last_day() == chrono::day(29) );
		CHECK ( Date1.year()  == chrono::year(2024) );
		CHECK ( Date1.is_leap()== true              );

		auto Date3 = Date1 + chrono::years(1);
		CHECK ( Date3.ok()    == true               );
		CHECK ( Date3.month() == chrono::February   );
		CHECK ( Date3.day()   == chrono::day(28)    );
		CHECK ( Date3.last_day() == chrono::day(28) );
		CHECK ( Date3.year()  == chrono::year(2025) );
		CHECK ( Date3.is_leap()== false             );

		KDate Date2(chrono::year(2018)/8/16);
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
		CHECK ( Date2.ok() == true  );
		CHECK ( Date2.month() == chrono::February   );
		CHECK ( Date2.day()   == chrono::day(28)    );
		CHECK ( Date2.last_day() == chrono::day(28) );
		CHECK ( Date2.year()  == chrono::year(2026) );
		CHECK ( Date2.is_leap()== false             );

		Date2 = Date1 + chrono::months(8);
		CHECK ( Date2 - Date1 == chrono::duration_cast<chrono::days>(chrono::months(8)) );
		Date2 = Date1 + chrono::days(60);
		CHECK ( Date2 - Date1 == chrono::days(60)  );
		CHECK ( Date1.to_tm().tm_isdst == 0        );
#if DEKAF2_HAS_FMT_FORMAT
		CHECK_NOTHROW( kFormTimestamp(Date1, "{:%Y%Z}") );
		auto s = kFormTimestamp(Date1, "{:%Y%Z}");
#else
		CHECK_NOTHROW( kFormTimestamp(Date1, "{:%Y}") );
		auto s = kFormTimestamp(Date1, "{:%Y}");
#endif
#if DEKAF2_HAS_STD_FORMAT
		// with time_put() and gcc the result is the empty string (actually it throws because:
		// "The supplied date time doesn't contain a time zone"
		if (!s.empty())
#endif
		{
			CHECK ( s == "2024"  );
		}
		CHECK ( kFormTimestamp(Date1, "Hello {:%Y}") == "Hello 2024");
		CHECK ( kFormTimestamp(Date1, "{:%Y} years") == "2024 years" );
		CHECK ( kFormTimestamp(Date1, "Hello {:%Y how %m} are you") == "Hello 2024 how 02 are you" );
//		CHECK ( kFormTimestamp(Date1, "Hello %Y how %m are you") == "Hello 2024 how 02 are you" );
	}

	SECTION("next previous")
	{
		KDate Date(chrono::year(2023)/chrono::April/12);
		CHECK ( Date.weekday() == chrono::Wednesday );
		Date.to_next(chrono::Monday);
		CHECK ( Date.weekday() == chrono::Monday );
		CHECK ( Date.days().count() == 17 );
		Date.to_previous(chrono::Tuesday);
		CHECK ( Date.days().count() == 11 );
		CHECK ( Date.weekday() == chrono::Tuesday );
		Date.to_next(chrono::Wednesday, 2);
		CHECK ( Date.days().count() == 19 );
		CHECK ( Date.weekday() == chrono::Wednesday );

		Date = KDate(chrono::year(2023)/chrono::April/chrono::weekday_indexed(chrono::Monday, 1));
		CHECK ( Date.to_string()     == "2023-04-03" );
		CHECK ( kFormTimestamp(Date) == "2023-04-03" );
		Date = KDate(chrono::year(2023)/chrono::April/chrono::weekday_last(chrono::Monday));
		CHECK ( kFormTimestamp(Date) == "2023-04-24" );
		Date.weekday(chrono::weekday_indexed(chrono::Monday, 1));
		CHECK ( kFormTimestamp(Date) == "2023-04-03" );
		Date.weekday(chrono::weekday_last(chrono::Monday));
		CHECK ( kFormTimestamp(Date) == "2023-04-24" );
	}

	SECTION("KConstDate")
	{
		KConstDate Date(chrono::year(2023)/chrono::April/chrono::weekday_indexed(chrono::Monday, 1));
		CHECK ( Date.weekday()       == chrono::Monday );
		CHECK ( Date.to_string()     == "2023-04-03" );
		CHECK ( kFormTimestamp(Date) == "2023-04-03" );
	}

	SECTION("KDateDiff")
	{
		KDate D1(chrono::year(2024)/chrono::March/1);
		KDate D2(chrono::year(2024)/chrono::February/28);
		auto diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "2d" );
		CHECK ( diff == chrono::days(2) );

		D1 = KDate(chrono::year(2023)/chrono::March/1);
		D2 = KDate(chrono::year(2023)/chrono::February/28);
		diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "1d" );
		CHECK ( diff == chrono::days(1) );
		diff = D2 - D1;
		CHECK ( kFormat("{}", diff) == "-1d" );
		CHECK ( diff == chrono::days(-1) );

		D1 = KDate(chrono::year(2023)/chrono::March/30);
		D2 = KDate(chrono::year(2023)/chrono::February/28);
		diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "1m 2d" );
		CHECK ( diff == chrono::days(30) );

		D1 = KDate(chrono::year(2023)/chrono::March/31);
		D2 = KDate(chrono::year(2023)/chrono::February/28);
		diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "1m 3d" );
		CHECK ( diff == chrono::days(31) );

		D1 = KDate(chrono::year(2023)/chrono::April/1);
		D2 = KDate(chrono::year(2023)/chrono::February/28);
		diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "1m 4d" );
		CHECK ( diff == chrono::days(32) );

		D1 = KDate(chrono::year(2024)/chrono::April/1);
		D2 = KDate(chrono::year(2023)/chrono::February/28);
		diff = D1 - D2;
		CHECK ( kFormat("{}", diff) == "1y 1m 4d" );
		CHECK ( diff == chrono::days(398) );

		diff = D2 - D1;
		CHECK ( kFormat("{}", diff) == "-1y 1m 4d" );
		CHECK ( diff == chrono::days(-398) );

		KDays D = chrono::days(5);
		CHECK ( kFormat("{}", D) == "5d" );

		D = chrono::days(-453);
		CHECK ( kFormat("{}", D) == "-453d" );
	}
}
