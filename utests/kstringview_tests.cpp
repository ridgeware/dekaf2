#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <vector>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KStringView") {

#ifdef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW
	KStringView::size_type npos = KStringView::npos;
#else
	KStringView::size_type npos = KString::npos;
#endif

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
		CHECK( sv.find('-') == npos );
		CHECK( sv.find("-") == npos );
		CHECK( sv.find("!-") == npos );
		CHECK( sv.find('1', 1) == 1 );
		CHECK( sv.find("1", 1) == 1 );
		CHECK( sv.find("12", 1) == 1 );
		CHECK( sv.find('1', 2) == npos );
		CHECK( sv.find("1", 2) == npos );
		CHECK( sv.find("12", 2) == npos );
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
		CHECK( sv.rfind('-') == npos );
		CHECK( sv.rfind("-") == npos );
		CHECK( sv.rfind("!-") == npos );
		CHECK( sv.rfind('f', 14) == npos );
		CHECK( sv.rfind("f", 14) == npos );
		CHECK( sv.rfind("ef", 13) == npos );
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
		CHECK( sv.find_first_of('-') == npos );
		CHECK( sv.find_first_of("-") == npos );
		CHECK( sv.find_first_of("!-") == npos );
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
		CHECK( sv.find_last_of('-') == npos );
		CHECK( sv.find_last_of("-") == npos );
		CHECK( sv.find_last_of("!-") == npos );
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
		CHECK( sv.find_first_not_of("0123456789abcdefgh ") == npos );
	}

}

