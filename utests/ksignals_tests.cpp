#include "catch.hpp"

#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/system/process/kchildprocess.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

int ReportHandlerThread(int, char**)
{
	// Dekaf::Fork() starts a signal handler thread of its own in the child
	return KSignals::HasHandlerThread() ? 0 : 1;
}

} // end of anonymous namespace

TEST_CASE("KSignals")
{
	SECTION("one signal handling per process")
	{
		// the utests run with KInit(true)
		CHECK ( KSignals::HasHandlerThread() );

		auto* pSignals = Dekaf::getInstance().Signals();
		REQUIRE ( pSignals != nullptr );

		pSignals->SetSignalHandler(SIGTERM, [](int) {});
		CHECK ( pSignals->GetSignalHandler(SIGTERM) );

		{
			// a second instance does not set the default handler for SIGTERM again
			KSignals Second(true);
			CHECK ( Second.GetSignalHandler(SIGTERM) );
		}

		CHECK ( pSignals->GetSignalHandler(SIGTERM) );

		// back to the default handler of dekaf2
		pSignals->SetDefaultHandler(SIGTERM);
		CHECK ( !pSignals->GetSignalHandler(SIGTERM) );
	}

	SECTION("a forked child has its own handler thread")
	{
		KChildProcess Child;
		REQUIRE ( Child.Fork(ReportHandlerThread) );
		CHECK   ( Child.Join() );
		CHECK   ( Child.GetExitStatus() == 0 );
	}
}

#endif
