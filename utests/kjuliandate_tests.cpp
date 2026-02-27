#include "catch.hpp"

#include <dekaf2/kjuliandate.h>

using namespace dekaf2;

TEST_CASE("KJulianDate")
{
	SECTION("KJulianDate")
	{
		KJulianDate jtp(2457354.310832);
		KUnixTime utp(jtp);

		CHECK ( utp.to_string() == "2015-11-27 19:27:35" );
		CHECK ( utp.subseconds().milliseconds().count() == 884 );
#if DEKAF2_HAS_NANOSECONDS_SYS_CLOCK
		CHECK ( utp.subseconds().microseconds().count() == 884788 );
#else
		CHECK ( utp.subseconds().microseconds().count() == 884789 );
#endif

		jtp = KJulianDate(utp);

		CHECK ( jtp.to_string() == "2457354.310832" );
		CHECK ( jtp.to_double() ==  2457354.310832  );
	}
}
