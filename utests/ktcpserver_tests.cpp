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
	CHECK ( KTCPServer::IsPortAvailable(30306, "127.0.0.1x")        == false );
	CHECK ( KTCPServer::IsPortAvailable(30306, "127.0.0.1:80")      == false );
	// the address as KIPAddress: an unspecified address is the wildcard
	CHECK ( KTCPServer::IsPortAvailable(30306, KIPAddress())                   == true );
	CHECK ( KTCPServer::IsPortAvailable(30306, KIPAddress(KIPAddress4(127,0,0,1))) == true );
	// an address that is none of this host (TEST-NET-1, RFC 5737)
	CHECK ( KTCPServer::IsPortAvailable(30306, KIPAddress(KIPAddress4(192,0,2,1))) == false );
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

TEST_CASE("KTCPServer bind address")
{
	SECTION("as KIPAddress")
	{
		EchoServer Server(0);
		CHECK   ( Server.SetBindAddress(KIPAddress(KIPAddress4(127,0,0,1))) == true );
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );
		CHECK   ( Echo(kFormat("127.0.0.1:{}", Server.GetPort())) == true );
		Server.Stop();
	}

	SECTION("an unspecified KIPAddress is refused")
	{
		// this is what a failed conversion from a string leaves: the server must
		// not listen on all interfaces then
		KIPError ec;
		KIPAddress Failed("not.an.ip.address", ec);
		CHECK ( ec );

		EchoServer Server(0);
		CHECK ( Server.SetBindAddress(Failed) == false );
		CHECK ( Server.Start(chrono::seconds(2), false) == false );
		CHECK ( Server.Error().contains("invalid bind address") );
		// a failed start still has to be stopped before the next one
		Server.Stop();

		// a valid address after that makes the server start
		CHECK   ( Server.SetBindAddress(KIPAddress(KIPAddress4(127,0,0,1))) == true );
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );
		Server.Stop();
	}

	SECTION("as text")
	{
		EchoServer Server(0);
		CHECK ( Server.SetBindAddress("not.an.ip.address") == false );
		CHECK ( Server.Start(chrono::seconds(2), false) == false );
		CHECK ( Server.Error().contains("invalid bind address 'not.an.ip.address'") );
		// a failed start still has to be stopped before the next one
		Server.Stop();

		// text behind an address makes it invalid
		CHECK ( Server.SetBindAddress("127.0.0.1x")    == false );
		CHECK ( Server.SetBindAddress("127.0.0.1:80")  == false );
		CHECK ( Server.SetBindAddress("127.0.0.1/8")   == false );

		// the wildcard address of a family is a valid bind address as text
		CHECK   ( Server.SetBindAddress("0.0.0.0") == true );
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );
		CHECK   ( Echo(kFormat("127.0.0.1:{}", Server.GetPort())) == true );
		Server.Stop();
	}

	SECTION("an empty text is all interfaces")
	{
		EchoServer Server(0);
		CHECK   ( Server.SetBindAddress("127.0.0.1") == true );
		CHECK   ( Server.SetBindAddress("")          == true );
		REQUIRE ( Server.Start(chrono::seconds(2), false) == true );
		CHECK   ( Echo(kFormat("127.0.0.1:{}", Server.GetPort())) == true );
		Server.Stop();
	}
}

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
