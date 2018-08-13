#include "catch.hpp"

#include <dekaf2/khtmlentities.h>

using namespace dekaf2;

TEST_CASE("KHTMLEntities")
{
	SECTION("basic lookup")
	{
		uint32_t cp1, cp2;
		
		CHECK ( KHTMLEntity::FromNamedEntity("real", cp1, cp2) );
		CHECK ( cp1 == 0x211c );
		CHECK ( cp2 == 0 );

		CHECK ( KHTMLEntity::FromNamedEntity("nLtv", cp1, cp2) );
		CHECK ( cp1 == 0x226a );
		CHECK ( cp2 == 0x0338 );
	}

	SECTION("Encode")
	{
		KString sIn = "This <i>is</i> a test™!";
		CHECK ( KHTMLEntity::Encode(sIn) == "This &lt;i&gt;is&lt;/i&gt; a test™!" );
	}

	SECTION("Decode")
	{
		KString sIn = "This &lt;i&gt;is&lt;/i&gt;&#32;a&#x20;test&trade;!";
		CHECK ( KHTMLEntity::Decode(sIn) == "This <i>is</i> a test™!" );
	}

	SECTION("Decode lazy")
	{
		KString sIn = "This &lt;i&gt;is&lt/i&gt&#32a&#x20test&trade!";
		CHECK ( KHTMLEntity::Decode(sIn) == "This <i>is</i> a test™!" );
	}

	SECTION("Decode failed name")
	{
		KString sIn = "This &luyt;i&gat;is&lOOt;/i&greaeet; a test&trassssde;!";
		CHECK ( KHTMLEntity::Decode(sIn) == "This &luyt;i&gat;is&lOOt;/i&greaeet; a test&trassssde;!" );
	}

	SECTION("Decode failed number")
	{
		CHECK ( KHTMLEntity::Decode("A&#rt; ds"_ksv)   == "A&#rt; ds" );
		CHECK ( KHTMLEntity::Decode("A&#rt ds"_ksv)    == "A&#rt ds" );
		CHECK ( KHTMLEntity::Decode("A&#xrt; ds"_ksv)  == "A&#xrt; ds" );
		CHECK ( KHTMLEntity::Decode("A&#Xt ds"_ksv)    == "A&#Xt ds" );
		CHECK ( KHTMLEntity::Decode("A&#33rt ds"_ksv)  == "A!rt ds" );
		CHECK ( KHTMLEntity::Decode("A&#x21rt ds"_ksv) == "A!rt ds" );
		CHECK ( KHTMLEntity::Decode("A&#X21rt ds"_ksv) == "A!rt ds" );
	}

	SECTION("DecodeOne")
	{
		CHECK ( KHTMLEntity::DecodeOne("&gt;") == ">" );
		CHECK ( KHTMLEntity::DecodeOne("&gt")  == ">" );
		CHECK ( KHTMLEntity::DecodeOne("gt;")  == ">" );
		CHECK ( KHTMLEntity::DecodeOne("gt")   == ">" );
	}

}
