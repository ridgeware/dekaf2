#include "catch.hpp"

#include <dekaf2/ksystem.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KSystem")
{
	SECTION("DNSLookups")
	{
		KString sTestIP ("66.249.66.1"); // this is supposed to be crawl-66-249-66-1.googlebot.com
		CHECK (kIsValidIPv4 (sTestIP));
		CHECK (!kIsValidIPv4 ("257.1.1.1"));
		CHECK (kIsValidIPv6 ("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
		CHECK (!kIsValidIPv6 (sTestIP));
		auto sHostname = kHostLookup (sTestIP);
		CHECK (kResolveHost (sHostname, true, false) == sTestIP);
	}
	
	SECTION("KPing")
	{
		CHECK ( kPing("localhost-nononono-kgjhdsgfjasf", chrono::seconds(1)) == KDuration::zero() );
		CHECK ( kPing("localhost", chrono::seconds(1)) != KDuration::zero() );
		CHECK ( kPing("www.google.com") != KDuration::zero() );
	}
}

