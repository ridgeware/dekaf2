#include "catch.hpp"

#include <dekaf2/kutf8.h>
#include <dekaf2/kstring.h>
#include <vector>
#ifdef DEKAF2_HAS_CPP_17
	#include <string_view>
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
			Unicode::Latin1ToUTF(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			Unicode::ForEachUTF(sTest, [&sOut](uint32_t ch)
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
			Unicode::Latin1ToUTF(it[0], sTest);
			CHECK( sTest == it[1] );
			stype sOut;
			Unicode::ForEachUTF(sTest, [&sOut](uint32_t ch)
			{
				sOut += static_cast<std::string::value_type>(ch);
				return true;
			});
			CHECK ( sOut == it[0] );
		}
	}

	SECTION("ValidUTF")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK( Unicode::ValidUTF(sStr) == true );
		CHECK( Unicode::ValidUTF(sStr.begin()+3, sStr.end()) == true );
		sStr[6] = 'a';
		CHECK( Unicode::ValidUTF(sStr) == false );
	}

	SECTION("CountUTF")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(              sStr.size() == 47 );
		CHECK( Unicode::CountUTF(sStr) == 27 );
		CHECK( Unicode::CountUTF(sStr.begin()+2, sStr.end()) == 25 );
	}

	SECTION("LeftUTF")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                sStr.size() == 47       );
		CHECK( Unicode::LeftUTF(sStr, 7) == "testäöü");
	}

	SECTION("RightUTF")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                  sStr.size() == 47           );
		CHECK( Unicode::RightUTF(sStr, 10) == "c中文Русский");
	}

	SECTION("MidUTF")
	{
		KString sStr("testäöü test日本語abc中文Русский");
		CHECK(                  sStr.size() == 47         );
		CHECK( Unicode::MidUTF(sStr, 8, 7) == "test日本語");
	}

	SECTION("CESU8::UTF8ToUTF16Bytes")
	{
		KString sUTF8("testäöü test日本語abc中文Русский");
		CHECK(                 sUTF8.size() == 47     );
		auto sBytes = Unicode::CESU8::UTF8ToUTF16Bytes(sUTF8);
		CHECK(                 sBytes.size() == 54    );
//		CHECK( sBytes == "" );
		auto sUTF8New = Unicode::CESU8::UTF16BytesToUTF8(sBytes);
		CHECK(                 sUTF8New.size() == 47  );
		CHECK(                 sUTF8New == sUTF8      );
	}

	SECTION("ToUTF")
	{
		KString sUTF;
		Unicode::ToUTF(128, sUTF);
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
			bool bValid = Unicode::ValidUTF(sInvalid);
			CHECK ( bValid == false );
			auto index = Unicode::InvalidUTF(sInvalid);
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

		Unicode::ConvertUTF(sUTF16_in, sUTF8);
		Unicode::ConvertUTF(sUTF8, sUTF16_out);

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
		auto sUTF8 = Unicode::ToUTF<KString>(ch);
		CHECK ( sUTF8.size() == 2 );
		if (sUTF8.size() == 2)
		{
			CHECK ( sUTF8[0] == char(0xc3) );
			CHECK ( sUTF8[1] == char(0xad) );
		}
		// sUTFw is UTF16 on Windows, and UTF32 on Unix
		auto sUTFw = Unicode::ConvertUTF<std::wstring>(sUTF8);
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
		KStringView sInput = "testäöü test日本語abc中文Русский ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( Unicode::IncrementUTF(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( Unicode::IncrementUTF(it, ie,  1) == true );
		CHECK ( *it == 'e' );
		CHECK ( Unicode::IncrementUTF(it, ie,  6) == true );
		CHECK ( *it == ' ' );
		CHECK ( Unicode::IncrementUTF(it, ie, 20) == true );
		CHECK ( *it == ' ' );
		CHECK ( Unicode::IncrementUTF(it, ie,  5) == false);
		CHECK ( it == ie );
	}

#ifdef DEKAF2_HAS_FULL_CPP_17
	SECTION("Increment UTF32")
	{
		std::wstring_view sInput = L"testäöü test日本語abc中文Русский ..";
		auto it = sInput.begin();
		auto ie = sInput.end();
		auto it2 = it;
		CHECK ( Unicode::IncrementUTF(it, ie,  0) == true );
		CHECK ( it == it2 );
		CHECK ( Unicode::IncrementUTF(it, ie,  1) == true );
		CHECK ( (*it == wchar_t('e')) );
		CHECK ( Unicode::IncrementUTF(it, ie,  6) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( Unicode::IncrementUTF(it, ie, 20) == true );
		CHECK ( (*it == wchar_t(' ')) );
		CHECK ( Unicode::IncrementUTF(it, ie,  5) == false);
		CHECK ( it == ie );
	}
#endif

	SECTION("Decrement UTF8")
	{
		KStringView sInput = "testäöü test日本語abc中文Русский ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( Unicode::DecrementUTF(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( Unicode::DecrementUTF(ibegin, it,   1) == true );
		CHECK ( *it == '.' );
		CHECK ( Unicode::DecrementUTF(ibegin, it,  12) == true );
		CHECK ( *it == 'c' );
		CHECK ( Unicode::DecrementUTF(ibegin, it,   6) == true );
		CHECK ( *it == 't' );
		CHECK ( Unicode::DecrementUTF(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}

#ifdef DEKAF2_HAS_FULL_CPP_17
	SECTION("Decrement UTF32")
	{
		std::wstring_view sInput = L"testäöü test日本語abc中文Русский ..";
		auto ibegin = sInput.begin();
		auto it     = sInput.end();
		CHECK ( Unicode::DecrementUTF(ibegin, it,   0) == true );
		CHECK ( it == sInput.end() );
		CHECK ( Unicode::DecrementUTF(ibegin, it,   1) == true );
		CHECK ( (*it == wchar_t('.')) );
		CHECK ( Unicode::DecrementUTF(ibegin, it,  12) == true );
		CHECK ( (*it == wchar_t('c')) );
		CHECK ( Unicode::DecrementUTF(ibegin, it,   6) == true );
		CHECK ( (*it == wchar_t('t')) );
		CHECK ( Unicode::DecrementUTF(ibegin, it,  12) == false);
		CHECK ( it == ibegin );
	}
#endif

#ifdef DEKAF2_HAS_FULL_CPP_17
	SECTION("ConvertUTF")
	{
		std::wstring   sWstring    = L"testäöü test日本語abc中文Русский ..";
		std::string    s8Expected  =  "testäöü test日本語abc中文Русский ..";
		std::u16string s16Expected = u"testäöü test日本語abc中文Русский ..";
		std::u32string s32Expected = U"testäöü test日本語abc中文Русский ..";

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( Unicode::ConvertUTF(s8Expected, s8Output) );
			CHECK ( s8Output == s8Expected );
			CHECK ( Unicode::ConvertUTF(s16Expected, s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( Unicode::ConvertUTF(s32Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
		}

		{
			std::string s8Output;

			CHECK ( Unicode::ConvertUTF(s8Expected, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;

			CHECK ( Unicode::ConvertUTF(s8Expected, s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( Unicode::ConvertUTF(s16Output, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::string    s8Output;
			std::u32string s32Output;

			CHECK ( Unicode::ConvertUTF(s8Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( Unicode::ConvertUTF(s32Output, s8Output) );
			CHECK ( s8Output == s8Expected );
		}

		{
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( Unicode::ConvertUTF(s16Expected, s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( Unicode::ConvertUTF(s32Output, s16Output) );
			CHECK ( s16Output == s16Expected );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( Unicode::ConvertUTF(sWstring.c_str(), s32Output) );
			CHECK ( s32Output == s32Expected );
			CHECK ( Unicode::ConvertUTF(sWstring.c_str(), s16Output) );
			CHECK ( s16Output == s16Expected );
			CHECK ( Unicode::ConvertUTF(sWstring.c_str(), s8Output) );
			CHECK ( s8Output == s8Expected );
		}
	}
#endif

	SECTION("Latin1ToUTF")
	{
		std::string sLatin1 = "test\xe4\xf6\xfc";
		std::string sUTF8   = "testäöü";
		std::string s8Output;
		CHECK( Unicode::Latin1ToUTF(sLatin1, s8Output) );
		CHECK( s8Output == sUTF8 );
	}

	SECTION("ToLowerUTF")
	{
		std::string    s8Lower     =  "testäöü test日本語abc中文русский ..";
		std::u16string s16Lower    = u"testäöü test日本語abc中文русский ..";
		std::u32string s32Lower    = U"testäöü test日本語abc中文русский ..";
		std::string    s8Upper     =  "TESTÄÖÜ TEST日本語ABC中文РУССКИЙ ..";
		std::u16string s16Upper    = u"TESTÄÖÜ TEST日本語ABC中文РУССКИЙ ..";
		std::u32string s32Upper    = U"TESTÄÖÜ TEST日本語ABC中文РУССКИЙ ..";

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;
			
			CHECK ( Unicode::ToUpperUTF(s8Lower, s8Output) );
			CHECK ( s8Output == s8Upper );
			CHECK ( Unicode::ToUpperUTF(s16Lower, s16Output) );
			CHECK ( s16Output == s16Upper );
			CHECK ( Unicode::ToUpperUTF(s32Lower, s32Output) );
			CHECK ( s32Output == s32Upper );
		}

		{
			std::string    s8Output;
			std::u16string s16Output;
			std::u32string s32Output;

			CHECK ( Unicode::ToLowerUTF(s8Upper, s8Output) );
			CHECK ( s8Output == s8Lower );
			CHECK ( Unicode::ToLowerUTF(s16Upper, s16Output) );
			CHECK ( (s16Output == s16Lower) );
			CHECK ( Unicode::ToLowerUTF(s32Upper, s32Output) );
			CHECK ( (s32Output == s32Lower) );
		}
	}
}

