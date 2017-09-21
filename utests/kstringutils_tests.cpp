#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>

using namespace dekaf2;

TEST_CASE("KStringUtils") {

	SECTION("Freestanding Replace on KString")
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
			dekaf2::kReplace(it[0], it[1], it[2], true);
			CHECK( it[0] == it[3] );
		}

	}

	/*
	 * wide strings are not yet supported for Replace()
	SECTION("Freestanding Replace on std::wstring")
	{
		// source, search, replace, target
		std::vector<std::vector<std::wstring>> stest
		{
			{ L"", L"abc", L"def", L"" },
			{ L" ", L"abc", L"def", L" " },
			{ L"   ", L"abc", L"def", L"   " },
			{ L"    ", L"abc", L"def", L"    " },
			{ L"    ", L"abc", L"defgh", L"    " },

			{ L"0123abcdefghijklmnopqrstuvwxyz", L"0123", L"56", L"56abcdefghijklmnopqrstuvwxyz" },
			{ L"0123abcdefghijklmnopqrstuvwxyz", L"0123", L"5678", L"5678abcdefghijklmnopqrstuvwxyz" },
			{ L"0123abcdefghijklmnopqrstuvwxyz", L"0123", L"567890", L"567890abcdefghijklmnopqrstuvwxyz" },

			{ L"abcd0123efghijklmnopqrstuvwxyz", L"0123", L"56", L"abcd56efghijklmnopqrstuvwxyz" },
			{ L"abcd0123efghijklmnopqrstuvwxyz", L"0123", L"5678", L"abcd5678efghijklmnopqrstuvwxyz" },
			{ L"abcd0123efghijklmnopqrstuvwxyz", L"0123", L"567890", L"abcd567890efghijklmnopqrstuvwxyz" },

			{ L"abcdefghijklmnopqrstuvwxyz0123", L"0123", L"56", L"abcdefghijklmnopqrstuvwxyz56" },
			{ L"abcdefghijklmnopqrstuvwxyz0123", L"0123", L"5678", L"abcdefghijklmnopqrstuvwxyz5678" },
			{ L"abcdefghijklmnopqrstuvwxyz0123", L"0123", L"567890", L"abcdefghijklmnopqrstuvwxyz567890" },

			{ L"0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", L"0123", L"56", L"56abcdefg56hij5656klmnopqrstuvwxyz56" },
			{ L"0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", L"0123", L"5678", L"5678abcdefg5678hij56785678klmnopqrstuvwxyz5678" },
			{ L"0123abcdefg0123hij01230123klmnopqrstuvwxyz0123", L"0123", L"567890", L"567890abcdefg567890hij567890567890klmnopqrstuvwxyz567890" },

			{ L"0123", L"0123", L"56", L"56" },
			{ L"0123", L"0123", L"5678", L"5678" },
			{ L"0123", L"0123", L"567890", L"567890" },

			{ L"01230123", L"0123", L"56", L"5656" },
			{ L"01230123", L"0123", L"5678", L"56785678" },
			{ L"01230123", L"0123", L"567890", L"567890567890" },
		};

		for (auto& it : stest)
		{
			dekaf2::Replace(it[0], it[1].data(), it[1].length(), it[2].data(), it[2].length(), true);
			CHECK( it[0] == it[3] );
		}

	}
	*/

	SECTION("Freestanding Trimming on std::string")
	{
		std::vector<std::string> stest
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
			std::vector<std::string> sexpect
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
					dekaf2::kTrimLeft(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Right")
		{
			std::vector<std::string> sexpect
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
					dekaf2::kTrimRight(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Left and Right")
		{
			std::vector<std::string> sexpect
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
					dekaf2::kTrim(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

	}

	SECTION("Freestanding Trimming on std::wstring")
	{
		std::vector<std::wstring> stest
		{
			L"",
			L" ",
			L"\t \r\n",
			L"abcde",
			L" abcde",
			L"  abcde",
			L"\t abcde",
			L"\n\r\t abcde",
			L"a abcde",
			L" a abcde",
			L"a\t abcde",
			L"\na\r\t abcde",
			L" abcde ",
			L"  abcde  ",
			L"\t abcde \t",
			L"\n\r\t abcde \t\r\n",
			L"a abcde a",
			L" a abcde a ",
			L"a\t abcde \t a",
			L"\na\r\t abcde \t\ra\n",
		};

		SECTION("Left")
		{
			std::vector<std::wstring> sexpect
			{
				L"",
				L"",
				L"",
				L"abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"a abcde",
				L"a abcde",
				L"a\t abcde",
				L"a\r\t abcde",
				L"abcde ",
				L"abcde  ",
				L"abcde \t",
				L"abcde \t\r\n",
				L"a abcde a",
				L"a abcde a ",
				L"a\t abcde \t a",
				L"a\r\t abcde \t\ra\n",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					dekaf2::kTrimLeft(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Right")
		{
			std::vector<std::wstring> sexpect
			{
				L"",
				L"",
				L"",
				L"abcde",
				L" abcde",
				L"  abcde",
				L"\t abcde",
				L"\n\r\t abcde",
				L"a abcde",
				L" a abcde",
				L"a\t abcde",
				L"\na\r\t abcde",
				L" abcde",
				L"  abcde",
				L"\t abcde",
				L"\n\r\t abcde",
				L"a abcde a",
				L" a abcde a",
				L"a\t abcde \t a",
				L"\na\r\t abcde \t\ra",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					dekaf2::kTrimRight(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

		SECTION("Left and Right")
		{
			std::vector<std::wstring> sexpect
			{
				L"",
				L"",
				L"",
				L"abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"a abcde",
				L"a abcde",
				L"a\t abcde",
				L"a\r\t abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"abcde",
				L"a abcde a",
				L"a abcde a",
				L"a\t abcde \t a",
				L"a\r\t abcde \t\ra",
			};

			SECTION("isspace()") {

				CHECK( sexpect.size() == stest.size() );
				for (size_t iCount = 0; iCount < stest.size(); ++iCount)
				{
					dekaf2::kTrim(stest[iCount]);
					CHECK( stest[iCount] == sexpect[iCount] );
				}

			}

		}

	}

	SECTION("conversion functions for strings")
	{
		KString s;
		s = "1234567";
		CHECK( kToInt(s)       == 1234567 );
		CHECK( kToLong(s)      == 1234567 );
		CHECK( kToLongLong(s)  == 1234567 );
		CHECK( kToUInt(s)      == 1234567 );
		CHECK( kToULong(s)     == 1234567 );
		CHECK( kToULongLong(s) == 1234567 );
		s = "-1234567";
		CHECK( kToInt(s)       == -1234567 );
		CHECK( kToLong(s)      == -1234567 );
		CHECK( kToLongLong(s)  == -1234567 );
		CHECK( kToUInt(s)      == -1234567U );
		CHECK( kToULong(s)     == -1234567UL );
		CHECK( kToULongLong(s) == -1234567ULL );
		s = "123456789012345";
		CHECK( kToLong(s)      == 123456789012345 );
		CHECK( kToLongLong(s)  == 123456789012345 );
		CHECK( kToULong(s)     == 123456789012345 );
		CHECK( kToULongLong(s) == 123456789012345 );
		s = "-123456789012345";
		CHECK( kToLong(s)      == -123456789012345 );
		CHECK( kToLongLong(s)  == -123456789012345 );
		CHECK( kToULong(s)     == -123456789012345UL );
		CHECK( kToULongLong(s) == -123456789012345ULL );
	}

	SECTION("conversion functions for char*")
	{
		const char* p = "1234567";
		CHECK( kToInt(p)       == 1234567 );
		CHECK( kToLong(p)      == 1234567 );
		CHECK( kToLongLong(p)  == 1234567 );
		CHECK( kToUInt(p)      == 1234567 );
		CHECK( kToULong(p)     == 1234567 );
		CHECK( kToULongLong(p) == 1234567 );
		p = "-1234567";
		CHECK( kToInt(p)       == -1234567 );
		CHECK( kToLong(p)      == -1234567 );
		CHECK( kToLongLong(p)  == -1234567 );
		CHECK( kToUInt(p)      == -1234567U );
		CHECK( kToULong(p)     == -1234567UL );
		CHECK( kToULongLong(p) == -1234567ULL );
		p = "123456789012345";
		CHECK( kToLong(p)      == 123456789012345 );
		CHECK( kToLongLong(p)  == 123456789012345 );
		CHECK( kToULong(p)     == 123456789012345 );
		CHECK( kToULongLong(p) == 123456789012345 );
		p = "-123456789012345";
		CHECK( kToLong(p)      == -123456789012345 );
		CHECK( kToLongLong(p)  == -123456789012345 );
		CHECK( kToULong(p)     == -123456789012345UL );
		CHECK( kToULongLong(p) == -123456789012345ULL );
	}

	SECTION("IsDecimal on strings")
	{
		KString s;
		s = "1";
		CHECK( kIsDecimal(s) );
		s = "123456";
		CHECK( kIsDecimal(s) );
		s = "-123456";
		CHECK( kIsDecimal(s) );
		s = "+123456";
		CHECK( kIsDecimal(s) );
		s = "123a456";
		CHECK( !kIsDecimal(s) );
		s = "aa";
		CHECK( !kIsDecimal(s) );
		s = "";
		CHECK( !kIsDecimal(s) );
	}

	SECTION("IsDecimal on char*")
	{
		const char* s;
		s = "1";
		CHECK( kIsDecimal(s) );
		s = "123456";
		CHECK( kIsDecimal(s) );
		s = "-123456";
		CHECK( kIsDecimal(s) );
		s = "+123456";
		CHECK( kIsDecimal(s) );
		s = "123a456";
		CHECK( !kIsDecimal(s) );
		s = "aa";
		CHECK( !kIsDecimal(s) );
		s = "";
		CHECK( !kIsDecimal(s) );
	}

	SECTION("KCountChar on strings")
	{
		KString s;
		s = "abcdeahgzaldkaakf";
		CHECK( kCountChar(s, 'a') == 5 );
		CHECK( kCountChar(s, 'x') == 0 );
		s = "a";
		CHECK( kCountChar(s, 'a') == 1 );
		s = "aaaaa";
		CHECK( kCountChar(s, 'a') == 5 );
		s = "";
		CHECK( kCountChar(s, 'a') == 0 );
	}

	SECTION("KCountChar on char*")
	{
		const char* s;
		s = "abcdeahgzaldkaakf";
		CHECK( kCountChar(s, 'a') == 5 );
		CHECK( kCountChar(s, 'x') == 0 );
		s = "a";
		CHECK( kCountChar(s, 'a') == 1 );
		s = "aaaaa";
		CHECK( kCountChar(s, 'a') == 5 );
		s = "";
		CHECK( kCountChar(s, 'a') == 0 );
		CHECK( kCountChar(s,  0 ) == 0 );
	}

	SECTION("KFormNumber")
	{
		KString s;
		s = kFormNumber(1);
		CHECK( s == "1");
		s = kFormNumber(12);
		CHECK( s == "12");
		s = kFormNumber(123);
		CHECK( s == "123");
		s = kFormNumber(1234);
		CHECK( s == "1,234");
		s = kFormNumber(123456789);
		CHECK( s == "123,456,789");
		s = kFormNumber(123456789, '.');
		CHECK( s == "123.456.789");
		s = kFormNumber(123456789, '-', 1);
		CHECK( s == "1-2-3-4-5-6-7-8-9");
		s = kFormNumber(123456789, '-', 3);
		CHECK( s == "123-456-789");
		s = kFormNumber(123456789, '-', 76);
		CHECK( s == "123456789");
		s = kFormNumber(123456789, '-', 0);
		CHECK( s == "123456789");
		s = kFormNumber(0, '-', 0);
		CHECK( s == "0");
		s = kFormNumber(-1234, ',', 3);
		CHECK( s == "-1,234");
		s = kFormNumber(-123, ',', 3);
		CHECK( s == "-123");
		s = kFormNumber(-12, ',', 3);
		CHECK( s == "-12");
		s = kFormNumber(-1, ',', 3);
		CHECK( s == "-1");
	}

}
