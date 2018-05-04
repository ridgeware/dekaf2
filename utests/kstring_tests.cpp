#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <vector>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KString") {

	SECTION("Exception safety of KString")
	{
		KString s("12345");
		KString s2;
#ifdef DEKAF2_EXCEPTIONS
		CHECK_THROWS_AS ( s.insert(10, "aaaaa"), std::exception );
		CHECK_THROWS_AS( (s2 = s.substr(20, 6)), std::exception );
#else
		s.insert(10, "aaaaa");
		s2 = s.substr(20, 6);
		CHECK ( s2 == "" );
#endif
		KString s3(s, 30, 10);
		CHECK ( s3 == "" );
	}

	SECTION("Replace on KString")
	{
		// source, search, replace, target
		std::vector<std::vector<KString>> stest
		{
			{ "", "abc", "def", "" },
			{ " ", "abc", "def", " " },
			{ "   ", "abc", "def", "   " },
			{ "    ", "abc", "def", "    " },
			{ "    ", "abc", "defgh", "    " },

			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "56", "56abcdefghijklmnopqrstuvwxyz" },
			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "5678", "5678abcdefghijklmnopqrstuvwxyz" },
			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "567890", "567890abcdefghijklmnopqrstuvwxyz" },

			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "56", "abcd56efghijklmnopqrstuvwxyz" },
			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "5678", "abcd5678efghijklmnopqrstuvwxyz" },
			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "567890", "abcd567890efghijklmnopqrstuvwxyz" },

			{ "abcdefghijklmnopqrstuvwxyz0123", "0123", "56", "abcdefghijklmnopqrstuvwxyz56" },
			{ "abcdefghijklmnopqrstuvwxyz0123", "0123", "5678", "abcdefghijklmnopqrstuvwxyz5678" },
			{ "abcdefghijklmnopqrstuvwxyz0123", "0123", "567890", "abcdefghijklmnopqrstuvwxyz567890" },

			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", "0123", "56", "56abcdefg56hij5656klmnopqrstuvwxyz56" },
			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", "0123", "5678", "5678abcdefg5678hij56785678klmnopqrstuvwxyz5678" },
			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", "0123", "567890", "567890abcdefg567890hij567890567890klmnopqrstuvwxyz567890" },

			{ "0123", "0123", "56", "56" },
			{ "0123", "0123", "5678", "5678" },
			{ "0123", "0123", "567890", "567890" },

			{ "01230123", "0123", "56", "5656" },
			{ "01230123", "0123", "5678", "56785678" },
			{ "01230123", "0123", "567890", "567890567890" },
		};

		for (auto& it : stest)
		{
			it[0].Replace(it[1], it[2], 0, true);
			CHECK( it[0] == it[3] );
		}

	}

	SECTION("Replace single char on KString")
	{
		// source, search, replace, target
		std::vector<std::vector<KString>> stest
		{
			{ "", "abc", "def", "" },
			{ " ", "abc", "def", " " },
			{ "   ", "abc", "def", "   " },
			{ "    ", "abc", "def", "    " },
			{ "    ", "abc", "defgh", "    " },

			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "56", "5123abcdefghijklmnopqrstuvwxyz" },

			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "56", "abcd5123efghijklmnopqrstuvwxyz" },

			{ "abcdefghijklmnopqrstuvwxyz0120", "0123", "56", "abcdefghijklmnopqrstuvwxyz5125" },

			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0120", "0123", "56", "5123abcdefg5123hij51235123klmnopqrstuvwxyz5125" },

			{ "0123", "0123", "56", "5123" },

			{ "01230123", "0123", "56", "51235123" },
		};

		for (auto& it : stest)
		{
			it[0].Replace(it[1][0], it[2][0], 0, true);
			CHECK( it[0] == it[3] );
		}

	}

	SECTION("Replace single char with offset on KString")
	{
		// source, search, replace, target
		std::vector<std::vector<KString>> stest
		{
			{ "", "abc", "def", "" },
			{ " ", "abc", "def", " " },
			{ "   ", "abc", "def", "   " },
			{ "    ", "abc", "def", "    " },
			{ "    ", "abc", "defgh", "    " },

			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "56", "0123abcdefghijklmnopqrstuvwxyz" },

			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "56", "abcd5123efghijklmnopqrstuvwxyz" },

			{ "abcdefghijklmnopqrstuvwxyz0120", "0123", "56", "abcdefghijklmnopqrstuvwxyz5125" },

			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0120", "0123", "56", "0123abcdefg5123hij51235123klmnopqrstuvwxyz5125" },

			{ "0123", "0123", "56", "0123" },

			{ "01230123", "0123", "56", "01235123" },
		};

		for (auto& it : stest)
		{
			it[0].Replace(it[1][0], it[2][0], 2, true);
			CHECK( it[0] == it[3] );
		}

	}

	SECTION("Replace range of chars on KString")
	{
		// source, search, replace, target
		std::vector<std::vector<KString>> stest
		{
			{ "", "abc", "def", "" },
			{ " ", "abc", "def", " " },
			{ "   ", "abc", "def", "   " },
			{ "    ", "abc", "def", "    " },
			{ "    ", "abc", "defgh", "    " },

			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "56", "5555abcdefghijklmnopqrstuvwxyz" },

			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "56", "abcd5555efghijklmnopqrstuvwxyz" },

			{ "abcdefghijklmnopqrstuvwxyz0120", "0123", "56", "abcdefghijklmnopqrstuvwxyz5555" },

			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0120", "0123", "56", "5555abcdefg5555hij55555555klmnopqrstuvwxyz5555" },

			{ "0123", "0123", "56", "5555" },

			{ "01230123", "0123", "56", "55555555" },
		};

		for (auto& it : stest)
		{
			it[0].Replace(it[1], it[2][0], 0, true);
			CHECK( it[0] == it[3] );
		}

	}

	SECTION("Replace range of chars with offset on KString")
	{
		// source, search, replace, target
		std::vector<std::vector<KString>> stest
		{
			{ "", "abc", "def", "" },
			{ " ", "abc", "def", " " },
			{ "   ", "abc", "def", "   " },
			{ "    ", "abc", "def", "    " },
			{ "    ", "abc", "defgh", "    " },

			{ "0123abcdefghijklmnopqrstuvwxyz", "0123", "56", "0155abcdefghijklmnopqrstuvwxyz" },

			{ "abcd0123efghijklmnopqrstuvwxyz", "0123", "56", "abcd5555efghijklmnopqrstuvwxyz" },

			{ "abcdefghijklmnopqrstuvwxyz0120", "0123", "56", "abcdefghijklmnopqrstuvwxyz5555" },

			{ "0123abcdefg0123hij01230123klmnopqrstuvwxyz0120", "0123", "56", "0155abcdefg5555hij55555555klmnopqrstuvwxyz5555" },

			{ "0123", "0123", "56", "0155" },

			{ "01230123", "0123", "56", "01555555" },
		};

		for (auto& it : stest)
		{
			it[0].Replace(it[1], it[2][0], 2, true);
			CHECK( it[0] == it[3] );
		}

	}

	SECTION("KString Trimming")
	{
		std::vector<KString> stest
		{
			"",
			" ",
			"\t \r\n",
			"abcde",
			" abcde",
			"  abcde",
			"\t abcde",
			"\n\r\t abcde",
			"a abcde",
			" a abcde",
			"a\t abcde",
			"\na\r\t abcde",
			" abcde ",
			"  abcde  ",
			"\t abcde \t",
			"\n\r\t abcde \t\r\n",
			"a abcde a",
			" a abcde a ",
			"a\t abcde \t a",
			"\na\r\t abcde \t\ra\n",
		};

		SECTION("Left")
		{
			std::vector<KString> sexpect
			{
				"",
				"",
				"",
				"abcde",
				"abcde",
				"abcde",
				"abcde",
				"abcde",
				"a abcde",
				"a abcde",
				"a\t abcde",
				"a\r\t abcde",
				"abcde ",
				"abcde  ",
				"abcde \t",
				"abcde \t\r\n",
				"a abcde a",
				"a abcde a ",
				"a\t abcde \t a",
				"a\r\t abcde \t\ra\n",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimLeft();
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimLeft(" \t\r\n");
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Right")
		{
			std::vector<KString> sexpect
			{
				"",
				"",
				"",
				"abcde",
				" abcde",
				"  abcde",
				"\t abcde",
				"\n\r\t abcde",
				"a abcde",
				" a abcde",
				"a\t abcde",
				"\na\r\t abcde",
				" abcde",
				"  abcde",
				"\t abcde",
				"\n\r\t abcde",
				"a abcde a",
				" a abcde a",
				"a\t abcde \t a",
				"\na\r\t abcde \t\ra",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimRight();
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimRight(" \t\r\n");
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Left and Right")
		{
			std::vector<KString> sexpect
			{
				"",
				"",
				"",
				"abcde",
				"abcde",
				"abcde",
				"abcde",
				"abcde",
				"a abcde",
				"a abcde",
				"a\t abcde",
				"a\r\t abcde",
				"abcde",
				"abcde",
				"abcde",
				"abcde",
				"a abcde a",
				"a abcde a",
				"a\t abcde \t a",
				"a\r\t abcde \t\ra",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].Trim();
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].Trim(" \t\r\n");
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

	}

	SECTION("Regular Expression Replacement")
	{
		KString s;

		s = "I like 456ABC and 496EFK.";
		s.ReplaceRegex("([0-9]+)([A-Z]+)", "Num \\1 and Letters \\2", true);
		CHECK( s == "I like Num 456 and Letters ABC and Num 496 and Letters EFK.");

		s = "abcdefghi";
		s.ReplaceRegex("c.*f", "--", true);
		CHECK( s == "ab--ghi" );

		s = "abcdefghi";
		s.ReplaceRegex("c(.*)f", "-\\1-", true);
		CHECK( s == "ab-de-ghi" );
	}

	SECTION("find")
	{
		KString str("0123456  9abcdef h");
		CHECK( str.find(' ') == 7 );
		CHECK( str.find(" ") == 7 );
		CHECK( str.find(" 9") == 8 );
		CHECK( str.find(' ', 3) == 7 );
		CHECK( str.find(" 9", 3) == 8 );
		CHECK( str.find('0') == 0 );
		CHECK( str.find("0") == 0 );
		CHECK( str.find("01") == 0 );
		CHECK( str.find('h') == 17 );
		CHECK( str.find("h") == 17 );
		CHECK( str.find(" h") == 16 );
		CHECK( str.find('-') == KString::npos );
		CHECK( str.find("-") == KString::npos );
		CHECK( str.find("!-") == KString::npos );
		CHECK( str.find('1', 1) == 1 );
		CHECK( str.find("1", 1) == 1 );
		CHECK( str.find("12", 1) == 1 );
		CHECK( str.find('1', 2) == KString::npos );
		CHECK( str.find("1", 2) == KString::npos );
		CHECK( str.find("12", 2) == KString::npos );
	}

	SECTION("rfind")
	{
		KString str("0123456  9abcdef");
		CHECK( str.rfind(' ') == 8 );
		CHECK( str.rfind(" ") == 8 );
		CHECK( str.rfind(" 9") == 8 );
		CHECK( str.rfind('0') == 0 );
		CHECK( str.rfind("0") == 0 );
		CHECK( str.rfind("01") == 0 );
		CHECK( str.rfind('f') == 15 );
		CHECK( str.rfind("f") == 15 );
		CHECK( str.rfind("ef") == 14 );
		CHECK( str.rfind('f', 15) == 15 );
		CHECK( str.rfind("f", 15) == 15 );
		CHECK( str.rfind("ef", 15) == 14 );
		CHECK( str.rfind(' ', 12) == 8 );
		CHECK( str.rfind(" 9", 12) == 8 );
		CHECK( str.rfind(' ', 20) == 8 );
		CHECK( str.rfind(" 9", 20) == 8 );
		CHECK( str.rfind('-') == KString::npos );
		CHECK( str.rfind("-") == KString::npos );
		CHECK( str.rfind("!-") == KString::npos );
		CHECK( str.rfind('f', 14) == KString::npos );
		CHECK( str.rfind("f", 14) == KString::npos );
		CHECK( str.rfind("ef", 13) == KString::npos );
		CHECK( str.rfind('f', 15) == 15 );
		CHECK( str.rfind("f", 15) == 15 );
		CHECK( str.rfind("ef", 15) == 14 );
	}

	SECTION("find_first_of")
	{
		KString str("0123456  9abcdef h");
		CHECK( str.find_first_of(' ') == 7 );
		CHECK( str.find_first_of(" ") == 7 );
		CHECK( str.find_first_of(" d") == 7 );
		CHECK( str.find_first_of('0') == 0 );
		CHECK( str.find_first_of("0") == 0 );
		CHECK( str.find_first_of("02") == 0 );
		CHECK( str.find_first_of('h') == 17 );
		CHECK( str.find_first_of("h") == 17 );
		CHECK( str.find_first_of("h-") == 17 );
		CHECK( str.find_first_of("ab f") == 7 );
		CHECK( str.find_first_of("abf ") == 7 );
		CHECK( str.find_first_of('-') == KString::npos );
		CHECK( str.find_first_of("-") == KString::npos );
		CHECK( str.find_first_of("!-") == KString::npos );
	}

	SECTION("find_first_of with 0")
	{
		KString str("0123456 9abcdef h");
		str.insert(7, 1, '\0');
		CHECK( str.find_first_of(' ') == 8 );
		CHECK( str.find_first_of(" ") == 8 );
		CHECK( str.find_first_of(" d") == 8 );
		CHECK( str.find_first_of('0') == 0 );
		CHECK( str.find_first_of("0") == 0 );
		CHECK( str.find_first_of("02") == 0 );
		CHECK( str.find_first_of('h') == 17 );
		CHECK( str.find_first_of("h") == 17 );
		CHECK( str.find_first_of("h-", 3) == 17 );
		CHECK( str.find_first_of("ab f") == 8 );
		CHECK( str.find_first_of("abf ") == 8 );
		KString set("abf");
		set.insert(1, 1, '\0');
		set.erase(0, 1);
		set.insert(1, 1, 'a');
		CHECK( str.find_first_of(set) == 7 );
		set = "abf";
		set.insert(3, 1, '\0');
		CHECK( str.find_first_of(set) == 7 );
		set = "ef1";
		set.insert(2, 1, '\0');
		CHECK( str.find_first_of(set) == 1);
		CHECK( str.find_first_of('-') == KString::npos );
		CHECK( str.find_first_of("-") == KString::npos );
		CHECK( str.find_first_of("!-") == KString::npos );
	}

	SECTION("find_last_of")
	{
		KString str("0123456  9abcdef");
		CHECK( str.find_last_of(' ') == 8 );
		CHECK( str.find_last_of(" ") == 8 );
		CHECK( str.find_last_of(" 1") == 8 );
		CHECK( str.find_last_of('0') == 0 );
		CHECK( str.find_last_of("0") == 0 );
		CHECK( str.find_last_of("0-") == 0 );
		CHECK( str.find_last_of('f') == 15 );
		CHECK( str.find_last_of("f") == 15 );
		CHECK( str.find_last_of("fe") == 15 );
		CHECK( str.find_last_of("fe", 6) == KString::npos );
		CHECK( str.find_last_of("123", 9) == 3 );
		CHECK( str.find_last_of("12 3") == 8 );
		CHECK( str.find_last_of("123 ") == 8 );
		CHECK( str.find_last_of('-') == KString::npos );
		CHECK( str.find_last_of("-") == KString::npos );
		CHECK( str.find_last_of("!-") == KString::npos );
	}

	SECTION("find_first_not_of")
	{
		KString str("0123456 9abcdef h");
		CHECK( str.find_first_not_of(' ') == 0 );
		CHECK( str.find_first_not_of(" ") == 0 );
		CHECK( str.find_first_not_of(" d") == 0 );
		CHECK( str.find_first_not_of('0') == 1 );
		CHECK( str.find_first_not_of("0") == 1 );
		CHECK( str.find_first_not_of("02", 2) == 3 );
		CHECK( str.find_first_not_of("0123456789abcdef ") == 16 );
		CHECK( str.find_first_not_of("0123456789abcdefgh ") == KString::npos );
	}

	SECTION("find_first_not_of with 0")
	{
		KString str("0123456  9abcdef h");
		str.insert(7, 1, '\0');
		CHECK( str.find_first_not_of(' ') == 0 );
		CHECK( str.find_first_not_of(" ") == 0 );
		CHECK( str.find_first_not_of(" d") == 0 );
		CHECK( str.find_first_not_of('0') == 1 );
		CHECK( str.find_first_not_of("0") == 1 );
		CHECK( str.find_first_not_of("02") == 1 );
		CHECK( str.find_first_not_of("0123456789abcdef ") == 7 );
		KString set = "0123456789abcdefgh ";
		set.insert(3, 1, '\0');
		CHECK( str.find_first_not_of(set) == KString::npos );
	}

	SECTION("erase by index")
	{
		KString str("0123456789abcdefgh");
		KString strd = str;
		KString stre;
		strd.erase(2);
		stre = "01";
		CHECK( strd == stre );
		strd = str;
		strd.erase(0, 2);
		stre = "23456789abcdefgh";
		CHECK( strd == stre );
		strd = str;
		strd.erase(4, 2);
		stre = "01236789abcdefgh";
		CHECK( strd == stre );
		strd = str;
		strd.erase(16, 2);
		stre = "0123456789abcdef";
		CHECK( strd == stre );
	}

	SECTION("erase by iterator")
	{
		KString str("0123456789abcdefgh");
		KString strd = str;
		KString stre;
		auto it = strd.erase(strd.begin());
		stre = "123456789abcdefgh";
		CHECK( strd == stre );
		CHECK( it == strd.begin() );
		strd = str;
		it = strd.erase(strd.begin()+2);
		stre = "013456789abcdefgh";
		CHECK( strd == stre );
		strd = str;
		it = strd.erase(strd.end()-1);
		stre = "0123456789abcdefg";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
		strd = str;
		it = strd.erase(strd.end()-2);
		stre = "0123456789abcdefh";
		CHECK( strd == stre );
		strd = str;
		it = strd.erase(strd.end());
		stre = "0123456789abcdefgh";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
	}

	SECTION("erase by iterator range")
	{
		KString str("0123456789abcdefgh");
		KString strd = str;
		KString stre;
		auto it = strd.erase(strd.begin(), strd.begin() + 3);
		stre = "3456789abcdefgh";
		CHECK( strd == stre );
		CHECK( it == strd.begin() );
		strd = str;
		it = strd.erase(strd.begin()+2, strd.begin() + 4);
		stre = "01456789abcdefgh";
		CHECK( strd == stre );
		strd = str;
		it = strd.erase(strd.end()-1, strd.end());
		stre = "0123456789abcdefg";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
		strd = str;
		it = strd.erase(strd.end()-2, strd.end());
		stre = "0123456789abcdef";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
		strd = str;
		it = strd.erase(strd.end(), strd.end());
		stre = "0123456789abcdefgh";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
	}

	SECTION("fmt::format")
	{
		KString s;
		KString str = "a string";
		s.Printf("This is %s", str);
		CHECK( s == "This is a string" );
		s.Format("This is {}", str);
		CHECK( s == "This is a string" );
	}

	SECTION("conversion functions for KString")
	{
		KString s;
		s = "1234567";
		CHECK( s.Int32()     == 1234567 );
		CHECK( s.Int64()     == 1234567 );
#ifdef DEKAF2_HAS_INT128
		// CHECK has issues with 128 bit integers, so we cast them down
		int64_t ii = s.Int128();
		CHECK( ii            == 1234567 );
#endif
		CHECK( s.UInt32()    == 1234567 );
		CHECK( s.UInt64()    == 1234567 );
#ifdef DEKAF2_HAS_INT128
		uint64_t uii = s.Int128();
		CHECK( uii           == 1234567 );
#endif
		s = "-1234567";
		CHECK( s.Int32()     == -1234567 );
		CHECK( s.Int64()     == -1234567 );
#ifdef DEKAF2_HAS_INT128
		ii = s.Int128();
		CHECK( ii            == -1234567 );
#endif
		CHECK( s.UInt32()    == -1234567U );
		CHECK( s.UInt64()    == static_cast<uint64_t>(-1234567) );
#ifdef DEKAF2_HAS_INT128
		uii = s.UInt128();
		CHECK( uii           == static_cast<uint64_t>(-1234567) );
#endif
		s = "123456789012345";
		CHECK( s.Int64()     == 123456789012345 );
#ifdef DEKAF2_HAS_INT128
		ii = s.Int128();
		CHECK( ii            == 123456789012345 );
#endif
		CHECK( s.UInt64()    == static_cast<uint64_t>(123456789012345) );
#ifdef DEKAF2_HAS_INT128
		uii = s.UInt128();
		CHECK( uii           == static_cast<uint64_t>(123456789012345) );
#endif
		s = "-123456789012345";
		CHECK( s.Int64()     == -123456789012345 );
#ifdef DEKAF2_HAS_INT128
		ii = s.Int128();
		CHECK( ii            == -123456789012345 );
#endif
		CHECK( s.UInt64()    == static_cast<uint64_t>(-123456789012345) );
#ifdef DEKAF2_HAS_INT128
		uii = s.UInt128();
		CHECK( uii           == static_cast<uint64_t>(-123456789012345) );
#endif
		
		s = "-12.34567";
		CHECK ( s.Float()    == -12.34567f );
		CHECK ( s.Double()   == -12.34567 );
	}

	SECTION("to_string()")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {          "0",  0         },
		    {  "123456789",  123456789 },
		    { "-123456789", -123456789 },
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second) == it.first );
		}
	}

	SECTION("to_hexstring()")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {         "FF",  255         },
		    {         "00",  0           },
		    {         "08",  8           },
		    {         "0F",  15          },
		    { "0123456789",  4886718345  },
		    { "12345ABCD2",  78187773138 },
		    {   "ABCDE123",  2882396451  }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_hexstring(it.second) == it.first );
		}
	}

	SECTION("to_hexstring() lowercase")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {         "ff",  255         },
		    {         "00",  0           },
		    {         "08",  8           },
		    {         "0f",  15          },
		    { "0123456789",  4886718345  },
		    { "12345abcd2",  78187773138 },
		    {   "abcde123",  2882396451  }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_hexstring(it.second, true, false) == it.first );
		}
	}

	SECTION("to_hexstring() lowercase no zeropad")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {         "ff",  255         },
		    {          "0",  0           },
		    {          "8",  8           },
		    {          "f",  15          },
		    {  "123456789",  4886718345  },
		    { "12345abcd2",  78187773138 },
		    {   "abcde123",  2882396451  }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_hexstring(it.second, false, false) == it.first );
		}
	}

	SECTION("Left")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "1234567890",        "123" ,  3 },
			{ "1234567890",          "1" ,  1 },
			{ "1234567890",           "" ,  0 },
			{ "1234567890", "1234567890" , 13 },
			{           "",           "" ,  3 },
			{           "",           "" ,  1 },
			{           "",           "" ,  0 },
			{           "",           "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.Left(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("Right")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "1234567890",        "890" ,  3 },
			{ "1234567890",          "0" ,  1 },
			{ "1234567890",           "" ,  0 },
			{ "1234567890", "1234567890" , 13 },
			{           "",           "" ,  3 },
			{           "",           "" ,  1 },
			{           "",           "" ,  0 },
			{           "",           "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.Right(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("Mid")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  start;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "1234567890",        "123" ,  0,  3 },
			{ "1234567890",          "1" ,  0,  1 },
			{ "1234567890",           "" ,  0,  0 },
			{ "1234567890", "1234567890" ,  0, 13 },
			{ "1234567890",        "890" ,  7,  3 },
			{ "1234567890",          "0" ,  9,  1 },
			{ "1234567890",           "" , 10,  0 },
			{ "1234567890",           "" , 13, 13 },
			{ "1234567890",        "456" ,  3,  3 },
			{ "1234567890",          "4" ,  3,  1 },
			{ "1234567890",           "" ,  3,  0 },
			{ "1234567890",    "4567890" ,  3, 13 },
			{           "",           "" ,  0,  3 },
			{           "",           "" ,  0,  1 },
			{           "",           "" ,  0,  0 },
			{           "",           "" ,  0, 13 },
			{           "",           "" ,  7,  3 },
			{           "",           "" ,  9,  1 },
			{           "",           "" , 10,  0 },
			{           "",           "" , 13, 13 },
			{           "",           "" ,  3,  3 },
			{           "",           "" ,  3,  1 },
			{           "",           "" ,  3,  0 },
			{           "",           "" ,  3, 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.Mid(it.start, it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("MakeUpper")
	{
		struct parms_t
		{
			KString input;
			KString output;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "HELLO"    , "HELLO"   },
			{ "ÖÄÜ"      , "ÖÄÜ"     },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			s.MakeUpper();
			CHECK ( s == it.output );
		}
	}

	SECTION("MakeLower")
	{
		struct parms_t
		{
			KString output;
			KString input;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "hello"    , "hello"   },
			{ "öäü"      , "öäü"     },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			s.MakeLower();
			CHECK ( s == it.output );
		}
	}

	SECTION("ToUpper")
	{
		struct parms_t
		{
			KString input;
			KString output;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "HELLO"    , "HELLO"   },
			{ "ÖÄÜ"      , "ÖÄÜ"     },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToUpper();
			CHECK ( o == it.output );
		}
	}

	SECTION("ToLower")
	{
		struct parms_t
		{
			KString output;
			KString input;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "hello"    , "hello"   },
			{ "öäü"      , "öäü"     },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToLower();
			CHECK ( o == it.output );
		}
	}

}

