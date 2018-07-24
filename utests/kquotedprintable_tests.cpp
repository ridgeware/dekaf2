#include "catch.hpp"

#include <dekaf2/kquotedprintable.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KQuotedPrintable")
{
	KString sData;
	for (int i = 255; i >= 0; --i)
	{
		sData += static_cast<KString::value_type>(i);
	}

	std::vector<std::vector<KString>> tests = {
	    { "", "" },
	    { "This is a test.", "This is a test." },
		{ "Hätten Hüte ein ß im Namen, wären sie möglicherweise keine Hüte mehr, sondern Hüße.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\nse keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe." }
	};

	SECTION("Encode/Decode all characters")
	{
		KString sEncoded = KQuotedPrintable::Encode(sData);
		KString sDecoded = KQuotedPrintable::Decode(sEncoded);
		CHECK ( sDecoded == sData );
	}

	SECTION("Proper padding")
	{
		for (const auto& it : tests)
		{
			KString sEncoded = KQuotedPrintable::Encode(it[0]);
			CHECK ( sEncoded == it[1] );
			KString sDecoded = KQuotedPrintable::Decode(it[1]);
			CHECK ( sDecoded == it[0] );
		}
	}
}
