#include "catch.hpp"

#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>

using namespace dekaf2;

TEST_CASE("KStreamOptions")
{
	KTCPServer Server(7613, false, 2);
	Server.Start(chrono::seconds(5), false);

	SECTION("keepalive options set and read back")
	{
		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7613"), KStreamOptions(chrono::seconds(5)));

		REQUIRE ( Stream.Good() == true );

		auto fd = Stream.GetNativeSocket();

		CHECK ( kSetTCPKeepAliveInterval    (fd, chrono::seconds(60))    == true );
		CHECK ( kSetTCPKeepAliveProbes      (fd, chrono::seconds(10), 3) == true );
		CHECK ( kSetTCPConnectionDropTimeout(fd, chrono::seconds(45))    == true );

#if !DEKAF2_IS_WINDOWS
		CHECK ( kGetTCPKeepAliveInterval     (fd) == chrono::seconds(60) );
		CHECK ( kGetTCPKeepAliveProbeInterval(fd) == chrono::seconds(10) );
		CHECK ( kGetTCPKeepAliveProbeCount   (fd) == 3                   );
#endif
#if DEKAF2_IS_LINUX || DEKAF2_IS_MACOS
		CHECK ( kGetTCPConnectionDropTimeout (fd) == chrono::seconds(45) );
#endif
	}

	SECTION("ApplySocketOptions applies the full set")
	{
		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7613"), KStreamOptions(chrono::seconds(5)));

		REQUIRE ( Stream.Good() == true );

		auto fd = Stream.GetNativeSocket();

		KStreamOptions Options;
		Options.SetKeepAliveInterval     (chrono::seconds(61))
		       .SetKeepAliveProbeInterval(chrono::seconds(11))
		       .SetKeepAliveProbeCount   (4)
		       .SetConnectionDropTimeout (chrono::seconds(46));

		CHECK ( Options.ApplySocketOptions(fd) == true );

#if !DEKAF2_IS_WINDOWS
		CHECK ( kGetTCPKeepAliveInterval     (fd) == chrono::seconds(61) );
		CHECK ( kGetTCPKeepAliveProbeInterval(fd) == chrono::seconds(11) );
		CHECK ( kGetTCPKeepAliveProbeCount   (fd) == 4                   );
#endif
#if DEKAF2_IS_LINUX || DEKAF2_IS_MACOS
		CHECK ( kGetTCPConnectionDropTimeout (fd) == chrono::seconds(46) );
#endif
	}

	SECTION("SetDeadPeerDetection derives the parameter set")
	{
		KStreamOptions Options;
		Options.SetDeadPeerDetection(chrono::seconds(60));

		// half waiting, three probes across the second half, and the
		// retransmission path bounded to the same total
		CHECK ( Options.GetKeepAliveInterval()      == chrono::seconds(30) );
		CHECK ( Options.GetKeepAliveProbeInterval() == chrono::seconds(10) );
		CHECK ( Options.GetKeepAliveProbeCount()    == 3                   );
		CHECK ( Options.GetConnectionDropTimeout()  == chrono::seconds(60) );

		// small budgets clamp to one second per value
		Options.SetDeadPeerDetection(chrono::seconds(4));

		CHECK ( Options.GetKeepAliveInterval()      == chrono::seconds(2) );
		CHECK ( Options.GetKeepAliveProbeInterval() == chrono::seconds(1) );
		CHECK ( Options.GetKeepAliveProbeCount()    == 3                  );
		CHECK ( Options.GetConnectionDropTimeout()  == chrono::seconds(4) );

		// and the set applies to a real socket
		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7613"), KStreamOptions(chrono::seconds(5)));

		REQUIRE ( Stream.Good() == true );

		auto fd = Stream.GetNativeSocket();

		Options.SetDeadPeerDetection(chrono::seconds(90));

		CHECK ( Options.ApplySocketOptions(fd) == true );

#if !DEKAF2_IS_WINDOWS
		CHECK ( kGetTCPKeepAliveInterval     (fd) == chrono::seconds(45) );
		CHECK ( kGetTCPKeepAliveProbeInterval(fd) == chrono::seconds(15) );
		CHECK ( kGetTCPKeepAliveProbeCount   (fd) == 3                   );
#endif
#if DEKAF2_IS_LINUX || DEKAF2_IS_MACOS
		CHECK ( kGetTCPConnectionDropTimeout (fd) == chrono::seconds(90) );
#endif
	}
}
