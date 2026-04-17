#include "catch.hpp"

#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/khex.h>

using namespace dekaf2;

TEST_CASE("KECKey")
{
	SECTION("default ctor is empty")
	{
		KECKey Key;
		CHECK ( Key.empty() );
		CHECK_FALSE ( Key.IsPrivateKey() );
	}

	SECTION("Generate creates a valid key pair")
	{
		KECKey Key(true);
		CHECK_FALSE ( Key.empty() );
		CHECK       ( Key.IsPrivateKey() );
		CHECK_FALSE ( Key.HasError() );
	}

	SECTION("GetPrivateKeyRaw returns 32 bytes")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPriv = Key.GetPrivateKeyRaw();
		CHECK ( sPriv.size() == 32 );
	}

	SECTION("GetPublicKeyRaw returns 65 bytes with 0x04 prefix")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPub = Key.GetPublicKeyRaw();
		REQUIRE ( sPub.size() == 65 );
		CHECK   ( static_cast<unsigned char>(sPub[0]) == 0x04 );
	}

	SECTION("PEM round-trip (private key)")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPEM = Key1.GetPEM(true);
		CHECK_FALSE ( sPEM.empty() );
		CHECK ( sPEM.starts_with("-----BEGIN") );

		KECKey Key2(sPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPrivateKeyRaw() == Key1.GetPrivateKeyRaw() );
		CHECK ( Key2.GetPublicKeyRaw()  == Key1.GetPublicKeyRaw() );
	}

	SECTION("PEM round-trip (public key)")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPubPEM = Key1.GetPEM(false);
		CHECK_FALSE ( sPubPEM.empty() );

		KECKey Key2(sPubPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == Key1.GetPublicKeyRaw() );
	}

	SECTION("raw key round-trip (private)")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPrivRaw = Key1.GetPrivateKeyRaw();
		auto sPubRaw  = Key1.GetPublicKeyRaw();

		KECKey Key2;
		CHECK ( Key2.CreateFromRaw(sPrivRaw, true) );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("raw key round-trip (public)")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPubRaw = Key1.GetPublicKeyRaw();

		KECKey Key2;
		CHECK ( Key2.CreateFromRaw(sPubRaw, false) );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("clear resets the key")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		Key.clear();
		CHECK ( Key.empty() );
	}

	SECTION("move construction")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KECKey Key2(std::move(Key1));
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("move assignment")
	{
		KECKey Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KECKey Key2;
		Key2 = std::move(Key1);
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("ECDH shared secret: both sides agree")
	{
		KECKey Alice(true);
		KECKey Bob(true);
		REQUIRE_FALSE ( Alice.empty() );
		REQUIRE_FALSE ( Bob.empty() );

		KString sSecretA = Alice.DeriveSharedSecret(Bob.GetPublicKeyRaw());
		KString sSecretB = Bob.DeriveSharedSecret(Alice.GetPublicKeyRaw());

		REQUIRE ( sSecretA.size() == 32 );
		REQUIRE ( sSecretB.size() == 32 );
		CHECK   ( sSecretA == sSecretB );
	}

	SECTION("ECDH with wrong key size fails")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sSecret = Key.DeriveSharedSecret("too_short");
		CHECK ( sSecret.empty() );
	}

	SECTION("CreateFromPEM with invalid PEM fails")
	{
		KECKey Key;
		CHECK_FALSE ( Key.CreateFromPEM("not a PEM") );
		CHECK       ( Key.empty() );
	}

	SECTION("CreateFromRaw with wrong size fails")
	{
		KECKey Key;
		CHECK_FALSE ( Key.CreateFromRaw("wrong_size", true) );
		CHECK       ( Key.empty() );
	}

	SECTION("two generated keys are different")
	{
		KECKey Key1(true);
		KECKey Key2(true);
		CHECK ( Key1.GetPublicKeyRaw() != Key2.GetPublicKeyRaw() );
	}
}

TEST_CASE("KECSign")
{
	SECTION("Sign produces 64-byte r||s signature")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		KECSign Signer;
		KString sSig = Signer.Sign(Key, "hello world");

		CHECK_FALSE ( sSig.empty() );
		CHECK       ( sSig.size() == 64 );
	}

	SECTION("Sign + Verify round-trip")
	{
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		KECSign   Signer;
		KECVerify Verifier;

		KString sData = "the quick brown fox";
		KString sSig  = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		CHECK ( Verifier.Verify(Key, sData, sSig) );
	}

	SECTION("Verify fails with wrong data")
	{
		KECKey Key(true);
		KECSign   Signer;
		KECVerify Verifier;

		KString sSig = Signer.Sign(Key, "original data");
		REQUIRE_FALSE ( sSig.empty() );

		CHECK_FALSE ( Verifier.Verify(Key, "tampered data", sSig) );
	}

	SECTION("Verify fails with wrong key")
	{
		KECKey Key1(true);
		KECKey Key2(true);
		KECSign   Signer;
		KECVerify Verifier;

		KString sData = "some data";
		KString sSig  = Signer.Sign(Key1, sData);
		REQUIRE_FALSE ( sSig.empty() );

		CHECK_FALSE ( Verifier.Verify(Key2, sData, sSig) );
	}

	SECTION("Verify fails with corrupted signature")
	{
		KECKey Key(true);
		KECSign   Signer;
		KECVerify Verifier;

		KString sData = "integrity test";
		KString sSig  = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// flip a bit
		sSig[0] ^= 0x01;
		CHECK_FALSE ( Verifier.Verify(Key, sData, sSig) );
	}

	SECTION("Verify with public-only key")
	{
		KECKey PrivKey(true);
		REQUIRE_FALSE ( PrivKey.empty() );

		// sign with private key
		KECSign Signer;
		KString sData = "verify with pub key";
		KString sSig  = Signer.Sign(PrivKey, sData);
		REQUIRE_FALSE ( sSig.empty() );

		// load public key only
		KECKey PubKey;
		PubKey.CreateFromRaw(PrivKey.GetPublicKeyRaw(), false);
		REQUIRE_FALSE ( PubKey.IsPrivateKey() );

		KECVerify Verifier;
		CHECK ( Verifier.Verify(PubKey, sData, sSig) );
	}

	SECTION("Sign with empty key fails")
	{
		KECKey Key;
		KECSign Signer;
		KString sSig = Signer.Sign(Key, "test");
		CHECK ( sSig.empty() );
	}
}
