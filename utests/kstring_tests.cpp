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
		s.insert(10, "aaaaa");
		KString s2 = s.substr(20, 6);
		CHECK ( s2 == "" );
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
			it[0].Replace(it[1], it[2], true);
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

}

