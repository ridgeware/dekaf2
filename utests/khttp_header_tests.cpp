#include "catch.hpp"
#include <dekaf2/khttp_header.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KHTTPHeader")
{
	SECTION("Construction and Compare")
	{
		KHTTPHeader Header1(KHTTPHeader::AUTHORIZATION);
		KHTTPHeader Header2("auTHoriZation");
		KHTTPHeader Header3(KHTTPHeader::CONTENT_TYPE);

		CHECK ( Header1 == Header2 );
		CHECK ( Header1 != Header3 );
		CHECK ( Header1.Hash() == Header2.Hash() );
		CHECK ( Header1.Hash() != Header3.Hash() );
		CHECK ( Header1 == "AuthoriZation"_ksv );
		CHECK ( Header2 == "AuthoriZation"_ksv );
		CHECK ( Header1 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header2 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header1 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( Header2 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( KHTTPHeader::CONTENT_TYPE != Header2 );
		CHECK ( Header1.Serialize() == "Authorization" );
		CHECK ( Header2.Serialize() == "Authorization" );

		Header1 = "non-standard-Header";
		CHECK ( Header1 == "non-standard-header"_ksv );
		CHECK ( Header1.Serialize() == "non-standard-Header"_ksv );
		CHECK ( Header1 != KHTTPHeader::X_FORWARDED_FOR );
	}
}
