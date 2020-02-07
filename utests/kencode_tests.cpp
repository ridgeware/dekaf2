#include "catch.hpp"

#include <dekaf2/kencode.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KEncoding") {

	SECTION("KEnc / KDec")
	{
		SECTION("AllHex")
		{
			for (uint16_t i = 0; i < 256; ++i)
			{
				char ch = static_cast<char>(i);
				KStringView s(&ch, 1);
				KString sEncoded = KEnc::Hex(s);
				CHECK ( sEncoded == KString::to_hexstring(i, true, false) );
				KString sDecoded = KDec::Hex(sEncoded);
				CHECK ( sDecoded == s );
			}
		}

		SECTION("Hex")
		{
			KString s = "café garçon dîner";
			KString sEncoded = KEnc::Hex(s);
			CHECK ( sEncoded == "636166c3a920676172c3a76f6e2064c3ae6e6572" );
			KString sDecoded = KDec::Hex(sEncoded);
			CHECK ( sDecoded == s );
		}

		SECTION("Hex high")
		{
			KString s = "随着确诊人数持续激增";
			KString sEncoded = KEnc::Hex(s);
			CHECK ( sEncoded == "e99a8fe79d80e7a1aee8af8ae4babae695b0e68c81e7bbade6bf80e5a29e" );
			KString sDecoded = KDec::Hex(sEncoded);
			CHECK ( sDecoded == s );
		}

		SECTION("Base64")
		{
			std::vector<std::vector<KString>> tests = {
			    { "", "" },
			    { "T", "VA==" },
			    { "Th", "VGg=" },
			    { "Thi", "VGhp" },
			    { "随着确诊人数持续激增", "6ZqP552A56Gu6K+K5Lq65pWw5oyB57ut5r+A5aKe" },
				{ "café garçon dîner", "Y2Fmw6kgZ2Fyw6dvbiBkw65uZXI=" },
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

		SECTION("Base64Url")
		{
			std::vector<std::vector<KString>> tests = {
			    { "", "" },
			    { "T", "VA" },
			    { "Th", "VGg" },
			    { "Thi", "VGhp" },
			    { "随着确诊人数持续激增", "6ZqP552A56Gu6K-K5Lq65pWw5oyB57ut5r-A5aKe" },
				{ "café garçon dîner", "Y2Fmw6kgZ2Fyw6dvbiBkw65uZXI" },
			    { "This is a test.", "VGhpcyBpcyBhIHRlc3Qu" },
			    { "his is a test.", "aGlzIGlzIGEgdGVzdC4" },
			    { "is is a test.", "aXMgaXMgYSB0ZXN0Lg" },
			    { "s is a test.", "cyBpcyBhIHRlc3Qu" },
			    { " is a test.", "IGlzIGEgdGVzdC4" },
			    { "is a test.", "aXMgYSB0ZXN0Lg" },
			    { "And here we go with a really long line to verify automatic line break insertion ==",
			      "QW5kIGhlcmUgd2UgZ28gd2l0aCBhIHJlYWxseSBsb25nIGxpbmUgdG8gdmVyaWZ5IGF1dG9tYXRp"
			      "YyBsaW5lIGJyZWFrIGluc2VydGlvbiA9PQ" }
			};

			for (const auto& it : tests)
			{
				KString sEncoded = KEnc::Base64Url(it[0]);
				CHECK ( sEncoded == it[1] );
				KString sDecoded = KDec::Base64Url(it[1]);
				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::Base64UrlInPlace(sEncoded);
				CHECK ( sEncoded == it[1] );
				KDec::Base64UrlInPlace(sEncoded);
				CHECK ( sEncoded == it[0] );
			}
		}

		SECTION("QuotedPrintable")
		{
			std::vector<std::vector<KString>> tests = {
				{ "café garçon dîner", "caf=C3=A9 gar=C3=A7on d=C3=AEner" },
			    { "And here we go with a really long line to verify automatic line break insertion ==",
			      "And here we go with a really long line to verify automatic line break inser=\r\n"
				  "tion =3D=3D" }
			};

			for (const auto& it : tests)
			{
				KString sEncoded = KEnc::QuotedPrintable(it[0], false);
				CHECK ( sEncoded == it[1] );
				KString sDecoded = KDec::QuotedPrintable(it[1]);
				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::QuotedPrintableInPlace(sEncoded, false);
				CHECK ( sEncoded == it[1] );
				KDec::QuotedPrintableInPlace(sEncoded);
				CHECK ( sEncoded == it[0] );
			}

			std::vector<std::vector<KString>> mtests = {
				{ "café garçon dîner", "=?UTF-8?Q?caf=C3=A9=20gar=C3=A7on=20d=C3=AEner?=" },
			    { "And here we go with a really long line to verify automatic line break insertion ==",
			      "=?UTF-8?Q?And=20here=20we=20go=20with=20a=20really=20long=20?=\r\n"
				  " =?UTF-8?Q?line=20to=20verify=20automatic=20line=20break=20insertion=20=3D?=\r\n"
				  " =?UTF-8?Q?=3D?=" }
			};

			for (const auto& it : mtests)
			{
				KString sEncoded = KEnc::QuotedPrintable(it[0], true);
				CHECK ( sEncoded == it[1] );
				// we do not support round trip on mail header encoding
			}

			for (const auto& it : mtests)
			{
				KString sEncoded = it[0];
				KEnc::QuotedPrintableInPlace(sEncoded, true);
				CHECK ( sEncoded == it[1] );
				// we do not support round trip on mail header encoding
			}
		}

		SECTION("URL")
		{
			KString sEncoded = KEnc::URL("café garçon dîner");
			CHECK ( sEncoded == "caf%C3%A9+gar%C3%A7on+d%C3%AEner");
			CHECK ( KDec::URL(sEncoded) == "café garçon dîner");

			sEncoded = KEnc::URL("随着确诊人数持续激增");
			CHECK ( sEncoded == "%E9%9A%8F%E7%9D%80%E7%A1%AE%E8%AF%8A%E4%BA%BA%E6%95%B0%E6%8C%81%E7%BB%AD%E6%BF%80%E5%A2%9E");
			CHECK ( KDec::URL(sEncoded) == "随着确诊人数持续激增");

			sEncoded = KEnc::URL("test here");
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
				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::HTMLInPlace(sEncoded);
				CHECK ( sEncoded == it[1] );
				KDec::HTMLInPlace(sEncoded);
				CHECK ( sEncoded == it[0] );
			}
		}

		SECTION("XML")
		{
			std::vector<std::vector<KString>> tests = {
			    { "", "" },
			    { "abcdefghijklmnopqrstuvwxyz .,:;/", "abcdefghijklmnopqrstuvwxyz .,:;/" },
			    { "test<ab>&", "test&lt;ab&gt;&amp;" }
			};

			for (const auto& it : tests)
			{
				KString sEncoded = KEnc::XML(it[0]);
				CHECK ( sEncoded == it[1] );
				KString sDecoded = KDec::XML(it[1]);
				CHECK ( sDecoded == it[0] );
			}

			for (const auto& it : tests)
			{
				KString sEncoded = it[0];
				KEnc::XMLInPlace(sEncoded);
				CHECK ( sEncoded == it[1] );
				KDec::XMLInPlace(sEncoded);
				CHECK ( sEncoded == it[0] );
			}
		}

	}

}
