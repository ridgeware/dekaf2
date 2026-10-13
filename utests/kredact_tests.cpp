#include "catch.hpp"

#include <dekaf2/core/logging/kredact.h>

using namespace dekaf2;

TEST_CASE("kRedactCredentials")
{
	SECTION("a text without credentials stays untouched")
	{
		KString sBuffer = "unchanged";
		CHECK ( kRedactCredentials("pass=3 fail=0 key=value", sBuffer) == false );
		CHECK ( sBuffer == "unchanged" );
		CHECK ( kRedactCredentials("pass=3 fail=0 key=value") == "pass=3 fail=0 key=value" );
	}

	SECTION("the shapes of credentials")
	{
		KString sBuffer;
		CHECK ( kRedactCredentials("Authorization: Bearer abc.def", sBuffer) );
		CHECK ( sBuffer == "Authorization: Bearer [redacted]" );

		CHECK ( kRedactCredentials("https://user:secret@example.com/path") == "https://user:[redacted]@example.com/path" );
		CHECK ( kRedactCredentials("login with password=hunter2 done")     == "login with password=[redacted] done" );
		CHECK ( kRedactCredentials("GET /api?api_key=abc123&x=1")          == "GET /api?api_key=[redacted]&x=1" );
		CHECK ( kRedactCredentials("token eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.c2lnbmF0dXJl end") == "token [redacted] end" );
	}
}
