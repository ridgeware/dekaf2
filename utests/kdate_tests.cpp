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
		day.time_since_epoch();

		// Convert local days-precision time_point to a local {y, m, d} calendar
		chrono::year_month_day ymd{day};

		// Split time since local midnight into {h, m, s, subseconds}
		chrono::hh_mm_ss<chrono::system_clock::duration> hms{now - day};

		using namespace chrono::literals;

#if DEKAF2_HAS_DATE_AND_YEAR_LITERALS
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
#if DEKAF2_HAS_DATE_AND_YEAR_LITERALS
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
		Date0.trunc();
		CHECK ( Date0.ok() == true );
		CHECK ( Date0.month() == chrono::January    );
		CHECK ( Date0.day()   == chrono::day(1)     );
		CHECK ( Date0.last_day() == chrono::day(31) );
		CHECK ( Date0.year()  == chrono::year(0)    );
		CHECK ( Date0.is_leap()== true              );

		KDate Date1;
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
		CHECK ( Date3.ok()    == false              );
		Date3.floor();
		CHECK ( Date3.month() == chrono::February   );
		CHECK ( Date3.day()   == chrono::day(28)    );
		CHECK ( Date3.last_day() == chrono::day(28) );
		CHECK ( Date3.year()  == chrono::year(2025) );
		CHECK ( Date3.is_leap()== false             );

		Date3 = Date1 + chrono::years(1);
		CHECK ( Date3.ok()    == false              );
		Date3.ceil();
		CHECK ( Date3.month() == chrono::March      );
		CHECK ( Date3.day()   == chrono::day(1)     );
		CHECK ( Date3.last_day() == chrono::day(31) );
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
		Date2.ceil();
		CHECK ( Date2 - Date1 == chrono::duration_cast<chrono::days>(chrono::months(8)) );
		Date2 = Date1 + chrono::days(60);
		CHECK ( Date2 - Date1 == chrono::days(60)  );
		CHECK ( Date1.to_tm().tm_isdst == 0        );
		CHECK_NOTHROW( Date1.to_string("%Y%Z")     );
		CHECK ( Date1.to_string("%Y%Z") == "2024"  ); // this would look differently with time_put() and gcc..
	}
}
