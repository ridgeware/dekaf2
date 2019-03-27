#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/ksystemstats.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

TEST_CASE("KSystemStats")
{
	SECTION("Gather")
	{
		KSystemStats KStats;
		KStats.GatherAll();
	}
}

#endif // of DEKAF2_IS_WINDOWS
