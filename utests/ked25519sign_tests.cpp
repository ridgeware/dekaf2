#include "catch.hpp"

#include <dekaf2/crypto/ec/ked25519sign.h>

#if DEKAF2_HAS_ED25519

#include <dekaf2/crypto/encoding/khex.h>

using namespace dekaf2;

TEST_CASE("KEd25519Key")
{
	SECTION("default ctor is empty")
	{
		KEd25519Key Key;
		CHECK ( Key.empty() );
		CHECK_FALSE ( Key.IsPrivateKey() );
	}

	SECTION("Generate creates a valid key pair")
	{
		KEd25519Key Key(true);
		CHECK_FALSE ( Key.empty() );
		CHECK       ( Key.IsPrivateKey() );
		CHECK_FALSE ( Key.HasError() );
	}

	SECTION("GetPrivateKeyRaw returns 32 bytes")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPriv = Key.GetPrivateKeyRaw();
		CHECK ( sPriv.size() == 32 );
	}

	SECTION("GetPublicKeyRaw returns 32 bytes")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPub = Key.GetPublicKeyRaw();
		CHECK ( sPub.size() == 32 );
	}

	SECTION("PEM round-trip (private key)")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPEM = Key1.GetPEM(true);
		CHECK_FALSE ( sPEM.empty() );
		CHECK ( sPEM.starts_with("-----BEGIN") );

		KEd25519Key Key2(sPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPrivateKeyRaw() == Key1.GetPrivateKeyRaw() );
		CHECK ( Key2.GetPublicKeyRaw()  == Key1.GetPublicKeyRaw() );
	}

	SECTION("PEM round-trip (public key)")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		KString sPubPEM = Key1.GetPEM(false);
		CHECK_FALSE ( sPubPEM.empty() );

		KEd25519Key Key2(sPubPEM);
		REQUIRE_FALSE ( Key2.empty() );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == Key1.GetPublicKeyRaw() );
	}

	SECTION("raw key round-trip (private)")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPrivRaw = Key1.GetPrivateKeyRaw();
		auto sPubRaw  = Key1.GetPublicKeyRaw();

		KEd25519Key Key2;
		CHECK ( Key2.CreateFromRaw(sPrivRaw, true) );
		CHECK ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("raw key round-trip (public)")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPubRaw = Key1.GetPublicKeyRaw();

		KEd25519Key Key2;
		CHECK ( Key2.CreateFromRaw(sPubRaw, false) );
		CHECK_FALSE ( Key2.IsPrivateKey() );
		CHECK ( Key2.GetPublicKeyRaw() == sPubRaw );
	}

	SECTION("clear resets the key")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		Key.clear();
		CHECK ( Key.empty() );
	}

	SECTION("move construction")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KEd25519Key Key2(std::move(Key1));
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("move assignment")
	{
		KEd25519Key Key1(true);
		REQUIRE_FALSE ( Key1.empty() );

		auto sPub = Key1.GetPublicKeyRaw();

		KEd25519Key Key2;
		Key2 = std::move(Key1);
		CHECK_FALSE ( Key2.empty() );
		CHECK       ( Key1.empty() );
		CHECK       ( Key2.GetPublicKeyRaw() == sPub );
	}

	SECTION("CreateFromPEM with invalid PEM fails")
	{
		KEd25519Key Key;
		CHECK_FALSE ( Key.CreateFromPEM("not a PEM") );
		CHECK       ( Key.empty() );
	}

	SECTION("CreateFromRaw with wrong size fails")
	{
		KEd25519Key Key;
		CHECK_FALSE ( Key.CreateFromRaw("wrong_size", true) );
		CHECK       ( Key.empty() );
	}

	SECTION("two generated keys are different")
	{
		KEd25519Key Key1(true);
		KEd25519Key Key2(true);
		CHECK ( Key1.GetPublicKeyRaw() != Key2.GetPublicKeyRaw() );
	}

	SECTION("known-answer test (RFC 8032 Section 7.1, Test 1)")
	{
		// 32-byte seed (private key)
		KString sPriv = kUnHex(
			"9d61b19deffd5a60ba844af492ec2cc4"
			"4449c5697b326919703bac031cae7f60");

		// expected public key
		KString sPubExpected = kUnHex(
			"d75a980182b10ab7d54bfed3c964073a"
			"0ee172f3daa62325af021a68f707511a");

		KEd25519Key Key;
		REQUIRE ( Key.CreateFromRaw(sPriv, true) );
		CHECK ( kHex(Key.GetPublicKeyRaw()) == kHex(sPubExpected) );
	}
}

TEST_CASE("KEd25519Sign")
{
	SECTION("Sign produces 64-byte signature")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		KEd25519Sign Signer;
		KString sSig = Signer.Sign(Key, "hello world");

		CHECK_FALSE ( sSig.empty() );
		CHECK       ( sSig.size() == 64 );
	}

	SECTION("Sign + Verify round-trip")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		KEd25519Sign   Signer;
		KEd25519Verify Verifier;

		KString sData = "the quick brown fox";
		KString sSig  = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		CHECK ( Verifier.Verify(Key, sData, sSig) );
	}

	SECTION("Verify fails with wrong data")
	{
		KEd25519Key Key(true);
		KEd25519Sign   Signer;
		KEd25519Verify Verifier;

		KString sSig = Signer.Sign(Key, "original data");
		REQUIRE_FALSE ( sSig.empty() );

		CHECK_FALSE ( Verifier.Verify(Key, "tampered data", sSig) );
	}

	SECTION("Verify fails with wrong key")
	{
		KEd25519Key Key1(true);
		KEd25519Key Key2(true);
		KEd25519Sign   Signer;
		KEd25519Verify Verifier;

		KString sData = "some data";
		KString sSig  = Signer.Sign(Key1, sData);
		REQUIRE_FALSE ( sSig.empty() );

		CHECK_FALSE ( Verifier.Verify(Key2, sData, sSig) );
	}

	SECTION("Verify fails with corrupted signature")
	{
		KEd25519Key Key(true);
		KEd25519Sign   Signer;
		KEd25519Verify Verifier;

		KString sData = "integrity test";
		KString sSig  = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// flip a bit
		sSig[0] ^= 0x01;
		CHECK_FALSE ( Verifier.Verify(Key, sData, sSig) );
	}

	SECTION("Verify with public-only key")
	{
		KEd25519Key PrivKey(true);
		REQUIRE_FALSE ( PrivKey.empty() );

		// sign with private key
		KEd25519Sign Signer;
		KString sData = "verify with pub key";
		KString sSig  = Signer.Sign(PrivKey, sData);
		REQUIRE_FALSE ( sSig.empty() );

		// load public key only
		KEd25519Key PubKey;
		PubKey.CreateFromRaw(PrivKey.GetPublicKeyRaw(), false);
		REQUIRE_FALSE ( PubKey.IsPrivateKey() );

		KEd25519Verify Verifier;
		CHECK ( Verifier.Verify(PubKey, sData, sSig) );
	}

	SECTION("Sign with empty key fails")
	{
		KEd25519Key Key;
		KEd25519Sign Signer;
		KString sSig = Signer.Sign(Key, "test");
		CHECK ( sSig.empty() );
	}

	SECTION("Ed25519 is deterministic")
	{
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		KEd25519Sign Signer;
		KString sData = "determinism test";

		KString sSig1 = Signer.Sign(Key, sData);
		KString sSig2 = Signer.Sign(Key, sData);

		REQUIRE ( sSig1.size() == 64 );
		CHECK   ( sSig1 == sSig2 );
	}

	SECTION("known-answer test (RFC 8032 Section 7.1, Test 2)")
	{
		// 32-byte seed
		KString sPriv = kUnHex(
			"4ccd089b28ff96da9db6c346ec114e0f"
			"5b8a319f35aba624da8cf6ed4fb8a6fb");

		// expected public key
		KString sPubExpected = kUnHex(
			"3d4017c3e843895a92b70aa74d1b7ebc"
			"9c982ccf2ec4968cc0cd55f12af4660c");

		// message: 0x72 (single byte 'r')
		KString sMsg(1, '\x72');

		// expected signature
		KString sSigExpected = kUnHex(
			"92a009a9f0d4cab8720e820b5f642540"
			"a2b27b5416503f8fb3762223ebdb69da"
			"085ac1e43e15996e458f3613d0f11d8c"
			"387b2eaeb4302aeeb00d291612bb0c00");

		KEd25519Key Key;
		REQUIRE ( Key.CreateFromRaw(sPriv, true) );
		CHECK   ( kHex(Key.GetPublicKeyRaw()) == kHex(sPubExpected) );

		KEd25519Sign Signer;
		KString sSig = Signer.Sign(Key, sMsg);
		CHECK ( kHex(sSig) == kHex(sSigExpected) );

		KEd25519Verify Verifier;
		CHECK ( Verifier.Verify(Key, sMsg, sSig) );
	}
}

#endif // DEKAF2_HAS_ED25519
