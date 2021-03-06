#include "catch.hpp"

#include <dekaf2/ksystem.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KSystem")
{
	SECTION("DNSLookups")
	{
		KString sTestIP ("66.249.66.1");
		CHECK (kIsValidIPv4 (sTestIP));
		CHECK (!kIsValidIPv4 ("257.1.1.1"));
		CHECK (kIsValidIPv6 ("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
		CHECK (!kIsValidIPv6 ("66.249.66.1"));
		auto sHostname = kHostLookup ("66.249.66.1");
		CHECK (kResolveHost (sHostname, true, false) == sTestIP);
	}
	
}

