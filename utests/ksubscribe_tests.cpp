#include "catch.hpp"

#include <dekaf2/ksubscribe.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KSubscribe") {

	typedef KSubscription<KString> MyStringBuf;
	typedef KSubscriber<KStringView, KString> MyStringView;

	SECTION("basic instantiation and release")
	{
		KString s("the string");
		MyStringBuf sb(s);
		MyStringView sv(sb);
		KString s2(sv);
		CHECK( s2 == s );
	}

	SECTION("basic instantiation and release 2")
	{
		KString s("the string");
		MyStringBuf sb(s);
		MyStringView sv;
		sv.SetSubscription(sb);
		KString s2(sv);
		CHECK( s2 == s );
	}

	SECTION("basic instantiation and release 3")
	{
		KString s("the string");
		MyStringBuf sb(s);
		MyStringView sv;
		sv = sb;
		KString s2(sv);
		CHECK( s2 == s );
	}

	SECTION("basic cloning")
	{
		KString s("the string");
		MyStringView sv;
		{
			MyStringBuf sb(s);
			sv = sb;
			KString s2(sv);
			CHECK( s2 == s );
		}
		KString s2(sv);
		CHECK( s2 == s );
	}

	SECTION("basic cloning 2")
	{
		KString s("the string");
		MyStringView sv;
		{
			MyStringBuf sb(s);
			sv.SetSubscription(sb);
			KString s2(sv);
			CHECK( s2 == s );
		}
		KString s2(sv);
		CHECK( s2 == s );
	}

	SECTION("tests on modification")
	{
		MyStringView sv;
		MyStringBuf sb = "this is a longer string into which we will point with string views";
		sv = sb;
		sv->remove_prefix(2);
		sv->remove_suffix(2);
		KString s2(sv);
		CHECK ( s2 ==  "is is a longer string into which we will point with string vie" );
		CHECK ( sb.CountSubscriptions() == 1 );
	}

	SECTION("tests on modification and cloning")
	{
		MyStringView sv;
		{
			MyStringBuf sb = "this is a longer string into which we will point with string views";
			sv = sb;
			sv->remove_prefix(2);
			sv->remove_suffix(2);
			CHECK ( sv.CountSubscriptions() == 1 );
		}
		KString s2(sv);
		CHECK ( s2 ==  "is is a longer string into which we will point with string vie" );
		CHECK ( sv.CountSubscriptions() == 0 );
	}

	SECTION("tests on modification and cloning")
	{
		MyStringView sv;
		MyStringView sv2;
		{
			MyStringBuf sb = "this is a longer string into which we will point with string views";
			sv = sb;
			sv2 = sb;
			sv->remove_prefix(2);
			sv->remove_suffix(2);
			CHECK ( sb.CountSubscriptions()  == 2 );
			CHECK ( sv.CountSubscriptions()  == 2 );
			CHECK ( sv2.CountSubscriptions() == 2 );
		}
		KString s2(sv);
		CHECK ( s2 ==  "is is a longer string into which we will point with string vie" );
		s2 = sv2;
		CHECK ( s2 ==  "this is a longer string into which we will point with string views" );
		CHECK ( sv.CountSubscriptions() == 0);
	}

}
