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

