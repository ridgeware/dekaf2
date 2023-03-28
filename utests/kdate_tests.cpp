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

	using namespace std::literals::chrono_literals;

	auto t = chrono::sys_days{10d/10/2012} + 12h + 38min + 40s + 123456us;
	auto x = chrono::April;
	auto y = 20d;
}
