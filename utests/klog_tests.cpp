#include "catch.hpp"

#include <dekaf2/klog.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

int neverBeCalled()
{
	static int iCallCount = 0;
	return iCallCount++;
}


TEST_CASE("KLog") {

	SECTION("KLog inline optimization")
	{
		kDebug(100, "this should never be output {}", neverBeCalled());
		CHECK ( neverBeCalled() == 0 );
	}

	SECTION("Program name")
	{
#ifdef DEKAF2_WITH_KLOG
		CHECK ( KLog::getInstance().GetName().empty() == false );
#else
		CHECK ( KLog::getInstance().GetName().empty() == true );
#endif
	}
}
