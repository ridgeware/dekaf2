#include "catch.hpp"

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystemstats.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

// this call does not work on docker builds, and
// it is not a test - we suppress it for the moment
#if 0

TEST_CASE("KSystemStats")
{
	SECTION("Gather")
	{
		KSystemStats KStats;
		KStats.GatherAll();
	}
}

#endif // of 0
#endif // of DEKAF2_IS_WINDOWS
