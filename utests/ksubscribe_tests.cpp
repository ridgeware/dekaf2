#include "catch.hpp"

#include <ksubscribe.h>
#include <kstring.h>

using namespace dekaf2;

TEST_CASE("KSubscribe") {

	typedef KSubscription<KString> MyStringBuf;
	typedef KSubscriber<KStringView, KString> MyStringView;

	SECTION("tests on construction and copying")
	{
		MyStringBuf sb = "this is a longer string into which we will point with string views";
		MyStringView sv;
		CHECK ( sv.HasClone() == false );
		CHECK ( sv.HasSubscription() == false );
		sv = sb;
		CHECK ( sv.HasSubscription() );
		CHECK ( sb.Subscriptions() == 1);
		sv.remove_prefix(5);
		sv.remove_suffix(10);
		sv.Clone();
		CHECK (sv.HasClone());
		CHECK ( sb.Subscriptions() == 0);
	}

}
