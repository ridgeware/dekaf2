#include "catch.hpp"

#include <dekaf2/net/mqtt/bits/kmqttcodec.h>
#include <dekaf2/io/streams/kinstringstream.h>
#include <dekaf2/io/streams/koutstringstream.h>

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
