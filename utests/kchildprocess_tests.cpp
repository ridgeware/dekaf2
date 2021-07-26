#include "catch.hpp"

#include <dekaf2/kchildprocess.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/ksystem.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

int chtest1(int argc, char** argv)
{
	kDebug(1, "hello parent")
	return 7;
}

// on MacOS, the leak detector is not functional even when
// ASAN is compiled in
#if defined(DEKAF2_HAS_ASAN) && !defined(DEKAF2_IS_OSX)
	#define CHECK_LEAK_DETECTOR 1
#endif

TEST_CASE("KChildProcess")
{
#ifdef CHECK_LEAK_DETECTOR
	kDebug(2, "check leak detector");
	// only run this test if leak detection is disabled -
	// the signal handler thread will not be properly
	// terminated at end of execution and be reported
	// as leak - which is of no practical relevance..
	auto sAsanOptions = kGetEnv("ASAN_OPTIONS");
	if (sAsanOptions.contains("detect_leaks=0"))
#endif
	{
		kDebug(1, "I am parent");
		KChildProcess Child;
		CHECK ( Child.Fork(chtest1) );
		CHECK ( Child.Join()        );
		kDebug(1, "child is back home");
		CHECK ( Child.GetExitStatus() == 7 );
	}
}

#endif
