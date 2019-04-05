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
		CHECK ( KLog::getInstance().GetName().empty() == false );
	}
}
