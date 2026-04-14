#include "catch.hpp"
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/http/protocol/khttp_request.h>
#include <dekaf2/http/protocol/khttp_response.h>

using namespace dekaf2;

TEST_CASE("KWebSocket")
{
	SECTION("FrameTypeToString")
	{
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Text)         == "Text"         );
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Binary)       == "Binary"       );
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Ping)         == "Ping"         );
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Pong)         == "Pong"         );
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Close)        == "Close"        );
		CHECK ( KWebSocket::FrameHeader::FrameTypeToString(KWebSocket::FrameHeader::Continuation) == "Continuation" );
	}

	SECTION("StatusCodeToString")
	{
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(1000) == "Normal Closure"       );
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(1001) == "Going Away"           );
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(1002) == "Protocol Error"       );
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(1007) == "Invalid Payload Data" );
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(1011) == "Internal Server Error");
		CHECK ( KWebSocket::FrameHeader::StatusCodeToString(9999) == "unknown status"       );
	}

	SECTION("sec-key generation")
	{
		auto sKey = KWebSocket::GenerateClientSecKey();

		CHECK ( sKey.size() == 24 );

		if (sKey.size() == 24)
		{
			// the last two bytes are always '='
			CHECK ( sKey[22] == '=' );
			CHECK ( sKey[23] == '=' );
		}

		CHECK ( KWebSocket::ClientSecKeyLooksValid(sKey, true) );
	}

	SECTION("sec-key validation")
	{
		// valid key
		CHECK ( KWebSocket::ClientSecKeyLooksValid("dGhlIHNhbXBsZSBub25jZQ==", false) == true  );
		// wrong size
		CHECK ( KWebSocket::ClientSecKeyLooksValid("tooshort==", false)               == false );
		// right size but not base64 ending
		CHECK ( KWebSocket::ClientSecKeyLooksValid("dGhlIHNhbXBsZSBub25jZXY=", false) == false );
	}

	SECTION("sec-key response - RFC 6455 sample")
	{
		// following is the sample key combo from RFC 6455
		KString sClientKey = "dGhlIHNhbXBsZSBub25jZQ==";
		auto sServerKey    = KWebSocket::GenerateServerSecKeyResponse(sClientKey, true);
		CHECK ( sServerKey == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" );
	}

	SECTION("sec-key response - invalid key")
	{
		auto sServerKey = KWebSocket::GenerateServerSecKeyResponse("bad", false);
		CHECK ( sServerKey.empty() );
	}

	SECTION("Frame Text construction and round-trip")
	{
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Text, "Hello, WebSocket!");

		CHECK ( TxFrame.Type()     == KWebSocket::FrameHeader::Text );
		CHECK ( TxFrame.Finished() == true );
		CHECK ( TxFrame.size()     == 17 );
		CHECK ( TxFrame.GetPayload() == "Hello, WebSocket!" );

		// write to output stream
		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// read it back
		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Text );
		CHECK ( RxFrame.GetPayload() == "Hello, WebSocket!" );
		CHECK ( RxFrame.Finished()   == true );
	}

	SECTION("Frame Binary construction and round-trip")
	{
		KString sData;
		for (int i = 0; i < 256; ++i)
		{
			sData += static_cast<char>(i);
		}

		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);
		CHECK ( TxFrame.Type() == KWebSocket::FrameHeader::Binary );
		CHECK ( TxFrame.size() == 256 );

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Binary );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Frame empty payload round-trip")
	{
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Text, "");
		CHECK ( TxFrame.empty() );

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Text );
		CHECK ( RxFrame.GetPayload() == "" );
	}

	SECTION("Frame medium payload (126..65535 bytes) round-trip")
	{
		// 16-bit extended length
		KString sData(200, 'M');

		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// header should use 16-bit extended length: 2 + 2 = 4 byte header
		CHECK ( sWire.size() == 4 + 200 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Frame large payload (>65535 bytes) round-trip")
	{
		// 64-bit extended length
		KString sData(70000, 'L');

		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// header should use 64-bit extended length: 2 + 8 = 10 byte header
		CHECK ( sWire.size() == 10 + 70000 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Frame masking round-trip")
	{
		KString sPayload = "Masked payload test";

		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Text, sPayload);

		// write with masking (client-to-server)
		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, true) );

		// masked frame should be larger (4 extra bytes for mask key)
		// 2 (header) + 4 (mask) + 19 (payload) = 25
		CHECK ( sWire.size() == 25 );

		// read it back (will unmask automatically)
		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sPayload );
	}

	SECTION("Frame Ping round-trip")
	{
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Ping, "ping-data");

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// Frame::Read returns the Ping frame directly (auto-sends Pong)
		KInStringStream iss(sWire);
		KString sPongOutput;
		KOutStringStream oss2(sPongOutput);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );

		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Ping );
		CHECK ( RxFrame.GetPayload() == "ping-data" );

		// a Pong response should have been written to the output
		CHECK ( sPongOutput.size() > 0 );
	}

	SECTION("Frame Close round-trip")
	{
		KWebSocket::Frame TxFrame;
		TxFrame.Close(1000, "normal");

		CHECK ( TxFrame.Type() == KWebSocket::FrameHeader::Close );

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		KInStringStream iss(sWire);
		KString sCloseReply;
		KOutStringStream oss2(sCloseReply);
		KWebSocket::Frame RxFrame;
		// Read returns false on Close (connection terminated)
		CHECK ( RxFrame.Read(iss, oss2, false) == false );
		CHECK ( RxFrame.GetStatusCode() == 1000 );
		// the close reply should have been written
		CHECK ( sCloseReply.size() > 0 );
	}

	SECTION("Frame close without reason")
	{
		KWebSocket::Frame TxFrame;
		TxFrame.Close(1001);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		KInStringStream iss(sWire);
		KString sCloseReply;
		KOutStringStream oss2(sCloseReply);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) == false );
		CHECK ( RxFrame.GetStatusCode() == 1001 );
	}

	SECTION("Frame clear")
	{
		KWebSocket::Frame Frame(KWebSocket::FrameHeader::Text, "hello");
		CHECK ( Frame.size() > 0 );
		Frame.clear();
		CHECK ( Frame.size()   == 0 );
		CHECK ( Frame.empty()  == true );
		CHECK ( Frame.Type()   == KWebSocket::FrameHeader::Continuation );
	}

	SECTION("Streaming write round-trip")
	{
		// test the streaming Write(OutStream, bMaskTx, InStream, bIsBinary, len)
		KString sPayload = "streaming payload data for websocket test";

		KInStringStream PayloadStream(sPayload);
		KWebSocket::Frame TxFrame;

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false, PayloadStream, false, sPayload.size()) );

		// read it back
		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Text );
		CHECK ( RxFrame.GetPayload() == sPayload );
	}

	SECTION("Multiple text frames in sequence")
	{
		KString sWire;
		KOutStringStream oss(sWire);

		KWebSocket::Frame Frame1(KWebSocket::FrameHeader::Text, "first");
		CHECK ( Frame1.Write(oss, false) );

		KWebSocket::Frame Frame2(KWebSocket::FrameHeader::Text, "second");
		CHECK ( Frame2.Write(oss, false) );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);

		KWebSocket::Frame RxFrame1;
		CHECK ( RxFrame1.Read(iss, oss2, false) );
		CHECK ( RxFrame1.GetPayload() == "first" );

		KWebSocket::Frame RxFrame2;
		CHECK ( RxFrame2.Read(iss, oss2, false) );
		CHECK ( RxFrame2.GetPayload() == "second" );
	}

	SECTION("CheckForUpgradeRequest - valid")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::GET;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::UPGRADE, "websocket");
		Request.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "13");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_KEY, "dGhlIHNhbXBsZSBub25jZQ==");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == true );
	}

	SECTION("CheckForUpgradeRequest - no upgrade header")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::GET;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "13");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == false );
	}

	SECTION("CheckForUpgradeRequest - wrong method")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::POST;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::UPGRADE, "websocket");
		Request.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "13");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == false );
	}

	SECTION("CheckForUpgradeRequest - wrong version")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::GET;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::UPGRADE, "websocket");
		Request.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "12");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == false );
	}

	SECTION("CheckForUpgradeRequest - missing Connection: Upgrade")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::GET;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::UPGRADE, "websocket");
		Request.Headers.Set(KHTTPHeader::CONNECTION, "keep-alive");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "13");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == false );
	}

	SECTION("CheckForUpgradeRequest - wrong HTTP version")
	{
		KInHTTPRequest Request;
		Request.Method = KHTTPMethod::GET;
		Request.SetHTTPVersion(KHTTPVersion::http2);
		Request.Headers.Set(KHTTPHeader::UPGRADE, "websocket");
		Request.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
		Request.Headers.Set(KHTTPHeader::SEC_WEBSOCKET_VERSION, "13");

		CHECK ( KWebSocket::CheckForUpgradeRequest(Request, false) == false );
	}

	SECTION("Payload exactly 125 bytes - small header")
	{
		KString sData(125, 'S');
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Text, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// 2-byte header + 125 payload
		CHECK ( sWire.size() == 2 + 125 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Payload exactly 126 bytes - 16-bit length header")
	{
		KString sData(126, 'M');
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Text, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// 2 + 2 (16-bit ext length) + 126 payload = 130
		CHECK ( sWire.size() == 4 + 126 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Payload exactly 65535 bytes - max 16-bit length")
	{
		KString sData(65535, 'X');
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// 2 + 2 (16-bit ext length) + 65535 payload
		CHECK ( sWire.size() == 4 + 65535 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Payload exactly 65536 bytes - 64-bit length header")
	{
		KString sData(65536, 'Y');
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// 2 + 8 (64-bit ext length) + 65536 payload
		CHECK ( sWire.size() == 10 + 65536 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Masked medium payload round-trip")
	{
		KString sData(300, 'Z');

		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Binary, sData);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, true) );

		// 2 + 2 (16-bit ext) + 4 (mask) + 300 = 308
		CHECK ( sWire.size() == 308 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.GetPayload() == sData );
	}

	SECTION("Pong frame round-trip")
	{
		KWebSocket::Frame TxFrame(KWebSocket::FrameHeader::Pong, "pong-response");

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );

		// Frame::Read returns the Pong frame directly
		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		KWebSocket::Frame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );
		CHECK ( RxFrame.Type()       == KWebSocket::FrameHeader::Pong );
		CHECK ( RxFrame.GetPayload() == "pong-response" );
	}
}

// A Frame subclass with a 4-byte preamble, like KTunnel::Message
class PreambleFrame : public KWebSocket::Frame
{
public:
	PreambleFrame() = default;
	PreambleFrame(uint8_t type, uint32_t channel, KString sPayload)
	: Frame(FrameType::Binary, std::move(sPayload), true)
	{
		m_Preamble[0] = static_cast<char>(type);
		m_Preamble[1] = static_cast<char>((channel >> 16) & 0xff);
		m_Preamble[2] = static_cast<char>((channel >>  8) & 0xff);
		m_Preamble[3] = static_cast<char>( channel        & 0xff);
	}

	std::size_t GetPreambleSize() const override { return m_Preamble.size(); }
	char*       GetPreambleBuf()  const override { return m_Preamble.data(); }

	uint8_t  GetType()    const { return static_cast<uint8_t>(m_Preamble[0]); }
	uint32_t GetChannel() const
	{
		return (static_cast<uint32_t>(static_cast<uint8_t>(m_Preamble[1])) << 16) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(m_Preamble[2])) <<  8) |
		        static_cast<uint32_t>(static_cast<uint8_t>(m_Preamble[3]));
	}

private:
	mutable std::array<char, 4> m_Preamble{};
};

TEST_CASE("KWebSocket Preamble")
{
	SECTION("Preamble round-trip without masking")
	{
		KString sPayload = "Basic dXNlcjpzZWNyZXQxMjM=";
		CHECK ( sPayload.size() == 26 );

		PreambleFrame TxFrame(1, 0, sPayload);
		CHECK ( TxFrame.GetType()    == 1 );
		CHECK ( TxFrame.GetChannel() == 0 );
		CHECK ( TxFrame.size()       == 26 );

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, false) );
		// 2 (header) + 4 (preamble) + 26 (payload) = 32
		CHECK ( sWire.size() == 32 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		PreambleFrame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );

		CHECK ( RxFrame.GetType()    == 1 );
		CHECK ( RxFrame.GetChannel() == 0 );
		INFO  ( "payload size: " << RxFrame.size() << " expected: 26" );
		CHECK ( RxFrame.size()       == 26 );
		CHECK ( RxFrame.GetPayload() == sPayload );
	}

	SECTION("Preamble round-trip with masking (client to server)")
	{
		KString sPayload = "Basic dXNlcjpzZWNyZXQxMjM=";
		CHECK ( sPayload.size() == 26 );

		PreambleFrame TxFrame(1, 0, sPayload);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, true) );
		// 2 (header) + 4 (mask key) + 4 (preamble) + 26 (payload) = 36
		CHECK ( sWire.size() == 36 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		PreambleFrame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );

		CHECK ( RxFrame.GetType()    == 1 );
		CHECK ( RxFrame.GetChannel() == 0 );
		INFO  ( "payload size: " << RxFrame.size() << " expected: 26" );
		CHECK ( RxFrame.size()       == 26 );
		CHECK ( RxFrame.GetPayload() == sPayload );
	}

	SECTION("Preamble round-trip with non-zero channel and masking")
	{
		PreambleFrame TxFrame(7, 42, "hello tunnel");

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, true) );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		PreambleFrame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );

		CHECK ( RxFrame.GetType()    == 7 );
		CHECK ( RxFrame.GetChannel() == 42 );
		CHECK ( RxFrame.GetPayload() == "hello tunnel" );
	}

	SECTION("Preamble round-trip with medium payload and masking")
	{
		KString sPayload(200, 'X');
		PreambleFrame TxFrame(2, 1000, sPayload);

		KString sWire;
		KOutStringStream oss(sWire);
		CHECK ( TxFrame.Write(oss, true) );
		// 2 + 2 (16-bit ext) + 4 (mask) + 4 (preamble) + 200 (payload) = 212
		CHECK ( sWire.size() == 212 );

		KInStringStream iss(sWire);
		KString sOutBuf;
		KOutStringStream oss2(sOutBuf);
		PreambleFrame RxFrame;
		CHECK ( RxFrame.Read(iss, oss2, false) );

		CHECK ( RxFrame.GetType()    == 2 );
		CHECK ( RxFrame.GetChannel() == 1000 );
		CHECK ( RxFrame.size()       == 200 );
		CHECK ( RxFrame.GetPayload() == sPayload );
	}
}
