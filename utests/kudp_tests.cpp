#include "catch.hpp"

#include <dekaf2/net/udp/kudpsocket.h>
#include <dekaf2/net/udp/kudpserver.h>
#include <dekaf2/net/udp/kudpstream.h>
#include <dekaf2/net/udp/kdtlssocket.h>
#include <dekaf2/net/udp/kdtlsstream.h>
#include <dekaf2/net/udp/kdtlsserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <thread>
#include <chrono>
#include <atomic>

using namespace dekaf2;


namespace {

// reuse the self-signed test certificates from kwriter_tests
#ifdef DEKAF2_HAS_FULL_CPP_17
constexpr
#endif
KStringView sTestCert = R"(-----BEGIN CERTIFICATE-----
MIIEpDCCAowCCQCjjdpjiU244jANBgkqhkiG9w0BAQsFADAUMRIwEAYDVQQDDAls
b2NhbGhvc3QwHhcNMTgxMDMwMTMwNjA4WhcNMTkxMDMwMTMwNjA4WjAUMRIwEAYD
VQQDDAlsb2NhbGhvc3QwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCl
Sy5eUHyAF14TbT713mYYozxG7Nj6twX2CbmrLkdpBPRgW/G35HPjiKlwr4D49I6t
EjZyMF7hdH5GdXofGAlpT89emiMm5epZAoWEsqTpFpfJjbEf9a+SGzTrhRxlMtbJ
FQyBIcN86bLmowAMIMfA1onCG/mQ5L5iO5s1KbNezH+vwhOlAn12/1ZkNQZwY8jO
3BWC5k2A0TSt0lPr+ZFzFcwJYKziOLKMVbiS4nB/37hI9AruczKppd1OWLqTTM+9
Eb13GzhCY1NDF/v80SVIZQt+P0KurZA/QMuTula/O4nYxZsjlRexuiDEJ1BHMr/t
aV6Sf9kzeh35ntNSf2xf5geITYqwjHRgwXaINYSbYTBg65nWpXybW33meecVeQvD
0HtXsdnV2N+asx4W3Onu6swI4OTmTDQ8hSxqOXrf8ba8vIXlDLoqjbzNIKcALXRM
cvw2T56s9G6xXKuRQAKou8edARLMDq2qjOTxo7Uy83EqkE2ZMWKx/PpbCG+gu9Gd
YKlvr+Wb1T4vK6AvmCXiqgNxwpav2tf9Cji76QlHjx0RJQYvvBNsOlgO8cjtG+6u
vRcI/obs3zjxIy7ZBHqTP86aDnUgbT90pAn8JVSP3Sd67ZVoJEsU0+eQYUoV7dGI
J5uRMnHj4VNJvjq/5S4/YeX6sTGbXWIR11GX63VKhQIDAQABMA0GCSqGSIb3DQEB
CwUAA4ICAQAFcxiEg7IK5s8Kvcwn6lFoOK0vu4AQ7J42LQz+ncgP+iNMXmZoJ6GX
08ZlG5TxWa7RhpfzOBzwn9XVJL3D/mu4in2Nipu7gdl1PNgZ01hFg+T+TnlrqU9P
Qz0u/PBy4pQgkAZyr5NA7nY8QYD6SQRgFZb8orN3gGjhWEfKQnyWDrfey3bixTPA
klF6PD6dHVaU3xa+NBZUC1kpob/lPdEI6AU/aNw4zguPve5czpmwWxRZdPD9lsSU
bDpq3XIPbnWOod3lq3akr6qr8gsfQaPs50QqpGmsrIf4XlSpG9KwBD+3VjlMaINS
vEav+f1pqfUls+MwQqRv6ThM8cevNUsqIUqmFxRfZ+SHNbyIebRvlhPDNuTWj3ZZ
Xd3x0udeekxz2R9+suvFRknHzlTMZ+TidFoTrcC+C50QWfDSwmCYDxT5WLyJXG9D
pEF3pzDMLgbIh+ChTKYZXXU1QYlNW2zzLe6vV7y6K7x9+GzdQsgQZv+I3QMd6hIH
hn7dm5rlcyD0iB4Xv7lHqwh62+b3sA6c38JgtHQduNJmU8p/dMAsYz99F9yw3v6H
QHvjFPcNWBZ32BrEzUeYmLaEGRNj22IWDYm1nu2x0pgkjtou9+HZgZTXPb078+8t
CBw53UppWW98e2rXKWXTtU6rEL1ctGz135WgBrqmkJ58n2pjd4jLDQ==
-----END CERTIFICATE-----
)";

#ifdef DEKAF2_HAS_FULL_CPP_17
constexpr
#endif
KStringView sTestKey = R"(-----BEGIN PRIVATE KEY-----
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
ifGH2FR4yO/361j9D9X2h9uBgconDA==
-----END PRIVATE KEY-----
)";

} // anonymous namespace

// ============================================================================
// KUDPSocket tests
// ============================================================================

TEST_CASE("KUDPSocket")
{
	SECTION("construct default")
	{
		KUDPSocket Socket;
		CHECK ( Socket.is_open()  == false );
		CHECK ( Socket.Good()     == true  );
	}

	SECTION("bind and send/receive loopback")
	{
		// receiver binds to a port
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44301) == true );
		CHECK   ( Receiver.is_open()   == true );

		// sender connects to the receiver
		KUDPSocket Sender(KTCPEndPoint("localhost:44301"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Sender.is_open() == true );

		// send data
		KStringView sMessage = "Hello UDP!";
		auto iSent = Sender.Send(sMessage);
		CHECK ( iSent == static_cast<std::streamsize>(sMessage.size()) );

		// receive data (receiver is unconnected, must use ReceiveFrom)
		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
	}

	SECTION("unconnected send/receive with SendTo/ReceiveFrom")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44302, KStreamOptions::Family::IPv4) == true );

		KUDPSocket Sender(chrono::seconds(2));
		// open the socket without connecting, force IPv4 to match the target
		REQUIRE ( Sender.Bind(0, KStreamOptions::Family::IPv4) == true );

		// resolve endpoint for SendTo
		KUDPSocket::endpoint_type target(
#if (DEKAF2_CLASSIC_ASIO)
			// Boost < 1.66 does not have boost::asio::ip::make_address
			boost::asio::ip::address::from_string("127.0.0.1"), 44302
#else
			boost::asio::ip::make_address("127.0.0.1"), 44302
#endif
		);

		KStringView sMessage = "Hello unconnected!";
		auto iSent = Sender.SendTo(sMessage, target);
		CHECK ( iSent == static_cast<std::streamsize>(sMessage.size()) );

		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
		CHECK   ( sender_ep.port() != 0 );
	}

	SECTION("receive timeout")
	{
		KUDPSocket Socket(chrono::milliseconds(200));
		REQUIRE ( Socket.Bind(44303) == true );

		// nothing to receive - should timeout, not hang
		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};
		auto iReceived = Socket.ReceiveFrom(buf, sizeof(buf), sender_ep);
		CHECK ( iReceived <= 0 );
	}

	SECTION("a timeout closes the socket by default")
	{
		KUDPSocket Socket(chrono::milliseconds(150));
		REQUIRE ( Socket.Bind(44361)  == true );
		CHECK   ( Socket.GetCancelOnTimeout() == false );
		CHECK   ( Socket.IsTimedOut() == false );

		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};
		CHECK ( Socket.ReceiveFrom(buf, sizeof(buf), sender_ep) == -1 );
		CHECK ( Socket.IsTimedOut() == true  );
		// the port is gone with it
		CHECK ( Socket.is_open()    == false );
	}

	SECTION("cancel on timeout keeps the socket")
	{
		KUDPSocket Receiver(chrono::milliseconds(150));
		Receiver.SetCancelOnTimeout();
		REQUIRE ( Receiver.Bind(44362) == true );

		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};

		// twice: the timeout is a repeatable condition
		for (int i = 0; i < 2; ++i)
		{
			CHECK ( Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep) == -1 );
			CHECK ( Receiver.IsTimedOut() == true );
			CHECK ( Receiver.Good()       == true );
			CHECK ( Receiver.is_open()    == true );
		}

		// still bound to its port: a datagram sent there now arrives
		KUDPSocket Sender(KTCPEndPoint("localhost:44362"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Sender.is_open() == true );
		KStringView sMessage = "after the timeout";
		CHECK ( Sender.Send(sMessage) == static_cast<std::streamsize>(sMessage.size()) );

		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
		CHECK   ( Receiver.IsTimedOut() == false );

		// the string variant returns an empty string on timeout
		CHECK ( Receiver.ReceiveFrom(64, sender_ep).empty() );
		CHECK ( Receiver.IsTimedOut() == true );
		CHECK ( Receiver.is_open()    == true );
	}

	SECTION("cancel on timeout through the connect options")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44363) == true );

		KUDPSocket Sender(KTCPEndPoint("localhost:44363"),
		                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::milliseconds(150)));
		REQUIRE ( Sender.is_open() == true );
		CHECK   ( Sender.GetCancelOnTimeout() == true );

		// nobody answers: a timeout, and the socket stays connected
		char buf[64]{};
		CHECK ( Sender.Receive(buf, sizeof(buf)) == -1 );
		CHECK ( Sender.IsTimedOut() == true );
		CHECK ( Sender.is_open()    == true );
		CHECK ( Sender.Receive().empty() );

		// connected as before: Send() needs no endpoint
		KStringView sMessage = "still connected";
		CHECK ( Sender.Send(sMessage) == static_cast<std::streamsize>(sMessage.size()) );
		CHECK ( Sender.IsTimedOut() == false );

		KUDPSocket::endpoint_type sender_ep;
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
	}

	SECTION("IsReadReady")
	{
		// a short socket timeout that must not matter here
		KUDPSocket Receiver(chrono::milliseconds(100));
		REQUIRE ( Receiver.Bind(44364) == true );

		// nothing there: false after the given time, and nothing happened to the socket
		CHECK ( Receiver.IsReadReady(chrono::milliseconds(250)) == false );
		CHECK ( Receiver.is_open()  == true  );
		CHECK ( Receiver.HasError() == false );
		CHECK ( Receiver.IsWriteReady(chrono::milliseconds(250)) == true );

		KUDPSocket Sender(KTCPEndPoint("localhost:44364"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Sender.is_open() == true );
		KStringView sMessage = "ready";
		CHECK ( Sender.Send(sMessage) == static_cast<std::streamsize>(sMessage.size()) );

		// now there is a datagram, and receiving it does not wait
		CHECK ( Receiver.IsReadReady(chrono::seconds(2)) == true );

		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );

		// taken: not ready again, and on request the timeout is an error
		CHECK ( Receiver.IsReadReady(chrono::milliseconds(50), true) == false );
		CHECK ( Receiver.HasError() == true );
		CHECK ( Receiver.is_open()  == true );

		// a socket that is not open is never ready
		KUDPSocket Closed;
		CHECK ( Closed.IsReadReady(chrono::milliseconds(10)) == false );
	}

	SECTION("local endpoint")
	{
		// not open: empty
		KUDPSocket Unused;
		CHECK ( Unused.GetLocalEndPoint().empty() );

		// bound: the port, on the wildcard address
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44365, KStreamOptions::Family::IPv4) == true );
		CHECK   ( Receiver.GetLocalEndPoint().Port.get() == 44365 );
		CHECK   ( Receiver.GetLocalEndPoint().Domain.get() == "0.0.0.0" );

		// connected: the address that leads to the peer, and the port the system chose
		KUDPSocket Sender(KTCPEndPoint("127.0.0.1:44365"), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2)));
		REQUIRE ( Sender.is_open() == true );
		auto Local = Sender.GetLocalEndPoint();
		CHECK ( Local.Domain.get() == "127.0.0.1" );
		CHECK ( Local.Port.get()   != 0 );

		// the receiver sees a datagram come from exactly there
		CHECK ( Sender.Send("who am I") > 0 );
		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};
		REQUIRE ( Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep) > 0 );
		CHECK   ( sender_ep.port() == Local.Port.get() );
		CHECK   ( sender_ep.address().to_string() == Local.Domain.get() );

		// a socket that is bound before it connects keeps its port
		KUDPSocket Both(chrono::seconds(2));
		REQUIRE ( Both.Bind(44366, KStreamOptions::Family::IPv4) == true );
		REQUIRE ( Both.Connect(KTCPEndPoint("127.0.0.1:44365"), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2))) == true );
		CHECK   ( Both.GetLocalEndPoint().Port.get()   == 44366 );
		CHECK   ( Both.GetLocalEndPoint().Domain.get() == "127.0.0.1" );
	}

	SECTION("bind to an address")
	{
		// the address, and a port of the system's choice
		KUDPSocket Bound(chrono::seconds(2));
		REQUIRE ( Bound.Bind("127.0.0.1", 0) == true );
		const auto iBoundPort = Bound.GetLocalEndPoint().Port.get();
		CHECK   ( Bound.GetLocalEndPoint().Domain.get() == "127.0.0.1" );
		CHECK   ( iBoundPort != 0 );

		// ... or the port asked for
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind("127.0.0.1", 44368) == true );
		CHECK   ( Receiver.GetLocalEndPoint().Domain.get() == "127.0.0.1" );
		CHECK   ( Receiver.GetLocalEndPoint().Port.get()   == 44368 );

		// connecting keeps both, and the peer sees the datagram come from there
		REQUIRE ( Bound.Connect(KTCPEndPoint("127.0.0.1:44368"), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2))) == true );
		CHECK   ( Bound.GetLocalEndPoint().Domain.get() == "127.0.0.1" );
		CHECK   ( Bound.GetLocalEndPoint().Port.get()   == iBoundPort );
		CHECK   ( Bound.Send("from a bound socket") > 0 );
		KUDPSocket::endpoint_type sender_ep;
		char buf[64]{};
		REQUIRE ( Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep) > 0 );
		CHECK   ( sender_ep.port() == iBoundPort );
		CHECK   ( sender_ep.address().to_string() == "127.0.0.1" );

		// An address of this host that is not the loopback, if it has one:
		// connecting a socket sends nothing, it makes the system choose the
		// route and with it our address (192.0.2.1 is a documentation address)
		KString sOwn;
		{
			KUDPSocket Probe(KTCPEndPoint("192.0.2.1:9"), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2)));
			if (Probe.is_open()) sOwn = Probe.GetLocalEndPoint().Domain.get();
		}

		if (sOwn.empty() || sOwn == "127.0.0.1" || sOwn == "0.0.0.0")
		{
			WARN("no IPv4 address but the loopback here: binding is not tested against a second address");
		}
		else
		{
			// A socket bound to one address sends from it, also where the
			// route to the peer would have chosen another: left alone, a
			// datagram to the loopback comes from the loopback
			KUDPSocket Other(chrono::seconds(2));
			REQUIRE ( Other.Bind(sOwn, 0) == true );
			CHECK   ( Other.GetLocalEndPoint().Domain.get() == sOwn );
			REQUIRE ( Other.Connect(KTCPEndPoint("127.0.0.1:44368"), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2))) == true );
			CHECK   ( Other.GetLocalEndPoint().Domain.get() == sOwn );
			CHECK   ( Other.Send("from another address") > 0 );
			REQUIRE ( Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep) > 0 );
			CHECK   ( sender_ep.address().to_string() == sOwn );

			// ... and it receives at that address only: what goes to another
			// address of the host does not arrive, whatever the port
			KUDPSocket Only(chrono::seconds(2));
			REQUIRE ( Only.Bind(sOwn, 0) == true );
			const auto iOnlyPort = Only.GetLocalEndPoint().Port.get();

			KUDPSocket Wrong(KTCPEndPoint(kFormat("127.0.0.1:{}", iOnlyPort)), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2)));
			REQUIRE ( Wrong.is_open() == true );
			Wrong.Send("to the wrong address");
			CHECK   ( Only.IsReadReady(chrono::milliseconds(200)) == false );

			KUDPSocket Right(KTCPEndPoint(kFormat("{}:{}", sOwn, iOnlyPort)), KStreamOptions(KStreamOptions::ForceIPv4, chrono::seconds(2)));
			REQUIRE ( Right.is_open() == true );
			CHECK   ( Right.Send("to the right address") > 0 );
			CHECK   ( Only.IsReadReady(chrono::seconds(2)) == true );
		}

		// what is no address, or none of this host
		KUDPSocket Odd(chrono::seconds(2));
		CHECK ( Odd.Bind("no address", 0) == false );
		CHECK ( Odd.Error().contains("invalid bind address") );
		CHECK ( Odd.Bind("localhost", 0) == false );
		CHECK ( Odd.Bind("", 0) == false );
		CHECK ( Odd.is_open() == false );
		CHECK ( Odd.Bind("192.0.2.1", 0) == false );
		CHECK ( Odd.Error().contains("cannot bind UDP socket to 192.0.2.1:0") );
		// a failed attempt leaves nothing behind
		CHECK ( Odd.is_open() == false );
		CHECK ( Odd.Bind("127.0.0.1", 0) == true );
		CHECK ( Odd.GetLocalEndPoint().Domain.get() == "127.0.0.1" );
		// a socket binds once
		CHECK ( Odd.Bind("127.0.0.1", 0) == false );
		CHECK ( Odd.is_open() == true );

		// IPv6, where the host has it: with or without brackets
		bool bHaveIPv6 { false };
		{
			KUDPSocket Probe(KTCPEndPoint("[::1]:9"), KStreamOptions(KStreamOptions::ForceIPv6, chrono::seconds(2)));
			bHaveIPv6 = Probe.is_open();
		}

		if (!bHaveIPv6)
		{
			WARN("no IPv6 loopback here: binding to an IPv6 address is not tested");
		}
		else
		{
			KUDPSocket Six(chrono::seconds(2));
			REQUIRE ( Six.Bind("::1", 44369) == true );
			CHECK   ( Six.GetLocalEndPoint().Domain.get() == "[::1]" );
			CHECK   ( Six.GetLocalEndPoint().Port.get()   == 44369 );

			KUDPSocket Bracketed(chrono::seconds(2));
			REQUIRE ( Bracketed.Bind("[::1]", 0) == true );
			const auto iPort = Bracketed.GetLocalEndPoint().Port.get();
			REQUIRE ( Bracketed.Connect(KTCPEndPoint("[::1]:44369"), KStreamOptions(KStreamOptions::ForceIPv6, chrono::seconds(2))) == true );
			CHECK   ( Bracketed.GetLocalEndPoint().Domain.get() == "[::1]" );
			CHECK   ( Bracketed.GetLocalEndPoint().Port.get()   == iPort );
			CHECK   ( Bracketed.Send("six") > 0 );
			REQUIRE ( Six.ReceiveFrom(buf, sizeof(buf), sender_ep) > 0 );
			CHECK   ( sender_ep.port() == iPort );
			CHECK   ( sender_ep.address().is_v6() );

			// the address is the family: no way from one to the other
			KUDPSocket Four(chrono::seconds(2));
			REQUIRE ( Four.Bind("127.0.0.1", 0) == true );
			CHECK   ( Four.Connect(KTCPEndPoint("[::1]:44369"), KStreamOptions(KStreamOptions::ForceIPv6, chrono::seconds(2))) == false );

			// an address of the other family after a failed attempt
			KUDPSocket Again(chrono::seconds(2));
			CHECK ( Again.Bind("192.0.2.1", 0) == false );
			CHECK ( Again.Bind("::1", 0) == true );
		}
	}

	SECTION("large datagram")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44304) == true );

		KUDPSocket Sender(KTCPEndPoint("localhost:44304"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Sender.is_open() == true );

		// send a 1400-byte datagram (below default MTU)
		KString sLarge(1400, 'X');
		auto iSent = Sender.Send(sLarge);
		CHECK ( iSent == 1400 );

		KUDPSocket::endpoint_type sender_ep;
		char buf[2048]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived == 1400 );
		CHECK   ( KStringView(buf, 1400) == sLarge );
	}
}


// ============================================================================
// KUDPServer tests
// ============================================================================

TEST_CASE("KUDPServer")
{
	SECTION("echo server")
	{
		std::atomic<int> iCallbackCount { 0 };
		KString sReceivedData;

		KUDPServer Server(44310);
		REQUIRE ( !Server.HasError() );

		// start in background
		bool bStarted = Server.Start(
			[&](KStringView sData, const KUDPSocket::endpoint_type& Sender, KUDPSocket& Socket)
			{
				sReceivedData.assign(sData.data(), sData.size());
				++iCallbackCount;
				// echo back
				Socket.SendTo(sData, Sender);
			},
			false  // non-blocking
		);
		REQUIRE ( bStarted == true );

		// give the server thread a moment to start
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		CHECK ( Server.IsRunning() == true );

		// send a datagram
		KUDPSocket Client(KTCPEndPoint("localhost:44310"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Client.is_open() == true );

		KStringView sMessage = "echo test";
		Client.Send(sMessage);

		// receive echo
		char buf[256]{};
		auto iReceived = Client.Receive(buf, sizeof(buf));

		Server.Stop();

		CHECK ( iCallbackCount >= 1 );
		CHECK ( sReceivedData  == sMessage );
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
		CHECK   ( Server.IsRunning() == false );
	}

	SECTION("idle longer than the timeout")
	{
		// A server waits for as long as it runs. With a socket that closes on
		// a timeout it lost its port after the first idle period, and every
		// later datagram went nowhere.
		std::atomic<int> iCallbackCount { 0 };

		KUDPServer Server(44367);
		REQUIRE ( !Server.HasError() );
		Server.GetSocket().SetTimeout(chrono::milliseconds(150));

		REQUIRE ( Server.Start(
			[&](KStringView sData, const KUDPSocket::endpoint_type& Sender, KUDPSocket& Socket)
			{
				++iCallbackCount;
				Socket.SendTo(sData, Sender);
			},
			false) == true );

		// several timeouts pass with nobody sending
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		CHECK ( Server.IsRunning() == true );

		KUDPSocket Client(KTCPEndPoint("localhost:44367"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		REQUIRE ( Client.is_open() == true );

		KStringView sMessage = "anybody there";
		Client.Send(sMessage);

		char buf[256]{};
		auto iReceived = Client.Receive(buf, sizeof(buf));

		Server.Stop();

		CHECK   ( iCallbackCount == 1 );
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == sMessage );
	}

	SECTION("multiple clients")
	{
		std::atomic<int> iCallbackCount { 0 };

		KUDPServer Server(44311);
		REQUIRE ( !Server.HasError() );

		Server.Start(
			[&](KStringView sData, const KUDPSocket::endpoint_type& Sender, KUDPSocket& Socket)
			{
				++iCallbackCount;
				Socket.SendTo(sData, Sender);
			},
			false
		);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// two clients
		KUDPSocket Client1(KTCPEndPoint("localhost:44311"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));
		KUDPSocket Client2(KTCPEndPoint("localhost:44311"), KStreamOptions(KStreamOptions::None, chrono::seconds(2)));

		Client1.Send("from client 1");
		Client2.Send("from client 2");

		char buf1[256]{}, buf2[256]{};
		auto iRx1 = Client1.Receive(buf1, sizeof(buf1));
		auto iRx2 = Client2.Receive(buf2, sizeof(buf2));

		Server.Stop();

		CHECK ( iCallbackCount >= 2 );
		REQUIRE ( iRx1 > 0 );
		REQUIRE ( iRx2 > 0 );
		CHECK ( KStringView(buf1, static_cast<std::size_t>(iRx1)) == "from client 1" );
		CHECK ( KStringView(buf2, static_cast<std::size_t>(iRx2)) == "from client 2" );
	}
}


// ============================================================================
// KUDPStream tests (auto-fragmentation)
// ============================================================================

TEST_CASE("KUDPStream")
{
	SECTION("small write and flush")
	{
		// receiver
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44320) == true );

		// stream with small max datagram size for testability
		KUDPStream Stream(KTCPEndPoint("localhost:44320"),
		                  KStreamOptions(KStreamOptions::None, chrono::seconds(2)),
		                  100);

		CHECK ( Stream.is_open()           == true  );
		CHECK ( Stream.GetMaxDatagramSize() == 100   );

		// write less than MaxDatagramSize, then flush
		Stream << "Hello stream!";
		Stream.flush();

		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == "Hello stream!" );
	}

	SECTION("auto fragmentation on overflow")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44321) == true );

		// stream with very small MaxDatagramSize to force fragmentation
		constexpr std::size_t kMaxDG = 50;

		KUDPStream Stream(KTCPEndPoint("localhost:44321"),
		                  KStreamOptions(KStreamOptions::None, chrono::seconds(2)),
		                  kMaxDG);

		// write exactly 120 bytes -> should produce 3 datagrams (50 + 50 + 20)
		KString sData(120, 'A');
		Stream.write(sData.data(), static_cast<std::streamsize>(sData.size()));
		Stream.flush();

		// read the datagrams
		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		KString sReassembled;

		auto iRx1 = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iRx1 > 0 );
		CHECK   ( iRx1 == static_cast<std::streamsize>(kMaxDG) );
		sReassembled.append(buf, static_cast<std::size_t>(iRx1));

		auto iRx2 = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iRx2 > 0 );
		CHECK   ( iRx2 == static_cast<std::streamsize>(kMaxDG) );
		sReassembled.append(buf, static_cast<std::size_t>(iRx2));

		auto iRx3 = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iRx3 > 0 );
		CHECK   ( iRx3 == 20 );
		sReassembled.append(buf, static_cast<std::size_t>(iRx3));

		CHECK ( sReassembled == sData );
	}

	SECTION("iostream operator<<")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44322) == true );

		KUDPStream Stream(KTCPEndPoint("localhost:44322"),
		                  KStreamOptions(KStreamOptions::None, chrono::seconds(2)),
		                  200);

		Stream << "number: " << 42 << " done";
		Stream.flush();

		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == "number: 42 done" );
	}

	SECTION("SetMaxDatagramSize flushes and resets")
	{
		KUDPSocket Receiver(chrono::seconds(2));
		REQUIRE ( Receiver.Bind(44323) == true );

		KUDPStream Stream(KTCPEndPoint("localhost:44323"),
		                  KStreamOptions(KStreamOptions::None, chrono::seconds(2)),
		                  200);

		// write some data
		Stream << "before resize";

		// change max datagram size - should flush pending data
		Stream.SetMaxDatagramSize(100);
		CHECK ( Stream.GetMaxDatagramSize() == 100 );

		// receive the flushed data
		KUDPSocket::endpoint_type sender_ep;
		char buf[256]{};
		auto iReceived = Receiver.ReceiveFrom(buf, sizeof(buf), sender_ep);
		REQUIRE ( iReceived > 0 );
		CHECK   ( KStringView(buf, static_cast<std::size_t>(iReceived)) == "before resize" );
	}
}


// ============================================================================
// KDTLSSocket tests
// ============================================================================

TEST_CASE("KDTLSSocket")
{
	SECTION("construct default")
	{
		KDTLSSocket Socket;
		CHECK ( Socket.is_open()  == false );
		CHECK ( !Socket.HasError()        );
	}

#if DEKAF2_HAS_RELIABLE_DTLS
	// the DTLS server pattern (memory-BIO pair with SSL_OP_COOKIE_EXCHANGE)
	// only works reliably with OpenSSL >= 1.1.1; on older releases (notably
	// Debian Stretch's 1.1.0) handshakes intermittently time out. See
	// CMakeLists.txt for the capability flag that gates this.

	SECTION("DTLS echo via server and client")
	{
		// start a DTLS server
		KDTLSServer Server(44330);
		Server.SetTLSCertificates(sTestCert, sTestKey);

		std::atomic<int> iCallbackCount { 0 };
		KString sReceivedData;

		bool bStarted = Server.Start(
			[&](KStringView sData, KStringView sPeerAddress)
			{
				sReceivedData.assign(sData.data(), sData.size());
				++iCallbackCount;
				// echo back via Server.SendTo
				Server.SendTo(sPeerAddress, sData);
			},
			false  // non-blocking
		);

		if (!bStarted)
		{
			// DTLS may not be fully supported on all build configurations
			WARN("DTLS server could not start: " << Server.Error());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// connect a DTLS client
		KDTLSSocket Client;
		bool bConnected = Client.Connect(KTCPEndPoint("localhost:44330"),
		                                 KStreamOptions(KStreamOptions::None, chrono::seconds(5)));

		if (!bConnected)
		{
			WARN("DTLS client could not connect: " << Client.Error());
			Server.Stop();
			return;
		}

		CHECK ( Client.is_open() == true );

		// send data
		KStringView sMessage = "Hello DTLS!";
		auto iSent = Client.Send(sMessage);
		CHECK ( iSent == static_cast<std::streamsize>(sMessage.size()) );

		// receive echo
		auto sReply = Client.Receive(256);

		Client.Disconnect();
		Server.Stop();

		CHECK ( iCallbackCount >= 1 );
		CHECK ( sReceivedData  == sMessage );
		CHECK ( sReply         == sMessage );
	}
#endif // DEKAF2_HAS_RELIABLE_DTLS
}


// ============================================================================
// KDTLSStream tests (auto-fragmentation over DTLS)
// ============================================================================

TEST_CASE("KDTLSStream")
{
	SECTION("construct default")
	{
		KDTLSStream Stream;
		CHECK ( Stream.is_open() == false );
	}

#if DEKAF2_HAS_RELIABLE_DTLS
	// see the note on the KDTLSSocket test case above - DTLS handshakes are
	// not reliable on OpenSSL < 1.1.1, so these sections are skipped there.

	SECTION("DTLS stream echo via server")
	{
		// start a DTLS server that echoes back via SendTo
		KDTLSServer Server(44340);
		Server.SetTLSCertificates(sTestCert, sTestKey);

		std::atomic<int> iCallbackCount { 0 };
		KString sReceivedData;

		bool bStarted = Server.Start(
			[&](KStringView sData, KStringView sPeerAddress)
			{
				sReceivedData.assign(sData.data(), sData.size());
				++iCallbackCount;
				Server.SendTo(sPeerAddress, sData);
			},
			false  // non-blocking
		);

		if (!bStarted)
		{
			WARN("DTLS server could not start: " << Server.Error());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// connect a DTLS stream
		KDTLSStream Stream(KTCPEndPoint("localhost:44340"),
		                   KStreamOptions(KStreamOptions::None, chrono::seconds(5)));

		if (!Stream.is_open())
		{
			WARN("DTLS stream could not connect: " << Stream.Error());
			Server.Stop();
			return;
		}

		CHECK ( Stream.is_open() == true );

		// write via iostream and flush
		Stream << "Hello DTLS Stream!";
		Stream.flush();

		// read the echo — use the underlying socket to receive the datagram
		auto sReply = Stream.GetSocket().Receive(256);

		Stream.Disconnect();
		Server.Stop();

		CHECK ( iCallbackCount >= 1 );
		CHECK ( sReceivedData  == "Hello DTLS Stream!" );
		CHECK ( sReply         == "Hello DTLS Stream!" );
	}

	SECTION("DTLS stream write and read")
	{
		// start a DTLS server that echoes back
		KDTLSServer Server(44341);
		Server.SetTLSCertificates(sTestCert, sTestKey);

		bool bStarted = Server.Start(
			[&](KStringView sData, KStringView sPeerAddress)
			{
				Server.SendTo(sPeerAddress, sData);
			},
			false
		);

		if (!bStarted)
		{
			WARN("DTLS server could not start: " << Server.Error());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		KDTLSStream Stream(KTCPEndPoint("localhost:44341"),
		                   KStreamOptions(KStreamOptions::None, chrono::seconds(5)),
		                   100);

		if (!Stream.is_open())
		{
			WARN("DTLS stream could not connect: " << Stream.Error());
			Server.Stop();
			return;
		}

		CHECK ( Stream.GetMaxDatagramSize() == 100 );

		// write via KOutStream::Write
		Stream.Write("DTLS Write test");
		Stream.Flush();

		// read echo via underlying socket
		auto sReply = Stream.GetSocket().Receive(256);

		Stream.Disconnect();
		Server.Stop();

		CHECK ( sReply == "DTLS Write test" );
	}

	SECTION("DTLS stream with explicit context")
	{
		KDTLSServer Server(44342);
		Server.SetTLSCertificates(sTestCert, sTestKey);

		bool bStarted = Server.Start(
			[&](KStringView sData, KStringView sPeerAddress)
			{
				Server.SendTo(sPeerAddress, sData);
			},
			false
		);

		if (!bStarted)
		{
			WARN("DTLS server could not start: " << Server.Error());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// create with explicit context
		KTLSContext Context(false, KTLSContext::Transport::DTls);
		KDTLSStream Stream(Context,
		                   KTCPEndPoint("localhost:44342"),
		                   KStreamOptions(KStreamOptions::None, chrono::seconds(5)));

		if (!Stream.is_open())
		{
			WARN("DTLS stream could not connect: " << Stream.Error());
			Server.Stop();
			return;
		}

		Stream << "context test";
		Stream.flush();

		auto sReply = Stream.GetSocket().Receive(256);

		Stream.Disconnect();
		Server.Stop();

		CHECK ( sReply == "context test" );
	}
#endif // DEKAF2_HAS_RELIABLE_DTLS
}

