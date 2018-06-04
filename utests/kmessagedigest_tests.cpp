#include "catch.hpp"

#include <dekaf2/kmessagedigest.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KMessageDigest") {

	SECTION("test vectors on empty string")
	{
		KSHA224 sha224;
		CHECK ( sha224 == "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" );
		KSHA256 sha256;
		CHECK ( sha256 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );
		KSHA384 sha384;
		CHECK ( sha384 == "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" );
		KSHA512 sha512;
		CHECK ( sha512 == "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" );
	}

	SECTION("known digests for nonempty strings")
	{
		KSHA224 sha224;
		sha224.Update("The quick brown fox jumps over the lazy dog");
		CHECK ( sha224 == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
		sha224.clear();
		sha224.Update("The quick brown fox jumps over the lazy dog.");
		CHECK ( sha224 == "619cba8e8e05826e9b8c519c0a5c68f4fb653e8a3d8aa04bb2c8cd4c" );
	}


	SECTION("nonempty constructor")
	{
		KSHA224 sha224("The quick brown fox jumps over the lazy dog");
		CHECK ( sha224 == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
	}
}
