#include "catch.hpp"

#include <dekaf2/khmac.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KHMAC") {

	SECTION("test vectors on empty string")
	{
		KHMAC_MD5 hmac_md5("", "");
		CHECK ( hmac_md5.HexDigest() == "74e6f7298a9c2d168935f58c001bad88" );
		KHMAC_SHA1 hmac_sha1("", "");
		CHECK ( hmac_sha1.HexDigest() == "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d" );
		KHMAC_SHA256 hmac_sha256("", "");
		CHECK ( hmac_sha256.HexDigest() == "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad" );
	}

	SECTION("known digests for nonempty strings")
	{
		KHMAC_MD5 hmac_md5("key", "The quick brown fox jumps over the lazy dog");
		CHECK ( hmac_md5.HexDigest() == "80070713463e7749b90c2dc24911e275" );
		KHMAC_SHA1 hmac_sha1("key", "The quick brown fox jumps over the lazy dog");
		CHECK ( hmac_sha1.HexDigest() == "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9" );
		KHMAC_SHA256 hmac_sha256("key", "The quick brown fox jumps over the lazy dog");
		CHECK ( hmac_sha256.HexDigest() == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" );
	}

	SECTION("known digests for nonempty strings, multiple updates")
	{
		KHMAC_SHA256 hmac_sha256("key", "The ");
		hmac_sha256.Update("quick brown");
		hmac_sha256.Update(" fox jumps over ");
		hmac_sha256.Update("the lazy dog");
		CHECK ( hmac_sha256.HexDigest() == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" );
	}

	SECTION("assignment")
	{
		KHMAC_MD5 hmac_md5("key");
		hmac_md5 += "The quick brown fox jumps over the lazy dog";
		CHECK ( hmac_md5.HexDigest() == "80070713463e7749b90c2dc24911e275" );
		KHMAC_SHA1 hmac_sha1("key");
		hmac_sha1 += "The quick brown fox jumps over the lazy dog";
		CHECK ( hmac_sha1.HexDigest() == "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9" );
		KHMAC_SHA256 hmac_sha256("key");
		hmac_sha256 += "The quick brown fox jumps over the lazy dog";
		CHECK ( hmac_sha256.HexDigest() == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" );
	}

	SECTION("temporary")
	{
		CHECK ( KHMAC_SHA256("key", "The quick brown fox jumps over the lazy dog").HexDigest() == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" );
	}
}
