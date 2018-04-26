#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <vector>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KStringViewZ") {

	SECTION("find")
	{
		KStringViewZ sv("0123456  9abcdef h");
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
		CHECK( sv.find('-') == KStringViewZ::npos );
		CHECK( sv.find("-") == KStringViewZ::npos );
		CHECK( sv.find("!-") == KStringViewZ::npos );
		CHECK( sv.find('1', 1) == 1 );
		CHECK( sv.find("1", 1) == 1 );
		CHECK( sv.find("12", 1) == 1 );
		CHECK( sv.find('1', 2) == KStringViewZ::npos );
		CHECK( sv.find("1", 2) == KStringViewZ::npos );
		CHECK( sv.find("12", 2) == KStringViewZ::npos );
	}

	SECTION("rfind")
	{
		KStringViewZ sv("0123456  9abcdef");
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
		CHECK( sv.rfind('-') == KStringViewZ::npos );
		CHECK( sv.rfind("-") == KStringViewZ::npos );
		CHECK( sv.rfind("!-") == KStringViewZ::npos );
		CHECK( sv.rfind('f', 14) == KStringViewZ::npos );
		CHECK( sv.rfind("f", 14) == KStringViewZ::npos );
		CHECK( sv.rfind("ef", 13) == KStringViewZ::npos );
		CHECK( sv.rfind('f', 15) == 15 );
		CHECK( sv.rfind("f", 15) == 15 );
		CHECK( sv.rfind("ef", 15) == 14 );
	}

	SECTION("find_first_of")
	{
		KStringViewZ sv("0123456  9abcdef h");
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
		CHECK( sv.find_first_of('-') == KStringViewZ::npos );
		CHECK( sv.find_first_of("-") == KStringViewZ::npos );
		CHECK( sv.find_first_of("!-") == KStringViewZ::npos );
	}

	SECTION("find_first_of with pos")
	{
		KStringViewZ sv("0123456  9abcdef h");
		CHECK( sv.find_first_of(' ', 2) == 7 );
		CHECK( sv.find_first_of(" ", 2) == 7 );
		CHECK( sv.find_first_of(" d", 2) == 7 );
		CHECK( sv.find_first_of('0', 2) == KStringViewZ::npos );
		CHECK( sv.find_first_of("0", 2) == KStringViewZ::npos );
		CHECK( sv.find_first_of("02", 2) == 2 );
		CHECK( sv.find_first_of('h', 2) == 17 );
		CHECK( sv.find_first_of("h", 2) == 17 );
		CHECK( sv.find_first_of("h-", 2) == 17 );
		CHECK( sv.find_first_of("ab f", 2) == 7 );
		CHECK( sv.find_first_of("abf ", 2) == 7 );
		CHECK( sv.find_first_of('-', 2) == KStringViewZ::npos );
		CHECK( sv.find_first_of("-", 2) == KStringViewZ::npos );
		CHECK( sv.find_first_of("!-", 2) == KStringViewZ::npos );
	}

	SECTION("find_last_of")
	{
		KStringViewZ sv("0123456  9abcdef");
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
		CHECK( sv.find_last_of('-') == KStringViewZ::npos );
		CHECK( sv.find_last_of("-") == KStringViewZ::npos );
		CHECK( sv.find_last_of("!-") == KStringViewZ::npos );
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

		KStringViewZ sv(haystack[0]);
		KStringViewZ sv1(haystack[1]);
		KStringViewZ sv2(haystack[2]);
		KStringViewZ svt(&haystack[0][12]);

		KStringViewZ big_sv(haystack2[0]);
		KStringViewZ big_sv1(haystack2[1]);
		KStringViewZ big_sv2(haystack2[2]);


		KStringViewZ huge_sv(haystack3[0]);
		KStringViewZ huge_sv_10(&haystack3[0][10]);// No numbers
		KStringViewZ huge_sv_36(&haystack3[0][36]);// No numbers or cap letters
		KStringViewZ huge_sv_66(&haystack3[0][66]);// No numbers or cap letters or specials
		KStringViewZ huge_sv1(haystack3[1]);
		KStringViewZ huge_sv2(haystack3[2]);
		KStringViewZ huge_sv2_26(&haystack3[2][26]); // No lowercase
		KStringViewZ huge_sv2_56(&haystack3[2][56]); // No lowercase or specials
		KStringViewZ huge_sv2_66(&haystack3[2][66]); // No lowercase or specials or numbers

		CHECK( sv.find_last_of(needle[0]) == KStringViewZ::npos );
		CHECK( sv1.find_last_of(needle[0]) == 12 );
		CHECK( sv.find_last_not_of(needle[0]) == 12);
		CHECK( sv1.find_last_of(needle[0]) == 12);
		CHECK( sv1.find_last_not_of(needle2[0]) == 11);
		CHECK( sv2.find_last_not_of(needle2[4]) == KStringViewZ::npos);
		CHECK( sv.find_last_of(needle2[4]) == KStringViewZ::npos);
		CHECK( sv.find_last_of(needle2[2]) == 5);

		CHECK( big_sv.find_last_of(needle[0]) == KStringViewZ::npos );
		CHECK( big_sv1.find_last_of(needle[0]) == 25 );
		CHECK( big_sv.find_last_not_of(needle[0]) == 25);
		CHECK( big_sv1.find_last_of(needle[0]) == 25);
		CHECK( big_sv1.find_last_not_of(needle2[0]) == 24);
		CHECK( big_sv2.find_last_not_of(needle2[4]) == KStringViewZ::npos);
		CHECK( big_sv.find_last_of(needle2[4]) == KStringViewZ::npos);
		CHECK( big_sv.find_last_of(needle2[2]) == 18);

		CHECK( huge_sv.find_last_of('z') == 91);
		CHECK( huge_sv.find_last_of('a') == 66);
		CHECK( huge_sv.find_last_not_of(needle3[2]) == 65);
		//CHECK( huge_sv.find_last_not_of(needle4[1]) == 67); // WRONG THINKING
		CHECK( huge_sv.find_last_not_of(needle3[1]) == 91);

		KStringViewZ temp0(&haystack3[0][64]);
		KStringViewZ temp1(&haystack3[0][65]);
		KStringViewZ temp2(&haystack3[0][82]);
		CHECK( temp0.find_last_not_of(needle3[1]) == 27);
		CHECK( temp1.find_last_not_of(needle3[1]) == 26);
		CHECK( huge_sv.find_last_of(needle3[2]) == 91);
		CHECK( temp2.find_last_of(needle3[2]) == 9);
		CHECK( huge_sv.find_last_not_of(huge_sv) == KStringViewZ::npos);
		CHECK( huge_sv1.find_last_not_of(huge_sv) == KStringViewZ::npos);

		KStringViewZ ttemp(&haystack3[0][16]);
		CHECK( huge_sv.find_last_not_of(ttemp) == 15);

		// Ensure logic doesn't break down no matter what the start position is

		for (int i = 0; i < 92; i++)
		{
			KStringViewZ temp(&haystack3[0][i]);
			CHECK(temp.find_last_of('z') == 91 - i);
			CHECK(temp.find_last_of(needle3[2]) == 91 - i);

			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringViewZ::npos);
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
				CHECK( temp.find_last_not_of(needle3[2]) == KStringViewZ::npos);
			}
			if (i < 67)
			{
				CHECK(temp.find_last_of('a') == 66 - i);
			}
			else
			{
				CHECK(temp.find_last_of('a') == KStringViewZ::npos);
			}
			if (i < 80)
			{
				CHECK( temp.find_last_of(needle3[1]) == 79 - i);
			}
			else
			{
				CHECK( temp.find_last_of(needle3[1]) == KStringViewZ::npos);
			}

			if (i > 90) continue;
			CHECK(temp.find_last_not_of('z') == 90 - i);
		}

		for (int i = 0; i < 26; i++)
		{
			KStringViewZ tneedle(&needle3[2][i]);
			KStringViewZ temp(&haystack3[0][i]);
			CHECK( huge_sv.find_last_not_of(tneedle) == 65 + i);
			CHECK( temp.find_last_not_of(tneedle) == 65);
			CHECK( temp.find_last_of(tneedle) == 91 - i);
			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringViewZ::npos);
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

		KStringViewZ sv(haystack[0]);
		KStringViewZ sv1(haystack[1]);
		KStringViewZ sv2(haystack[2]);
		KStringViewZ svt(&haystack[0][12]);

		KStringViewZ big_sv(haystack2[0]);
		KStringViewZ big_sv1(haystack2[1]);
		KStringViewZ big_sv2(haystack2[2]);


		KStringViewZ huge_sv(haystack3[0]);
		KStringViewZ huge_sv_10(&haystack3[0][10]);// No numbers
		KStringViewZ huge_sv_36(&haystack3[0][36]);// No numbers or cap letters
		KStringViewZ huge_sv_66(&haystack3[0][66]);// No numbers or cap letters or specials
		KStringViewZ huge_sv1(haystack3[1]);
		KStringViewZ huge_sv2(haystack3[2]);
		KStringViewZ huge_sv2_26(&haystack3[2][26]); // No lowercase
		KStringViewZ huge_sv2_56(&haystack3[2][56]); // No lowercase or specials
		KStringViewZ huge_sv2_66(&haystack3[2][66]); // No lowercase or specials or numbers

		CHECK( sv.find_first_of(needle[0]) == KStringViewZ::npos );
		CHECK( sv1.find_first_of(needle[0]) == 0 );
		CHECK( sv.find_first_not_of(needle[0]) == 0);
		CHECK( sv1.find_first_of(needle[0]) == 0);
		CHECK( sv1.find_first_not_of(needle2[0]) == 1);
		CHECK( sv2.find_first_not_of(needle2[4]) == KStringViewZ::npos);
		CHECK( sv.find_first_of(needle2[4]) == KStringViewZ::npos);
		CHECK( sv.find_first_of(needle2[2]) == 0);

		CHECK( big_sv.find_first_of(needle[0]) == KStringViewZ::npos );
		CHECK( big_sv1.find_first_of(needle[0]) == 0 );
		CHECK( big_sv.find_first_not_of(needle[0]) == 0);
		CHECK( big_sv1.find_first_of(needle[0]) == 0);
		CHECK( big_sv1.find_first_not_of(needle2[0]) == 1);
		CHECK( big_sv2.find_first_not_of(needle2[4]) == KStringViewZ::npos);
		CHECK( big_sv.find_first_of(needle2[4]) == KStringViewZ::npos);
		CHECK( big_sv.find_first_of(needle2[2]) == 0);

		CHECK( huge_sv.find_first_of('z') == 91);
		CHECK( huge_sv.find_first_of('a') == 66);
		CHECK( huge_sv.find_first_not_of(needle3[2]) == 0);
		CHECK( huge_sv.find_first_not_of(needle3[1]) == 0);

		KStringViewZ temp0(&haystack3[0][64]);
		KStringViewZ temp1(&haystack3[0][65]);
		KStringViewZ temp2(&haystack3[0][82]);
		CHECK( temp0.find_first_not_of(needle3[1]) == 0);
		CHECK( temp1.find_first_not_of(needle3[1]) == 0);
		CHECK( huge_sv.find_first_of(needle3[2]) == 66);
		CHECK( temp2.find_first_of(needle3[2]) == 0);
		CHECK( huge_sv.find_first_not_of(huge_sv) == KStringViewZ::npos);
		CHECK( huge_sv1.find_first_not_of(huge_sv) == KStringViewZ::npos);

		KStringViewZ ttemp(&haystack3[0][16]);
		CHECK( huge_sv.find_first_not_of(ttemp) == 0);



		// Ensure logic doesn't break down no matter what the start position is

		for (int i = 0; i < 92; i++)
		{
			KStringViewZ temp(&haystack3[0][i]);
			CHECK(temp.find_first_of('z') == 91 - i);


			if (i == 0)
			{
				INFO("i = " + std::to_string(i));
				CHECK( huge_sv.find_first_not_of(temp) == KStringViewZ::npos);
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
				CHECK( temp.find_first_not_of(needle3[2]) == KStringViewZ::npos);
			}
			if (i < 67)
			{
				CHECK(temp.find_first_of(needle3[2]) == 66 - i);
				CHECK(temp.find_first_of('a') == 66 - i);
			}
			else
			{
				CHECK(temp.find_first_of('a') == KStringViewZ::npos);
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
				CHECK( temp.find_first_of(needle3[1]) == KStringViewZ::npos);
			}

			if (i > 90) continue;
			CHECK(temp.find_first_not_of('z') == 0);
		}

		for (int i = 0; i < 26; i++)
		{
			KStringViewZ tneedle(&needle3[2][i]);
			KStringViewZ temp(&haystack3[0][i]);
			CHECK( huge_sv.find_last_not_of(tneedle) == 65 + i);
			CHECK( temp.find_last_not_of(tneedle) == 65);
			CHECK( temp.find_last_of(tneedle) == 91 - i);
			if (i == 0)
			{
				CHECK( huge_sv.find_last_not_of(temp) == KStringViewZ::npos);
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

		KStringViewZ sv(haystack[0]);

		CHECK(sv.find_first_of('z') == 182);
		CHECK(sv.find_first_of("z") == 182);
		CHECK(sv.find_last_of('z') == 183);
		CHECK(sv.find_last_of("z") == 183);
		CHECK(sv.find_last_not_of('z') == 181);
		CHECK(sv.find_last_not_of("z") == 181);

		CHECK(sv.find_first_of(needle[0]) == 132);

		for (int i = 0; i < 184; i++)
		{
			INFO("i = " + std::to_string(i));
			KStringViewZ tsv(&haystack[0][i]);
			if (i < 182)
			{
				CHECK(tsv.find_last_not_of('z') == 181 - i);
				CHECK(tsv.find_last_not_of("z") == 181 - i);
			}
			else
			{
				CHECK(tsv.find_last_not_of('z') == KStringViewZ::npos);
				CHECK(tsv.find_last_not_of("z") == KStringViewZ::npos);
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
		for (int i = 0; i < 26; i++)
		{
			INFO("i = " + std::to_string(i));
			KStringViewZ tsv(&haystack[0][i]);
			KStringViewZ tneedle(&needle[0][i]);
			KStringViewZ tneedle2(&needle[2][i]);

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
		KStringViewZ sv("0123456  9abcdef h");
		KStringViewZ sv2("0123456  9abcdefh "); // slight variant for find last not of
		CHECK( sv.find_first_not_of(' ') == 0 );
		CHECK( sv.find_first_not_of(" ") == 0 );
		CHECK( sv.find_first_not_of(" d") == 0 );
		CHECK( sv.find_first_not_of('0') == 1 );
		CHECK( sv.find_first_not_of("0") == 1 );
		CHECK( sv.find_first_not_of("02") == 1 );
		CHECK( sv.find_first_not_of("0123456789abcdef ") == 17 ); // This test revealed problems
		CHECK( sv.find_last_not_of("0123456789abcdef ") == 17 ); // In this case, both should be same
		CHECK( sv2.find_last_not_of("0123456789abcdef ") == 16 ); // Checking for positional bugs
		CHECK( sv.find_first_not_of("0123456789abcdefgh ") == KStringViewZ::npos );
	}

	SECTION("find_first_not_of with pos")
	{
		KStringViewZ sv("0123456  9abcdef h");
		CHECK( sv.find_first_not_of(' ', 2) == 2 );
		CHECK( sv.find_first_not_of(" ", 2) == 2 );
		CHECK( sv.find_first_not_of(" d", 2) == 2 );
		CHECK( sv.find_first_not_of('2', 2) == 3 );
		CHECK( sv.find_first_not_of("2", 2) == 3 );
		CHECK( sv.find_first_not_of("02", 2) == 3 );
		CHECK( sv.find_first_not_of("0123456789abcdef ", 2) == 17 );
		CHECK( sv.find_first_not_of("0123456789abcdefgh ", 2) == KStringViewZ::npos );
	}

	SECTION("find_last_of")
	{
		KString s(20, '-');
		s.insert(0, "abcdefg");
		KStringViewZ sv(s);
		auto pos = sv.find_last_of("abcdefghijklmnopqrstuvwxyz");
		CHECK( pos == 6 );
	}

	SECTION("find_last_not_of")
	{
		KString s(20, '-');
		s.insert(0, "abcdefg");
		KStringViewZ sv(s);
		auto pos = sv.find_last_not_of("&%-!§&/()=?*#1234567890üöä");
		CHECK( pos == 6 );
	}

	SECTION("erase by index")
	{
		KStringViewZ sv("0123456789abcdefgh");
		KStringViewZ svd = sv;
		KStringViewZ sve;
		svd.erase(2);
		sve = "0123456789abcdefgh";
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
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
	}

	SECTION("erase by iterator")
	{
		KStringViewZ sv("0123456789abcdefgh");
		KStringViewZ svd = sv;
		KStringViewZ sve;
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
		sve = "0123456789abcdefgh";
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
		KStringViewZ sv("0123456789abcdefgh");
		KStringViewZ svd = sv;
		KStringViewZ sve;
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
		sve = "0123456789abcdefgh";
		CHECK( svd == sve );
		CHECK( it == svd.end() );
		svd = sv;
		it = svd.erase(svd.end()-2, svd.end());
		sve = "0123456789abcdefgh";
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
		KStringViewZ sv = "a string view";
		s.Printf("This is %s", sv);
		CHECK( s == "This is a string view" );
		s.Format("This is {}", sv);
		CHECK( s == "This is a string view" );
	}

	SECTION("SSE Short Haystack")
	{
		KStringViewZ sv = "abcdefabcdef";
		auto pos = sv.find_first_of("ghijk");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_not_of("abcdef");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_of("ghijk");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_not_of("abcdef");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringViewZ::npos );
	}

	SECTION("SSE Long Haystack")
	{
		KStringViewZ sv = "abcdefabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdeffabcdef";
		auto pos = sv.find_first_of("ghijk");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_not_of("abcdef");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_of("ghijk");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_not_of("abcdef");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_first_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_of("ghijklmnopqrstuvwxyz");
		CHECK( pos == KStringViewZ::npos );
		pos = sv.find_last_not_of("abcdefghijklmnopqrst");
		CHECK( pos == KStringViewZ::npos );
	}

	SECTION("conversion functions for KStringViewZ")
	{
		KStringViewZ s;
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

	SECTION("String assignments")
	{
		KStringViewZ svz = "abcdefg";
		KString s = svz;
		CHECK ( s == svz );
		KStringView sv = svz;
		CHECK ( sv == svz );
		s = "ghijklmnop";
		sv = s;
		CHECK ( sv == s );
		svz = s;
		CHECK ( svz == s );
	}

	SECTION("String literals")
	{
		KStringViewZ svz = "abcdefg"_ksz;
		KString s = svz;
		CHECK ( s == svz );
		KStringView sv = svz;
		CHECK ( sv == svz );
		s = "ghijklmnop"_ks;
		sv = s;
		CHECK ( sv == s );
		svz = s;
		CHECK ( svz == s );
	}


}

