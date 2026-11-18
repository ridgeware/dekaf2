#include "catch.hpp"

#include <dekaf2/crypto/ec/kx25519.h>

#if DEKAF2_HAS_X25519

#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KX25519")
{
	SECTION("default ctor is empty")
	{
		KX25519 Key;
		CHECK ( Key.empty() );
		CHECK_FALSE ( Key.IsPrivateKey() );
	}

	SECTION("a PEM string as a pointer to char and as a string literal")
	{
		KX25519 Original(true);
		REQUIRE_FALSE ( Original.empty() );

		auto sPEM = Original.GetPEM(false);

		// this is the PEM string, not the request to generate a key
		KX25519 FromPointer(sPEM.c_str());

		CHECK_FALSE ( FromPointer.empty()        );
		CHECK_FALSE ( FromPointer.IsPrivateKey() );
		CHECK       ( FromPointer.GetPublicKeyRaw() == Original.GetPublicKeyRaw() );

		// and a string literal that is no PEM gives no key
		KX25519 FromLiteral("this is not a key");

		CHECK ( FromLiteral.empty() );
	}

	SECTION("Generate creates a valid key pair")
	{
		KX25519 Key(true);
		CHECK_FALSE ( Key.empty() );
		CHECK       ( Key.IsPrivateKey() );
		CHECK_FALSE ( Key.HasError() );
	}

	SECTION("GetPrivateKeyRaw returns 32 bytes")
	{
		KX25519 Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPriv = Key.GetPrivateKeyRaw();
		CHECK ( sPriv.size() == 32 );
	}

	SECTION("GetPublicKeyRaw returns 32 bytes")
	{
		KX25519 Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPub = Key.GetPublicKeyRaw();
		CHECK ( sPub.size() == 32 );
	}

	SECTION("PEM round-trip (private key)")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPEM = Key1.GetPEM(true);
		CHECK_FALSE ( sPEM.empty() );
		CHECK ( sPEM.starts_with("-----BEGIN") );

		KX25519 Key2(sPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPrivateKeyRaw() == Key1.GetPrivateKeyRaw() );
		CHECK ( Key2.GetPublicKeyRaw()  == Key1.GetPublicKeyRaw() );
	}

	SECTION("PEM round-trip (public key)")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPubPEM = Key1.GetPEM(false);
		CHECK_FALSE ( sPubPEM.empty() );

		KX25519 Key2(sPubPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == Key1.GetPublicKeyRaw() );
	}

	SECTION("raw key round-trip (private)")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPrivRaw = Key1.GetPrivateKeyRaw();
		auto sPubRaw  = Key1.GetPublicKeyRaw();

		KX25519 Key2;
		CHECK ( Key2.CreateFromRaw(sPrivRaw, true) );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("raw key round-trip (public)")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPubRaw = Key1.GetPublicKeyRaw();

		KX25519 Key2;
		CHECK ( Key2.CreateFromRaw(sPubRaw, false) );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("clear resets the key")
	{
		KX25519 Key(true);
		REQUIRE_FALSE ( Key.empty() );

		Key.clear();
		CHECK ( Key.empty() );
	}

	SECTION("move construction")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KX25519 Key2(std::move(Key1));
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("move assignment")
	{
		KX25519 Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KX25519 Key2;
		Key2 = std::move(Key1);
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("X25519 DH: both sides agree")
	{
		KX25519 Alice(true);
		KX25519 Bob(true);
		REQUIRE_FALSE ( Alice.empty() );
		REQUIRE_FALSE ( Bob.empty() );

		KString sSecretA = Alice.DeriveSharedSecret(Bob.GetPublicKeyRaw());
		KString sSecretB = Bob.DeriveSharedSecret(Alice.GetPublicKeyRaw());

		REQUIRE ( sSecretA.size() == 32 );
		REQUIRE ( sSecretB.size() == 32 );
		CHECK   ( sSecretA == sSecretB );
	}

	SECTION("X25519 DH with wrong key size fails")
	{
		KX25519 Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sSecret = Key.DeriveSharedSecret("too_short");
		CHECK ( sSecret.empty() );
	}

	SECTION("CreateFromPEM with invalid PEM fails")
	{
		KX25519 Key;
		CHECK_FALSE ( Key.CreateFromPEM("not a PEM") );
		CHECK       ( Key.empty() );
	}

	SECTION("CreateFromRaw with wrong size fails")
	{
		KX25519 Key;
		CHECK_FALSE ( Key.CreateFromRaw("wrong_size", true) );
		CHECK       ( Key.empty() );
	}

	SECTION("two generated keys are different")
	{
		KX25519 Key1(true);
		KX25519 Key2(true);
		CHECK ( Key1.GetPublicKeyRaw() != Key2.GetPublicKeyRaw() );
	}

	SECTION("known-answer test (RFC 7748 Section 6.1)")
	{
		// Alice's private key (scalar)
		KString sAlicePriv = kUnHex(
			"77076d0a7318a57d3c16c17251b26645"
			"df4c2f87ebc0992ab177fba51db92c2a");

		// Bob's private key
		KString sBobPriv = kUnHex(
			"5dab087e624a8a4b79e17f8b83800ee6"
			"6f3bb1292618b6fd1c2f8b27ff88e0eb");

		// expected public keys
		KString sAlicePubExpected = kUnHex(
			"8520f0098930a754748b7ddcb43ef75a"
			"0dbf3a0d26381af4eba4a98eaa9b4e6a");

		KString sBobPubExpected = kUnHex(
			"de9edb7d7b7dc1b4d35b61c2ece43537"
			"3f8343c85b78674dadfc7e146f882b4f");

		// expected shared secret
		KString sSharedExpected = kUnHex(
			"4a5d9d5ba4ce2de1728e3bf480350f25"
			"e07e21c947d19e3376f09b3c1e161742");

		KX25519 Alice;
		REQUIRE ( Alice.CreateFromRaw(sAlicePriv, true) );
		CHECK ( Alice.GetPublicKeyRaw() == sAlicePubExpected );

		KX25519 Bob;
		REQUIRE ( Bob.CreateFromRaw(sBobPriv, true) );
		CHECK ( Bob.GetPublicKeyRaw() == sBobPubExpected );

		KString sSecretA = Alice.DeriveSharedSecret(Bob.GetPublicKeyRaw());
		KString sSecretB = Bob.DeriveSharedSecret(Alice.GetPublicKeyRaw());

		CHECK ( sSecretA == sSharedExpected );
		CHECK ( sSecretB == sSharedExpected );
	}

	SECTION("X25519 DH with a peer key of small order is refused")
	{
		// the points of small order on Curve25519 and its twist: with any of
		// them as the peer's key the shared secret would be all zeros,
		// whatever the own key is (RFC 7748 section 6.1)
		const std::vector<KStringView> SmallOrder {
			// 0, order 4
			"0000000000000000000000000000000000000000000000000000000000000000",
			// 1, order 1
			"0100000000000000000000000000000000000000000000000000000000000000",
			// order 8
			"e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
			// order 8
			"5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
			// p - 1, order 2
			"ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
			// p, the same as 0
			"edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
			// p + 1, the same as 1
			"eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f"
		};

		KX25519 Key(true);
		REQUIRE_FALSE ( Key.empty() );

		for (auto sHex : SmallOrder)
		{
			INFO  ( sHex );
			auto sPeer = kUnHex(sHex);
			REQUIRE ( sPeer.size() == 32 );
			CHECK   ( Key.DeriveSharedSecret(sPeer).empty() );

			// the highest bit of the peer's key is ignored (RFC 7748 section 5)
			sPeer[31] |= 0x80;
			CHECK   ( Key.DeriveSharedSecret(sPeer).empty() );
		}

		// and the key still agrees with a proper peer
		KX25519 Peer(true);
		CHECK ( Key.DeriveSharedSecret(Peer.GetPublicKeyRaw()).size() == 32 );
	}

	SECTION("JWK round-trip (public key)")
	{
		KX25519 Key(true);

		auto jJWK = Key.GetPublicJWK();
		CHECK ( jJWK.size() == 3 );
		CHECK ( jJWK["kty"] == "OKP"    );
		CHECK ( jJWK["crv"] == "X25519" );
		CHECK ( KBase64Url::Decode(jJWK["x"].String()) == Key.GetPublicKeyRaw() );

		KX25519 Key2;
		REQUIRE     ( Key2.CreateFromJWK(jJWK) );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK       ( Key2.GetPublicKeyRaw() == Key.GetPublicKeyRaw() );
		CHECK       ( Key2.GetPrivateJWK().empty() );
	}

	SECTION("JWK round-trip (private key)")
	{
		KX25519 Key(true);

		auto jJWK = Key.GetPrivateJWK();
		CHECK ( jJWK.size() == 4 );
		CHECK ( KBase64Url::Decode(jJWK["d"].String()) == Key.GetPrivateKeyRaw() );

		KX25519 Key2;
		REQUIRE ( Key2.CreateFromJWK(jJWK) );
		CHECK   ( Key2.IsPrivateKey() );
		CHECK   ( Key2.GetPrivateKeyRaw() == Key.GetPrivateKeyRaw() );
		CHECK   ( Key2.GetPublicKeyRaw()  == Key.GetPublicKeyRaw()  );
	}

	SECTION("JWK known-answer test (RFC 8037 Appendix A.6)")
	{
		// the keys of RFC 7748 section 6.1 as RFC 8037 writes them
		KJSON jBob {
			{ "kty", "OKP"    },
			{ "crv", "X25519" },
			{ "kid", "Bob"    },
			{ "x"  , "3p7bfXt9wbTTW2HC7OQ1Nz-DQ8hbeGdNrfx-FG-IK08" }
		};

		KJSON jEphemeral {
			{ "kty", "OKP"    },
			{ "crv", "X25519" },
			{ "x"  , "hSDwCYkwp1R0i33ctD73Wg2_Og0mOBr066SpjqqbTmo" }
		};

		KX25519 Bob;
		REQUIRE ( Bob.CreateFromJWK(jBob) );
		CHECK   ( kHex(Bob.GetPublicKeyRaw()) == "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f" );

		KX25519 Ephemeral;
		REQUIRE ( Ephemeral.CreateFromRaw(kUnHex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"), true) );
		CHECK   ( Ephemeral.GetPublicJWK() == jEphemeral );

		CHECK   ( kHex(Ephemeral.DeriveSharedSecret(Bob.GetPublicKeyRaw()))
		          == "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742" );
	}

	SECTION("CreateFromJWK refuses what is not an X25519 key")
	{
		KX25519 Good(true);
		auto jGood = Good.GetPrivateJWK();

		KX25519 Key;
		CHECK_FALSE ( Key.CreateFromJWK(KJSON{}) );
		CHECK_FALSE ( Key.CreateFromJWK(KJSON::array()) );
		CHECK_FALSE ( Key.CreateFromJWK(KJSON{{ "kty", "OKP" }, { "crv", "X25519" }}) );
		CHECK_FALSE ( Key.CreateFromJWK(KJSON{{ "kty", "OKP" }, { "crv", "X25519" }, { "x", "AAAA" }}) );

		auto jBad = jGood;
		jBad["kty"] = "EC";
		CHECK_FALSE ( Key.CreateFromJWK(jBad) );

		// an Ed25519 key is not an X25519 key
		jBad = jGood;
		jBad["crv"] = "Ed25519";
		CHECK_FALSE ( Key.CreateFromJWK(jBad) );

		// a private key that does not belong to the public key
		jBad = jGood;
		jBad["x"] = KX25519(true).GetPublicJWK()["x"];
		CHECK_FALSE ( Key.CreateFromJWK(jBad) );
		CHECK       ( Key.empty() );
		CHECK       ( Key.HasError() );

		// and a failed load leaves no earlier key behind
		REQUIRE     ( Key.CreateFromJWK(jGood) );
		CHECK_FALSE ( Key.CreateFromJWK(KJSON{}) );
		CHECK       ( Key.empty() );
	}
}

#endif // DEKAF2_HAS_X25519
