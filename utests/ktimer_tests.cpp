
#include "catch.hpp"
#include <dekaf2/ktimer.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/klog.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KTimer")
//-----------------------------------------------------------------------------
{
	std::size_t iCalled = 0;

	KTimer Timer(std::chrono::milliseconds(50));

	auto ID = Timer.CallEvery(std::chrono::milliseconds(50), [&](KTimer::Timepoint tp)
	{
//		kDebug(1, "the current timepoint is: {}", KTimer::ToTimeT(tp));
		++iCalled;
	});

	kMilliSleep(500);

	CHECK ( Timer.Cancel(ID) );
	CHECK ( iCalled >= 8 );
	CHECK ( iCalled <= 11 );

}
