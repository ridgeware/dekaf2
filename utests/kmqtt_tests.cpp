#include "catch.hpp"

#include <dekaf2/net/mqtt/bits/kmqttcodec.h>
#include <dekaf2/net/mqtt/kmqttclient.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/io/streams/kinstringstream.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>

#include <atomic>

using namespace dekaf2;

TEST_CASE("KMQTT") {

	SECTION("VarInt encoding")
	{
		auto VarInt = [](std::size_t iValue) -> KString
		{
			KString sBuffer;
			kmqtt::AppendVarInt(sBuffer, iValue);
			return sBuffer;
		};

		// the boundary values of the one to four byte encodings, and the
		// specification's own worked example (321)
		CHECK ( VarInt(0)         == KStringView("\x00", 1)             );
		CHECK ( VarInt(127)       == "\x7F"                             );
		CHECK ( VarInt(128)       == KStringView("\x80\x01", 2)         );
		CHECK ( VarInt(321)       == KStringView("\xC1\x02", 2)         );
		CHECK ( VarInt(16383)     == "\xFF\x7F"                         );
		CHECK ( VarInt(16384)     == KStringView("\x80\x80\x01", 3)     );
		CHECK ( VarInt(2097151)   == "\xFF\xFF\x7F"                     );
		CHECK ( VarInt(2097152)   == KStringView("\x80\x80\x80\x01", 4) );
		CHECK ( VarInt(268435455) == "\xFF\xFF\xFF\x7F"                 );
	}

	SECTION("String encoding and decoding")
	{
		KString sBuffer;
		kmqtt::AppendString(sBuffer, "abc");
		CHECK ( sBuffer == KStringView("\x00\x03" "abc", 5) );

		std::size_t iPos { 0 };
		KStringView sValue;

		CHECK ( kmqtt::ReadString(sBuffer, iPos, sValue) == true );
		CHECK ( sValue == "abc" );
		CHECK ( iPos   == 5     );

		// an empty string is valid
		iPos = 0;
		CHECK ( kmqtt::ReadString(KStringView("\x00\x00", 2), iPos, sValue) == true );
		CHECK ( sValue.empty() );

		// truncated length header
		iPos = 0;
		CHECK ( kmqtt::ReadString(KStringView("\x00", 1), iPos, sValue) == false );

		// announced length reaches beyond the buffer
		iPos = 0;
		CHECK ( kmqtt::ReadString(KStringView("\x00\x04" "abc", 5), iPos, sValue) == false );
	}

	SECTION("packet write and read roundtrip")
	{
		KString sBody;
		kmqtt::AppendString(sBody, "topic/a");
		sBody += "payload";

		KString sWire;
		{
			KOutStringStream oss(sWire);
			CHECK ( kmqtt::WritePacket(oss, kmqtt::Publish << 4, sBody) == true );
		}

		// first byte, a one byte remaining length, then the body
		CHECK ( sWire.size() == 2 + sBody.size() );
		CHECK ( static_cast<uint8_t>(sWire[0]) == (kmqtt::Publish << 4) );
		CHECK ( static_cast<uint8_t>(sWire[1]) == sBody.size() );

		KInStringStream iss(sWire);

		uint8_t iFirstByte { 0 };
		KString sReadBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sReadBody, 1024) == true );
		CHECK ( iFirstByte == (kmqtt::Publish << 4) );
		CHECK ( sReadBody  == sBody );
	}

	SECTION("packet with empty body")
	{
		KString sWire;
		{
			KOutStringStream oss(sWire);
			CHECK ( kmqtt::WritePacket(oss, kmqtt::PingResp << 4, "") == true );
		}

		CHECK ( sWire == KStringView("\xD0\x00", 2) );

		KInStringStream iss(sWire);

		uint8_t iFirstByte { 0 };
		KString sBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sBody, 1024) == true );
		CHECK ( iFirstByte == (kmqtt::PingResp << 4) );
		CHECK ( sBody.empty() );
	}

	SECTION("packet with a multi byte length")
	{
		KString sBody;
		sBody.assign(200, 'x');

		KString sWire;
		{
			KOutStringStream oss(sWire);
			CHECK ( kmqtt::WritePacket(oss, kmqtt::Publish << 4, sBody) == true );
		}

		CHECK ( sWire.size() == 3 + sBody.size() );

		KInStringStream iss(sWire);

		uint8_t iFirstByte { 0 };
		KString sReadBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sReadBody, 1024) == true );
		CHECK ( sReadBody == sBody );
	}

	SECTION("a fifth length byte is refused")
	{
		KInStringStream iss(KStringView("\xD0\x80\x80\x80\x80\x01", 6));

		uint8_t iFirstByte { 0 };
		KString sBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sBody, 1024) == false );
	}

	SECTION("an oversized body is refused unread")
	{
		KString sWire;
		sWire += '\xD0';
		kmqtt::AppendVarInt(sWire, 200);

		KInStringStream iss(sWire);

		uint8_t iFirstByte { 0 };
		KString sBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sBody, 100) == false );
	}

	SECTION("a truncated body is an error")
	{
		KString sWire;
		{
			KOutStringStream oss(sWire);
			CHECK ( kmqtt::WritePacket(oss, kmqtt::Publish << 4, "0123456789") == true );
		}

		sWire.resize(sWire.size() - 4);

		KInStringStream iss(sWire);

		uint8_t iFirstByte { 0 };
		KString sBody;

		CHECK ( kmqtt::ReadPacket(iss, iFirstByte, sBody, 1024) == false );
	}
}

namespace {

/// a broker with just enough MQTT for the client tests: it acks, answers
/// pings, records what arrives, and pushes one message per subscription
class KMiniBroker : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;

	KThreadSafe<std::vector<KString>>  m_Subscribed;
	KThreadSafe<std::vector<KString>>  m_Unsubscribed;
	KThreadSafe<std::vector<std::pair<KString, KString>>> m_Published;
	std::atomic<int>                   m_iPings    { 0 };
	std::atomic<int>                   m_iConnects { 0 };

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		++m_iConnects;

		uint8_t iFirstByte { 0 };
		KString sBody;

		while (kmqtt::ReadPacket(*Stream, iFirstByte, sBody, 1024 * 1024))
		{
			switch (iFirstByte >> 4)
			{
				case kmqtt::Connect:
					kmqtt::WritePacket(*Stream, kmqtt::ConnAck << 4, KStringView("\x00\x00", 2));
					break;

				case kmqtt::Subscribe:
				{
					std::size_t iPos { 2 };
					KStringView sTopic;

					if (!kmqtt::ReadString(sBody, iPos, sTopic)) return;

					m_Subscribed.unique()->push_back(sTopic);

					KString sAck;
					sAck += sBody[0];
					sAck += sBody[1];
					sAck += static_cast<char>(0);
					kmqtt::WritePacket(*Stream, kmqtt::SubAck << 4, sAck);

					// push one message so the client's dispatch can be checked
					KString sMsg;
					kmqtt::AppendString(sMsg, sTopic);
					sMsg += "hello";
					kmqtt::WritePacket(*Stream, kmqtt::Publish << 4, sMsg);
					break;
				}

				case kmqtt::Unsubscribe:
				{
					std::size_t iPos { 2 };
					KStringView sTopic;

					if (!kmqtt::ReadString(sBody, iPos, sTopic)) return;

					m_Unsubscribed.unique()->push_back(sTopic);

					KString sAck;
					sAck += sBody[0];
					sAck += sBody[1];
					kmqtt::WritePacket(*Stream, kmqtt::UnsubAck << 4, sAck);
					break;
				}

				case kmqtt::Publish:
				{
					std::size_t iPos { 0 };
					KStringView sTopic;

					if (!kmqtt::ReadString(sBody, iPos, sTopic)) return;

					KStringView sPayload(sBody.data() + iPos, sBody.size() - iPos);
					m_Published.unique()->push_back({ KString(sTopic), KString(sPayload) });
					break;
				}

				case kmqtt::PingReq:
					++m_iPings;
					kmqtt::WritePacket(*Stream, kmqtt::PingResp << 4, "");
					break;

				case kmqtt::Disconnect:
					return;

				default:
					break;
			}
		}
	}

}; // KMiniBroker

} // end of anonymous namespace

TEST_CASE("KMQTTClient")
{
	SECTION("single connection against an in-process broker")
	{
		auto WaitFor = [](std::function<bool()> Condition, KDuration Timeout = chrono::seconds(3)) -> bool
		{
			KStopTime Since;
			while (!Condition() && Since.elapsed() < Timeout) kSleep(chrono::milliseconds(10));
			return Condition();
		};

		KMiniBroker Broker(7612, false, 2);
		Broker.Start(chrono::seconds(10), false);

		KMQTTClient Client("mqtt://127.0.0.1:7612", "utest-client");
		// short, so that the watchdog ping shows up within the test run
		Client.SetSilenceTimeout(chrono::milliseconds(300));

		KThreadSafe<std::vector<std::pair<KString, KString>>> Received;

		Client.SetMessageCallback([&](KStringView sTopic, KStringView sPayload)
		{
			Received.unique()->push_back({ KString(sTopic), KString(sPayload) });
		});

		Client.Subscribe("test/one");
		Client.Start();

		CHECK ( WaitFor([&]{ return Client.IsConnected(); }) == true );

		// the replayed subscription triggers the broker's push message
		CHECK ( WaitFor([&]{ return !Received.shared()->empty(); }) == true );
		CHECK ( Received.shared()->front().first  == "test/one" );
		CHECK ( Received.shared()->front().second == "hello"    );

		// a live subscription takes effect within the poll interval
		Client.Subscribe("test/two");
		CHECK ( WaitFor([&]{ return Received.shared()->size() >= 2; }) == true );

		// an asynchronous publish reaches the broker
		CHECK ( Client.Publish("out/data", "payload-1") == true );
		CHECK ( WaitFor([&]{ return !Broker.m_Published.shared()->empty(); }) == true );
		CHECK ( Broker.m_Published.shared()->front().first  == "out/data"  );
		CHECK ( Broker.m_Published.shared()->front().second == "payload-1" );

		// a live unsubscription goes out on the wire
		CHECK ( Client.Unsubscribe("test/one") == true );
		CHECK ( WaitFor([&]{ return !Broker.m_Unsubscribed.shared()->empty(); }) == true );
		CHECK ( Broker.m_Unsubscribed.shared()->front() == "test/one" );

		// the silence watchdog probes with PINGREQ, and because the broker
		// answers, the connection is never re-established
		CHECK ( WaitFor([&]{ return Broker.m_iPings.load() >= 2; }, chrono::seconds(5)) == true );
		CHECK ( Broker.m_iConnects.load() == 1    );
		CHECK ( Client.IsConnected()      == true );

		Client.Stop();
	}
}
