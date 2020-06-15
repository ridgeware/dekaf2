#include "catch.hpp"
#include <dekaf2/kgetruntimestack.h>
#include <dekaf2/kstring.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

TEST_CASE("kGetRuntimeStack")
{
	SECTION("backtrace")
	{
		KString sTrace = kGetRuntimeStack();
		INFO  ( sTrace );
		CHECK ( sTrace.Split("\n", "").size() >= 5 );
	}

}

#endif
