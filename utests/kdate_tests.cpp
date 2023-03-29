#include "catch.hpp"

#include <dekaf2/kdate.h>

using namespace dekaf2;

TEST_CASE("KDate")
{

	auto now = chrono::system_clock::now();

	// Get a local time_point with days precision
	auto day = chrono::floor<chrono::days>(now);

	// Convert local days-precision time_point to a local {y, m, d} calendar
	chrono::year_month_day ymd{day};

	// Split time since local midnight into {h, m, s, subseconds}
	chrono::hh_mm_ss<chrono::system_clock::duration> hms{now - day};

	using namespace chrono::literals;

#if DEKAF2_HAS_CPP_20 || !DEKAF2_IS_CLANG
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
#if DEKAF2_HAS_CPP_20 || !DEKAF2_IS_CLANG
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
