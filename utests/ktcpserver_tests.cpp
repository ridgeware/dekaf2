#include "catch.hpp"

#include <dekaf2/ktcpserver.h>
#include <dekaf2/ksystem.h>
#include <thread>

using namespace dekaf2;

// This test works, but somehow the signal gets the catch framework into
// a bad state so that other tests spuriously fail when this test is run.
// Therefore it is in general disabled, except when we want to explicitly
// test this use case.

#ifdef DEKAF2_ENABLE_KTCPSERVER_TEST

TEST_CASE("KTCPServer")
{
	KTCPServer server(6789, false);
	server.RegisterShutdownWithSignals({ SIGTERM });

	// now start a thread that waits a bit and then sends a SIGTERM
	std::thread t1([]()
	{
		kSleep(chrono::milliseconds(100));
		kill(kGetPid(), SIGTERM);
	});

	CHECK( server.Start(1, true) == true );

	t1.join();
}

#endif
