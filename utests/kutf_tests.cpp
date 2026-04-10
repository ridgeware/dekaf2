#include "catch.hpp"

#include <dekaf2/core/types/kutf.h>
#include <dekaf2/core/strings/kcesu8.h>
#include <dekaf2/core/strings/kstring.h>
#include <vector>
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	#include <string_view>
	using namespace std::string_literals;
#endif

using namespace dekaf2;

TEST_CASE("UTF") {

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
			kutf::Latin1ToUTF(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			kutf::ForEach(sTest, [&sOut](uint32_t ch)
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
			kutf::Latin1ToUTF(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			kutf::ForEach(sTest, [&sOut](uint32_t ch)
			{
				sOut += static_cast<std::string::value_type>(ch);
				return true;
			});
			CHECK ( sOut == it[0] );
		}
	}

	SECTION("Valid")
	{
		KString sStr("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK( kutf::Valid(sStr) == true );
		CHECK( kutf::Valid(sStr.begin()+3, sStr.end()) == true );
		sStr[6] = 'a';
		CHECK( kutf::Valid(sStr) == false );
	}

	SECTION("Count")
	{
		KString sStr("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK(              sStr.size() == 53 );
		CHECK( kutf::Count(sStr) == 29 );
		CHECK( kutf::Count(sStr.begin()+2, sStr.end()) == 27 );
	}

	SECTION("Left")
	{
		KString sStr("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK(                sStr.size() == 53       );
		CHECK( kutf::Left(sStr, 7) == "testäöü");
	}

	SECTION("Right")
	{
		KString sStr("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK(                  sStr.size() == 53           );
		CHECK( kutf::Right(sStr, 10) == "文Русскийɠ𐑅");
	}

	SECTION("Mid")
	{
		KString sStr("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK(                  sStr.size() == 53         );
		CHECK( kutf::Mid(sStr, 8, 7) == "test日本語");
	}

	SECTION("CESU8::UTF8ToUTF16Bytes")
	{
		KString sUTF8("testäöü test日本語abc中文Русскийɠ𐑅");
		CHECK(                 sUTF8.size() == 53     );
		auto sBytes = kutf::CESU8::UTF8ToUTF16Bytes(sUTF8);
		CHECK(                 sBytes.size() == 60    );
//		CHECK( sBytes == "" );
		auto sUTF8New = kutf::CESU8::UTF16BytesToUTF8(sBytes);
		CHECK(                 sUTF8New.size() == 53  );
		CHECK(                 sUTF8New == sUTF8      );
	}

	SECTION("ToUTF")
	{
		KString sUTF;
		kutf::ToUTF(128, sUTF);
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
			bool bValid = kutf::Valid(sInvalid);
			CHECK ( bValid == false );
			auto index = kutf::Invalid(sInvalid);
			CHECK ( index != sInvalid.size() );
		}
	}

	SECTION("IsSurrogate")
	{
		for (std::size_t ch = 0; ch <= kutf::CODEPOINT_MAX; ++ch)
		{
			if (ch < kutf::SURROGATE_LOW_START || ch > kutf::SURROGATE_HIGH_END)
			{
				if (kutf::IsSurrogate(static_cast<kutf::codepoint_t>(ch)))
				{
					INFO(ch);
					CHECK ( kutf::IsSurrogate(static_cast<kutf::codepoint_t>(ch)) == false );
				}
			}
			else
			{
				if (!kutf::IsSurrogate(static_cast<kutf::codepoint_t>(ch)))
				{
					INFO(ch);
					CHECK ( kutf::IsSurrogate(static_cast<kutf::codepoint_t>(ch)) == true );
				}
			}
		}
	}

	SECTION("SurrogatePair")
	{
		for (std::size_t ch = kutf::NEEDS_SURROGATE_START; ch <= kutf::NEEDS_SURROGATE_END; ++ch)
		{
			INFO(ch);

			if (!kutf::NeedsSurrogates(static_cast<kutf::codepoint_t>(ch)))
			{
				CHECK(kutf::NeedsSurrogates(static_cast<kutf::codepoint_t>(ch)));
			}
			kutf::SurrogatePair sp(static_cast<kutf::codepoint_t>(ch));
			if (sp.ToCodepoint() != ch)
			{
				CHECK( uint32_t(sp.ToCodepoint()) == ch);
			}
			if (!kutf::IsLeadSurrogate(sp.low))
			{
				CHECK(kutf::IsLeadSurrogate(sp.low));
			}
			if (!kutf::IsTrailSurrogate(sp.high))
			{
				CHECK(kutf::IsTrailSurrogate(sp.high));
			}
			kutf::SurrogatePair sp2(sp.low, sp.high);
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
				CHECK ( kutf::IsStartByte(ch)        == true  );
				CHECK ( kutf::IsContinuationByte(ch) == false );
			}
			else if (ch >= 0x080 && ch <= 0x0bf)
			{
				// continuation
				CHECK ( kutf::IsStartByte(ch)        == false );
				CHECK ( kutf::IsContinuationByte(ch) == true  );
			}
			else
			{
				CHECK ( kutf::IsStartByte(ch)        == false );
				CHECK ( kutf::IsContinuationByte(ch) == false );
			}

			if (ch == 255) break;
		}
	}

	SECTION("UTF16")
	{
		std::u16string sUTF16_in;
		std::string sUTF8;
		std::u16string sUTF16_out;

//		for (kutf::codepoint_t ch = 0x010020; ch <= 0x010020; ++ch)
		for (kutf::codepoint_t ch = 0; ch <= kutf::CODEPOINT_MAX; ++ch)
		{
			if (kutf::NeedsSurrogates(ch))
			{
				kutf::SurrogatePair sp(ch);
				sUTF16_in += sp.low;
				sUTF16_in += sp.high;
			}
			else
			{
				if (kutf::IsValid(ch))
				{
					sUTF16_in += static_cast<char16_t>(ch);
				}
				else
				{
					sUTF16_in += kutf::REPLACEMENT_CHARACTER;
				}
			}
		}

		CHECK ( kutf::Valid(sUTF16_in)                );
		CHECK ( kutf::Convert(sUTF16_in, sUTF8)       );
		CHECK ( kutf::Valid(sUTF8)                    );
		CHECK ( kutf::Convert(sUTF8, sUTF16_out)      );
		CHECK ( kutf::Valid(sUTF16_out)               );
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
		kutf::codepoint_t ch = 0xed;
		CHECK ( kutf::IsValid(ch) );
		auto sUTF8 = kutf::ToUTF<KString>(ch);
		CHECK ( sUTF8.size() == 2 );
		if (sUTF8.size() == 2)
		{
			CHECK ( sUTF8[0] == char(0xc3) );
			CHECK ( sUTF8[1] == char(0xad) );
		}
		// sUTFw is UTF16 on Windows, and UTF32 on Unix
		auto sUTFw = kutf::Convert<std::wstring>(sUTF8);
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

	SECTION("Increment UTF8")
	{
		KStringView sInput = "testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( kutf::Increment(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( kutf::Increment(it, ie,  1) == true );
		CHECK ( *it == 'e' );
		CHECK ( kutf::Increment(it, ie,  6) == true );
		CHECK ( *it == ' ' );
		CHECK ( kutf::Increment(it, ie, 22) == true );
		CHECK ( *it == ' ' );
		CHECK ( kutf::Increment(it, ie,  5) == false);
		CHECK ( it == ie );
	}

#ifdef __cpp_lib_string_view
	SECTION("Increment wstring")
	{
		std::wstring_view sInput = L"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( kutf::Increment(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( kutf::Increment(it, ie,  1) == true );
		CHECK ( (*it == wchar_t('e')) );
		CHECK ( kutf::Increment(it, ie,  6) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie, 22) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie,  5) == false);
		CHECK ( it == ie );
	}

	SECTION("Increment UTF16")
	{
		std::u16string_view sInput = u"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( kutf::Increment(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( kutf::Increment(it, ie,  1) == true );
		CHECK ( (*it == wchar_t('e')) );
		CHECK ( kutf::Increment(it, ie,  6) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie, 22) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie,  5) == false);
		CHECK ( it == ie );
	}

	SECTION("Increment UTF32")
	{
		std::u32string_view sInput = U"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( kutf::Increment(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( kutf::Increment(it, ie,  1) == true );
		CHECK ( (*it == wchar_t('e')) );
		CHECK ( kutf::Increment(it, ie,  6) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie, 22) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( kutf::Increment(it, ie,  5) == false);
		CHECK ( it == ie );
	}
#endif

	SECTION("Decrement UTF8")
	{
		KStringView sInput = "testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( kutf::Decrement(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( kutf::Decrement(ibegin, it,   1) == true );
		CHECK ( *it == '.' );
		CHECK ( kutf::Decrement(ibegin, it,  14) == true );
		CHECK ( *it == 'c' );
		CHECK ( kutf::Decrement(ibegin, it,   6) == true );
		CHECK ( *it == 't' );
		CHECK ( kutf::Decrement(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}

#ifdef __cpp_lib_string_view
	SECTION("Decrement wstring")
	{
		std::wstring_view sInput = L"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( kutf::Decrement(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( kutf::Decrement(ibegin, it,   1) == true );
		CHECK ( (*it == wchar_t('.')) );
		CHECK ( kutf::Decrement(ibegin, it,  14) == true );
		CHECK ( (*it == wchar_t('c')) );
		CHECK ( kutf::Decrement(ibegin, it,   6) == true );
		CHECK ( (*it == wchar_t('t')) );
		CHECK ( kutf::Decrement(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}

	SECTION("Decrement UTF16")
	{
		std::u16string_view sInput = u"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( kutf::Decrement(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( kutf::Decrement(ibegin, it,   1) == true );
		CHECK ( (*it == wchar_t('.')) );
		CHECK ( kutf::Decrement(ibegin, it,  14) == true );
		CHECK ( (*it == wchar_t('c')) );
		CHECK ( kutf::Decrement(ibegin, it,   6) == true );
		CHECK ( (*it == wchar_t('t')) );
		CHECK ( kutf::Decrement(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}

	SECTION("Decrement UTF32")
	{
		std::u32string_view sInput = U"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( kutf::Decrement(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( kutf::Decrement(ibegin, it,   1) == true );
		CHECK ( (*it == wchar_t('.')) );
		CHECK ( kutf::Decrement(ibegin, it,  14) == true );
		CHECK ( (*it == wchar_t('c')) );
		CHECK ( kutf::Decrement(ibegin, it,   6) == true );
		CHECK ( (*it == wchar_t('t')) );
		CHECK ( kutf::Decrement(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}
#endif

#ifdef __cpp_lib_string_view
	SECTION("Convert")
	{
		std::wstring   sWstring    = L"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		std::string    s8Expected  =  "testäöü test日本語abc中文Русскийɠ𐑅 ..";
		std::u16string s16Expected = u"testäöü test日本語abc中文Русскийɠ𐑅 ..";
		std::u32string s32Expected = U"testäöü test日本語abc中文Русскийɠ𐑅 ..";

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( kutf::Convert(s8Expected, s8Output) );
			CHECK ( s8Output == s8Expected );
			CHECK ( kutf::Convert(s16Expected, s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( kutf::Convert(s32Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
		}

		{
			std::string s8Output;

			CHECK ( kutf::Convert(s8Expected, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;

			CHECK ( kutf::Convert(s8Expected, s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( kutf::Convert(s16Output, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::string    s8Output;
			std::u32string s32Output;

			CHECK ( kutf::Convert(s8Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( kutf::Convert(s32Output, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( kutf::Convert(s16Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( kutf::Convert(s32Output, s16Output) );
			CHECK ( s16Output == s16Expected );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( kutf::Convert(sWstring.c_str(), s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( kutf::Convert(sWstring.c_str(), s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( kutf::Convert(sWstring.c_str(), s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::vector<char> Vec;
			CHECK ( kutf::Convert(s32Expected, Vec) );
			CHECK ( Vec.size() == s8Expected.size() );
		}
	}
#endif

	SECTION("Latin1ToUTF")
	{
		std::string sLatin1 = "test\xe4\xf6\xfc";
		std::string sUTF8   = "testäöü";
		std::string s8Output;
		CHECK( kutf::Latin1ToUTF(sLatin1, s8Output) );
		CHECK( s8Output == sUTF8 );
	}

	SECTION("ToLower")
	{
		std::string    s8Lower     =  "testäöü test日本語abc中文русскийɠ𐑅 ..";
		std::u16string s16Lower    = u"testäöü test日本語abc中文русскийɠ𐑅 ..";
		std::u32string s32Lower    = U"testäöü test日本語abc中文русскийɠ𐑅 ..";
		std::string    s8Upper     =  "TESTÄÖÜ TEST日本語ABC中文РУССКИЙƓ𐐝 ..";
		std::u16string s16Upper    = u"TESTÄÖÜ TEST日本語ABC中文РУССКИЙƓ𐐝 ..";
		std::u32string s32Upper    = U"TESTÄÖÜ TEST日本語ABC中文РУССКИЙƓ𐐝 ..";

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;
			
			CHECK ( kutf::ToUpper(s8Lower, s8Output) );
			CHECK ( s8Output == s8Upper );
			CHECK ( kutf::ToUpper(s16Lower, s16Output) );
			CHECK ( s16Output == s16Upper );
			CHECK ( kutf::ToUpper(s32Lower, s32Output) );
			CHECK ( s32Output == s32Upper );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( kutf::ToLower(s8Upper, s8Output) );
			CHECK ( s8Output == s8Lower );
			CHECK ( kutf::ToLower(s16Upper, s16Output) );
			CHECK ( (s16Output == s16Lower) );
			CHECK ( kutf::ToLower(s32Upper, s32Output) );
			CHECK ( (s32Output == s32Lower) );
		}
	}

	SECTION("IsSingleByte")
	{
		for (uint16_t ch = 0; ch < 256; ++ch)
		{
			auto u = static_cast<kutf::utf8_t>(ch);
			if (ch < 0x80)
			{
				CHECK ( kutf::IsSingleByte(u) == true  );
			}
			else
			{
				CHECK ( kutf::IsSingleByte(u) == false );
			}
		}
	}

	SECTION("CodepointCast")
	{
		// signed char should not sign-extend
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<char>(-1)))          == 0xFF       );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<int8_t>(-1)))        == 0xFF       );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<uint8_t>(0x80)))     == 0x80       );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<int16_t>(-1)))       == 0xFFFF     );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<uint16_t>(0xD800)))  == 0xD800     );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<int32_t>(0x10FFFF))) == 0x10FFFF   );
		CHECK ( uint32_t(kutf::CodepointCast(static_cast<uint32_t>(0)))       == 0          );
		CHECK ( uint32_t(kutf::CodepointCast('A'))                            == 0x41       );
	}

	SECTION("UTFChars")
	{
		// UTF-8 widths
		CHECK ( kutf::UTFChars<8>(0x00)      == 1 );
		CHECK ( kutf::UTFChars<8>(0x7F)      == 1 );
		CHECK ( kutf::UTFChars<8>(0x80)      == 2 );
		CHECK ( kutf::UTFChars<8>(0x7FF)     == 2 );
		CHECK ( kutf::UTFChars<8>(0x800)     == 3 );
		CHECK ( kutf::UTFChars<8>(0xFFFF)    == 3 );
		CHECK ( kutf::UTFChars<8>(0x10000)   == 4 );
		CHECK ( kutf::UTFChars<8>(0x10FFFF)  == 4 );
		// surrogates encode as REPLACEMENT_CHARACTER (3 bytes)
		CHECK ( kutf::UTFChars<8>(0xD800)    == 3 );
		// above max -> REPLACEMENT_CHARACTER (3 bytes)
		CHECK ( kutf::UTFChars<8>(0x110000)  == 3 );
		// UTF-16 widths
		CHECK ( kutf::UTFChars<16>(0x00)     == 1 );
		CHECK ( kutf::UTFChars<16>(0xFFFF)   == 1 );
		CHECK ( kutf::UTFChars<16>(0x10000)  == 2 );
		CHECK ( kutf::UTFChars<16>(0x10FFFF) == 2 );
		// not NeedsSurrogates -> 1
		CHECK ( kutf::UTFChars<16>(0x110000) == 1 );
		// UTF-32 widths
		CHECK ( kutf::UTFChars<32>(0x00)     == 1 );
		CHECK ( kutf::UTFChars<32>(0x10FFFF) == 1 );
	}

	SECTION("CodepointFromUTF8")
	{
		// 1-byte ASCII
		{
			KStringView sv("A");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0x41 );
			CHECK ( it == sv.end() );
		}
		// 2-byte
		{
			KStringView sv("é");  // U+00E9
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0xE9 );
			CHECK ( it == sv.end() );
		}
		// 3-byte
		{
			KStringView sv("日");  // U+65E5
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0x65E5 );
			CHECK ( it == sv.end() );
		}
		// 4-byte
		{
			KStringView sv("𐑅");  // U+10445
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0x10445 );
			CHECK ( it == sv.end() );
		}
		// overlong 2-byte (0xC0 0x80 encodes NUL)
		{
			KStringView sv("\xC0\x80");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
		}
		// truncated 2-byte
		{
			KStringView sv("\xC2");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
		}
	}

	SECTION("CodepointFromUTF8 rejects surrogates")
	{
		// U+D800 encoded in UTF-8: 0xED 0xA0 0x80
		{
			KStringView sv("\xED\xA0\x80");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
		}
		// U+DFFF encoded in UTF-8: 0xED 0xBF 0xBF
		{
			KStringView sv("\xED\xBF\xBF");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
		}
		// U+D7FF (just below surrogates) should be valid
		{
			KStringView sv("\xED\x9F\xBF");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0xD7FF );
		}
		// U+E000 (just above surrogates) should be valid
		{
			KStringView sv("\xEE\x80\x80");
			auto it = sv.begin();
			CHECK ( uint32_t(kutf::CodepointFromUTF8(it, sv.end())) == 0xE000 );
		}
	}

	SECTION("Codepoint UTF32")
	{
		std::u32string s = U"Aé日𐑅";
		auto it = s.begin();
		auto ie = s.end();
		CHECK ( uint32_t(kutf::Codepoint(it, ie)) == 0x41    );
		CHECK ( uint32_t(kutf::Codepoint(it, ie)) == 0xE9    );
		CHECK ( uint32_t(kutf::Codepoint(it, ie)) == 0x65E5  );
		CHECK ( uint32_t(kutf::Codepoint(it, ie)) == 0x10445 );
		CHECK ( it == ie );
	}

	SECTION("PrevCodepoint UTF8")
	{
		KStringView sv("Aé日𐑅");
		auto ibegin = sv.begin();
		auto it     = sv.end();
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x10445 );
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x65E5  );
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0xE9    );
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x41    );
		CHECK ( it == ibegin );
		// calling again at begin returns INVALID_CODEPOINT
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == uint32_t(kutf::INVALID_CODEPOINT) );
	}

	SECTION("PrevCodepoint UTF32")
	{
		// PrevCodepoint for UTF32 calls Codepoint() internally which
		// re-advances the iterator - must save/restore like kutf_iterator.h does
		std::u32string s = U"Aé日";
		auto ibegin = s.begin();
		auto it     = s.end();
		auto saved  = it;
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x65E5 );
		it = saved - 1;
		saved = it;
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0xE9   );
		it = saved - 1;
		saved = it;
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x41   );
	}

	SECTION("Sync")
	{
		// start in the middle of a 3-byte UTF-8 sequence (日 = E6 97 A5)
		KStringView sv("日X");
		auto it = sv.begin() + 1;  // points to continuation byte 0x97
		auto ie = sv.end();
		kutf::Sync(it, ie);
		// should advance to 'X' (past all continuation bytes)
		CHECK ( *it == 'X' );

		// already at a start byte -> no change
		auto it2 = sv.begin();
		kutf::Sync(it2, ie);
		CHECK ( it2 == sv.begin() );
	}

	SECTION("InvalidASCII")
	{
		{
			KStringView sv("hello world");
			CHECK ( kutf::ValidASCII(sv) == true );
			CHECK ( kutf::InvalidASCII(sv) == std::size_t(-1) );
		}
		{
			KStringView sv("hello\x80world");
			CHECK ( kutf::ValidASCII(sv) == false );
			CHECK ( kutf::InvalidASCII(sv) == 5 );
		}
		{
			KStringView sv("héllo");
			auto it = kutf::InvalidASCII(sv.begin(), sv.end());
			CHECK ( it != sv.end() );
			CHECK ( static_cast<std::size_t>(it - sv.begin()) == 1 );
		}
		{
			KStringView sv("");
			CHECK ( kutf::ValidASCII(sv) == true );
		}
		// long ASCII string (triggers SWAR path)
		{
			std::string sLong(64, 'A');
			CHECK ( kutf::ValidASCII(sLong) == true );
			sLong[60] = '\x80';
			CHECK ( kutf::ValidASCII(sLong) == false );
			CHECK ( kutf::InvalidASCII(sLong) == 60 );
		}
	}

	SECTION("HasUTF8")
	{
		// pure ASCII
		CHECK ( kutf::HasUTF8(KStringView("hello")) == false );
		// valid UTF-8 multibyte
		CHECK ( kutf::HasUTF8(KStringView("helloä")) == true );
		// invalid non-ASCII byte (bare continuation byte)
		CHECK ( kutf::HasUTF8(KStringView("hello\x80")) == false );
		// empty string
		CHECK ( kutf::HasUTF8(KStringView("")) == false );
	}

	SECTION("At")
	{
		KStringView sv("Aé日𐑅Z");
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 0)) == 0x41    );
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 1)) == 0xE9    );
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 2)) == 0x65E5  );
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 3)) == 0x10445 );
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 4)) == 'Z'     );
		// past end
		CHECK ( uint32_t(kutf::At(sv.begin(), sv.end(), 5)) == uint32_t(kutf::INVALID_CODEPOINT) );
		// string overload
		CHECK ( uint32_t(kutf::At(sv, 0)) == 0x41   );
		CHECK ( uint32_t(kutf::At(sv, 4)) == 'Z'    );
		CHECK ( uint32_t(kutf::At(sv, 5)) == uint32_t(kutf::INVALID_CODEPOINT) );
	}

	SECTION("Count with iMaxCount")
	{
		KStringView sv("Aé日𐑅Z");  // 5 codepoints
		CHECK ( kutf::Count(sv) == 5 );
		CHECK ( kutf::Count(sv, 3) >= 3 );
		CHECK ( kutf::Count(sv, 1) >= 1 );
		CHECK ( kutf::Count(sv, 100) == 5 );
	}

	SECTION("Transform direct")
	{
		// transform UTF-8 to UTF-8 with identity functor
		KStringView sv("Aé日");
		std::string sOutput;
		CHECK ( kutf::Transform(sv.begin(), sv.end(), sOutput,
		                         [](kutf::codepoint_t cp) { return cp; }) );
		CHECK ( sOutput == "Aé日" );

		// transform with offset: add 1 to each ASCII codepoint
		std::string sOutput2;
		KStringView sv2("ABC");
		CHECK ( kutf::Transform(sv2.begin(), sv2.end(), sOutput2,
		                         [](kutf::codepoint_t cp) { return cp + 1; }) );
		CHECK ( sOutput2 == "BCD" );

		// string overload
		std::string sOutput3;
		CHECK ( kutf::Transform(sv2, sOutput3,
		                         [](kutf::codepoint_t cp) { return cp + 1; }) );
		CHECK ( sOutput3 == "BCD" );
	}

	SECTION("ForEach iterator version")
	{
		KStringView sv("Aé日");
		std::vector<uint32_t> cps;
		CHECK ( kutf::ForEach(sv.begin(), sv.end(),
		                       [&cps](kutf::codepoint_t cp) { cps.push_back(cp); return true; }) );
		CHECK ( cps.size() == 3 );
		CHECK ( cps[0] == 0x41   );
		CHECK ( cps[1] == 0xE9   );
		CHECK ( cps[2] == 0x65E5 );
	}

	SECTION("ForEach early abort")
	{
		KStringView sv("ABCDE");
		std::size_t iCount = 0;
		CHECK ( kutf::ForEach(sv, [&iCount](kutf::codepoint_t) { return ++iCount < 3; }) == false );
		CHECK ( iCount == 3 );
	}

	SECTION("Latin1ToUTF iterator versions")
	{
		std::string sLatin1 = "test\xe4\xf6\xfc";
		std::string sExpected = "testäöü";

		// iterator pair -> output container
		{
			std::string sOut;
			CHECK ( kutf::Latin1ToUTF(sLatin1.begin(), sLatin1.end(), sOut) );
			CHECK ( sOut == sExpected );
		}
		// iterator pair -> return value
		{
			auto sOut = kutf::Latin1ToUTF<std::string>(sLatin1.begin(), sLatin1.end());
			CHECK ( sOut == sExpected );
		}
		// string -> return value
		{
			auto sOut = kutf::Latin1ToUTF<std::string>(sLatin1);
			CHECK ( sOut == sExpected );
		}
		// empty input
		{
			std::string sEmpty;
			std::string sOut;
			CHECK ( kutf::Latin1ToUTF(sEmpty.begin(), sEmpty.end(), sOut) );
			CHECK ( sOut.empty() );
		}
	}

	SECTION("ToUTF various codepoints")
	{
		// 1-byte
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0x41));
			CHECK ( s == "A" );
		}
		// 2-byte
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0xE9));
			CHECK ( s.size() == 2 );
			CHECK ( s == "é" );
		}
		// 3-byte
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0x65E5));
			CHECK ( s.size() == 3 );
			CHECK ( s == "日" );
		}
		// 4-byte
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0x10445));
			CHECK ( s.size() == 4 );
			CHECK ( s == "𐑅" );
		}
		// surrogate -> REPLACEMENT_CHARACTER
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0xD800));
			auto sRepl = kutf::ToUTF<std::string>(kutf::REPLACEMENT_CHARACTER);
			CHECK ( s == sRepl );
		}
		// above max -> REPLACEMENT_CHARACTER
		{
			auto s = kutf::ToUTF<std::string>(static_cast<kutf::codepoint_t>(0x110000));
			auto sRepl = kutf::ToUTF<std::string>(kutf::REPLACEMENT_CHARACTER);
			CHECK ( s == sRepl );
		}
	}

	SECTION("ToUTF to UTF-16")
	{
		// BMP character
		{
			std::u16string s;
			kutf::ToUTF(static_cast<kutf::codepoint_t>(0x41), s);
			CHECK ( s.size() == 1 );
			CHECK ( uint16_t(s[0]) == 0x41 );
		}
		// supplementary (needs surrogates)
		{
			std::u16string s;
			kutf::ToUTF(static_cast<kutf::codepoint_t>(0x10445), s);
			CHECK ( s.size() == 2 );
			CHECK ( kutf::IsLeadSurrogate(s[0]) );
			CHECK ( kutf::IsTrailSurrogate(s[1]) );
		}
	}

	SECTION("ToUTF to UTF-32")
	{
		std::u32string s;
		kutf::ToUTF(static_cast<kutf::codepoint_t>(0x10445), s);
		CHECK ( s.size() == 1 );
		CHECK ( uint32_t(s[0]) == 0x10445 );
	}

	SECTION("Convert return-type overloads")
	{
		// iterator pair -> return type
		{
			KStringView sv("Aé日");
			auto s32 = kutf::Convert<std::u32string>(sv.begin(), sv.end());
			CHECK ( s32.size() == 3 );
			CHECK ( uint32_t(s32[0]) == 0x41   );
			CHECK ( uint32_t(s32[1]) == 0xE9   );
			CHECK ( uint32_t(s32[2]) == 0x65E5 );
		}
		// string -> return type
		{
			std::string s8 = "Aé日";
			auto s32 = kutf::Convert<std::u32string>(s8);
			CHECK ( s32.size() == 3 );
		}
#ifdef __cpp_lib_string_view
		// wchar_t* -> return type
		{
			auto s8 = kutf::Convert<std::string>(L"Aé日");
			CHECK ( s8 == "Aé日" );
		}
		// wchar_t* nullptr
		{
			std::string sOut;
			CHECK ( kutf::Convert(static_cast<const wchar_t*>(nullptr), sOut) == false );
		}
#endif
	}

	SECTION("ReadIterator")
	{
		// test with a simple string feeder
		std::string sInput = "Aé";  // A = 0x41, é = 0xC3 0xA9
		std::size_t iPos = 0;
		kutf::ReadIterator it([&sInput, &iPos]() -> int
		{
			if (iPos >= sInput.size()) return EOF;
			return static_cast<unsigned char>(sInput[iPos++]);
		});
		kutf::ReadIterator ie; // end iterator

		CHECK ( it != ie );
		auto cp1 = kutf::CodepointFromUTF8(it, ie);
		CHECK ( uint32_t(cp1) == 0x41 );
		CHECK ( it != ie );
		auto cp2 = kutf::CodepointFromUTF8(it, ie);
		CHECK ( uint32_t(cp2) == 0xE9 );
		CHECK ( it == ie );
	}

	SECTION("Valid and Invalid for empty input")
	{
		KStringView svEmpty("");
		CHECK ( kutf::Valid(svEmpty) == true );
		CHECK ( kutf::Invalid(svEmpty) == std::size_t(-1) );
	}

	SECTION("Left iterator version")
	{
		KStringView sv("Aé日Z");
		auto it = kutf::Left(sv.begin(), sv.end(), 2);
		// should be past 'A' and 'é' (1 + 2 = 3 bytes)
		CHECK ( static_cast<std::size_t>(it - sv.begin()) == 3 );
		// 0 codepoints
		auto it2 = kutf::Left(sv.begin(), sv.end(), 0);
		CHECK ( it2 == sv.begin() );
		// more than available
		auto it3 = kutf::Left(sv.begin(), sv.end(), 100);
		CHECK ( it3 == sv.end() );
	}

	SECTION("Right iterator version")
	{
		KStringView sv("Aé日Z");
		auto it = kutf::Right(sv.begin(), sv.end(), 2);
		// should be at "日Z" start (日 is 3 bytes, Z is 1 byte = 4 bytes from end)
		CHECK ( *it == '\xE6' ); // first byte of 日
	}

	SECTION("Invalid iterator version")
	{
		// valid string
		{
			KStringView sv("Aé日");
			auto it = kutf::Invalid(sv.begin(), sv.end());
			CHECK ( it == sv.end() );
		}
		// invalid at position 1
		{
			KStringView sv("A\x80Z");
			auto it = kutf::Invalid(sv.begin(), sv.end());
			CHECK ( it != sv.end() );
			CHECK ( static_cast<std::size_t>(it - sv.begin()) == 1 );
		}
	}

	SECTION("Codepoint UTF16 surrogates")
	{
		// supplementary character as surrogate pair
		std::u16string s16;
		kutf::SurrogatePair sp(static_cast<kutf::codepoint_t>(0x10445));
		s16 += sp.low;
		s16 += sp.high;
		auto it = s16.begin();
		auto ie = s16.end();
		auto cp = kutf::Codepoint(it, ie);
		CHECK ( uint32_t(cp) == 0x10445 );
		CHECK ( it == ie );

		// lone trail surrogate -> INVALID_CODEPOINT
		std::u16string sBadTrail;
		sBadTrail += static_cast<char16_t>(0xDC00);
		auto it2 = sBadTrail.begin();
		CHECK ( uint32_t(kutf::Codepoint(it2, sBadTrail.end())) == uint32_t(kutf::INVALID_CODEPOINT) );

		// lone lead surrogate at end -> INVALID_CODEPOINT
		std::u16string sBadLead;
		sBadLead += static_cast<char16_t>(0xD800);
		auto it3 = sBadLead.begin();
		CHECK ( uint32_t(kutf::Codepoint(it3, sBadLead.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
	}

	SECTION("Codepoint UTF32 invalid")
	{
		// invalid codepoint (surrogate value)
		std::u32string s32;
		s32 += static_cast<char32_t>(0xD800);
		auto it = s32.begin();
		CHECK ( uint32_t(kutf::Codepoint(it, s32.end())) == uint32_t(kutf::INVALID_CODEPOINT) );
	}

	SECTION("PrevCodepoint UTF16")
	{
		// PrevCodepoint for UTF16 calls Codepoint() internally which
		// re-advances the iterator - must save/restore like kutf_iterator.h does
		std::u16string s16;
		kutf::SurrogatePair sp(static_cast<kutf::codepoint_t>(0x10445));
		s16 += static_cast<char16_t>('A');
		s16 += sp.low;
		s16 += sp.high;
		auto ibegin = s16.begin();
		auto it     = s16.end();
		auto saved  = it;
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == 0x10445 );
		it = saved - 2;  // surrogate pair = 2 units
		saved = it;
		CHECK ( uint32_t(kutf::PrevCodepoint(ibegin, it)) == uint32_t('A') );
	}

	SECTION("Sync UTF16")
	{
		// construct a surrogate pair + BMP char
		std::u16string s16;
		kutf::SurrogatePair sp(static_cast<kutf::codepoint_t>(0x10445));
		s16 += sp.low;
		s16 += sp.high;
		s16 += static_cast<char16_t>('Z');
		// point to trail surrogate
		auto it = s16.begin() + 1;
		auto ie = s16.end();
		kutf::Sync(it, ie);
		// should advance past the trail surrogate to 'Z'
		CHECK ( uint16_t(*it) == uint16_t('Z') );
	}

	SECTION("Count UTF16")
	{
		std::u16string s16;
		s16 += static_cast<char16_t>('A');
		kutf::SurrogatePair sp(static_cast<kutf::codepoint_t>(0x10445));
		s16 += sp.low;
		s16 += sp.high;
		s16 += static_cast<char16_t>('Z');
		CHECK ( kutf::Count(s16) == 3 );
	}

	SECTION("Count UTF32")
	{
		std::u32string s32 = U"Aé日𐑅Z";
		CHECK ( kutf::Count(s32) == 5 );
	}

	SECTION("Left Right Mid UTF32")
	{
		std::u32string s32 = U"ABCDE";
		std::u32string sLeft  = U"ABC";
		std::u32string sRight = U"DE";
		std::u32string sMid   = U"BCD";
		CHECK ( kutf::Left(s32, 3)    == sLeft  );
		CHECK ( kutf::Right(s32, 2)   == sRight );
		CHECK ( kutf::Mid(s32, 1, 3)  == sMid   );
	}
}


