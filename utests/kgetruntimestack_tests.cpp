#include "catch.hpp"
#include <dekaf2/system/os/kgetruntimestack.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

TEST_CASE("kGetRuntimeStack")
{
	SECTION("backtrace")
	{
#ifndef DEKAF2_HAS_MUSL
		KString sTrace = kGetRuntimeStack();
		INFO  ( sTrace );
		CHECK ( sTrace.Split('\n').size() >= 5 );
#endif
	}

	SECTION("short backtrace")
	{
#ifndef DEKAF2_HAS_MUSL
		// on Windows from CaptureStackBackTrace() and DbgHelp - without a symbol
		// file (PDB) the frames name the module and the offset in it
		KString sTrace = kGetBacktrace();
		INFO  ( sTrace );
		CHECK ( sTrace.Split('\n').size() >= 5 );
#endif
	}

}
