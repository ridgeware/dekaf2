#include "catch.hpp"

#include <dekaf2/kstringview.h>

using namespace dekaf2;

TEST_CASE("KFindSetOfChars") {

	SECTION("find_first_in")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( KFindSetOfChars(" ")   .find_first_in(sv) == 7 );
		CHECK( KFindSetOfChars(" d")  .find_first_in(sv) == 7 );
		CHECK( KFindSetOfChars("0")   .find_first_in(sv) == 0 );
		CHECK( KFindSetOfChars("02")  .find_first_in(sv) == 0 );
		CHECK( KFindSetOfChars("h")   .find_first_in(sv) == 17 );
		CHECK( KFindSetOfChars("h-")  .find_first_in(sv) == 17 );
		CHECK( KFindSetOfChars("ab f").find_first_in(sv) == 7 );
		CHECK( KFindSetOfChars("abf ").find_first_in(sv) == 7 );
		CHECK( KFindSetOfChars("-")   .find_first_in(sv) == KStringView::npos );
		CHECK( KFindSetOfChars("!-")  .find_first_in(sv) == KStringView::npos );
	}

	SECTION("find_first_in with pos")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( KFindSetOfChars(" ")   .find_first_in(sv, 2) == 7 );
		CHECK( KFindSetOfChars(" d")  .find_first_in(sv, 2) == 7 );
		CHECK( KFindSetOfChars("0")   .find_first_in(sv, 2) == KStringView::npos );
		CHECK( KFindSetOfChars("02")  .find_first_in(sv, 2) == 2 );
		CHECK( KFindSetOfChars("h")   .find_first_in(sv, 2) == 17 );
		CHECK( KFindSetOfChars("h-")  .find_first_in(sv, 2) == 17 );
		CHECK( KFindSetOfChars("ab f").find_first_in(sv, 2) == 7 );
		CHECK( KFindSetOfChars("abf ").find_first_in(sv, 2) == 7 );
		CHECK( KFindSetOfChars("-")   .find_first_in(sv, 2) == KStringView::npos );
		CHECK( KFindSetOfChars("!-")  .find_first_in(sv, 2) == KStringView::npos );
	}

	SECTION("find_last_in")
	{
		KStringView sv("0123456  9abcdef");
		CHECK( KFindSetOfChars(" ")   .find_last_in(sv) == 8 );
		CHECK( KFindSetOfChars(" 1")  .find_last_in(sv) == 8 );
		CHECK( KFindSetOfChars("0")   .find_last_in(sv) == 0 );
		CHECK( KFindSetOfChars("0-")  .find_last_in(sv) == 0 );
		CHECK( KFindSetOfChars("f")   .find_last_in(sv) == 15 );
		CHECK( KFindSetOfChars("fe")  .find_last_in(sv) == 15 );
		CHECK( KFindSetOfChars("12 3").find_last_in(sv) == 8 );
		CHECK( KFindSetOfChars("123 ").find_last_in(sv) == 8 );
		CHECK( KFindSetOfChars("-")   .find_last_in(sv) == KStringView::npos );
		CHECK( KFindSetOfChars("!-")  .find_last_in(sv) == KStringView::npos );

		KFindSetOfChars MySet("abcdef");
		auto iPos = MySet.find_last_in(sv);
		CHECK ( iPos == 15 );
		CHECK ( sv[iPos] == 'f' );
		auto iPos2 = MySet.find_last_in(sv, --iPos);
		CHECK ( iPos2 == 14 );
		CHECK ( sv[iPos2] == 'e' );

		KFindSetOfChars MySet2(" \r\n\t");
		iPos = MySet2.find_last_not_in(sv);
		CHECK ( iPos == 15 );
		CHECK ( sv[iPos] == 'f' );
		iPos2 = MySet2.find_last_not_in(sv, --iPos);
		CHECK ( iPos2 == 14 );
		CHECK ( sv[iPos2] == 'e' );
	}

	SECTION("find_first_not_in")
	{
		KStringView sv("0123456  9abcdef h");
		KStringView sv2("0123456  9abcdefh "); // slight variant for find last not of
		CHECK( KFindSetOfChars(" ")  .find_first_not_in(sv) == 0 );
		CHECK( KFindSetOfChars(" d") .find_first_not_in(sv) == 0 );
		CHECK( KFindSetOfChars("0")  .find_first_not_in(sv) == 1 );
		CHECK( KFindSetOfChars("02") .find_first_not_in(sv) == 1 );
		CHECK( KFindSetOfChars("0123456789abcdef ")  .find_first_not_in(sv ) == 17 ); // This test revealed problems
		CHECK( KFindSetOfChars("0123456789abcdef ")  .find_first_not_in(sv ) == 17 ); // In this case, both should be same
		CHECK( KFindSetOfChars("0123456789abcdef ")  .find_first_not_in(sv2) == 16 ); // Checking for positional bugs
		CHECK( KFindSetOfChars("0123456789abcdefgh ").find_first_not_in(sv ) == KStringView::npos );
	}

	SECTION("find_first_not_in with pos")
	{
		KStringView sv("0123456  9abcdef h");
		CHECK( KFindSetOfChars(" ") .find_first_not_in(sv, 2) == 2 );
		CHECK( KFindSetOfChars(" d").find_first_not_in(sv, 2) == 2 );
		CHECK( KFindSetOfChars("2") .find_first_not_in(sv, 2) == 3 );
		CHECK( KFindSetOfChars("02").find_first_not_in(sv, 2) == 3 );
		CHECK( KFindSetOfChars("0123456789abcdef ")  .find_first_not_in(sv, 2) == 17 );
		CHECK( KFindSetOfChars("0123456789abcdefgh ").find_first_not_in(sv, 2) == KStringView::npos );
	}

	SECTION("find_last_in")
	{
		KString s(20, '-');
		s.insert(0, "abcdefg");
		KStringView sv(s);
		auto pos = KFindSetOfChars("abcdefghijklmnopqrstuvwxyz").find_last_in(s);
		CHECK( pos == 6 );
	}

}
