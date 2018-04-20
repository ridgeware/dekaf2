#include "catch.hpp"

#include <dekaf2/khtmlentities.h>

using namespace dekaf2;

TEST_CASE("KHTMLEntities")
{
	SECTION("basic lookup")
	{
		uint32_t cp1, cp2;
		
		CHECK ( kNamedEntity("real", cp1, cp2) );
		CHECK ( cp1 == 0x211c );
		CHECK ( cp2 == 0 );

		CHECK ( kNamedEntity("nLtv", cp1, cp2) );
		CHECK ( cp1 == 0x226a );
		CHECK ( cp2 == 0x0338 );
	}

}
