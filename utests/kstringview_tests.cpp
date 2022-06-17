#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kprops.h>
#include <dekaf2/bits/kcppcompat.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KStringView") {

	SECTION("nullptr")
	{
#ifndef _MSC_VER
		// MSC outputs this on the nullptr constructor/assignment:
		// fatal error C1001: An internal error has occurred in the compiler.
		// (compiler file 'msc1.cpp', line 1518)
		// To work around this problem, try simplifying or changing the program near the locations listed above.
	#if !defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION > 90000
		static constexpr KStringView sv1 { nullptr };
		static constexpr KStringView sv2 = nullptr;
	#else
		static KStringView sv1 { nullptr };
		static KStringView sv2 = nullptr;
	#endif
		CHECK ( sv1.empty() );
		CHECK ( sv2.empty() );
	#endif
	}

	SECTION("find")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( sv.find("") == 0 );
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
		sv = "abcdefgabcdefiuhsgw";
		CHECK( sv.rfind("abcdefg") == 0 );
		CHECK( sv.rfind("abcdef") == 7 );
		CHECK( sv.rfind("abcabcde") == KStringView::npos );
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

		auto iPos = sv.find_last_of("abcdef");
		CHECK ( iPos == 15 );
		CHECK ( sv[iPos] == 'f' );
		auto iPos2 = sv.find_last_of("abcdef", --iPos);
		CHECK ( iPos2 == 14 );
		CHECK ( sv[iPos2] == 'e' );

		iPos = sv.find_last_not_of(" \r\n\t");
		CHECK ( iPos == 15 );
		CHECK ( sv[iPos] == 'f' );
		iPos2 = sv.find_last_not_of(" \r\n\t", --iPos);
		CHECK ( iPos2 == 14 );
		CHECK ( sv[iPos2] == 'e' );
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

		char needle4[4][27] = {"ABCDEFGHIJKLMABCDEFGHIJKLM", "NCDEFGHIJKLMNNCDEFGHIJKLMN", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "YZNOPQRSTUVWXYZNOPQRSTUVWX"};
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
		CHECK( huge_sv.find_last_not_of(needle3[2]) == 65);
		//CHECK( huge_sv.find_last_not_of(needle4[1]) == 67); // WRONG THINKING
		CHECK( huge_sv.find_last_not_of(needle3[1]) == 91);

		KStringView temp0(&haystack3[0][64]);
		KStringView temp1(&haystack3[0][65]);
		KStringView temp2(&haystack3[0][82]);
		CHECK( temp0.find_last_not_of(needle3[1]) == 27);
		CHECK( temp1.find_last_not_of(needle3[1]) == 26);
		CHECK( huge_sv.find_last_of(needle3[2]) == 91);
		CHECK( temp2.find_last_of(needle3[2]) == 9);
		CHECK( huge_sv.find_last_not_of(huge_sv) == KStringView::npos);
		CHECK( huge_sv1.find_last_not_of(huge_sv) == KStringView::npos);

		KStringView ttemp(&haystack3[0][16]);
		CHECK( huge_sv.find_last_not_of(ttemp) == 15);

		// Ensure logic doesn't break down no matter what the start position is

		for (std::size_t i = 0; i < 92; i++)
		{
			KStringView temp(&haystack3[0][i]);
			CHECK(temp.find_last_of('z') == 91 - i);
			CHECK(temp.find_last_of(needle3[2]) == 91 - i);

			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringView::npos);
			}
			else
			{
				CHECK( huge_sv.find_last_not_of(temp) == i - 1);
			}

			if (i < 66)
			{
				CHECK( temp.find_last_not_of(needle3[2]) == 65 - i);
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
			if (i < 80)
			{
				CHECK( temp.find_last_of(needle3[1]) == 79 - i);
			}
			else
			{
				CHECK( temp.find_last_of(needle3[1]) == KStringView::npos);
			}

			if (i > 90) continue;
			CHECK(temp.find_last_not_of('z') == 90 - i);
		}

		for (std::size_t i = 0; i < 26; i++)
		{
			KStringView tneedle(&needle3[2][i]);
			KStringView temp(&haystack3[0][i]);
			CHECK( huge_sv.find_last_not_of(tneedle) == 65 + i);
			CHECK( temp.find_last_not_of(tneedle) == 65);
			CHECK( temp.find_last_of(tneedle) == 91 - i);
			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringView::npos);
			}
			else
			{
				CHECK( huge_sv.find_last_not_of(temp) == i - 1);
			}
			CHECK( huge_sv.find_last_of('_') == 63);

		}

	}

	SECTION("find_first_of find_first_not_of with controlled 'noise'")
	{

#pragma pack(push, 1)
		char haystack[4][14] = {"ABCDEFGHIJKLM", "NCDEFGHIJKLMN", "NOPQRSOPQRSQQ", "NZNOPQRSTUVWX"};

		char haystack2[4][27] = {"ABCDEFGHIJKLMABCDEFGHIJKLM", "NCDEFGHIJKLMNNCDEFGHIJKLMN", "NOPQRSOPQRSQQNOPQRSOPQRSQQ", "YZNOPQRSTUVWXYZNOPQRSTUVWX"};

		char haystack3[4][93] = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+abcdefghijklmnopqrstuvwxyz", "z0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+abcdefghijklmnopqrstuvwxy", "abcdefghijklmnopqrstuvwxyz;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz;:'\",<.>/?\\|[{]}!@#$%^&*()-_=+0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

		char needle[3][14] = {"NOPQRSTUVWXYZ", "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ"};

		char needle2[7][8] = {"NNOPQRS", "TUVWXYZ", "AABCDEF", "GHIJKLM", "NNOPQRS", "WXYZABC", "FGHIJKL"};

		char needle3[4][27] = {"abcdefghijklmabcdefghijklm", "ncdefghijklmnncdefghijklmn", "abcdefghijklmnopqrstuvwxyz", "yznopqrstuvwxyznopqrstuvwx"};

		char needle4[4][27] = {"ABCDEFGHIJKLMABCDEFGHIJKLM", "NCDEFGHIJKLMNNCDEFGHIJKLMN", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "YZNOPQRSTUVWXYZNOPQRSTUVWX"};
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

		CHECK( sv.find_first_of(needle[0]) == KStringView::npos );
		CHECK( sv1.find_first_of(needle[0]) == 0 );
		CHECK( sv.find_first_not_of(needle[0]) == 0);
		CHECK( sv1.find_first_of(needle[0]) == 0);
		CHECK( sv1.find_first_not_of(needle2[0]) == 1);
		CHECK( sv2.find_first_not_of(needle2[4]) == KStringView::npos);
		CHECK( sv.find_first_of(needle2[4]) == KStringView::npos);
		CHECK( sv.find_first_of(needle2[2]) == 0);

		CHECK( big_sv.find_first_of(needle[0]) == KStringView::npos );
		CHECK( big_sv1.find_first_of(needle[0]) == 0 );
		CHECK( big_sv.find_first_not_of(needle[0]) == 0);
		CHECK( big_sv1.find_first_of(needle[0]) == 0);
		CHECK( big_sv1.find_first_not_of(needle2[0]) == 1);
		CHECK( big_sv2.find_first_not_of(needle2[4]) == KStringView::npos);
		CHECK( big_sv.find_first_of(needle2[4]) == KStringView::npos);
		CHECK( big_sv.find_first_of(needle2[2]) == 0);

		CHECK( huge_sv.find_first_of('z') == 91);
		CHECK( huge_sv.find_first_of('a') == 66);
		CHECK( huge_sv.find_first_not_of(needle3[2]) == 0);
		CHECK( huge_sv.find_first_not_of(needle3[1]) == 0);

		KStringView temp0(&haystack3[0][64]);
		KStringView temp1(&haystack3[0][65]);
		KStringView temp2(&haystack3[0][82]);
		CHECK( temp0.find_first_not_of(needle3[1]) == 0);
		CHECK( temp1.find_first_not_of(needle3[1]) == 0);
		CHECK( huge_sv.find_first_of(needle3[2]) == 66);
		CHECK( temp2.find_first_of(needle3[2]) == 0);
		CHECK( huge_sv.find_first_not_of(huge_sv) == KStringView::npos);
		CHECK( huge_sv1.find_first_not_of(huge_sv) == KStringView::npos);

		KStringView ttemp(&haystack3[0][16]);
		CHECK( huge_sv.find_first_not_of(ttemp) == 0);



		// Ensure logic doesn't break down no matter what the start position is

		for (std::size_t i = 0; i < 92; i++)
		{
			KStringView temp(&haystack3[0][i]);
			CHECK(temp.find_first_of('z') == 91 - i);


			if (i == 0)
			{
				INFO("i = " + std::to_string(i));
				CHECK( huge_sv.find_first_not_of(temp) == KStringView::npos);
			}
			else
			{
				INFO("i = " + std::to_string(i));
				CHECK( huge_sv.find_first_not_of(temp) == 0);
				CHECK( huge_sv.find_first_of(temp) == i);
			}

			if (i < 66)
			{
				CHECK( temp.find_first_not_of(needle3[2]) == 0);
			}
			else
			{
				INFO("i = " + std::to_string(i));
				CHECK( temp.find_first_not_of(needle3[2]) == KStringView::npos);
			}
			if (i < 67)
			{
				CHECK(temp.find_first_of(needle3[2]) == 66 - i);
				CHECK(temp.find_first_of('a') == 66 - i);
			}
			else
			{
				CHECK(temp.find_first_of('a') == KStringView::npos);
				CHECK(temp.find_first_of(needle3[2]) == 0);
			}
			if (i < 69) // 80 orig // ends up keying on the 'c' in the haystack
			{
				CHECK( temp.find_first_of(needle3[1]) == 68 - i);
			}
			else if (i < 80) // 'c' becomes first char, then 'd', needle goes to 'n'
			{
				CHECK( temp.find_first_of(needle3[1]) == 0);
			}
			else
			{
				INFO("i = " + std::to_string(i));
				CHECK( temp.find_first_of(needle3[1]) == KStringView::npos);
			}

			if (i > 90) continue;
			CHECK(temp.find_first_not_of('z') == 0);
		}

		for (std::size_t i = 0; i < 26; i++)
		{
			KStringView tneedle(&needle3[2][i]);
			KStringView temp(&haystack3[0][i]);
			CHECK( huge_sv.find_last_not_of(tneedle) == 65 + i);
			CHECK( temp.find_last_not_of(tneedle) == 65);
			CHECK( temp.find_last_of(tneedle) == 91 - i);
			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringView::npos);
			}
			else
			{
				CHECK( huge_sv.find_last_not_of(temp) == i - 1);
			}



			CHECK( huge_sv.find_last_of('_') == 63);

		}


	}

	SECTION("find_first... find_last... with controlled 'noise' and and tests designed to get edge cases")
	{

#pragma pack(push, 1)
		char haystack[4][185] = {"00112233445566778899AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ;;::''\"\",,<<..>>//??\\\\||[[{{]]}}!!@@##$$%%^^&&**(())--__==++aabbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz", "zz00112233445566778899AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ;;::''\"\",,<<..>>//??\\\\||[[{{]]}}!!@@##$$%%^^&&**(())--__==++aabbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyy", "aabbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz00112233445566778899AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPPQQRRSSTTUUVVWWXXYYZZ;;::''\"\",,<<..>>//??\\\\||[[{{]]}}!!@@##$$%%^^&&**(())--__==++"};
		char needle[4][27] = {"abcdefghijklmabcdefghijklm", "ncdefghijklmnncdefghijklmn", "abcdefghijklmnopqrstuvwxyz", "yznopqrstuvwxyznopqrstuvwx"};
		char needle2[4][27] = {"ABCDEFGHIJKLMABCDEFGHIJKLM", "NCDEFGHIJKLMNNCDEFGHIJKLMN", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "YZNOPQRSTUVWXYZNOPQRSTUVWX"};
#pragma pack(pop)

		KStringView sv(haystack[0]);

		CHECK(sv.find_first_of('z') == 182);
		CHECK(sv.find_first_of("z") == 182);
		CHECK(sv.find_last_of('z') == 183);
		CHECK(sv.find_last_of("z") == 183);
		CHECK(sv.find_last_not_of('z') == 181);
		CHECK(sv.find_last_not_of("z") == 181);

		CHECK(sv.find_first_of(needle[0]) == 132);

		for (std::size_t i = 0; i < 184; i++)
		{
			INFO("i = " + std::to_string(i));
			KStringView tsv(&haystack[0][i]);
			if (i < 182)
			{
				CHECK(tsv.find_last_not_of('z') == 181 - i);
				CHECK(tsv.find_last_not_of("z") == 181 - i);
			}
			else
			{
				CHECK(tsv.find_last_not_of('z') == KStringView::npos);
				CHECK(tsv.find_last_not_of("z") == KStringView::npos);
			}

			if (i < 183)
			{
				CHECK(tsv.find_first_of('z') == 182 - i);
				CHECK(tsv.find_first_of("z") == 182 - i);
			}
			else
			{
				CHECK(tsv.find_first_of('z') == 0); // z is last and second to last
				CHECK(tsv.find_first_of("z") == 0);
			}

			if (i < 184)
			{
				CHECK(tsv.find_last_of('z') == 183 - i);
				CHECK(tsv.find_last_of("z") == 183 - i);
			}
		}
		for (std::size_t i = 0; i < 26; i++)
		{
			INFO("i = " + std::to_string(i));
			KStringView tsv(&haystack[0][i]);
			KStringView tneedle(&needle[0][i]);
			KStringView tneedle2(&needle[2][i]);

			CHECK(sv.find_last_not_of(tneedle2) == 131 + i*2);
			CHECK(tsv.find_last_not_of(tneedle2) == 131 - i + i*2);
			CHECK(sv.find_first_of(tneedle2) == 132 + i*2);
			CHECK(tsv.find_first_of(tneedle2) == 132 - i + i*2);
			if (i < 14)
			{
				CHECK(sv.find_first_of(tneedle) == 132);
			}
			else
			{
				CHECK(sv.find_first_of(tneedle) == 132 + ((i-13)*2));
			}

		}

	}

	SECTION("find_first_not_of")
	{
		KStringView sv("0123456  9abcdef h");
		KStringView sv2("0123456  9abcdefh "); // slight variant for find last not of
		CHECK( sv.find_first_not_of(' ') == 0 );
		CHECK( sv.find_first_not_of(" ") == 0 );
		CHECK( sv.find_first_not_of(" d") == 0 );
		CHECK( sv.find_first_not_of('0') == 1 );
		CHECK( sv.find_first_not_of("0") == 1 );
		CHECK( sv.find_first_not_of("02") == 1 );
		CHECK( sv.find_first_not_of("0123456789abcdef ") == 17 ); // This test revealed problems
		CHECK( sv.find_last_not_of("0123456789abcdef ") == 17 ); // In this case, both should be same
		CHECK( sv2.find_last_not_of("0123456789abcdef ") == 16 ); // Checking for positional bugs
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

	SECTION("find_last_of")
	{
		KString s(20, '-');
		s.insert(0, "abcdefg");
		KStringView sv(s);
		auto pos = sv.find_last_of("abcdefghijklmnopqrstuvwxyz");
		CHECK( pos == 6 );
	}

	SECTION("find_last_not_of")
	{
		KString s(20, '-');
		s.insert(0, "abcdefg");
		KStringView sv(s);
		auto pos = sv.find_last_not_of("&%-!§&/()=?*#1234567890üöä");
		CHECK( pos == 6 );
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
#ifdef DEKAF2_EXCEPTIONS
		CHECK_THROWS_AS( svd.erase(4, 2), std::runtime_error );
#else
		svd.erase(4, 2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
#endif
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
#ifdef DEKAF2_EXCEPTIONS
		CHECK_THROWS_AS( it = svd.erase(svd.begin()+2), std::runtime_error );
#else
		it = svd.erase(svd.begin()+2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
#endif
		svd = sv;
		it = svd.erase(svd.end()-1);
		sve = "0123456789abcdefg";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
#ifdef DEKAF2_EXCEPTIONS
		CHECK_THROWS_AS(it = svd.erase(svd.end()-2), std::runtime_error);
#else
		it = svd.erase(svd.end()-2);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
#endif
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
#ifdef DEKAF2_EXCEPTIONS
		CHECK_THROWS_AS( it = svd.erase(svd.begin()+2, svd.begin() + 4), std::runtime_error );
#else
		it = svd.erase(svd.begin()+2, svd.begin() + 4);
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
#endif
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
		s.Format("This is {}", sv);
		CHECK( s == "This is a string view" );
	}

	SECTION("SSE Short Haystack")
	{
		KStringView sv = "abcdefabcdef";
		auto pos = sv.find_first_of("ghijk");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_not_of("abcdef");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_of("ghijk");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_not_of("abcdef");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringView::npos );
	}

	SECTION("SSE Long Haystack")
	{
		KStringView sv = "abcdefabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdef";
		auto pos = sv.find_first_of("ghijk");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_not_of("abcdef");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_of("ghijk");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_not_of("abcdef");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringView::npos );
		pos = sv.find_first_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringView::npos );
		pos = sv.find_last_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringView::npos );
	}

	SECTION("conversion functions for KStringView")
	{
		KStringView s;
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

	SECTION("KString Trimming")
	{
		std::vector<KStringView> stest
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
			std::vector<KStringView> sexpect
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
			std::vector<KStringView> sexpect
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
			std::vector<KStringView> sexpect
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

	SECTION("Left")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.Left(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("Right")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.Right(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("Mid")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.Mid(it.start, it.count);
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
			KStringView s(it.input);
			KStringView sv = s.Mid(it.start);
			CHECK ( sv == it.output );
		}
	}

	SECTION("LeftUTF8")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.LeftUTF8(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("RightUTF8")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.RightUTF8(it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("MidUTF8")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.MidUTF8(it.start, it.count);
			CHECK ( sv == it.output );
		}
	}

	SECTION("MidUTF8 with one parm")
	{
		struct parms_t
		{
			KStringView input;
			KStringView output;
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
			KStringView s(it.input);
			KStringView sv = s.MidUTF8(it.start);
			CHECK ( sv == it.output );
		}
	}

	SECTION ("starts_with")
	{
		KStringView sLine = "This is a line with some data";

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
		KStringView sLine = "This is a line with some data";

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
		KStringView sLine = "This is a line with some data";

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
		KStringView sLine = "This is a line with some data";

		CHECK ( sLine.StartsWith("This") == true );
		CHECK ( sLine.StartsWith("This is") == true );
		CHECK ( sLine.StartsWith("his") == false );
		CHECK ( sLine.StartsWith("is") == false );
		CHECK ( sLine.StartsWith("data") == false );
		CHECK ( sLine.StartsWith("") == true );
	}

	SECTION ("EndsWith")
	{
		KStringView sLine = "This is a line with some data";

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
		KStringView sLine = "This is a line with some data";

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
		KStringView sString;
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

	SECTION ("operator ==()")
	{
		KStringView sv { "test" };
		CHECK ( "test" == sv );
		CHECK ( sv == "test" );
		CHECK ( ("rest" == sv) == false );
		CHECK ( (sv == "rest") == false );
		KString s { "test" };
		CHECK ( s == sv );
		CHECK ( sv == s );
		s = "rest";
		CHECK ( (s == sv) == false );
		CHECK ( (sv == s) == false );

	}

	SECTION ("operator !=()")
	{
		KStringView sv { "test" };
		CHECK ( "rest" != sv );
		CHECK ( sv != "rest" );
		KString s { "rest" };
		CHECK ( s != sv );
		CHECK ( sv != s );
	}

	SECTION("split vector")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KStringView sString = "one,two,three,four";
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

		KStringView sString = "one,two,three,four";
		auto rlist = sString.Split<std::list<KStringView>>();

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

		KStringView sString = "1=one,2=two,3=three,4=four";
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

		KStringView sString = "1-one 2-two 3-three 4-four";
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

		KStringView sString = "1-one **2-two 3-three** **4-four**";
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

		KStringView sString = "1=one,2=two,3=three,4=four";
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
		KStringView sInput { "line 1\nline 2\nline 3" };
		for (auto it : sInput.Split("\n", " \t\r\b"))
		{
			CHECK ( it == *rit++);
		}
	}

	SECTION("ToLowerASCII")
	{
		KStringView sv { "TeS@t中文Русский" };
		CHECK ( sv.ToLowerASCII() == "tes@t中文Русский" );
	}

	SECTION("ToUpperASCII")
	{
		KStringView sv { "TeS@t中文Русский" };
		CHECK ( sv.ToUpperASCII() == "TES@T中文Русский" );
	}

	SECTION("append")
	{
		KStringView sv { "0123456789abcdefghijklm" };
		KStringView s1 { sv };
		KStringView s2 { sv };
		s1.remove_prefix(10);
		s2.remove_suffix(13);
		CHECK ( s1 == "abcdefghijklm"  );
		CHECK ( s2 == "0123456789"     );
		CHECK ( s1.append(s2) == false );
		CHECK ( s2.append(s1)          );
		CHECK ( s2 == sv               );
		s2.remove_suffix(14);
		CHECK ( s2 == "012345678"      );
		CHECK ( s2.append(s1) == false );
		CHECK ( s2 == "012345678"      );
		s2 = sv;
		s2.remove_suffix(10);
		CHECK ( s2.append(s1)          );
		CHECK ( s2 == sv               );
	}

	SECTION("Merge")
	{
		KStringView sv { "0123456789abcdefghijklm" };
		KStringView s1 { sv };
		KStringView s2 { sv };
		s1.remove_prefix(10);
		CHECK ( s1 == "abcdefghijklm"  );
		s2.remove_suffix(13);
		CHECK ( s2 == "0123456789"     );
		CHECK ( s1.Merge(s2)           );
		CHECK ( s1 == sv               );
		CHECK ( s2.Merge(s1)           );
		CHECK ( s2 == sv               );
		s1.remove_prefix(10);
		CHECK ( s1 == "abcdefghijklm"  );
		s2.remove_suffix(14);
		CHECK ( s2 == "012345678"      );
		CHECK ( s2.Merge(s1) == false  );
		CHECK ( s1 == "abcdefghijklm"  );
		CHECK ( s2 == "012345678"      );
		CHECK ( s1.Merge(s2) == false  );
		CHECK ( s1 == "abcdefghijklm"  );
		CHECK ( s2 == "012345678"      );
		s2 = sv;
		s2.remove_suffix(10);
		CHECK ( s2.Merge(s1)           );
		CHECK ( s2 == sv               );
	}

	SECTION("HasUTF8")
	{
		CHECK ( KStringView(""            ).HasUTF8() == false );
		CHECK ( KStringView("abcdefg12345").HasUTF8() == false );
		CHECK ( KStringView("àbcdefg12345").HasUTF8() == true  );
		CHECK ( KStringView("abcdefà12345").HasUTF8() == true  );
		CHECK ( KStringView("abcdef12345à").HasUTF8() == true  );
		CHECK ( KStringView("abc\23412345").HasUTF8() == false );
		CHECK ( KStringView("abc\210\1231").HasUTF8() == false );
		CHECK ( KStringView("âbc\210\1231").HasUTF8() == true  );
		CHECK ( KStringView("abc\210\123â").HasUTF8() == false );
	}

	SECTION("AtUTF8")
	{
		CHECK ( KStringView(""     ).AtUTF8(9) == Unicode::INVALID_CODEPOINT );
		CHECK ( KStringView("abcæå").AtUTF8(0) ==   'a' );
		CHECK ( KStringView("abcæå").AtUTF8(1) ==   'b' );
		CHECK ( KStringView("abcæå").AtUTF8(2) ==   'c' );
		CHECK ( KStringView("abcæå").AtUTF8(3) ==   230 );
		CHECK ( KStringView("abcæå").AtUTF8(4) ==   229 );
		CHECK ( KStringView("aꜩꝙæå").AtUTF8(3) ==  230 );
		CHECK ( KStringView("aꜩꝙæå").AtUTF8(4) ==  229 );
		CHECK ( KStringView("åabcæ").AtUTF8(0) ==   229 );
		CHECK ( KStringView("åꜩbcꝙ").AtUTF8(3) ==  'c' );
		CHECK ( KStringView("åꜩbcꝙ").AtUTF8(4) == 42841 );
		CHECK ( KStringView("abcæå").AtUTF8(5) == Unicode::INVALID_CODEPOINT );
	}
}
