#include "catch.hpp"

#include <dekaf2/kmessagedigest.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KMessageDigest") {

	SECTION("test vectors on empty string")
	{
		KSHA224 sha224;
		CHECK ( sha224.HexDigest() == "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" );
		KSHA256 sha256;
		CHECK ( sha256.HexDigest() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );
		KSHA384 sha384;
		CHECK ( sha384.HexDigest() == "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" );
		KSHA512 sha512;
		CHECK ( sha512.HexDigest() == "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" );
#ifdef DEKAF2_HAS_BLAKE2
		KBLAKE2B blake2;
		CHECK ( blake2.HexDigest() == "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");
#endif
	}

	SECTION("known digests for nonempty strings")
	{
		KSHA224 sha224;
		sha224.Update("The quick brown fox jumps over the lazy dog");
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
		sha224.clear();
		sha224.Update("The quick brown fox jumps over the lazy dog.");
		CHECK ( sha224.HexDigest() == "619cba8e8e05826e9b8c519c0a5c68f4fb653e8a3d8aa04bb2c8cd4c" );
#ifdef DEKAF2_HAS_BLAKE2
		KBLAKE2B blake2;
		blake2.Update("The quick brown fox jumps over the lazy dog");
		CHECK ( blake2.HexDigest() == "a8add4bdddfd93e4877d2746e62817b116364a1fa7bc148d95090bc7333b3673f82401cf7aa2e4cb1ecd90296e3f14cb5413f8ed77be73045b13914cdcd6a918");
#endif
	}

	SECTION("known digests for nonempty strings, multiple updates")
	{
		KSHA224 sha224("The ");
		sha224.Update("quick brown");
		sha224.Update(" fox jumps over ");
		sha224.Update("the lazy dog");
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
	}

	SECTION("nonempty constructor")
	{
		KSHA224 sha224("The quick brown fox jumps over the lazy dog");
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
	}

	SECTION("assignment")
	{
		KSHA224 sha224("test");
		sha224 = "The quick brown fox jumps over the lazy dog"_ksz;
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
		sha224 = "The quick brown fox jumps over the lazy dog"_ksv;
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
		sha224 = "The quick brown fox jumps over the lazy dog"_ks;
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
		sha224 = "The quick brown fox jumps over the lazy dog";
		CHECK ( sha224.HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
	}

	SECTION("temporary")
	{
		CHECK ( KSHA224("The quick brown fox jumps over the lazy dog").HexDigest() == "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525" );
	}
}
