#include "catch.hpp"
#include <dekaf2/khttp_method.h>

using namespace dekaf2;

namespace {

} // end of anonymous namespace

TEST_CASE("KHTTPMethod")
{
	SECTION("Format")
	{
		KHTTPMethod M = KHTTPMethod::POST;
		auto sMethod = kFormat("{}", M);
		CHECK ( sMethod == "POST");
	}
}
