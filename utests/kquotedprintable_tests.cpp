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
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\nse keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe." }
	};

	std::vector<std::vector<KString>> dottests = {
		{ "", "", "" },
		{ "This is a test.", "This is a test.", "This is a test." },
		{ ".This is a test\n.", "=2EThis is a test\n=2E", "..This is a test\n.." },
		{ "Hätten Hüte ein ß im Namen, wären sie möglicherwei.se keine Hüte mehr, sondern Hüße.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\n=2Ese keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\n..se keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe."
		},
		{ "Hätten Hüte ein ß im Namen, wären sie möglicherwei..se keine Hüte mehr, sondern Hüße.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\n=2E.se keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\n...se keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe."
		}
	};

	std::vector<std::vector<KString>> errortests = {
		{ "", "" },
		{ "This is a test.=", "This is a test.=" },
		{ "This is a test.=b", "This is a test.=b" },
		{ "This is a test.=v", "This is a test.=v" },
		{ "This is a test.=B", "This is a test.=B" },
		{ "This is a test.=bx", "This is a test.=bx" },
		{ "This =Ais a test.", "This =Ais a test." },
		{ "This =ais a test.", "This =ais a test." },
		{ "This =is a test.", "This =is a test." },
		{ "This ==is a test.", "This ==is a test." },
		{ "This =i.s a test.", "This =i=2Es a test." },
		{ "This =ao.s a test.", "This =ao=2Es a test." },
		{ "Hätten Hüte ein ß im Namen, wären sie möglicherweise keine Hüte mehr, sondern Hüße.",
			"H=C3=A4tten H=C3=BCte ein =C3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\nse keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe." },
		{ "Hätten Hüte ein ß im Namen, wären sie möglicherweise keine Hüte mehr, sondern Hüße.",
			"H=C3=A4tten H=C3=bcte ein =c3=9F im Namen, w=C3=A4ren sie m=C3=B6glicherwei=\r\n.se keine H=C3=BCte mehr, sondern H=C3=BC=C3=9Fe." }
	};

	SECTION("Encode/Decode all characters")
	{
		KString sEncoded = KQuotedPrintable::Encode(sData);
		KString sDecoded = KQuotedPrintable::Decode(sEncoded);
		CHECK ( sDecoded == sData );
	}

	SECTION("Proper line wrapping")
	{
		for (const auto& it : tests)
		{
			KString sEncoded = KQuotedPrintable::Encode(it[0]);
			CHECK ( sEncoded == it[1] );
			KString sDecoded = KQuotedPrintable::Decode(it[1]);
			CHECK ( sDecoded == it[0] );
		}
	}

	SECTION("Dot stuffing")
	{
		for (const auto& it : dottests)
		{
			KString sEncoded = KQuotedPrintable::Encode(it[0]);
			CHECK ( sEncoded == it[1] );
			KString sDecoded = KQuotedPrintable::Decode(it[1]);
			CHECK ( sDecoded == it[0] );
				 	sDecoded = KQuotedPrintable::Decode(it[2]);
			CHECK ( sDecoded == it[0] );
		}
	}

	SECTION("Error recovery")
	{
		for (const auto& it : errortests)
		{
			KString sDecoded = KQuotedPrintable::Decode(it[1]);
			CHECK ( sDecoded == it[0] );
		}
	}

}
