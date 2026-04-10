#include "catch.hpp"

#include <dekaf2/net/address/kresolve.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

TEST_CASE("KResolve")
{
	SECTION("DNSLookups")
	{
		auto sIP = kResolveHostIPV4("www.google.com");
		CHECK ( sIP.empty() == false );

		KString sTestIP ("66.249.66.1"); // this is supposed to be crawl-66-249-66-1.googlebot.com
		auto sHostname = kReverseLookup (sTestIP);
		CHECK (kResolveHost (sHostname, true, false) == sTestIP);
	}
}

