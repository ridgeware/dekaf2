#include "catch.hpp"
#include <dekaf2/kwebsocket.h>

using namespace dekaf2;

TEST_CASE("KWebSocket")
{
	SECTION("sec-key")
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

		// following is the sample key combo from RFC 6455
		KString sClientKey = "dGhlIHNhbXBsZSBub25jZQ==";
		auto sServerKey    = KWebSocket::GenerateServerSecKeyResponse(sClientKey, true);
		CHECK ( sServerKey == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" );
	}
}
