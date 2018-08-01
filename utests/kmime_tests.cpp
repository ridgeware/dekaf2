#include "catch.hpp"

#include <dekaf2/kmime.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KMIME")
{
	SECTION("by extension")
	{
		KMIME a;

		CHECK( a.ByExtension("test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("/folder/test.dir/test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".html") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension(".HTML") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension("css") );
		CHECK( a == KMIME::CSS );

	}
}
