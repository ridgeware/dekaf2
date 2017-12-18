#include "catch.hpp"

#include <dekaf2/kbase64.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KBase64")
{
	KString sData;
	for (int i = 0; i < 256; ++i)
	{
		sData += static_cast<KString::value_type>(i);
	}

	std::vector<std::vector<KString>> tests = {
	    { "", "" },
	    { "T", "VA==" },
	    { "Th", "VGg=" },
	    { "Thi", "VGhp" },
	    { "This is a test.", "VGhpcyBpcyBhIHRlc3Qu" },
	    { "his is a test.", "aGlzIGlzIGEgdGVzdC4=" },
	    { "is is a test.", "aXMgaXMgYSB0ZXN0Lg==" },
	    { "s is a test.", "cyBpcyBhIHRlc3Qu" },
	    { " is a test.", "IGlzIGEgdGVzdC4=" },
	    { "is a test.", "aXMgYSB0ZXN0Lg==" },
	    { "And here we go with a really long line to verify automatic line break insertion ==",
	      "QW5kIGhlcmUgd2UgZ28gd2l0aCBhIHJlYWxseSBsb25nIGxpbmUgdG8gdmVyaWZ5IGF1dG9tYXRp\n"
	      "YyBsaW5lIGJyZWFrIGluc2VydGlvbiA9PQ==" }
	};

	SECTION("Encode/Decode all characters")
	{
		KString sEncoded = KBase64::Encode(sData);
		KString sDecoded = KBase64::Decode(sEncoded);
		CHECK ( sDecoded == sData );
	}

	SECTION("Proper padding")
	{
		for (const auto& it : tests)
		{
			KString sEncoded = KBase64::Encode(it[0]);
			CHECK ( sEncoded == it[1] );
			KString sDecoded = KBase64::Decode(it[1]);
			CHECK ( sDecoded == it[0] );
		}
	}
}
