#include "catch.hpp"
#include <dekaf2/http/protocol/khttp_method.h>

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

	SECTION("ALL is a route wildcard, not a request method")
	{
		CHECK ( KHTTPMethod::Parse("ALL") == KHTTPMethod::INVALID );
		CHECK ( KHTTPMethod("ALL") == KHTTPMethod::INVALID );
		CHECK ( KHTTPMethod(KHTTPMethod::ALL).Serialize() == "" );
		CHECK_FALSE ( KHTTPMethod(KHTTPMethod::ALL).empty() );
	}
}
