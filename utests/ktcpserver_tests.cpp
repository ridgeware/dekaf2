#include "catch.hpp"

#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/system/os/ksystem.h>
#include <thread>

using namespace dekaf2;

// This test works, but somehow the signal gets the catch framework into
// a bad state so that other tests spuriously fail when this test is run.
// Therefore it is in general disabled, except when we want to explicitly
// test this use case.

TEST_CASE("KTCPServer::IsPortAvailable")
{
	// wildcard and explicit loopback must both be bindable on an unused port
	CHECK ( KTCPServer::IsPortAvailable(30306)              == true  );
	CHECK ( KTCPServer::IsPortAvailable(30306, "127.0.0.1") == true  );
	// an invalid address is not available, whatever the port
	CHECK ( KTCPServer::IsPortAvailable(30306, "not.an.ip.address") == false );
}

namespace {

// answers the first request line with the same line
class EchoServer : public KTCPServer
{

public:

	EchoServer(uint16_t iPort)
	: KTCPServer(iPort, false, 5)
	{
	}

protected:

	void Session(KStream& stream, KStringView sRemoteEndPoint, int iSocketFd) override
	{
		KString sLine;

		if (stream.ReadLine(sLine))
		{
			stream.WriteLine(sLine).Flush();
		}
	}

}; // EchoServer

// connects to sEndpoint and returns true if the server echoes a line
bool Echo(KStringView sEndpoint)
{
	KTCPStream Stream(KTCPEndPoint(sEndpoint),
	                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::milliseconds(500)));

	if (!Stream.Good())
	{
		return false;
	}

	Stream.WriteLine("ping").Flush();

	KString sLine;
	return Stream.ReadLine(sLine) && sLine == "ping";
}

} // end of anonymous namespace

TEST_CASE("KTCPServer ephemeral port")
{
	bool bHaveIPv6Loopback { false };

	// wildcard listener, dual stack where available
	{
		EchoServer Server(0);
		CHECK   ( Server.GetPort() == 0 );
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );

		auto iPort = Server.GetPort();
		CHECK ( iPort != 0 );
		CHECK ( Echo(kFormat("127.0.0.1:{}", iPort)) == true );
		bHaveIPv6Loopback = Echo(kFormat("[::1]:{}", iPort));

		Server.Stop();
	}

	// loopback-only listener on an ephemeral port
	{
		EchoServer Server(0);
		Server.SetBindAddress("127.0.0.1");
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );

		auto iPort = Server.GetPort();
		CHECK ( iPort != 0 );
		CHECK ( Echo(kFormat("127.0.0.1:{}", iPort)) == true );

		if (bHaveIPv6Loopback)
		{
			// the same port on another interface address must not connect
			CHECK ( Echo(kFormat("[::1]:{}", iPort)) == false );
		}

		Server.Stop();
	}
}

TEST_CASE("KTCPServer stop while accepting", "[.stress]")
{
	// Stop() must not close the acceptor while the IO thread chains the next accept for a
	// connection that just came in. Every round connects and stops after a short busy
	// wait, which runs from 0 to 63 microseconds over the rounds: the accept handler of
	// the connection runs only for a few microseconds, at a time that depends on the
	// machine, and the waits move the stop across it. It takes about a thousand rounds
	// to meet that window reliably, so the test is hidden as a stress test - run it
	// with dekaf2-utests "[stress]".
	for (int iRound = 0; iRound < 1000; ++iRound)
	{
		EchoServer Server(0);
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );

		KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", Server.GetPort())),
		                  KStreamOptions(chrono::seconds(2)));
		REQUIRE ( Stream.Good() == true );

		KStopTime Wait;

		while (Wait.elapsed() < chrono::microseconds(iRound % 64))
		{
		}

		Server.Stop();
	}
}

TEST_CASE("KTCPServer restart after stop while accepting")
{
	// after a restart, Start() runs the accept handlers that a stop left pending, with
	// the acceptors that the stop closed - they have to return, and the restarted server
	// has to accept again
	EchoServer Server(0);

	for (int iRound = 0; iRound < 5; ++iRound)
	{
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );

		KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", Server.GetPort())),
		                  KStreamOptions(chrono::seconds(2)));
		REQUIRE ( Stream.Good() == true );

		Server.Stop();
	}

	REQUIRE ( Server.Start(chrono::seconds(2), false) == true );
	CHECK   ( Echo(kFormat("127.0.0.1:{}", Server.GetPort())) == true );
	Server.Stop();
}

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

	CHECK( server.Start(chrono::seconds(5), true) == true );

	t1.join();
}

#endif
