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
		auto sBytes = Unicode::UTF8ToUTF16Bytes(sUTF8);
		CHECK(                 sBytes.size() == 54    );
//		CHECK( sBytes == "" );
		auto sUTF8New = Unicode::UTF16BytesToUTF8(sBytes);
		CHECK(                 sUTF8New.size() == 47  );
		CHECK(                 sUTF8New == sUTF8      );
	}

}

