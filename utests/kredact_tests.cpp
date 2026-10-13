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

	SECTION("the schemes of an Authorization header")
	{
		// a known scheme is kept, the credentials after it are redacted
		CHECK ( kRedactCredentials("Authorization: DeepL-Auth-Key 00000000-1111-2222-3333-444444444444:fx")
		        == "Authorization: DeepL-Auth-Key [redacted]" );
		CHECK ( kRedactCredentials("Authorization: Bearer abc123 sent"  ) == "Authorization: Bearer [redacted] sent" );
		CHECK ( kRedactCredentials("Authorization: Basic  dXNlcjpwYXNz" ) == "Authorization: Basic  [redacted]"      );
		CHECK ( kRedactCredentials("Authorization:\tBasic\tdXNlcjpwYXNz") == "Authorization:\tBasic\t[redacted]"     );

		// a scheme that ends with "key" or "token" is kept as well
		CHECK ( kRedactCredentials("Authorization: ApiKey abc123 sent"  ) == "Authorization: ApiKey [redacted] sent" );
		CHECK ( kRedactCredentials("authorization: token abc123"        ) == "authorization: token [redacted]"       );
		CHECK ( kRedactCredentials("Authorization: Api-Token abc123"    ) == "Authorization: Api-Token [redacted]"   );

		// but not a word with a digit, nor one without a word after it
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0key sent"    ) == "Authorization: [redacted]"             );
		CHECK ( kRedactCredentials("Authorization: secretkey"           ) == "Authorization: [redacted]"             );
		CHECK ( kRedactCredentials("Authorization: secretkey\nnext line") == "Authorization: [redacted]\nnext line"  );

		// an unknown scheme is redacted together with its credentials
		CHECK ( kRedactCredentials("Authorization: CustomScheme c2VjcmV0 sent") == "Authorization: [redacted] sent"  );

		// so is a credential without a scheme, also when other text follows
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0"            ) == "Authorization: [redacted]"             );
		CHECK ( kRedactCredentials("Authorization: c2VjcmV0 from client") == "Authorization: [redacted] client"      );
		CHECK ( kRedactCredentials("'Authorization: c2VjcmV0' sent"     ) == "'Authorization: [redacted]' sent"      );
	}
}
