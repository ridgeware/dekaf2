#include "catch.hpp"
#include <dekaf2/kwebsocket.h>

using namespace dekaf2;

TEST_CASE("KWebSocket")
{
	SECTION("sec-key")
	{
		auto sKey = kwebsocket::GenerateClientSecKey();

		CHECK ( sKey.size() == 24 );

		if (sKey.size() == 24)
		{
			// the last two bytes are always '='
			CHECK ( sKey[22] == '=' );
			CHECK ( sKey[23] == '=' );
		}

		CHECK ( kwebsocket::ClientSecKeyLooksValid(sKey, true) );

		// following is the sample key combo from RFC 6455
		KString sClientKey = "dGhlIHNhbXBsZSBub25jZQ==";
		auto sServerKey    = kwebsocket::GenerateServerSecKeyResponse(sClientKey, true);
		CHECK ( sServerKey == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" );
	}

	SECTION("AsioStream")
	{
		{
			auto Stream = CreateKSSLClient();
			KStream& RefStream = *Stream;
			CHECK ( kwebsocket::GetStreamType(RefStream) == kwebsocket::StreamType::TLS );
			CHECK_THROWS( kwebsocket::GetAsioTCPStream (RefStream) );
			CHECK_THROWS( kwebsocket::GetAsioUnixStream(RefStream) );
#if BOOST_VERSION >= 107400
			auto AsioStream = kwebsocket::GetAsioTLSStream(RefStream);
			CHECK_THROWS( AsioStream.shutdown() );
#endif
		}
		{
			auto Stream = CreateKTCPStream();
			KStream& RefStream = *Stream;
			CHECK ( kwebsocket::GetStreamType(RefStream) == kwebsocket::StreamType::TCP );
			CHECK_THROWS( kwebsocket::GetAsioTLSStream (RefStream) );
			CHECK_THROWS( kwebsocket::GetAsioUnixStream(RefStream) );
			auto AsioStream = kwebsocket::GetAsioTCPStream(RefStream);
			CHECK_THROWS( AsioStream.available() );
		}
		{
			auto Stream = CreateKUnixStream();
			KStream& RefStream = *Stream;
			CHECK ( kwebsocket::GetStreamType(RefStream) == kwebsocket::StreamType::UNIX );
			CHECK_THROWS( kwebsocket::GetAsioTCPStream(RefStream) );
			CHECK_THROWS( kwebsocket::GetAsioTLSStream(RefStream) );
			auto AsioStream = kwebsocket::GetAsioUnixStream(RefStream);
			CHECK_THROWS ( AsioStream.available() );
		}
	}
}
