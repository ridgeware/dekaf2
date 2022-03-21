#include "catch.hpp"
#include <dekaf2/kron.h>
#include <dekaf2/ktime.h>
#include <dekaf2/bits/kron_utils.h>
#include <dekaf2/libs/croncpp/include/croncpp.h>

using namespace dekaf2;

using kron = cron_base<cron_standard_traits<KString, KStringView>, Kron_utils>;

#define ARE_EQUAL(x, y)          CHECK(x == y)
#define CRON_EXPR(x)             kron::make_cron(x)
#define CRON_STD_EQUAL(x, y)     ARE_EQUAL(kron::make_cron(x), kron::make_cron(y))
#define CRON_EXPECT_EXCEPT(x)    CHECK_THROWS_AS(kron::make_cron(x), kron::bad_cronexpr)
#define CRON_EXPECT_MSG(x, msg)  CHECK_THROWS_WITH(kron::make_cron(x), msg)

constexpr bool operator==(std::tm const & tm1, std::tm const & tm2) noexcept
{
	return
		tm1.tm_sec   == tm2.tm_sec  &&
		tm1.tm_min   == tm2.tm_min  &&
		tm1.tm_hour  == tm2.tm_hour &&
		tm1.tm_mday  == tm2.tm_mday &&
		tm1.tm_mon   == tm2.tm_mon  &&
		tm1.tm_year  == tm2.tm_year &&
		tm1.tm_wday  == tm2.tm_wday &&
		tm1.tm_yday  == tm2.tm_yday &&
		tm1.tm_isdst == tm2.tm_isdst;
}

namespace {

void check_next(kron::stringview_t expr, kron::stringview_t time, kron::stringview_t expected)
{
	auto cex = kron::make_cron(expr);

	auto initial_time = kron::utils::to_tm(time);

	auto result1    = kron::cron_next(cex, kron::utils::tm_to_time(initial_time));
	auto result2_tm = kron::cron_next(cex, initial_time);

	std::tm result1_tm;
	kron::utils::time_to_tm(&result1, &result1_tm);

	INFO(kron::utils::to_string(result1_tm));
	INFO(kron::utils::to_string(result2_tm));
	CHECK(result1_tm == result2_tm);

	auto value = kron::utils::to_string(result1_tm);

	CHECK(value == expected);
}

void check_cron_conv(kron::stringview_t expr)
{
	auto cex = kron::make_cron(expr);

	CHECK(cex.to_cronstr().compare(expr) == 0);
}

} // end of anonymous namespace

TEST_CASE("KRON")
{
	SECTION("Test simple", "")
	{
		auto cex = kron::make_cron("* * * * * *");
		REQUIRE(cex.to_string() == "111111111111111111111111111111111111111111111111111111111111 111111111111111111111111111111111111111111111111111111111111 111111111111111111111111 1111111111111111111111111111111 111111111111 1111111");
	}

	SECTION("standard: check seconds", "[std]")
	{
		CRON_STD_EQUAL("*/5 * * * * *", "0,5,10,15,20,25,30,35,40,45,50,55 * * * * *");
		CRON_STD_EQUAL("1/6 * * * * *", "1,7,13,19,25,31,37,43,49,55 * * * * *");
		CRON_STD_EQUAL("0/30 * * * * *", "0,30 * * * * *");
		CRON_STD_EQUAL("0-5 * * * * *", "0,1,2,3,4,5 * * * * *");
		CRON_STD_EQUAL("55/1 * * * * *", "55,56,57,58,59 * * * * *");
		CRON_STD_EQUAL("57,59 * * * * *", "57/2 * * * * *");
		CRON_STD_EQUAL("1,3,5 * * * * *", "1-6/2 * * * * *");
		CRON_STD_EQUAL("0,5,10,15 * * * * *", "0-15/5 * * * * *");
	}

	SECTION("standard: check minutes", "[std]")
	{
		CRON_STD_EQUAL("* */5 * * * *", "* 0,5,10,15,20,25,30,35,40,45,50,55 * * * *");
		CRON_STD_EQUAL("* 1/6 * * * *", "* 1,7,13,19,25,31,37,43,49,55 * * * *");
		CRON_STD_EQUAL("* 0/30 * * * *", "* 0,30 * * * *");
		CRON_STD_EQUAL("* 0-5 * * * *", "* 0,1,2,3,4,5 * * * *");
		CRON_STD_EQUAL("* 55/1 * * * *", "* 55,56,57,58,59 * * * *");
		CRON_STD_EQUAL("* 57,59 * * * *", "* 57/2 * * * *");
		CRON_STD_EQUAL("* 1,3,5 * * * *", "* 1-6/2 * * * *");
		CRON_STD_EQUAL("* 0,5,10,15 * * * *", "* 0-15/5 * * * *");
	}

	SECTION("standard: check hours", "[std]")
	{
		CRON_STD_EQUAL("* * */5 * * *", "* * 0,5,10,15,20 * * *");
		CRON_STD_EQUAL("* * 1/6 * * *", "* * 1,7,13,19 * * *");
		CRON_STD_EQUAL("* * 0/12 * * *", "* * 0,12 * * *");
		CRON_STD_EQUAL("* * 0-5 * * *", "* * 0,1,2,3,4,5 * * *");
		CRON_STD_EQUAL("* * 15/1 * * *", "* * 15,16,17,18,19,20,21,22,23 * * *");
		CRON_STD_EQUAL("* * 17,19,21,23 * * *", "* * 17/2 * * *");
		CRON_STD_EQUAL("* * 1,3,5 * * *", "* * 1-6/2 * * *");
		CRON_STD_EQUAL("* * 0,5,10,15 * * *", "* * 0-15/5 * * *");
	}

	SECTION("standard: check days of month", "[std]")
	{
		CRON_STD_EQUAL("* * * 1-31 * *", "* * * 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 * *");
		CRON_STD_EQUAL("* * * 1/5 * *", "* * * 1,6,11,16,21,26,31 * *");
		CRON_STD_EQUAL("* * * 1,11,21,31 * *", "* * * 1-31/10 * *");
		CRON_STD_EQUAL("* * * */5 * *", "* * * 1,6,11,16,21,26,31 * *");
	}

	SECTION("standard: check months", "[std]")
	{
		CRON_STD_EQUAL("* * * * 1,6,11 *", "* * * * 1/5 *");
		CRON_STD_EQUAL("* * * * 1-12 *", "* * * * 1,2,3,4,5,6,7,8,9,10,11,12 *");
		CRON_STD_EQUAL("* * * * 1-12/3 *", "* * * * 1,4,7,10 *");
		CRON_STD_EQUAL("* * * * */2 *", "* * * * 1,3,5,7,9,11 *");
		CRON_STD_EQUAL("* * * * 2/2 *", "* * * * 2,4,6,8,10,12 *");
		CRON_STD_EQUAL("* * * * 1 *", "* * * * JAN *");
		CRON_STD_EQUAL("* * * * 2 *", "* * * * FEB *");
		CRON_STD_EQUAL("* * * * 3 *", "* * * * mar *");
		CRON_STD_EQUAL("* * * * 4 *", "* * * * apr *");
		CRON_STD_EQUAL("* * * * 5 *", "* * * * may *");
		CRON_STD_EQUAL("* * * * 6 *", "* * * * Jun *");
		CRON_STD_EQUAL("* * * * 7 *", "* * * * Jul *");
		CRON_STD_EQUAL("* * * * 8 *", "* * * * auG *");
		CRON_STD_EQUAL("* * * * 9 *", "* * * * sEp *");
		CRON_STD_EQUAL("* * * * 10 *", "* * * * oCT *");
		CRON_STD_EQUAL("* * * * 11 *", "* * * * nOV *");
		CRON_STD_EQUAL("* * * * 12 *", "* * * * DEC *");
		CRON_STD_EQUAL("* * * * 1,FEB *", "* * * * JAN,2 *");
		CRON_STD_EQUAL("* * * * 1,6,11 *", "* * * * NOV,JUN,jan *");
		CRON_STD_EQUAL("* * * * 1,6,11 *", "* * * * jan,jun,nov *");
		CRON_STD_EQUAL("* * * * 1,6,11 *", "* * * * jun,jan,nov *");
		CRON_STD_EQUAL("* * * * JAN,FEB,MAR,APR,MAY,JUN,JUL,AUG,SEP,OCT,NOV,DEC *", "* * * * 1,2,3,4,5,6,7,8,9,10,11,12 *");
		CRON_STD_EQUAL("* * * * JUL,AUG,SEP,OCT,NOV,DEC,JAN,FEB,MAR,APR,MAY,JUN *", "* * * * 1,2,3,4,5,6,7,8,9,10,11,12 *");
	}

	SECTION("standard: check days of week", "[std]")
	{
		CRON_STD_EQUAL("* * * * * 0-6", "* * * * * 0,1,2,3,4,5,6");
		CRON_STD_EQUAL("* * * * * 0-6/2", "* * * * * 0,2,4,6");
		CRON_STD_EQUAL("* * * * * 0-6/3", "* * * * * 0,3,6");
		CRON_STD_EQUAL("* * * * * */3", "* * * * * 0,3,6");
		CRON_STD_EQUAL("* * * * * 1/3", "* * * * * 1,4");
		CRON_STD_EQUAL("* * * * * 0", "* * * * * SUN");
		CRON_STD_EQUAL("* * * * * 1", "* * * * * MON");
		CRON_STD_EQUAL("* * * * * 2", "* * * * * TUE");
		CRON_STD_EQUAL("* * * * * 3", "* * * * * WED");
		CRON_STD_EQUAL("* * * * * 4", "* * * * * THU");
		CRON_STD_EQUAL("* * * * * 5", "* * * * * FRI");
		CRON_STD_EQUAL("* * * * * 6", "* * * * * SAT");
		CRON_STD_EQUAL("* * * * * 0-6", "* * * * * SUN,MON,TUE,WED,THU,FRI,SAT");
		CRON_STD_EQUAL("* * * * * 0-6/2", "* * * * * SUN,TUE,THU,SAT");
		CRON_STD_EQUAL("* * * * * 0-6/3", "* * * * * SUN,WED,SAT");
		CRON_STD_EQUAL("* * * * * */3", "* * * * * SUN,WED,SAT");
		CRON_STD_EQUAL("* * * * * 1/3", "* * * * * MON,THU");
	}

	SECTION("standard: invalid seconds", "[std]")
	{
		CRON_EXPECT_EXCEPT("TEN * * * * *");
		CRON_EXPECT_EXCEPT("60 * * * * *");
		CRON_EXPECT_EXCEPT("-1 * * * * *");
		CRON_EXPECT_EXCEPT("0-60 * * * * *");
		CRON_EXPECT_EXCEPT("0-10-20 * * * * *");
		CRON_EXPECT_EXCEPT(", * * * * *");
		CRON_EXPECT_EXCEPT("0,,1 * * * * *");
		CRON_EXPECT_EXCEPT("0,1, * * * * *");
		CRON_EXPECT_EXCEPT(",0,1 * * * * *");
		CRON_EXPECT_EXCEPT("0/10/2 * * * * *");
		CRON_EXPECT_EXCEPT("/ * * * * *");
		CRON_EXPECT_EXCEPT("/2 * * * * *");
		CRON_EXPECT_EXCEPT("2/ * * * * *");
		CRON_EXPECT_EXCEPT("*/ * * * * *");
		CRON_EXPECT_EXCEPT("/* * * * * *");
		CRON_EXPECT_EXCEPT("-/ * * * * *");
		CRON_EXPECT_EXCEPT("/- * * * * *");
		CRON_EXPECT_EXCEPT("*-/ * * * * *");
		CRON_EXPECT_EXCEPT("-*/ * * * * *");
		CRON_EXPECT_EXCEPT("/-* * * * * *");
		CRON_EXPECT_EXCEPT("/*- * * * * *");
		CRON_EXPECT_EXCEPT("*2/ * * * * *");
		CRON_EXPECT_EXCEPT("/2* * * * * *");
		CRON_EXPECT_EXCEPT("-2/ * * * * *");
		CRON_EXPECT_EXCEPT("/2- * * * * *");
		CRON_EXPECT_EXCEPT("*2-/ * * * * *");
		CRON_EXPECT_EXCEPT("-2*/ * * * * *");
		CRON_EXPECT_EXCEPT("/2-* * * * * *");
		CRON_EXPECT_EXCEPT("/2*- * * * * *");
	}

	SECTION("standard: invalid minutes", "[std]")
	{
		CRON_EXPECT_EXCEPT("* TEN * * * *");
		CRON_EXPECT_EXCEPT("* 60 * * * *");
		CRON_EXPECT_EXCEPT("* -1 * * * *");
		CRON_EXPECT_EXCEPT("* 0-60 * * * *");
		CRON_EXPECT_EXCEPT("* 0-10-20 * * * *");
		CRON_EXPECT_EXCEPT("* , * * * *");
		CRON_EXPECT_EXCEPT("* 0,,1 * * * *");
		CRON_EXPECT_EXCEPT("* 0,1, * * * *");
		CRON_EXPECT_EXCEPT("* ,0,1 * * * *");
		CRON_EXPECT_EXCEPT("* 0/10/2 * * * *");
		CRON_EXPECT_EXCEPT("* / * * * *");
		CRON_EXPECT_EXCEPT("* /2 * * * *");
		CRON_EXPECT_EXCEPT("* 2/ * * * *");
		CRON_EXPECT_EXCEPT("* */ * * * *");
		CRON_EXPECT_EXCEPT("* /* * * * *");
		CRON_EXPECT_EXCEPT("* -/ * * * *");
		CRON_EXPECT_EXCEPT("* /- * * * *");
		CRON_EXPECT_EXCEPT("* *-/ * * * *");
		CRON_EXPECT_EXCEPT("* -*/ * * * *");
		CRON_EXPECT_EXCEPT("* /-* * * * *");
		CRON_EXPECT_EXCEPT("* /*- * * * *");
		CRON_EXPECT_EXCEPT("* *2/ * * * *");
		CRON_EXPECT_EXCEPT("* /2* * * * *");
		CRON_EXPECT_EXCEPT("* -2/ * * * *");
		CRON_EXPECT_EXCEPT("* /2- * * * *");
		CRON_EXPECT_EXCEPT("* *2-/ * * * *");
		CRON_EXPECT_EXCEPT("* -2*/ * * * *");
		CRON_EXPECT_EXCEPT("* /2-* * * * *");
		CRON_EXPECT_EXCEPT("* /2*- * * * *");
	}

	SECTION("standard: invalid hours", "[std]")
	{
		CRON_EXPECT_EXCEPT("* * TEN * * *");
		CRON_EXPECT_EXCEPT("* * 24 * * *");
		CRON_EXPECT_EXCEPT("* * -1 * * *");
		CRON_EXPECT_EXCEPT("* * 0-24 * * *");
		CRON_EXPECT_EXCEPT("* * 0-10-20 * * *");
		CRON_EXPECT_EXCEPT("* * , * * *");
		CRON_EXPECT_EXCEPT("* * 0,,1 * * *");
		CRON_EXPECT_EXCEPT("* * 0,1, * * *");
		CRON_EXPECT_EXCEPT("* * ,0,1 * * *");
		CRON_EXPECT_EXCEPT("* * 0/10/2 * * *");
		CRON_EXPECT_EXCEPT("* * / * * *");
		CRON_EXPECT_EXCEPT("* * /2 * * *");
		CRON_EXPECT_EXCEPT("* * 2/ * * *");
		CRON_EXPECT_EXCEPT("* * */ * * *");
		CRON_EXPECT_EXCEPT("* * /* * * *");
		CRON_EXPECT_EXCEPT("* * -/ * * *");
		CRON_EXPECT_EXCEPT("* * /- * * *");
		CRON_EXPECT_EXCEPT("* * *-/ * * *");
		CRON_EXPECT_EXCEPT("* * -*/ * * *");
		CRON_EXPECT_EXCEPT("* * /-* * * *");
		CRON_EXPECT_EXCEPT("* * /*- * * *");
		CRON_EXPECT_EXCEPT("* * *2/ * * *");
		CRON_EXPECT_EXCEPT("* * /2* * * *");
		CRON_EXPECT_EXCEPT("* * -2/ * * *");
		CRON_EXPECT_EXCEPT("* * /2- * * *");
		CRON_EXPECT_EXCEPT("* * *2-/ * * *");
		CRON_EXPECT_EXCEPT("* * -2*/ * * *");
		CRON_EXPECT_EXCEPT("* * /2-* * * *");
		CRON_EXPECT_EXCEPT("* * /2*- * * *");
	}

	SECTION("standard: invalid days of month", "[std]")
	{
		CRON_EXPECT_EXCEPT("* * * TEN * *");
		CRON_EXPECT_EXCEPT("* * * 32 * *");
		CRON_EXPECT_EXCEPT("* * * 0 * *");
		CRON_EXPECT_EXCEPT("* * * 0-32 * *");
		CRON_EXPECT_EXCEPT("* * * 0-10-20 * *");
		CRON_EXPECT_EXCEPT("* * * , * *");
		CRON_EXPECT_EXCEPT("* * * 0,,1 * *");
		CRON_EXPECT_EXCEPT("* * * 0,1, * *");
		CRON_EXPECT_EXCEPT("* * * ,0,1 * *");
		CRON_EXPECT_EXCEPT("* * * 0/10/2 * * *");
		CRON_EXPECT_EXCEPT("* * * / * *");
		CRON_EXPECT_EXCEPT("* * * /2 * *");
		CRON_EXPECT_EXCEPT("* * * 2/ * *");
		CRON_EXPECT_EXCEPT("* * * */ * *");
		CRON_EXPECT_EXCEPT("* * * /* * *");
		CRON_EXPECT_EXCEPT("* * * -/ * *");
		CRON_EXPECT_EXCEPT("* * * /- * *");
		CRON_EXPECT_EXCEPT("* * * *-/ * *");
		CRON_EXPECT_EXCEPT("* * * -*/ * *");
		CRON_EXPECT_EXCEPT("* * * /-* * *");
		CRON_EXPECT_EXCEPT("* * * /*- * *");
		CRON_EXPECT_EXCEPT("* * * *2/ * *");
		CRON_EXPECT_EXCEPT("* * * /2* * *");
		CRON_EXPECT_EXCEPT("* * * -2/ * *");
		CRON_EXPECT_EXCEPT("* * * /2- * *");
		CRON_EXPECT_EXCEPT("* * * *2-/ * *");
		CRON_EXPECT_EXCEPT("* * * -2*/ * *");
		CRON_EXPECT_EXCEPT("* * * /2-* * *");
		CRON_EXPECT_EXCEPT("* * * /2*- * *");
	}

	SECTION("standard: invalid months", "[std]")
	{
		CRON_EXPECT_EXCEPT("* * * * TEN *");
		CRON_EXPECT_EXCEPT("* * * * 13 *");
		CRON_EXPECT_EXCEPT("* * * * 0 *");
		CRON_EXPECT_EXCEPT("* * * * 0-13 *");
		CRON_EXPECT_EXCEPT("* * * * 0-10-11 *");
		CRON_EXPECT_EXCEPT("* * * * , *");
		CRON_EXPECT_EXCEPT("* * * * 0,,1 *");
		CRON_EXPECT_EXCEPT("* * * * 0,1, *");
		CRON_EXPECT_EXCEPT("* * * * ,0,1 *");
		CRON_EXPECT_EXCEPT("* * * * 0/10/2 *");
		CRON_EXPECT_EXCEPT("* * * * / *");
		CRON_EXPECT_EXCEPT("* * * * /2 *");
		CRON_EXPECT_EXCEPT("* * * * 2/ *");
		CRON_EXPECT_EXCEPT("* * * * */ *");
		CRON_EXPECT_EXCEPT("* * * * /* *");
		CRON_EXPECT_EXCEPT("* * * * -/ *");
		CRON_EXPECT_EXCEPT("* * * * /- *");
		CRON_EXPECT_EXCEPT("* * * * *-/ *");
		CRON_EXPECT_EXCEPT("* * * * -*/ *");
		CRON_EXPECT_EXCEPT("* * * * /-* *");
		CRON_EXPECT_EXCEPT("* * * * /*- *");
		CRON_EXPECT_EXCEPT("* * * * *2/ *");
		CRON_EXPECT_EXCEPT("* * * * /2* *");
		CRON_EXPECT_EXCEPT("* * * * -2/ *");
		CRON_EXPECT_EXCEPT("* * * * /2- *");
		CRON_EXPECT_EXCEPT("* * * * *2-/ *");
		CRON_EXPECT_EXCEPT("* * * * -2*/ *");
		CRON_EXPECT_EXCEPT("* * * * /2-* *");
		CRON_EXPECT_EXCEPT("* * * * /2*- *");
	}

	SECTION("standard: invalid days of week", "[std]")
	{
		CRON_EXPECT_EXCEPT("* * * * * TEN");
		CRON_EXPECT_EXCEPT("* * * * * 7");
		CRON_EXPECT_EXCEPT("* * * * * -1");
		CRON_EXPECT_EXCEPT("* * * * * -1-7");
		CRON_EXPECT_EXCEPT("* * * * * 0-5-6");
		CRON_EXPECT_EXCEPT("* * * * * ,");
		CRON_EXPECT_EXCEPT("* * * * * 0,,1");
		CRON_EXPECT_EXCEPT("* * * * * 0,1,");
		CRON_EXPECT_EXCEPT("* * * * * ,0,1");
		CRON_EXPECT_EXCEPT("* * * * * 0/2/2 *");
		CRON_EXPECT_EXCEPT("* * * * * /");
		CRON_EXPECT_EXCEPT("* * * * * /2");
		CRON_EXPECT_EXCEPT("* * * * * 2/");
		CRON_EXPECT_EXCEPT("* * * * * */");
		CRON_EXPECT_EXCEPT("* * * * * /*");
		CRON_EXPECT_EXCEPT("* * * * * -/");
		CRON_EXPECT_EXCEPT("* * * * * /-");
		CRON_EXPECT_EXCEPT("* * * * * *-/");
		CRON_EXPECT_EXCEPT("* * * * * -*/");
		CRON_EXPECT_EXCEPT("* * * * * /-*");
		CRON_EXPECT_EXCEPT("* * * * * /*-");
		CRON_EXPECT_EXCEPT("* * * * * *2/");
		CRON_EXPECT_EXCEPT("* * * * * /2*");
		CRON_EXPECT_EXCEPT("* * * * * -2/");
		CRON_EXPECT_EXCEPT("* * * * * /2-");
		CRON_EXPECT_EXCEPT("* * * * * *2-/");
		CRON_EXPECT_EXCEPT("* * * * * -2*/");
		CRON_EXPECT_EXCEPT("* * * * * /2-*");
		CRON_EXPECT_EXCEPT("* * * * * /2*-");
	}

	SECTION("next", "[std]")
	{
		check_next("*/15 * 1-4 * * *",  "2012-07-01 09:53:50", "2012-07-02 01:00:00");
		check_next("*/15 * 1-4 * * *",  "2012-07-01 09:53:00", "2012-07-02 01:00:00");
		check_next("0 */2 1-4 * * *",   "2012-07-01 09:00:00", "2012-07-02 01:00:00");
		check_next("* * * * * *",       "2012-07-01 09:00:00", "2012-07-01 09:00:01");
		check_next("* * * * * *",       "2012-12-01 09:00:58", "2012-12-01 09:00:59");
		check_next("* * * * *",         "2012-12-01 09:00:58", "2012-12-01 09:01:00");
		check_next("10 * * * * *",      "2012-12-01 09:42:09", "2012-12-01 09:42:10");
		check_next("11 * * * * *",      "2012-12-01 09:42:10", "2012-12-01 09:42:11");
		check_next("10 * * * * *",      "2012-12-01 09:42:10", "2012-12-01 09:43:10");
		check_next("10-15 * * * * *",   "2012-12-01 09:42:09", "2012-12-01 09:42:10");
		check_next("10-15 * * * * *",   "2012-12-01 21:42:14", "2012-12-01 21:42:15");
		check_next("0 * * * * *",       "2012-12-01 21:10:42", "2012-12-01 21:11:00");
		check_next("0 * * * * *",       "2012-12-01 21:11:00", "2012-12-01 21:12:00");
		check_next("0 11 * * * *",      "2012-12-01 21:10:42", "2012-12-01 21:11:00");
		check_next("11 * * * *",        "2012-12-01 21:10:42", "2012-12-01 21:11:00");
		check_next("0 10 * * * *",      "2012-12-01 21:11:00", "2012-12-01 22:10:00");
		check_next("0 0 * * * *",       "2012-09-30 11:01:00", "2012-09-30 12:00:00");
		check_next("0 0 * * * *",       "2012-09-30 12:00:00", "2012-09-30 13:00:00");
		check_next("0 0 * * * *",       "2012-09-10 23:01:00", "2012-09-11 00:00:00");
		check_next("0 0 * * * *",       "2012-09-11 00:00:00", "2012-09-11 01:00:00");
		check_next("0 0 0 * * *",       "2012-09-01 14:42:43", "2012-09-02 00:00:00");
		check_next("0 0 0 * * *",       "2012-09-02 00:00:00", "2012-09-03 00:00:00");
		check_next("* * * 10 * *",      "2012-10-09 15:12:42", "2012-10-10 00:00:00");
		check_next("* * * 10 * *",      "2012-10-11 15:12:42", "2012-11-10 00:00:00");
		check_next("0 0 0 * * *",       "2012-09-30 15:12:42", "2012-10-01 00:00:00");
		check_next("0 0 0 * * *",       "2012-10-01 00:00:00", "2012-10-02 00:00:00");
		check_next("0 0 0 * * *",       "2012-08-30 15:12:42", "2012-08-31 00:00:00");
		check_next("0 0 0 * * *",       "2012-08-31 00:00:00", "2012-09-01 00:00:00");
		check_next("0 0 0 * * *",       "2012-10-30 15:12:42", "2012-10-31 00:00:00");
		check_next("0 0 0 * * *",       "2012-10-31 00:00:00", "2012-11-01 00:00:00");
		check_next("0 0 0 1 * *",       "2012-10-30 15:12:42", "2012-11-01 00:00:00");
		check_next("0 0 0 1 * *",       "2012-11-01 00:00:00", "2012-12-01 00:00:00");
		check_next("0 0 0 1 * *",       "2010-12-31 15:12:42", "2011-01-01 00:00:00");
		check_next("0 0 0 1 * *",       "2011-01-01 00:00:00", "2011-02-01 00:00:00");
		check_next("0 0 0 31 * *",      "2011-10-30 15:12:42", "2011-10-31 00:00:00");
		check_next("0 0 0 1 * *",       "2011-10-30 15:12:42", "2011-11-01 00:00:00");
		check_next("* * * * * 2",       "2010-10-25 15:12:42", "2010-10-26 00:00:00");
		check_next("* * * * * 2",       "2010-10-20 15:12:42", "2010-10-26 00:00:00");
		check_next("* * * * * 2",       "2010-10-27 15:12:42", "2010-11-02 00:00:00");
		check_next("55 5 * * * *",      "2010-10-27 15:04:54", "2010-10-27 15:05:55");
		check_next("55 5 * * * *",      "2010-10-27 15:05:55", "2010-10-27 16:05:55");
		check_next("55 * 10 * * *",     "2010-10-27 09:04:54", "2010-10-27 10:00:55");
		check_next("55 * 10 * * *",     "2010-10-27 10:00:55", "2010-10-27 10:01:55");
		check_next("* 5 10 * * *",      "2010-10-27 09:04:55", "2010-10-27 10:05:00");
		check_next("* 5 10 * * *",      "2010-10-27 10:05:00", "2010-10-27 10:05:01");
		check_next("0 5 10 * * *",      "2010-10-27 10:05:00", "2010-10-28 10:05:00");
		check_next("5 10 * * *",        "2010-10-27 10:05:00", "2010-10-28 10:05:00");
		check_next("55 * * 3 * *",      "2010-10-02 10:05:54", "2010-10-03 00:00:55");
		check_next("55 * * 3 * *",      "2010-10-03 00:00:55", "2010-10-03 00:01:55");
		check_next("* * * 3 11 *",      "2010-10-02 14:42:55", "2010-11-03 00:00:00");
		check_next("* * * 3 11 *",      "2010-11-03 00:00:00", "2010-11-03 00:00:01");
		check_next("0 * * 3 11 *",      "2010-11-03 00:00:00", "2010-11-03 00:01:00");
		check_next("* * 3 11 *",        "2010-11-03 00:00:00", "2010-11-03 00:01:00");
		check_next("0 0 0 29 2 *",      "2007-02-10 14:42:55", "2008-02-29 00:00:00");
		check_next("0 0 0 29 2 *",      "2008-02-29 00:00:00", "2012-02-29 00:00:00");
		check_next("0 0 7 ? * MON-FRI", "2009-09-26 00:42:55", "2009-09-28 07:00:00");
		check_next("0 0 7 ? * MON-FRI", "2009-09-28 07:00:00", "2009-09-29 07:00:00");
		check_next("0 30 23 30 1/3 ?",  "2010-12-30 00:00:00", "2011-01-30 23:30:00");
		check_next("0 30 23 30 1/3 ?",  "2011-01-30 23:30:00", "2011-04-30 23:30:00");
		check_next("0 30 23 30 1/3 ?",  "2011-04-30 23:30:00", "2011-07-30 23:30:00");
		check_next("30 23 30 1/3 ?",    "2011-04-30 23:30:00", "2011-07-30 23:30:00");
	}

	SECTION("cronexpr", "[std]")
	{
		check_cron_conv("*/15 * 1-4 * * *");
		check_cron_conv("0 */2 1-4 * * *");
		check_cron_conv("* * * * * *");
		check_cron_conv("10 * * * * *");
		check_cron_conv("11 * * * * *");
		check_cron_conv("10-15 * * * * *");
		check_cron_conv("0 * * * * *");
		check_cron_conv("0 11 * * * *");
		check_cron_conv("0 10 * * * *");
		check_cron_conv("0 0 * * * *");
		check_cron_conv("0 0 0 * * *");
		check_cron_conv("* * * 10 * *");
		check_cron_conv("0 0 0 1 * *");
		check_cron_conv("0 0 0 31 * *");
		check_cron_conv("* * * * * 2");
		check_cron_conv("55 5 * * * *");
		check_cron_conv("55 * 10 * * *");
		check_cron_conv("* 5 10 * * *");
		check_cron_conv("55 * * 3 * *");
		check_cron_conv("* * * 3 11 *");
		check_cron_conv("0 0 0 29 2 *");
		check_cron_conv("0 0 7 ? * MON-FRI");
		check_cron_conv("0 30 23 30 1/3 ?");
	}

	SECTION("raw std")
	{
		auto cronexp = cron::make_cron("* 0/5 * * * ?");
		std::time_t now = 1647620612;
		std::time_t next = cron::cron_next(cronexp, now);
		CHECK ( kFormTimestamp(next) == "2022-03-18 16:25:00" );

		std::tm time = cron::utils::to_tm("2022-03-18 16:24:45");
		std::tm nexttm = cron::cron_next(cronexp, time);
		CHECK ( kFormTimestamp(nexttm) == kFormTimestamp(next) );
	}

	SECTION("KRON")
	{
		Kron::Job Job("* 0/5 * * * ? \t fstrim -a >/dev/null 2>&1  ");
		CHECK ( Job.Command() == "fstrim -a >/dev/null 2>&1" );
		auto next = Job.Next(1647620612);
		CHECK ( kFormTimestamp(next) == "2022-03-18 16:25:00" );

		KUTCTime Now("2022-03-18 16:24:45");
		auto Next = Job.Next(Now);
		CHECK ( Next.Format() == kFormTimestamp(next) );
	}
}


