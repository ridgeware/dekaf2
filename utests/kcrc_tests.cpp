#include "catch.hpp"

#include <dekaf2/kcrc.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCRC")
{
	SECTION("KCRC32")
	{
		KCRC32 crc1("abcdefg");
		crc1 += "hijklm";
		crc1("nopq");
		crc1.Update("rstuvw");
		KCRC32 crc2("abcdefghijklmnopqrstuvw");
		CHECK( crc2() == crc1() );
		CHECK( KString::to_string(crc1()) == "955461994"_ks );
		crc1.clear();
		crc2.clear();
		CHECK( crc2() == crc1() );
	}
}
