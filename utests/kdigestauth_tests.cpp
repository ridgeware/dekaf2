#include "catch.hpp"

#include <dekaf2/crypto/auth/kdigestauth.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/http/client/khttpclient.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>

using namespace dekaf2;

TEST_CASE("KDigestAuth")
{
	SECTION("RFC 2617 example")
	{
		// the example of RFC 2617 section 3.5, with its nonce count and client nonce
		auto sHeader = kDigestAuthorization("Mufasa",
		                                    "Circle Of Life",
		                                    "testrealm@host.com",
		                                    "dcd98b7102dd2f0e8b11d0f600bfb0c093",
		                                    "5ccc069c403ebaf9f0171e9517f40e41",
		                                    "auth",
		                                    "GET",
		                                    "/dir/index.html",
		                                    1,
		                                    "0a4f113b");

		CHECK ( sHeader ==
		        "Digest username=\"Mufasa\","
		        " realm=\"testrealm@host.com\","
		        " nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
		        " uri=\"/dir/index.html\","
		        " qop=auth,"
		        " nc=00000001,"
		        " cnonce=\"0a4f113b\","
		        " response=\"6629fae49393a05397450978507c4ef1\","
		        " opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"" );
	}

	SECTION("nonce count")
	{
		auto sHeader = kDigestAuthorization("u", "p", "r", "n", "", "auth", "GET", "/", 26, "c");
		// eight lower case hex digits
		CHECK ( sHeader.contains(" nc=0000001a,") );
	}

	SECTION("without qop")
	{
		// the response of RFC 2069, as many SIP registrars ask for it:
		// MD5(MD5(user:realm:password):nonce:MD5(method:uri))
		auto sHeader = kDigestAuthorization("u", "p", "r", "n", "", "", "REGISTER", "sip:r", 7, "ignored");

		auto sExpected = KMD5(kFormat("{}:n:{}", KMD5("u:r:p").HexDigest(), KMD5("REGISTER:sip:r").HexDigest())).HexDigest();

		CHECK ( sHeader == kFormat("Digest username=\"u\", realm=\"r\", nonce=\"n\", uri=\"sip:r\", response=\"{}\"", sExpected) );
		// the same value, computed with md5(1)
		CHECK ( sExpected == "5790559ba44538767b07fb5adb649909" );
		// neither a nonce count nor a client nonce, and no opaque the server never sent
		CHECK ( sHeader.contains("nc=")     == false );
		CHECK ( sHeader.contains("cnonce")  == false );
		CHECK ( sHeader.contains("qop")     == false );
		CHECK ( sHeader.contains("opaque")  == false );
	}

	SECTION("opaque")
	{
		// returned unchanged if the server sent one
		CHECK ( kDigestAuthorization("u", "p", "r", "n", "o p a q u e", "", "GET", "/", 1, "c").ends_with(", opaque=\"o p a q u e\"") );
		CHECK ( kDigestAuthorization("u", "p", "r", "n", "xyz", "auth", "GET", "/", 1, "c").ends_with(", opaque=\"xyz\"") );
		// and left out if it did not
		CHECK ( kDigestAuthorization("u", "p", "r", "n", "", "auth", "GET", "/", 1, "c").contains("opaque") == false );
	}

	SECTION("auth-int")
	{
		// HA2 = MD5(method:uri:MD5(body))
		auto sHeader = kDigestAuthorization("u", "p", "r", "n", "", "auth-int", "POST", "/submit", 1, "c", "the body");

		auto sHA1 = KMD5("u:r:p").HexDigest();
		auto sHA2 = KMD5(kFormat("POST:/submit:{}", KMD5("the body").HexDigest())).HexDigest();
		auto sExpected = KMD5(kFormat("{}:n:00000001:c:auth-int:{}", sHA1, sHA2)).HexDigest();

		CHECK ( sHeader.contains(kFormat("response=\"{}\"", sExpected)) );
		CHECK ( sHeader.contains(" qop=auth-int,") );

		// another body, another response
		CHECK ( kDigestAuthorization("u", "p", "r", "n", "", "auth-int", "POST", "/submit", 1, "c", "another body").contains(sExpected) == false );
		// and with plain auth the body does not count
		CHECK ( kDigestAuthorization("u", "p", "r", "n", "", "auth", "POST", "/submit", 1, "c", "a") ==
		        kDigestAuthorization("u", "p", "r", "n", "", "auth", "POST", "/submit", 1, "c", "b") );
	}

	SECTION("the HTTP client hashes the URI it sends")
	{
		// A path that needs encoding: the uri parameter carries the encoded
		// path, and the server computes its hash over what it reads there.
		// The response has to be computed over the same string.
		KHTTPClient::DigestAuthenticator Auth("u", "p", "r", "n", "", "");

		KOutHTTPRequest Request;
		Request.Method   = KHTTPMethod::GET;
		Request.Resource = KURL("http://example.com/dir/a%20file.html");

		KString sHeader = Auth.GetAuthHeader(Request, "");

		CHECK ( sHeader.contains("uri=\"/dir/a%20file.html\"") );

		auto sExpected = KMD5(kFormat("{}:n:{}", KMD5("u:r:p").HexDigest(), KMD5("GET:/dir/a%20file.html").HexDigest())).HexDigest();

		CHECK ( sHeader.contains(kFormat("response=\"{}\"", sExpected)) );
	}

	SECTION("the HTTP client counts its nonce uses")
	{
		KHTTPClient::DigestAuthenticator Auth("u", "p", "r", "n", "o", "auth");

		KOutHTTPRequest Request;
		Request.Method   = KHTTPMethod::GET;
		Request.Resource = KURL("http://example.com/");

		CHECK ( KString(Auth.GetAuthHeader(Request, "")).contains(" nc=00000001,") );
		CHECK ( KString(Auth.GetAuthHeader(Request, "")).contains(" nc=00000002,") );
	}
}
