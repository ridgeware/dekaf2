#include "catch.hpp"
#include <dekaf2/khttp_header.h>
#include <dekaf2/khttpclient.h>
#include <vector>

using namespace dekaf2;

namespace {

void testHeader(const KHTTPHeader& header)
{
}

} // end of anonymous namespace

TEST_CASE("KHTTPHeader")
{
	SECTION("Construction and Compare")
	{
		KHTTPHeader Header1(KHTTPHeader::AUTHORIZATION);
		KHTTPHeader Header2("auTHoriZation");
		KHTTPHeader Header3(KHTTPHeader::CONTENT_TYPE);
		KHTTPHeader Header4(Header3);
		KHTTPHeader Header5(std::move(Header4));
		KHTTPHeader Header6;
		Header6 = Header5;
		KHTTPHeader Header7;
		Header7 = std::move(Header6);

		KHTTPHeader Header;
		Header = "abc";
		Header = "abc"_ks;
		Header = "abc"_ksv;
		Header = "abc"_ksz;
		Header = std::string("abc");

		testHeader(KHTTPHeader::CONTENT_TYPE);
		testHeader("abc");
		testHeader("abc"_ks);
		testHeader("abc"_ksv);
		testHeader("abc"_ksz);
		testHeader(std::string("abc"));

		CHECK ( Header1 == Header2 );
		CHECK ( Header1 != Header3 );
		CHECK ( Header1.Hash() == Header2.Hash() );
		CHECK ( Header1.Hash() != Header3.Hash() );
		CHECK ( Header1 == "Authorization"     );
		CHECK ( Header1 == "Authorization"_ks  );
		CHECK ( Header1 == "Authorization"_ksv );
		CHECK ( Header2 == "Authorization"_ksz );
		CHECK ( Header1 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header2 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header1 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( Header2 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( KHTTPHeader::CONTENT_TYPE != Header2 );
		CHECK ( Header1.Serialize() == "Authorization" );
		CHECK ( Header2.Serialize() == "Authorization" );

		Header1 = KHTTPHeader("non-standard-Header");
		CHECK ( Header1 == KHTTPHeader("non-stAnDard-header") );
		CHECK ( Header1.Serialize() == "non-standard-Header"_ks);
		CHECK ( Header1 == "non-standard-Header" );
		CHECK ( Header1 != KHTTPHeader::X_FORWARDED_FOR );

		KHTTPClient Client;
		Client.AddHeader("key", "value");
		Client.AddHeader("key"_ks, "value"_ks);
		Client.AddHeader("key"_ksv, "value"_ksv);
		Client.AddHeader("key"_ksz, "value"_ksz);
	}
}
