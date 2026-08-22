#include "catch.hpp"

#include <dekaf2/net/util/ktunnel.h>
#include <thread>

using namespace dekaf2;

TEST_CASE("KTunnel")
{
	SECTION("Connection disconnect reason")
	{
		auto Connection = std::make_shared<KTunnel::Connection>(
			1, [](KTunnel::Message&&){});

		CHECK ( Connection->GetDisconnectReason().empty() );

		// a Disconnect frame without payload leaves the reason empty
		Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 1));
		CHECK ( Connection->GetDisconnectReason().empty() );

		// a Disconnect frame with payload carries the peer's error
		Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 1,
		                                      "cannot connect to target host:1: refused"));
		CHECK ( Connection->GetDisconnectReason() == "cannot connect to target host:1: refused" );
	}

	SECTION("Connection wait for late disconnect reason")
	{
		auto Connection = std::make_shared<KTunnel::Connection>(
			2, [](KTunnel::Message&&){});

		// the reason arrives while another thread already waits for it -
		// the exact situation after a downstream that closed first
		std::thread Sender([Connection]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 2, "late error"));
		});

		auto sReason = Connection->WaitForDisconnectReason(chrono::seconds(2));
		CHECK ( sReason == "late error" );

		Sender.join();
	}

	SECTION("Connection wait times out without reason")
	{
		KTunnel::Connection Connection(3, [](KTunnel::Message&&){});

		auto sReason = Connection.WaitForDisconnectReason(chrono::milliseconds(20));
		CHECK ( sReason.empty() );
	}

	SECTION("Connection target and peer diagnostics")
	{
		KTunnel::Connection Connection(4, [](KTunnel::Message&&){});

		CHECK ( Connection.GetTarget().empty() );
		CHECK ( Connection.GetPeer().empty()   );

		Connection.SetTarget(KTCPEndPoint("db.example.com", 3306));
		Connection.SetPeer("10.0.0.1:54321");

		CHECK ( Connection.GetTarget().Serialize() == "db.example.com:3306" );
		CHECK ( Connection.GetPeer()               == "10.0.0.1:54321"      );
		CHECK ( Connection.GetStartTime() > KUnixTime() );
		CHECK ( Connection.GetBytesToDirect()   == 0 );
		CHECK ( Connection.GetBytesFromDirect() == 0 );
	}
}
