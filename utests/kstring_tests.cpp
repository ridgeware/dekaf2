#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/kctype.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kstack.h>
#include <dekaf2/ksystem.h>
#include <vector>
#include <list>
#include <iostream>
#include <cctype>
#include <cwctype>

using namespace dekaf2;

TEST_CASE("KString") {

	SECTION("large allocation")
	{
		// Run this test only in one of 20 test runs. It takes a long
		// time and needs only be verified after changes on the
		// string allocation scheme
		if (kRandom(1, 20) == 1)
		{
			KString s;

			// we want 1GB, but drop on machines with lower phys memory
			auto iMaxSize = std::min(std::size_t(1*1024*1024*1024), kGetPhysicalMemory() / 4);

			for (;s.size() < iMaxSize;)
			{
				s += "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
			}

			CHECK ( s.size() >= iMaxSize );
		}
	}

	SECTION("self assignment")
	{
		// 50 chars to get into a real allocated buffer
		static constexpr KStringView sv = "1234567890123456789012345678901234567890123456789012345678901234567890";
		{
			KString s(sv);
			KString& s2(s);
			s = s2;
			CHECK ( s == sv );
		}

		// now through a string view:
		{
			KString s(sv);
			KStringView sv2(s);
			s = sv2;
			CHECK ( s == sv );
		}
	}

	SECTION("substr")
	{
		KString s = "1234567890123456789012345678901234567890";
		CHECK ( s.substr(1, 10) == "2345678901" );
		// test the rvalue version!
		CHECK ( KString("1234567890123456789012345678901234567890").substr(1, 10) == "2345678901" );
	}

/*
 * This blows up log output with a trace. Only test on request.
 *
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
*/

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

			{ "0123", "0", "5", "5123" },
			{ "0123", "0123", "56", "56" },
			{ "0123", "0123", "5678", "5678" },
			{ "0123", "0123", "567890", "567890" },

			{ "01230123", "1", "5", "05230523" },
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
					INFO ( iCount );
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimLeft(" \t\r\n");
					INFO ( iCount );
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
					INFO ( iCount );
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].TrimRight(" \t\r\n");
					INFO ( iCount );
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
					INFO ( iCount );
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

			SECTION("char*") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					stest[iCount].Trim(" \t\r\n");
					INFO ( iCount );
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

	}

	SECTION("KString Collapse")
	{
		std::vector<std::vector<KString>> stest
		{
			{ "", " \f\n\r\t\v\b", " ", "" },
			{ " ", " \f\n\r\t\v\b", " ", " " },
			{ " ", " \f\n\r\t\v\b", "-", "-" },
			{ "\t \r\n", " \f\n\r\t\v\b", " ", " " },
			{ "abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ " abcde", " \f\n\r\t\v\b", " ", " abcde" },
			{ "  abcde", " \f\n\r\t\v\b", " ", " abcde" },
			{ "\t abcde", " \f\n\r\t\v\b", " ", " abcde" },
			{ "\n\r\t abcde", " \f\n\r\t\v\b", " ", " abcde" },
			{ "a abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "a  abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ " a  abcde", " \f\n\r\t\v\b", " ", " a abcde" },
			{ "  a  abcde", " \f\n\r\t\v\b", " ", " a abcde" },
			{ "a\t abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "\na\r\t abcde", " \f\n\r\t\v\b", " ", " a abcde" },
			{ " abcde ", " \f\n\r\t\v\b", " ", " abcde " },
			{ "  abcde  ", " \f\n\r\t\v\b", " ", " abcde " },
			{ "\t abcde \t", " \f\n\r\t\v\b", " ", " abcde " },
			{ "\n\r\t abcde \t\r\n", " \f\n\r\t\v\b", " ", " abcde " },
			{ "a abcde a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "a  abcde  a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ " a abcde a ", " \f\n\r\t\v\b", " ", " a abcde a " },
			{ "  a  abcde  a  ", " \f\n\r\t\v\b", " ", " a abcde a " },
			{ "  a  abcde  a  ", " \f\n\r\t\v\b", "-", "-a-abcde-a-" },
			{ "a\t abcde \t a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "\na\r\t abcde \t\ra\n", " \f\n\r\t\v\b", " ", " a abcde a " }
		};

		for (size_t iCount = 0; iCount < stest.size(); ++iCount)
		{
			KString sCollapse{ stest[iCount][0] };
			sCollapse.Collapse(stest[iCount][1], stest[iCount][2][0]);
			INFO ( std::to_string(iCount) );
			CHECK( stest[iCount][3] == sCollapse );
		}

		for (size_t iCount = 0; iCount < stest.size(); ++iCount)
		{
			// only take the tests that collapse to space
			if (stest[iCount][2][0] == ' ')
			{
				KString sCollapse{ stest[iCount][0] };
				sCollapse.Collapse();
				INFO ( std::to_string(iCount) );
				CHECK( stest[iCount][3] == sCollapse );
			}
		}
	}

	SECTION("KString CollapseAndTrim")
	{
		std::vector<std::vector<KString>> stest
		{
			{ "", " \f\n\r\t\v\b", " ", "" },
			{ " ", " \f\n\r\t\v\b", " ", "" },
			{ " ", " \f\n\r\t\v\b", "-", "" },
			{ "\t \r\n", " \f\n\r\t\v\b", " ", "" },
			{ "abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ " abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ "  abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ "\t abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ "\n\r\t abcde", " \f\n\r\t\v\b", " ", "abcde" },
			{ "a abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "a  abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ " a  abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "  a  abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "a\t abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ "\na\r\t abcde", " \f\n\r\t\v\b", " ", "a abcde" },
			{ " abcde ", " \f\n\r\t\v\b", " ", "abcde" },
			{ "  abcde  ", " \f\n\r\t\v\b", " ", "abcde" },
			{ "\t abcde \t", " \f\n\r\t\v\b", " ", "abcde" },
			{ "\n\r\t abcde \t\r\n", " \f\n\r\t\v\b", " ", "abcde" },
			{ "a abcde a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "a  abcde  a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ " a abcde a ", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "  a  abcde  a  ", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "  a  abcde  a  ", " \f\n\r\t\v\b", "-", "a-abcde-a" },
			{ "a\t abcde \t a", " \f\n\r\t\v\b", " ", "a abcde a" },
			{ "\na\r\t abcde \t\ra\n", " \f\n\r\t\v\b", " ", "a abcde a" }
		};

		for (size_t iCount = 0; iCount < stest.size(); ++iCount)
		{
			KString sCollapse{ stest[iCount][0] };
			sCollapse.CollapseAndTrim(stest[iCount][1], stest[iCount][2][0]);
			INFO ( std::to_string(iCount) );
			CHECK( stest[iCount][3] == sCollapse );
		}

		for (size_t iCount = 0; iCount < stest.size(); ++iCount)
		{
			// only take the tests that collapse to space
			if (stest[iCount][2][0] == ' ')
			{
				KString sCollapse{ stest[iCount][0] };
				sCollapse.CollapseAndTrim();
				INFO ( std::to_string(iCount) );
				CHECK( stest[iCount][3] == sCollapse );
			}
		}
	}

	SECTION("Regular Expression Replacement")
	{
		KString s;

		s = "I like 456ABC and 496EFK.";
		auto iCount = s.ReplaceRegex("([0-9]+)([A-Z]+)", "Num \\1 and Letters \\2", true);
		CHECK( iCount == 2 );
		CHECK( s == "I like Num 456 and Letters ABC and Num 496 and Letters EFK.");

		s = "I like 456ABC.";
		iCount = s.ReplaceRegex("([0-9]+)([A-Z]+)", "the \\0", true);
		CHECK( iCount == 1 );
		CHECK( s == "I like the 456ABC.");

		s = "I like 456ABC.";
		iCount = s.ReplaceRegex("([0-9]+)([A-Z]+)", "the \\2", true);
		CHECK( iCount == 1 );
		CHECK( s == "I like the ABC.");

		s = "abcdefghi";
		iCount = s.ReplaceRegex("c.*f", "--", true);
		CHECK( iCount == 1 );
		CHECK( s == "ab--ghi" );

		s = "abcdefghi";
		iCount = s.ReplaceRegex("c(.*)f", "-\\1-", true);
		CHECK( iCount == 1 );
		CHECK( s == "ab-de-ghi" );

		s = "abcdefghi";
		iCount = s.ReplaceRegex("cde", "", true);
		CHECK( iCount == 1 );
		CHECK( s == "abfghi" );

		s = "abcdefghiabcdefghi";
		iCount = s.ReplaceRegex("cde", "", true);
		CHECK( iCount == 2 );
		CHECK( s == "abfghiabfghi" );

		s = "a\nbcdefghi";
		iCount = s.ReplaceRegex("(?i)CdE", "", true);
		CHECK( iCount == 1 );
		CHECK( s == "a\nbfghi" );

		s = "a\nbcdefgh\niabcdefghi";
		iCount = s.ReplaceRegex("(?i)CdE", "", true);
		CHECK( iCount == 2 );
		CHECK( s == "a\nbfgh\niabfghi" );
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
		str = "abcdefgabcdefiuhsgw";
		CHECK( str.rfind("abcdefg") == 0 );
		CHECK( str.rfind("abcdef") == 7 );
		CHECK( str.rfind("abcabcde") == KString::npos );
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

	SECTION("find_last_not_of")
	{
		KString str("0123456 9abcdef h");
		CHECK( str.find_last_not_of(' ') == 16 );
		CHECK( str.find_last_not_of(" ") == 16 );
		CHECK( str.find_last_not_of("fh d") == 13 );
		CHECK( str.find_last_not_of('h') == 15 );
		CHECK( str.find_last_not_of("h") == 15 );
		CHECK( str.find_last_not_of(" h", 13) == 13 );
		CHECK( str.find_last_not_of("123456789abcdefh ") == 0 );
		CHECK( str.find_last_not_of("0123456789abcdefgh ") == KString::npos );
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
        CHECK( it == strd.begin()+2 );
		stre = "013456789abcdefgh";
		CHECK( strd == stre );
		strd = str;
		it = strd.erase(strd.end()-1);
		stre = "0123456789abcdefg";
		CHECK( strd == stre );
		CHECK( it == strd.end() );
		strd = str;
		it = strd.erase(strd.end()-2);
        CHECK( it == strd.begin()+16 );
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
        CHECK( it == strd.begin()+2 );
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
		CHECK( s.UInt32()    == static_cast<uint32_t>(-1234567) );
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

	SECTION("to_string(base1)")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    { "", 0                   },
		    { "", 123456789           },
		    { "", -123456789          },
			{ "", 7021572290552076995 }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second, 1, false, false) == it.first );
		}
	}

	SECTION("to_string(base37)")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    { "", 0                   },
		    { "", 123456789           },
		    { "", -123456789          },
			{ "", 7021572290552076995 }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second, 37, false, false) == it.first );
		}
	}

	SECTION("to_string(base2)")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {                                   "0", 0                   },
		    {         "111010110111100110100010101", 123456789           },
		    {        "-111010110111100110100010101", -123456789          },
			{ "110000101110001101000101100101111011111000101010001101011000011" ,
				                                     7021572290552076995 }
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second, 2, false, false) == it.first );
		}
	}

	SECTION("to_string(base36)")
	{
		using stype = KString;
		std::vector<std::pair<stype, uint64_t>> svector = {
		    {              "0", 0                     },
		    {         "21i3v9", 123456789             },
		    {  "3w5e11243acx7", -123456789            },
			{ "1hch7n7gak72b" , 7021572290552076995   },
			{ "28rxu7q22x6gr" , 10631889302401681659u },
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second, 36, false, false) == it.first );
		}
	}

	SECTION("to_string(base36) signed")
	{
		using stype = KString;
		std::vector<std::pair<stype, int64_t>> svector = {
		    {              "0", 0                     },
		    {         "21i3v9", 123456789             },
		    {        "-21i3v9", -123456789            },
			{ "1hch7n7gak72b" , 7021572290552076995   },
		};

		for (const auto& it : svector)
		{
			CHECK ( KString::to_string(it.second, 36, false, false) == it.first );
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

	SECTION("RValue Left")
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
			auto sv = std::move(s).Left(it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
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

	SECTION("RValue Right")
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
			auto sv = std::move(s).Right(it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
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

	SECTION("RValue Mid")
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
			auto sv = std::move(s).Mid(it.start, it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("Mid with one parm")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
			size_t  start;
		};

		std::vector<parms_t> pvector {
			{ "1234567890", "1234567890" ,  0 },
			{ "1234567890",  "234567890" ,  1 },
			{ "1234567890",   "34567890" ,  2 },
			{ "1234567890",        "890" ,  7 },
			{ "1234567890",          "0" ,  9 },
			{ "1234567890",           "" , 10 },
			{ "1234567890",           "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.Mid(it.start);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RValue Mid with one parm")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
			size_t  start;
		};

		std::vector<parms_t> pvector {
			{ "1234567890", "1234567890" ,  0 },
			{ "1234567890",  "234567890" ,  1 },
			{ "1234567890",   "34567890" ,  2 },
			{ "1234567890",        "890" ,  7 },
			{ "1234567890",          "0" ,  9 },
			{ "1234567890",           "" , 10 },
			{ "1234567890",           "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			auto sv = std::move(s).Mid(it.start);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("LeftUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",              "test日本語" ,  7 },
			{ "test日本語abc中文Русский",                      "t" ,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" , 25 },
			{                       "",                       "" ,  3 },
			{                       "",                       "" ,  1 },
			{                       "",                       "" ,  0 },
			{                       "",                       "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.LeftUTF8(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RValue LeftUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",              "test日本語" ,  7 },
			{ "test日本語abc中文Русский",                      "t" ,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" , 25 },
			{                       "",                       "" ,  3 },
			{                       "",                       "" ,  1 },
			{                       "",                       "" ,  0 },
			{                       "",                       "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			auto sv = std::move(s).LeftUTF8(it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("RightUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",             "中文Русский" ,  9 },
			{ "test日本語abc中文Русский",                      "й" ,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" , 25 },
			{                       "",                       "" ,  3 },
			{                       "",                       "" ,  1 },
			{                       "",                       "" ,  0 },
			{                       "",                       "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringViewZ sv = s.RightUTF8(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RValue RightUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",             "中文Русский" ,  9 },
			{ "test日本語abc中文Русский",                      "й" ,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" , 25 },
			{                       "",                       "" ,  3 },
			{                       "",                       "" ,  1 },
			{                       "",                       "" ,  0 },
			{                       "",                       "" , 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			auto sv = std::move(s).RightUTF8(it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("MidUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  start;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",              "test日本語" ,  0,  7 },
			{ "test日本語abc中文Русский",                      "t" ,  0,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" ,  0, 25 },
			{ "test日本語abc中文Русский",             "t日本語abc中" ,  3,  8 },
			{ "test日本語abc中文Русский",                      "日" ,  4,  1 },
			{ "test日本語abc中文Русский",                       "" ,  3,  0 },
			{ "test日本語abc中文Русский",    "t日本語abc中文Русский" ,  3, 25 },
			{                       "",                       "" ,  0,  3 },
			{                       "",                       "" ,  0,  1 },
			{                       "",                       "" ,  0,  0 },
			{                       "",                       "" ,  0, 13 },
			{                       "",                       "" ,  7,  3 },
			{                       "",                       "" ,  9,  1 },
			{                       "",                       "" , 10,  0 },
			{                       "",                       "" , 13, 13 },
			{                       "",                       "" ,  3,  3 },
			{                       "",                       "" ,  3,  1 },
			{                       "",                       "" ,  3,  0 },
			{                       "",                       "" ,  3, 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.MidUTF8(it.start, it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RValue MidUTF8")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  start;
			size_t  count;
		};

		std::vector<parms_t> pvector = {
			{ "test日本語abc中文Русский",              "test日本語" ,  0,  7 },
			{ "test日本語abc中文Русский",                      "t" ,  0,  1 },
			{ "test日本語abc中文Русский",                       "" ,  0,  0 },
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" ,  0, 25 },
			{ "test日本語abc中文Русский",             "t日本語abc中" ,  3,  8 },
			{ "test日本語abc中文Русский",                      "日" ,  4,  1 },
			{ "test日本語abc中文Русский",                       "" ,  3,  0 },
			{ "test日本語abc中文Русский",    "t日本語abc中文Русский" ,  3, 25 },
			{                       "",                       "" ,  0,  3 },
			{                       "",                       "" ,  0,  1 },
			{                       "",                       "" ,  0,  0 },
			{                       "",                       "" ,  0, 13 },
			{                       "",                       "" ,  7,  3 },
			{                       "",                       "" ,  9,  1 },
			{                       "",                       "" , 10,  0 },
			{                       "",                       "" , 13, 13 },
			{                       "",                       "" ,  3,  3 },
			{                       "",                       "" ,  3,  1 },
			{                       "",                       "" ,  3,  0 },
			{                       "",                       "" ,  3, 13 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			auto sv = std::move(s).MidUTF8(it.start, it.count);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("MidUTF8 with one parm")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  start;
		};

		std::vector<parms_t> pvector {
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" ,  0 },
			{ "test日本語abc中文Русский",  "est日本語abc中文Русский" ,  1 },
			{ "test日本語abc中文Русский",       "本語abc中文Русский" ,  5 },
			{ "test日本語abc中文Русский",            "c中文Русский" ,  9 },
			{ "test日本語abc中文Русский",                       "" , 19 },
			{ "test日本語abc中文Русский",                       "" , 27 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KStringView sv = s.MidUTF8(it.start);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RValue MidUTF8 with one parm")
	{
		struct parms_t
		{
			KString input;
			KString output;
			size_t  start;
		};

		std::vector<parms_t> pvector {
			{ "test日本語abc中文Русский", "test日本語abc中文Русский" ,  0 },
			{ "test日本語abc中文Русский",  "est日本語abc中文Русский" ,  1 },
			{ "test日本語abc中文Русский",       "本語abc中文Русский" ,  5 },
			{ "test日本語abc中文Русский",            "c中文Русский" ,  9 },
			{ "test日本語abc中文Русский",                       "" , 19 },
			{ "test日本語abc中文Русский",                       "" , 27 },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			auto sv = std::move(s).MidUTF8(it.start);
			static_assert(std::is_same<decltype(sv), KString>::value, "return type is not a KString?");
			CHECK ( sv == it.output );
		}
	}

	SECTION("TestLocale")
	{
#if defined(DEKAF2_IS_OSX) 
		{
			INFO(std::locale().name());
			wint_t ch = 246; // oe
			CHECK(std::islower(ch));
			CHECK(std::toupper(ch) == 214);
			CHECK(std::iswlower(ch));
			CHECK(std::towupper(ch) == 214);
		}
#endif
		{
			wint_t ch = 246; // oe
			CHECK(kIsLower(ch));
			CHECK(kToUpper(ch) == 214);
		}
		{
			char ch = static_cast<char>(0xf6); // oe
			CHECK(kIsLower(ch));
			CHECK(static_cast<unsigned char>(kToUpper(ch)) == 0xd6);
		}
		{
			char ch = static_cast<char>(0xf6); // oe
			CHECK(KASCII::kIsLower(ch) == false);
			CHECK(static_cast<unsigned char>(KASCII::kToUpper(ch)) == 0xf6);
		}
	}

	SECTION("MakeUpper")
	{
		struct parms_t
		{
			KString input;
			KString output;
		};

		std::vector<parms_t> pvector =
		{
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "HELLO"    , "HELLO"   },
			{ "ÖÄÜ"      , "ÖÄÜ"     },
			{ "ⴀⴃ"     , "ႠႣ"      },
			{ "ალჯ"      , "ᲐᲚᲯ"     },
			{ "δθμ"      , "ΔΘΜ"     },
			{ "οδυσσευς" , "ΟΔΥΣΣΕΥΣ"},
			{ "οδυσσευσ" , "ΟΔΥΣΣΕΥΣ"},
			{ "ꝗɠŋա𐐲𐑅"   , "ꝖƓŊԱ𐐊𐐝" },
			{ "жъꚁꙭ"     , "ЖЪꚀꙬ"   },
			{ "ϡϧϯꜻꜿⱥⱡⱦ" , "ϠϦϮꜺꜾȺⱠȾ"},
			{ "ꜩꝙꞣꞇ"     , "ꜨꝘꞢꞆ"  },
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

		std::vector<parms_t> pvector =
		{
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "hello"    , "hello"   },
			{ "öäü"      , "öäü"     },
			{ "ⴀⴃ"     , "ႠႣ"      },
			{ "ალჯ"      , "ᲐᲚᲯ"     },
			{ "δθμ"      , "ΔΘΜ"     },
			{ "οδυσσευσ" , "ΟΔΥΣΣΕΥΣ"},
			{ "ꝗɠŋա𐐲𐑅"   , "ꝖƓŊԱ𐐊𐐝" },
			{ "жъꚁꙭ"     , "ЖЪꚀꙬ"   },
			{ "ϡϧϯꜻꜿⱥⱡⱦ" , "ϠϦϮꜺꜾȺⱠȾ"},
			{ "ꜩꝙꞣꞇ"     , "ꜨꝘꞢꞆ"  },
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

		std::vector<parms_t> pvector =
		{
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "HELLO"    , "HELLO"   },
			{ "ÖÄÜ"      , "ÖÄÜ"     },
			{ "ⴀⴃ"     , "ႠႣ"      },
			{ "ალჯ"      , "ᲐᲚᲯ"     },
			{ "δθμ"      , "ΔΘΜ"     },
			{ "οδυσσευς" , "ΟΔΥΣΣΕΥΣ"},
			{ "οδυσσευσ" , "ΟΔΥΣΣΕΥΣ"},
			{ "ꝗɠŋա𐐲𐑅"   , "ꝖƓŊԱ𐐊𐐝" },
			{ "жъꚁꙭ"     , "ЖЪꚀꙬ"   },
			{ "ϡϧϯꜻꜿⱥⱡⱦ" , "ϠϦϮꜺꜾȺⱠȾ"},
			{ "ꜩꝙꞣꞇ"     , "ꜨꝘꞢꞆ"  },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToUpper();
			CHECK ( o == it.output );
			// check rvalue version
			o = KString(it.input).ToUpper();
			CHECK ( o == it.output );
		}
	}

	SECTION("ToUpperASCII")
	{
		struct parms_t
		{
			KString input;
			KString output;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "öäü"     },
			{ "HELLO"    , "HELLO"   },
			{ "ÖÄÜ"      , "ÖÄÜ"     },
			{ "ⴀⴃ"     , "ⴀⴃ"      },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToUpperASCII();
			CHECK ( o == it.output );
			// check rvalue version
			o = KString(it.input).ToUpperASCII();
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

		std::vector<parms_t> pvector =
		{
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "ÖÄÜ"     },
			{ "hello"    , "hello"   },
			{ "öäü"      , "öäü"     },
			{ "ⴀⴃ"     , "ႠႣ"      },
			{ "ალჯ"      , "ᲐᲚᲯ"     },
			{ "δθμ"      , "ΔΘΜ"     },
			{ "οδυσσευσ" , "ΟΔΥΣΣΕΥΣ"},
			{ "ꝗɠŋա𐐲𐑅"   , "ꝖƓŊԱ𐐊𐐝" },
			{ "жъꚁꙭ"     , "ЖЪꚀꙬ"   },
			{ "ϡϧϯꜻꜿⱥⱡⱦ" , "ϠϦϮꜺꜾȺⱠȾ"},
			{ "ꜩꝙꞣꞇ"     , "ꜨꝘꞢꞆ"  },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToLower();
			CHECK ( o == it.output );
			// check rvalue version
			o = KString(it.input).ToLower();
			CHECK ( o == it.output );
		}
	}

	SECTION("ToLowerASCII")
	{
		struct parms_t
		{
			KString output;
			KString input;
		};

		std::vector<parms_t> pvector = {
			{ ""         , ""        },
			{ "hello"    , "HELLO"   },
			{ "öäü"      , "öäü"     },
			{ "hello"    , "hello"   },
			{ "öäü"      , "öäü"     },
		};

		for (const auto& it : pvector)
		{
			KString s(it.input);
			KString o = s.ToLowerASCII();
			CHECK ( o == it.output );
			// check rvalue version
			o = KString(it.input).ToLowerASCII();
			CHECK ( o == it.output );
		}
	}

	SECTION("Left")
	{
		KString s("1234567890");
		CHECK ( s.Left(0)  == ""           );
		CHECK ( s.Left(4)  == "1234"       );
		CHECK ( s.Left(12) == "1234567890" );
	}

	SECTION("Right")
	{
		KString s("1234567890");
		CHECK ( s.Right(0)  == ""           );
		CHECK ( s.Right(4)  == "7890"       );
		CHECK ( s.Right(12) == "1234567890" );
	}

	SECTION("Mid")
	{
		KString s("1234567890");
		CHECK ( s.Mid(4, 0)  == ""       );
		CHECK ( s.Mid(12, 2) == ""       );
		CHECK ( s.Mid(4, 4)  == "5678"   );
		CHECK ( s.Mid(4, 9)  == "567890" );
	}

	SECTION ("starts_with")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.starts_with("This") == true );
		CHECK ( sLine.starts_with("This is") == true );
		CHECK ( sLine.starts_with("his") == false );
		CHECK ( sLine.starts_with("is") == false );
		CHECK ( sLine.starts_with("data") == false );
		CHECK ( sLine.starts_with("") == true );
		CHECK ( sLine.starts_with('T') == true );
		CHECK ( sLine.starts_with('h') == false );
	}

	SECTION ("ends_with")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.ends_with("This") == false );
		CHECK ( sLine.ends_with("This is") == false );
		CHECK ( sLine.ends_with("his") == false );
		CHECK ( sLine.ends_with("is") == false );
		CHECK ( sLine.ends_with("data") == true );
		CHECK ( sLine.ends_with("ome data") == true );
		CHECK ( sLine.ends_with("") == true );
		CHECK ( sLine.ends_with('a') == true );
		CHECK ( sLine.ends_with('t') == false );
	}

	SECTION ("contains")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.contains("This") == true );
		CHECK ( sLine.contains("This is") == true );
		CHECK ( sLine.contains("his") == true );
		CHECK ( sLine.contains("is") == true );
		CHECK ( sLine.contains("data") == true );
		CHECK ( sLine.contains("nothing") == false );
		CHECK ( sLine.contains("") == true );
	}

	SECTION ("StartsWith")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.StartsWith("This") == true );
		CHECK ( sLine.StartsWith("This is") == true );
		CHECK ( sLine.StartsWith("his") == false );
		CHECK ( sLine.StartsWith("is") == false );
		CHECK ( sLine.StartsWith("data") == false );
		CHECK ( sLine.StartsWith("") == true );
	}

	SECTION ("EndsWith")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.EndsWith("This") == false );
		CHECK ( sLine.EndsWith("This is") == false );
		CHECK ( sLine.EndsWith("his") == false );
		CHECK ( sLine.EndsWith("is") == false );
		CHECK ( sLine.EndsWith("data") == true );
		CHECK ( sLine.EndsWith("ome data") == true );
		CHECK ( sLine.EndsWith("") == true );
	}

	SECTION ("Contains")
	{
		KString sLine = "This is a line with some data";

		CHECK ( sLine.Contains("This") == true );
		CHECK ( sLine.Contains("This is") == true );
		CHECK ( sLine.Contains("his") == true );
		CHECK ( sLine.Contains("is") == true );
		CHECK ( sLine.Contains("data") == true );
		CHECK ( sLine.Contains("nothing") == false );
		CHECK ( sLine.Contains("") == true );
	}

	SECTION ("operator bool()")
	{
		KString sString;
		CHECK ( !sString );
		if (sString)
		{
			CHECK( sString == "empty" );
		}

		sString = "abc";
		CHECK ( sString );
		if (!sString)
		{
			CHECK( sString == "non-empty" );
		}
	}

	SECTION("join vector")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		sResult.Join(vec);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("join list")
	{
		std::list<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		sResult.Join(list);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("join kstack")
	{
		KStack<KStringView> stack {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		sResult.Join(stack);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("join map")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		sResult.Join(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("join map with non-default limiters")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		sResult.Join(map, " and ", " is ");

		CHECK (sResult == "1 is one and 2 is two and 3 is three and 4 is four" );
	}

	SECTION("join kprops")
	{
		KProps<KStringView, KStringView, true, true> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		sResult.Join(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("split vector")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KString sString = "one,two,three,four";
		auto rvec = sString.Split();

		CHECK (rvec == vec );
	}

	SECTION("split list")
	{
		std::list<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sString = "one,two,three,four";
		auto rlist = sString.Split<std::list<KStringView>>();

		CHECK (rlist == list );
	}

	SECTION("split kstack")
	{
		KStack<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sString = "one,two,three,four";
		auto rlist = sString.Split<KStack<KStringView>>();

		CHECK (rlist == list );
	}

	SECTION("split map")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sString = "1=one,2=two,3=three,4=four";
		auto rmap = sString.Split<std::map<KStringView, KStringView>>();

		CHECK (rmap == map );
	}

	SECTION("split map with non-default limiters")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sString = "1-one 2-two 3-three 4-four";
		auto rmap = sString.Split<std::map<KStringView, KStringView>>(" ", "-");

		CHECK (rmap == map );
	}

	SECTION("split map with non-default limiters and trim")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sString = "1-one **2-two 3-three** **4-four**";
		auto rmap = sString.Split<std::map<KStringView, KStringView>>(" ", "-", "*");

		CHECK (rmap == map );
	}

	SECTION("split kprops")
	{
		KProps<KStringView, KStringView, true, true> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sString = "1=one,2=two,3=three,4=four";
		auto rmap = sString.Split<KProps<KStringView, KStringView, true, true>>();

		CHECK (rmap == map );
	}

	SECTION("split auto range")
	{
		std::vector<KStringView> vresult {
			"line 1",
			"line 2",
			"line 3"
		};

		auto rit = vresult.begin();
		KString sInput { "line 1\nline 2\nline 3" };
		for (auto it : sInput.Split("\n", " \t\r\b"))
		{
			CHECK ( it == *rit++);
		}
	}

	SECTION("split temporary")
	{
		KProps<KString, KString> Props;
		Props["column"] = "simple";

		for (auto& sCol : KStringView("test,some,column,names,to,happily,split,here").Split(","))
		{
			Props.Remove (sCol);
		}
	}

	SECTION("split constexpr")
	{
		KProps<KString, KString> Props;
		Props["column"] = "simple";

		static constexpr KStringView sNames("test,some,column,names,to,happily,split,here");

		for (auto& sCol : sNames.Split(","))
		{
			Props.Remove (sCol);
		}
	}

	SECTION("trimming")
	{
		KString sTrim;

		sTrim.Trim();
		sTrim.Trim(' ');
		sTrim.Trim("    ");
		sTrim.TrimLeft();
		sTrim.TrimLeft(' ');
		sTrim.TrimLeft("    ");
		sTrim.TrimRight();
		sTrim.TrimRight(' ');
		sTrim.TrimRight("    ");

		KString sNew;
		sNew = KString().Trim();
		sNew = KString().Trim(' ');
		sNew = KString().Trim("   ");
		sNew = KString().TrimLeft();
		sNew = KString().TrimLeft(' ');
		sNew = KString().TrimLeft("   ");
		sNew = KString().TrimRight();
		sNew = KString().TrimRight(' ');
		sNew = KString().TrimRight("   ");
	}

	SECTION("RemoveChars")
	{
		KString s = "abcdddefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
		auto iRemoved = s.RemoveChars("dgh");
		CHECK (iRemoved == 8 );
		CHECK (s == "abcefijklmnopqrstuvwxyzabcefijklmnopqrstuvwxyz" );
		iRemoved = s.RemoveChars("1234");
		CHECK (iRemoved == 0 );
		iRemoved = s.RemoveChars("abxyz");
		CHECK (iRemoved == 10 );
		CHECK (s == "cefijklmnopqrstuvwcefijklmnopqrstuvw" );
		s.clear();
		iRemoved = s.RemoveChars("1234");
		CHECK (iRemoved == 0 );
		CHECK ( s == "" );
	}

	SECTION("ClipAt")
	{
		KString s = "//aaa";
		bool bFound = s.ClipAt("//");
		CHECK ( s == "" );
		CHECK ( bFound == true );

		s = "//";
		bFound = s.ClipAt("//");
		CHECK ( s == "" );
		CHECK ( bFound == true );

		s = "aaa//";
		bFound = s.ClipAt("//");
		CHECK ( s == "aaa" );
		CHECK ( bFound == true );

		s = "aaa//bbb";
		bFound = s.ClipAt("//");
		CHECK ( s == "aaa" );
		CHECK ( bFound == true );
	}

	SECTION("HasUTF8")
	{
		CHECK ( KString(""            ).HasUTF8() == false );
		CHECK ( KString("abcdefg12345").HasUTF8() == false );
		CHECK ( KString("àbcdefg12345").HasUTF8() == true  );
		CHECK ( KString("abcdefà12345").HasUTF8() == true  );
		CHECK ( KString("abcdef12345à").HasUTF8() == true  );
		CHECK ( KString("abc\23412345").HasUTF8() == false );
		CHECK ( KString("abc\210\1231").HasUTF8() == false );
		CHECK ( KString("âbc\210\1231").HasUTF8() == true  );
		CHECK ( KString("abc\210\123â").HasUTF8() == false );
	}

	SECTION("AtUTF8")
	{
		CHECK ( uint32_t(KString(""     ).AtUTF8(9)) == uint32_t(Unicode::INVALID_CODEPOINT) );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(0)) ==   'a' );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(1)) ==   'b' );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(2)) ==   'c' );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(3)) ==   230 );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(4)) ==   229 );
		CHECK ( uint32_t(KString("aꜩꝙæå").AtUTF8(3)) ==  230 );
		CHECK ( uint32_t(KString("aꜩꝙæå").AtUTF8(4)) ==  229 );
		CHECK ( uint32_t(KString("åabcæ").AtUTF8(0)) ==   229 );
		CHECK ( uint32_t(KString("åꜩbcꝙ").AtUTF8(3)) ==  'c' );
		CHECK ( uint32_t(KString("åꜩbcꝙ").AtUTF8(4)) == 42841 );
		CHECK ( uint32_t(KString("abcæå").AtUTF8(5)) == uint32_t(Unicode::INVALID_CODEPOINT) );
	}

	SECTION("remove_prefix")
	{
		KString sString { "abcdefghijklmnopqrstuvwxyz" };
		auto sStr = sString;
		sStr.remove_prefix(1);
		CHECK ( sStr == "bcdefghijklmnopqrstuvwxyz" );
		sStr.remove_prefix(50);
		CHECK ( sStr == "" );
		sStr = sString;
		CHECK ( sStr.remove_prefix("bcd") == false );
		CHECK ( sStr.remove_prefix('b') == false );
		CHECK ( sStr.remove_prefix("abcd") == true );
		CHECK ( sStr == "efghijklmnopqrstuvwxyz" );
		CHECK ( sStr.remove_prefix('e') == true );
		CHECK ( sStr == "fghijklmnopqrstuvwxyz" );
	}

	SECTION("remove_suffix")
	{
		KString sString { "abcdefghijklmnopqrstuvwxyz" };
		auto sStr = sString;
		sStr.remove_suffix(1);
		CHECK ( sStr == "abcdefghijklmnopqrstuvwxy" );
		sStr.remove_suffix(50);
		CHECK ( sStr == "" );
		sStr = sString;
		CHECK ( sStr.remove_suffix("bcd") == false );
		CHECK ( sStr.remove_suffix('b') == false );
		CHECK ( sStr.remove_suffix("xyz") == true );
		CHECK ( sStr == "abcdefghijklmnopqrstuvw" );
		CHECK ( sStr.remove_suffix('w') == true );
		CHECK ( sStr == "abcdefghijklmnopqrstuv" );
	}

	SECTION("std::to_string")
	{
		std::array<std::pair<double, KStringView>, 3> dTests
		{{
			{ 1.0         , "1.000000" },
			{ 1.1         , "1.100000" },
			{ 1.1234567890, "1.123457" },
		}};

		for (auto& Test : dTests)
		{
			auto s = std::to_string(Test.first);
			CHECK ( s == Test.second );
		}
	}

	SECTION("KString::to_string")
	{
		std::array<std::pair<double, KStringView>, 5> dTests
		{{
			{ 1.0         , "1" },
			{ 1.1         , "1.1" },
			{ 1.1234567890, "1.123456789" },
			{ 1.123456784444444441, "1.1234567844444445" }, // this is an interesting rounding behavior in fmtlib
			{ 1.123456784444444447, "1.1234567844444445" },
		}};

		for (auto& Test : dTests)
		{
			auto s = KString::to_string(Test.first);
			CHECK ( s == Test.second );
		}
	}

}
