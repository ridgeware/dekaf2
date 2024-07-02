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
		CHECK ( Header1 == "authorization"     );
		CHECK ( Header1 == "authorization"_ks  );
		CHECK ( Header1 == "authorization"_ksv );
		CHECK ( Header2 == "authorization"_ksz );
		CHECK ( Header1 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header2 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header1 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( Header2 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( KHTTPHeader::CONTENT_TYPE != Header2 );
		CHECK ( Header1.Serialize() == "authorization" );
		CHECK ( Header2.Serialize() == "authorization" );

		Header1 = KHTTPHeader("non-standard-Header");
		CHECK ( Header1 == KHTTPHeader("non-stAnDard-header") );
		CHECK ( Header1.Serialize() == "non-standard-header"_ks);
		CHECK ( Header1 == "non-standard-header" );
		CHECK ( Header1 != KHTTPHeader::X_FORWARDED_FOR );

		KHTTPClient Client;
		Client.AddHeader("key", "value");
		Client.AddHeader("key"_ks, "value"_ks);
		Client.AddHeader("key"_ksv, "value"_ksv);
		Client.AddHeader("key"_ksz, "value"_ksz);
	}

	SECTION("QualityValue")
	{
		KStringView sContent = "text/css,  text/html  ;q=0.9  ,text/*;q=0.81s,x-none/*;q=0.8 , zzz/xx ;q=8, zzz/yy ;q=0*12";
		auto ContentTypes    = sContent.Split();

		CHECK ( ContentTypes.size() == 6);

		if (ContentTypes.size() == 6)
		{
			auto iQuality = KHTTPHeader::GetQualityValue(ContentTypes[0], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[0] == "text/css" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[1], true);
			CHECK ( iQuality == 90 );
			CHECK ( ContentTypes[1] == "text/html" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[2], true);
			CHECK ( iQuality == 81 );
			CHECK ( ContentTypes[2] == "text/*" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[3], true);
			CHECK ( iQuality == 80 );
			CHECK ( ContentTypes[3] == "x-none/*" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[4], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[4] == "zzz/xx" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[5], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[5] == "zzz/yy" );
		}
	}

	SECTION("SortedQualityValue")
	{
		KStringView sContent = "text/css,  text/html  ;q=0.5  ,text/*;q=0.81s,x-none/*;q=0.84 , zzz/xx ;q=8, zzz/yy ;q=0*12";
		auto ContentTypes    = KHTTPHeader::Split(sContent);

		CHECK ( ContentTypes.size() == 6);

		if (ContentTypes.size() == 6)
		{
			CHECK ( ContentTypes[0].iQuality == 100 );
			CHECK ( ContentTypes[0].sValue == "text/css" );
			CHECK ( ContentTypes[1].iQuality == 100 );
			CHECK ( ContentTypes[1].sValue == "zzz/xx" );
			CHECK ( ContentTypes[2].iQuality == 100 );
			CHECK ( ContentTypes[2].sValue == "zzz/yy" );
			CHECK ( ContentTypes[3].iQuality == 84 );
			CHECK ( ContentTypes[3].sValue == "x-none/*" );
			CHECK ( ContentTypes[4].iQuality == 81 );
			CHECK ( ContentTypes[4].sValue == "text/*" );
			CHECK ( ContentTypes[5].iQuality == 50 );
			CHECK ( ContentTypes[5].sValue == "text/html" );
		}
	}

	SECTION("Format")
	{
		KHTTPHeader H = KHTTPHeader::CONTENT_TYPE;
		auto sHeader = kFormat("{}: none", H);
		CHECK ( sHeader == "content-type: none");
	}
}
