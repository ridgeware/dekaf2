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

	auto yy     = 2000y;
	auto dd     = 12d;
	auto hh     = 12h;
	auto mm     = 12min;
	auto ss     = 12s;
	auto ms     = 12ms;
	auto us     = 12us;
	auto ns     = 12ns;
	auto tp     = chrono::sys_days{dd/10/yy} + hh + mm + ss + ms + us + ns;
	auto t      = chrono::sys_days{10d/10/2012} + 12h + 38min + 40s + 123456us;
	auto x      = chrono::April;
	auto y      = chrono::Monday;

	CHECK ( tp < t );
	CHECK ( x > chrono::February );
	CHECK ( y != chrono::Friday  );
}
