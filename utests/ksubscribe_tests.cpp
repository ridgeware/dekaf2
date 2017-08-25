#include "catch.hpp"

#include <dekaf2/ksubscribe.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KSubscribe") {

	typedef KSubscription<KString> MyStringBuf;
	typedef KSubscriber<KStringView, KString> MyStringView;

	SECTION("tests on construction and copying")
	{
		MyStringBuf sb = "this is a longer string into which we will point with string views";
		MyStringView sv;
		sv = sb;
		sv->remove_prefix(5);
		sv->remove_suffix(10);
		CHECK ( sb.CountSubscriptions() == 1);
	}

}
