#include "catch.hpp"

#include <dekaf2/ksystem.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KSystem")
{
	SECTION("KPing")
	{
		// do not run the pings on github actions (or other CIs) or inside containers (no ping binary)
		if (kGetEnv("CI").empty() && kGetEnv("GITHUB_ACTION").empty() && !kIsInsideContainer())
		{
			CHECK ( kPing("localhost-nononono-kgjhdsgfjasf", chrono::seconds(1)) == KDuration::zero() );
			CHECK ( kPing("localhost", chrono::seconds(1)) != KDuration::zero() );
			CHECK ( kPing("www.google.com") != KDuration::zero() );
		}
	}
}

