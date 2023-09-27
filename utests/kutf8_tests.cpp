#include "catch.hpp"

#include <dekaf2/kutf8.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("UTF8") {

	SECTION("std::string")
	{
		using stype = std::string;
		// source, target
		std::vector<std::vector<stype>> stest
		{
			{ "abcdefghijklmnopqrstuvwxyz.,;:", "abcdefghijklmnopqrstuvwxyz.,;:" },
			{ "testäöütest", "testÃ¤Ã¶Ã¼test" }
		};

		for (auto& it : stest)
		{
			stype sTest;
			Unicode::ToUTF8(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			Unicode::FromUTF8(sTest, [&sOut](uint32_t ch)
			{
				sOut += static_cast<std::string::value_type>(ch);
				return true;
			});
			CHECK ( sOut == it[0] );
		}
	}

	SECTION("KString")
	{
		using stype = KString;
		// source, target
		std::vector<std::vector<stype>> stest
		{
			{ "abcdefghijklmnopqrstuvwxyz.,;:", "abcdefghijklmnopqrstuvwxyz.,;:" },
			{ "testäöütest", "testÃ¤Ã¶Ã¼test" }
		};

		for (auto& it : stest)
		{
			stype sTest;
			Unicode::ToUTF8(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			Unicode::FromUTF8(sTest, [&sOut](uint32_t ch)
			{
				sOut += static_cast<std::string::value_type>(ch);
				return true;
			});
			CHECK ( sOut == it[0] );
		}
	}

	SECTION("ValidUTF8")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK( Unicode::ValidUTF8(sStr) == true );
		CHECK( Unicode::ValidUTF8(sStr.begin()+3, sStr.end()) == true );
		sStr[6] = 'a';
		CHECK( Unicode::ValidUTF8(sStr) == false );
	}

	SECTION("CountUTF8")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(              sStr.size() == 47 );
		CHECK( Unicode::CountUTF8(sStr) == 27 );
		CHECK( Unicode::CountUTF8(sStr.begin()+2, sStr.end()) == 25 );
	}

	SECTION("LeftUTF8")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                sStr.size() == 47       );
		CHECK( Unicode::LeftUTF8(sStr, 7) == "testäöü");
	}

	SECTION("RightUTF8")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                  sStr.size() == 47           );
		CHECK( Unicode::RightUTF8(sStr, 10) == "c中文Русский");
	}

	SECTION("MidUTF8")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                  sStr.size() == 47         );
		CHECK( Unicode::MidUTF8(sStr, 8, 7) == "test日本語");
	}

	SECTION("UTF8ToUTF16Bytes")
	{
		KString sUTF8("testäöü test日本語abc中文Русский");
		CHECK(                 sUTF8.size() == 47     );
		auto sBytes = Unicode::special::UTF8ToUTF16Bytes(sUTF8);
		CHECK(                 sBytes.size() == 54    );
//		CHECK( sBytes == "" );
		auto sUTF8New = Unicode::special::UTF16BytesToUTF8(sBytes);
		CHECK(                 sUTF8New.size() == 47  );
		CHECK(                 sUTF8New == sUTF8      );
	}

	SECTION("ToUTF8")
	{
		KString sUTF;
		Unicode::ToUTF8(128, sUTF);
		CHECK ( sUTF.size() == 2 );
		CHECK ( static_cast<uint8_t>(sUTF[0]) == 0xc2 );
		CHECK ( static_cast<uint8_t>(sUTF[1]) == 0x80 );
	}

	SECTION("Invalid")
	{
		std::vector<KStringView> Test {
			"\xF5\x80\x80\x80",
			"\xF6\x80\x80\x80",
			"\xF7\x80\x80\x80",
			"\xF8\x88\x80\x80\x80",
			"\xFC\x84\x80\x80\x80\x80",
			"\xFB\xBF\xBF\xBF\xBF",
			"\xFD\xBF\xBF\xBF\xBF\xBF",
			"\x80",
			"\xBF",
			"\x80\xBF",
			"\x80\xBF\x80",
			"\x80\xBF\x80\xBF",
			"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F",
			"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F",
			"\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF",
			"\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF",
			"\xC0\x20\xC1\x20\xC2\x20\xC3\x20\xC4\x20\xC5\x20\xC6\x20\xC7\x20\xC8\x20\xC9\x20\xCA\x20\xCB\x20\xCC\x20\xCD\x20\xCE\x20\xCF\x20",
			"\xD0\x20\xD1\x20\xD2\x20\xD3\x20\xD4\x20\xD5\x20\xD6\x20\xD7\x20\xD8\x20\xD9\x20\xDA\x20\xDB\x20\xDC\x20\xDD\x20\xDE\x20\xDF\x20",
			"\xE0\x20\xE1\x20\xE2\x20\xE3\x20\xE4\x20\xE5\x20\xE6\x20\xE7\x20\xE8\x20\xE9\x20\xEA\x20\xEB\x20\xEC\x20\xED\x20\xEE\x20\xEF\x20",
			"\xF0\x20\xF1\x20\xF2\x20\xF3\x20\xF4\x20\xF5\x20\xF6\x20\xF7\x20",
			"\xF8\x20\xF9\x20\xFA\x20\xFB\x20",
			"\xFC\x20\xFD\x20",
			"\xFE",
			"\xFF",
			"\xFE\xFE\xFF\xFF",
			"\xC0\xAF",
			"\xE0\x80\xAF",
			"\xF0\x80\x80\xAF",
			"\xF8\x80\x80\x80\xAF",
			"\xFC\x80\x80\x80\x80\xAF",
			"\xC1\xBF",
			"\xE0\x9F\xBF",
			"\xF0\x8F\xBF\xBF",
			"\xF8\x87\xBF\xBF\xBF",
			"\xFC\x83\xBF\xBF\xBF\xBF",
			"\xC0\x80",
			"\xF0\x80\x80\x80",
		};

		std::size_t i = 0;
		for (auto& sInvalid : Test)
		{
			INFO ( ++i );
			bool bValid = Unicode::ValidUTF8(sInvalid);
			CHECK ( bValid == false );
			auto index = Unicode::InvalidUTF8(sInvalid);
			CHECK ( index != sInvalid.size() );
		}
	}

	SECTION("IsSurrogate")
	{
		for (std::size_t ch = 0; ch <= Unicode::CODEPOINT_MAX; ++ch)
		{
			if (ch < Unicode::SURROGATE_LOW_START || ch > Unicode::SURROGATE_HIGH_END)
			{
				if (Unicode::IsSurrogate(static_cast<Unicode::codepoint_t>(ch)))
				{
					INFO(ch);
					CHECK ( Unicode::IsSurrogate(static_cast<Unicode::codepoint_t>(ch)) == false );
				}
			}
			else
			{
				if (!Unicode::IsSurrogate(static_cast<Unicode::codepoint_t>(ch)))
				{
					INFO(ch);
					CHECK ( Unicode::IsSurrogate(static_cast<Unicode::codepoint_t>(ch)) == true );
				}
			}
		}
	}

	SECTION("SurrogatePair")
	{
		for (std::size_t ch = Unicode::NEEDS_SURROGATE_START; ch <= Unicode::NEEDS_SURROGATE_END; ++ch)
		{
			INFO(ch);

			if (!Unicode::NeedsSurrogates(static_cast<Unicode::codepoint_t>(ch)))
			{
				CHECK(Unicode::NeedsSurrogates(static_cast<Unicode::codepoint_t>(ch)));
			}
			Unicode::SurrogatePair sp(static_cast<Unicode::codepoint_t>(ch));
			if (sp.ToCodepoint() != ch)
			{
				CHECK( uint32_t(sp.ToCodepoint()) == ch);
			}
			if (!Unicode::IsLeadSurrogate(sp.low))
			{
				CHECK(Unicode::IsLeadSurrogate(sp.low));
			}
			if (!Unicode::IsTrailSurrogate(sp.high))
			{
				CHECK(Unicode::IsTrailSurrogate(sp.high));
			}
			Unicode::SurrogatePair sp2(sp.low, sp.high);
			if (sp2.ToCodepoint() != ch)
			{
				CHECK( uint32_t(sp2.ToCodepoint()) == ch);
			}
		}
	}

	SECTION("IsStartByte")
	{
		for (uint8_t ch = 0;; ++ch)
		{
			INFO(ch);

			if (ch >= 0x0c0 /* && ch <= 0x0ff */ )
			{
				// lead bytes
				CHECK ( Unicode::IsStartByte(ch)        == true  );
				CHECK ( Unicode::IsContinuationByte(ch) == false );
			}
			else if (ch >= 0x080 && ch <= 0x0bf)
			{
				// continuation
				CHECK ( Unicode::IsStartByte(ch)        == false );
				CHECK ( Unicode::IsContinuationByte(ch) == true  );
			}
			else
			{
				CHECK ( Unicode::IsStartByte(ch)        == false );
				CHECK ( Unicode::IsContinuationByte(ch) == false );
			}

			if (ch == 255) break;
		}
	}

	SECTION("UTF16")
	{
		std::basic_string<uint16_t> sUTF16_in;
		std::string sUTF8;
		std::basic_string<uint16_t> sUTF16_out;

//		for (Unicode::codepoint_t ch = 0x010020; ch <= 0x010020; ++ch)
		for (Unicode::codepoint_t ch = 0; ch <= Unicode::CODEPOINT_MAX; ++ch)
		{
			if (Unicode::NeedsSurrogates(ch))
			{
				Unicode::SurrogatePair sp(ch);
				sUTF16_in += sp.low;
				sUTF16_in += sp.high;
			}
			else
			{
				if (Unicode::IsValid(ch))
				{
					sUTF16_in += ch;
				}
				else
				{
					sUTF16_in += Unicode::REPLACEMENT_CHARACTER;
				}
			}
		}

		Unicode::Convert(sUTF16_in, sUTF8);
		Unicode::Convert(sUTF8, sUTF16_out);

		CHECK ( sUTF16_in.size() == sUTF16_out.size() );

		auto it = std::mismatch(sUTF16_in.begin(), sUTF16_in.end(), sUTF16_out.begin(), sUTF16_out.end());

		if (it.first != sUTF16_in.end())
		{
			INFO(kFormat("in:  mismatch at pos {}", it.first - sUTF16_in.begin()));
			CHECK ( false );
		}
		if (it.second != sUTF16_out.end())
		{
			INFO(kFormat("out: mismatch at pos {}", it.second - sUTF16_out.begin()));
			CHECK ( false );
		}
	}

	SECTION("UTF16-2")
	{
		Unicode::codepoint_t ch = 0xed;
		CHECK ( Unicode::IsValid(ch) );
		auto sUTF8 = Unicode::ToUTF8<KString>(ch);
		CHECK ( sUTF8.size() == 2 );
		if (sUTF8.size() == 2)
		{
			CHECK ( sUTF8[0] == char(0xc3) );
			CHECK ( sUTF8[1] == char(0xad) );
		}
		// sUTFw is UTF16 on Windows, and UCS4 on Unix
		auto sUTFw = Unicode::FromUTF8(sUTF8);
		CHECK ( sUTFw.size() == 1 );
		if (sUTFw.size() == 1)
		{
			CHECK ( int(sUTFw[0]) == int(0xed) );
		}
		sUTF8.MakeUpper();
		CHECK ( sUTF8.size() == 2 );
		if (sUTF8.size() == 2)
		{
			CHECK ( sUTF8[0] == char(0xc3) );
			CHECK ( sUTF8[1] == char(0x8d) );
		}
	}
}

