#include "catch.hpp"

#include <dekaf2/ktcpserver.h>
#include <dekaf2/ksystem.h>
#include <thread>

using namespace dekaf2;

TEST_CASE("KTCPServer")
{
	KTCPServer server(6789, false);
	server.RegisterShutdownWithSignals({ SIGTERM });

	// now start a thread that waits a bit and then sends a SIGTERM
	std::thread t1([]()
	{
		kMilliSleep(100);
		kill(kGetPid(), SIGTERM);
	});

	CHECK( server.Start(1, true) == true );

	t1.join();
}

