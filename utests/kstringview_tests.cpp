#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <vector>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KStringView") {

	SECTION("find")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find(' ') == 7 );
		CHECK( sv.find(" ") == 7 );
		CHECK( sv.find(" 9") == 8 );
		CHECK( sv.find(' ', 3) == 7 );
		CHECK( sv.find(" 9", 3) == 8 );
		CHECK( sv.find('0') == 0 );
		CHECK( sv.find("0") == 0 );
		CHECK( sv.find("01") == 0 );
		CHECK( sv.find('h') == 17 );
		CHECK( sv.find("h") == 17 );
		CHECK( sv.find(" h") == 16 );
		CHECK( sv.find('-') == KStringView::npos );
		CHECK( sv.find("-") == KStringView::npos );
		CHECK( sv.find("!-") == KStringView::npos );
		CHECK( sv.find('1', 1) == 1 );
		CHECK( sv.find("1", 1) == 1 );
		CHECK( sv.find("12", 1) == 1 );
		CHECK( sv.find('1', 2) == KStringView::npos );
		CHECK( sv.find("1", 2) == KStringView::npos );
		CHECK( sv.find("12", 2) == KStringView::npos );
	}

	SECTION("rfind")
	{
		KStringView sv("0123456  9abcdef");
		CHECK( sv.rfind(' ') == 8 );
		CHECK( sv.rfind(" ") == 8 );
		CHECK( sv.rfind(" 9") == 8 );
		CHECK( sv.rfind('0') == 0 );
		CHECK( sv.rfind("0") == 0 );
		CHECK( sv.rfind("01") == 0 );
		CHECK( sv.rfind('f') == 15 );
		CHECK( sv.rfind("f") == 15 );
		CHECK( sv.rfind("ef") == 14 );
		CHECK( sv.rfind('f', 15) == 15 );
		CHECK( sv.rfind("f", 15) == 15 );
		CHECK( sv.rfind("ef", 15) == 14 );
		CHECK( sv.rfind(' ', 12) == 8 );
		CHECK( sv.rfind(" 9", 12) == 8 );
		CHECK( sv.rfind(' ', 20) == 8 );
		CHECK( sv.rfind(" 9", 20) == 8 );
		CHECK( sv.rfind('-') == KStringView::npos );
		CHECK( sv.rfind("-") == KStringView::npos );
		CHECK( sv.rfind("!-") == KStringView::npos );
		CHECK( sv.rfind('f', 14) == KStringView::npos );
		CHECK( sv.rfind("f", 14) == KStringView::npos );
		CHECK( sv.rfind("ef", 13) == KStringView::npos );
		CHECK( sv.rfind('f', 15) == 15 );
		CHECK( sv.rfind("f", 15) == 15 );
		CHECK( sv.rfind("ef", 15) == 14 );
	}

	SECTION("find_first_of")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find_first_of(' ') == 7 );
		CHECK( sv.find_first_of(" ") == 7 );
		CHECK( sv.find_first_of(" d") == 7 );
		CHECK( sv.find_first_of('0') == 0 );
		CHECK( sv.find_first_of("0") == 0 );
		CHECK( sv.find_first_of("02") == 0 );
		CHECK( sv.find_first_of('h') == 17 );
		CHECK( sv.find_first_of("h") == 17 );
		CHECK( sv.find_first_of("h-") == 17 );
		CHECK( sv.find_first_of("ab f") == 7 );
		CHECK( sv.find_first_of("abf ") == 7 );
		CHECK( sv.find_first_of('-') == KStringView::npos );
		CHECK( sv.find_first_of("-") == KStringView::npos );
		CHECK( sv.find_first_of("!-") == KStringView::npos );
	}

	SECTION("find_first_of with pos")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find_first_of(' ', 2) == 7 );
		CHECK( sv.find_first_of(" ", 2) == 7 );
		CHECK( sv.find_first_of(" d", 2) == 7 );
		CHECK( sv.find_first_of('0', 2) == KStringView::npos );
		CHECK( sv.find_first_of("0", 2) == KStringView::npos );
		CHECK( sv.find_first_of("02", 2) == 2 );
		CHECK( sv.find_first_of('h', 2) == 17 );
		CHECK( sv.find_first_of("h", 2) == 17 );
		CHECK( sv.find_first_of("h-", 2) == 17 );
		CHECK( sv.find_first_of("ab f", 2) == 7 );
		CHECK( sv.find_first_of("abf ", 2) == 7 );
		CHECK( sv.find_first_of('-', 2) == KStringView::npos );
		CHECK( sv.find_first_of("-", 2) == KStringView::npos );
		CHECK( sv.find_first_of("!-", 2) == KStringView::npos );
	}

	SECTION("find_last_of")
	{
		KStringView sv("0123456  9abcdef");
		CHECK( sv.find_last_of(' ') == 8 );
		CHECK( sv.find_last_of(" ") == 8 );
		CHECK( sv.find_last_of(" 1") == 8 );
		CHECK( sv.find_last_of('0') == 0 );
		CHECK( sv.find_last_of("0") == 0 );
		CHECK( sv.find_last_of("0-") == 0 );
		CHECK( sv.find_last_of('f') == 15 );
		CHECK( sv.find_last_of("f") == 15 );
		CHECK( sv.find_last_of("fe") == 15 );
		CHECK( sv.find_last_of("12 3") == 8 );
		CHECK( sv.find_last_of("123 ") == 8 );
		CHECK( sv.find_last_of('-') == KStringView::npos );
		CHECK( sv.find_last_of("-") == KStringView::npos );
		CHECK( sv.find_last_of("!-") == KStringView::npos );
	}

	SECTION("find_last_of find_last_not_of with controlled 'noise'")
	{

#pragma pack(push, 1)
		char haystack[4][14] = {"ABCDEFGHIJKLM", "NCDEFGHIJKLMN", "NOPQRSOPQRSQQ", "NZNOPQRSTUVWX"};

		char haystack2[4][27] = {"ABCDEFGHIJKLMABCDEFGHIJKLM", "NCDEFGHIJKLMNNCDEFGHIJKLMN", "NOPQRSOPQRSQQNOPQRSOPQRSQQ", "YZNOPQRSTUVWXYZNOPQRSTUVWX"};

		char haystack3[4][93] = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+abcdefghijklmnopqrstuvwxyz", "z0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+abcdefghijklmnopqrstuvwxy", "abcdefghijklmnopqrstuvwxyz;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

		char needle[3][14] = {"NOPQRSTUVWXYZ", "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ"};

		char needle2[7][8] = {"NNOPQRS", "TUVWXYZ", "AABCDEF", "GHIJKLM", "NNOPQRS", "WXYZABC", "FGHIJKL"};

		char needle3[4][27] = {"abcdefghijklmabcdefghijklm", "ncdefghijklmnncdefghijklmn", "abcdefghijklmnopqrstuvwxyz", "yznopqrstuvwxyznopqrstuvwx"};
#pragma pack(pop)

		KStringView sv(haystack[0]);
		KStringView sv1(haystack[1]);
		KStringView sv2(haystack[2]);
		KStringView svt(&haystack[0][12]);

		KStringView big_sv(haystack2[0]);
		KStringView big_sv1(haystack2[1]);
		KStringView big_sv2(haystack2[2]);


		KStringView huge_sv(haystack3[0]);
		KStringView huge_sv_10(&haystack3[0][10]);// No numbers
		KStringView huge_sv_36(&haystack3[0][36]);// No numbers or cap letters
		KStringView huge_sv_66(&haystack3[0][66]);// No numbers or cap letters or specials
		KStringView huge_sv1(haystack3[1]);
		KStringView huge_sv2(haystack3[2]);
		KStringView huge_sv2_26(&haystack3[2][26]); // No lowercase
		KStringView huge_sv2_56(&haystack3[2][56]); // No lowercase or specials
		KStringView huge_sv2_66(&haystack3[2][66]); // No lowercase or specials or numbers

		CHECK( sv.find_last_of(needle[0]) == KStringView::npos );
		CHECK( sv1.find_last_of(needle[0]) == 12 );
		CHECK( sv.find_last_not_of(needle[0]) == 12);
		CHECK( sv1.find_last_of(needle[0]) == 12);
		CHECK( sv1.find_last_not_of(needle2[0]) == 11);
		CHECK( sv2.find_last_not_of(needle2[4]) == KStringView::npos);
		CHECK( sv.find_last_of(needle2[4]) == KStringView::npos);
		CHECK( sv.find_last_of(needle2[2]) == 5);

		CHECK( big_sv.find_last_of(needle[0]) == KStringView::npos );
		CHECK( big_sv1.find_last_of(needle[0]) == 25 );
		CHECK( big_sv.find_last_not_of(needle[0]) == 25);
		CHECK( big_sv1.find_last_of(needle[0]) == 25);
		CHECK( big_sv1.find_last_not_of(needle2[0]) == 24);
		CHECK( big_sv2.find_last_not_of(needle2[4]) == KStringView::npos);
		CHECK( big_sv.find_last_of(needle2[4]) == KStringView::npos);
		CHECK( big_sv.find_last_of(needle2[2]) == 18);

		CHECK( huge_sv.find_last_of('z') == 91);
		CHECK( huge_sv.find_last_of('a') == 66);
		CHECK( huge_sv.find_last_not_of(needle3[2]) == 66);

		// Ensure logic doesn't break down no matter what the start position is
		for (int i = 0; i < 92; i++)
		{
			KStringView temp(&haystack3[0][i]);
			CHECK(temp.find_last_of('z') == 91 - i);

			if (i < 66)
			{
				CHECK( temp.find_last_not_of(needle3[2]) == 66 - i);
			}
			else
			{
				CHECK( temp.find_last_not_of(needle3[2]) == KStringView::npos);
			}


			if (i < 67)
			{
				CHECK(temp.find_last_of('a') == 66 - i);
			}
			else
			{
				CHECK(temp.find_last_of('a') == KStringView::npos);
			}

			if (i > 90) continue;
			CHECK(temp.find_last_not_of('z') == 90 - i);
		}


	}

	SECTION("find_first_not_of")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find_first_not_of(' ') == 0 );
		CHECK( sv.find_first_not_of(" ") == 0 );
		CHECK( sv.find_first_not_of(" d") == 0 );
		CHECK( sv.find_first_not_of('0') == 1 );
		CHECK( sv.find_first_not_of("0") == 1 );
		CHECK( sv.find_first_not_of("02") == 1 );
		CHECK( sv.find_first_not_of("0123456789abcdef ") == 17 );
		CHECK( sv.find_first_not_of("0123456789abcdefgh ") == KStringView::npos );
	}

	SECTION("find_first_not_of with pos")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find_first_not_of(' ', 2) == 2 );
		CHECK( sv.find_first_not_of(" ", 2) == 2 );
		CHECK( sv.find_first_not_of(" d", 2) == 2 );
		CHECK( sv.find_first_not_of('2', 2) == 3 );
		CHECK( sv.find_first_not_of("2", 2) == 3 );
		CHECK( sv.find_first_not_of("02", 2) == 3 );
		CHECK( sv.find_first_not_of("0123456789abcdef ", 2) == 17 );
		CHECK( sv.find_first_not_of("0123456789abcdefgh ", 2) == KStringView::npos );
	}

	SECTION("erase by index")
	{
		KStringView sv("0123456789abcdefgh");
		KStringView svd = sv;
		KStringView sve;
		svd.erase(2);
		sve = "01";
		CHECK( svd == sve );
		svd = sv;
		svd.erase(0, 2);
		sve = "23456789abcdefgh";
		CHECK( svd == sve );
		svd = sv;
		svd.erase(4, 2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		svd = sv;
		svd.erase(16, 2);
		sve = "0123456789abcdef";
		CHECK( svd == sve );
	}

	SECTION("erase by iterator")
	{
		KStringView sv("0123456789abcdefgh");
		KStringView svd = sv;
		KStringView sve;
		auto it = svd.erase(svd.begin());
		sve = "123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.begin() );
		svd = sv;
		it = svd.erase(svd.begin()+2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end()-1);
		sve = "0123456789abcdefg";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end()-2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end());
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
	}

	SECTION("erase by iterator range")
	{
		KStringView sv("0123456789abcdefgh");
		KStringView svd = sv;
		KStringView sve;
		auto it = svd.erase(svd.begin(), svd.begin() + 3);
		sve = "3456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.begin() );
		svd = sv;
		it = svd.erase(svd.begin()+2, svd.begin() + 4);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end()-1, svd.end());
		sve = "0123456789abcdefg";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end()-2, svd.end());
		sve = "0123456789abcdef";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end(), svd.end());
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
	}

	SECTION("fmt::format")
	{
		KString s;
		KStringView sv = "a string view";
		s.Printf("This is %s", sv);
		CHECK( s == "This is a string view" );
		s.Format("This is {}", sv);
		CHECK( s == "This is a string view" );
	}
}

