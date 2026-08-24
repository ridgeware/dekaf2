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

TEST_CASE("KTunnel client relay")
{
	SECTION("a tunnel name round-trips through KTCPEndPoint")
	{
		// the client encodes the tunnel name in the Connect frame's
		// endpoint - the relay reads it back from the domain part
		KTCPEndPoint Named("sqlserver", 0);
		KTCPEndPoint Parsed(Named.Serialize());

		CHECK ( KString(Parsed.Domain.get()) == "sqlserver" );
	}

	SECTION("Connection duplex is what the relay bridges with")
	{
		// the relay pumps ClientChannel <-> NodeChannel purely through
		// ReadData()/WriteData(), so verify that pairing in isolation
		std::vector<KTunnel::Message> Sent;

		auto Channel = std::make_shared<KTunnel::Connection>(
			7, [&Sent](KTunnel::Message&& msg) { Sent.push_back(std::move(msg)); });

		// outbound: WriteData turns into a Data frame on our channel
		Channel->WriteData("select 1");

		REQUIRE ( Sent.size() == 1 );
		CHECK   ( Sent[0].GetType()    == KTunnel::Message::Data );
		CHECK   ( Sent[0].GetChannel() == 7                      );
		CHECK   ( Sent[0].GetMessage() == "select 1"             );

		// inbound: a Data frame handed in is what ReadData() returns
		Channel->SendData(KTunnel::Message(KTunnel::Message::Data, 7, "one row"));

		KString sIn;
		REQUIRE ( Channel->ReadData(sIn) );
		CHECK   ( sIn == "one row"       );

		// a Disconnect closes the channel for the reader
		Channel->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 7));
		CHECK ( !Channel->ReadData(sIn) );
	}
}
