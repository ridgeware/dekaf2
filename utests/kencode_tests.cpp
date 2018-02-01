#include "catch.hpp"

#include <dekaf2/kencode.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KEncoding") {

	SECTION("KEnc / KDec")
	{
		SECTION("Base64")
		{
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

			for (const auto& it : tests)
			{
				KString sEncoded = KEnc::Base64(it[0]);
				CHECK ( sEncoded == it[1] );
				KString sDecoded = KDec::Base64(it[1]);
				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::Base64InPlace(sEncoded);
				CHECK ( sEncoded == it[1] );
				KDec::Base64InPlace(sEncoded);
				CHECK ( sEncoded == it[0] );
			}

		}

		SECTION("URL")
		{
			KString sEncoded = KEnc::URL("test here");
			CHECK ( sEncoded == "test+here");
			CHECK ( KDec::URL(sEncoded) == "test here");

			sEncoded = "test here";
			KEnc::URLInPlace(sEncoded);
			CHECK ( sEncoded == "test+here");
			KDec::URLInPlace(sEncoded);
			CHECK ( sEncoded == "test here");

			sEncoded = KEnc::URL("test here", URIPart::Path);
			CHECK ( sEncoded == "test%20here");
			CHECK ( KDec::URL(sEncoded) == "test here");

			sEncoded = "test here";
			KEnc::URLInPlace(sEncoded, URIPart::Path);
			CHECK ( sEncoded == "test%20here");
			KDec::URLInPlace(sEncoded);
			CHECK ( sEncoded == "test here");

			sEncoded = KEnc::URL("test here", URIPart::Path);
			CHECK ( sEncoded == "test%20here");
			CHECK ( KDec::URL(sEncoded, URIPart::Path) == "test here");

			sEncoded = "test here";
			KEnc::URLInPlace(sEncoded, URIPart::Path);
			CHECK ( sEncoded == "test%20here");
			KDec::URLInPlace(sEncoded, URIPart::Path);
			CHECK ( sEncoded == "test here");
		}

		SECTION("HTML")
		{
			std::vector<std::vector<KString>> tests = {
			    { "", "" },
			    { "abcdefghijklmnopqrstuvwxyz .,:;/", "abcdefghijklmnopqrstuvwxyz .,:;/" },
			    { "test<ab>&", "test&lt;ab&gt;&amp;" }
			};

			for (const auto& it : tests)
			{
				KString sEncoded = KEnc::HTML(it[0]);
				CHECK ( sEncoded == it[1] );
				KString sDecoded = KDec::HTML(it[1]);
//				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::HTMLInPlace(sEncoded);
				CHECK ( sEncoded == it[1] );
				KDec::HTMLInPlace(sEncoded);
//				CHECK ( sEncoded == it[0] );
			}

		}

	}

}
