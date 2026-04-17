#include "catch.hpp"

#include <dekaf2/crypto/auth/kopenid.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/encoding/khex.h>

using namespace dekaf2;

TEST_CASE("KOpenIDKeys")
{
	SECTION("empty default ctor")
	{
		KOpenIDKeys Keys;
		CHECK ( Keys.empty() );
		CHECK ( Keys.size() == 0 );
	}

	SECTION("JWKS with no keys fails validation")
	{
		KJSON jwks = { {"keys", KJSON::array()} };
		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.HasError() );
		CHECK ( Keys.empty() );
	}

	SECTION("ES256: JWK parse + VerifySignature")
	{
		// generate an EC P-256 key pair
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPubRaw = Key.GetPublicKeyRaw();
		REQUIRE ( sPubRaw.size() == 65 );

		// extract x (bytes 1..32) and y (bytes 33..64) from uncompressed point
		auto sX = KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32));
		auto sY = KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32));

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   sX},
				{"y",   sY},
				{"kid", "test-ec"},
				{"alg", "ES256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data
		KStringView sData = "eyJhbGciOiJFUzI1NiJ9.eyJzdWIiOiJ0ZXN0In0";
		KECSign Signer;
		auto sSig = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// positive verification
		CHECK ( Keys.VerifySignature("test-ec", "ES256", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "ES256", "", sData, sSig) );

		// wrong algorithm
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "RS256", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES256", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES256", "", "tampered.data", sSig) );
	}

#if DEKAF2_HAS_ED25519
	SECTION("EdDSA: JWK parse + VerifySignature")
	{
		// generate an Ed25519 key pair
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPubRaw = Key.GetPublicKeyRaw();
		REQUIRE ( sPubRaw.size() == 32 );

		auto sX = KBase64Url::Encode(sPubRaw);

		KJSON jwks = { {"keys", {
			{
				{"kty", "OKP"},
				{"crv", "Ed25519"},
				{"x",   sX},
				{"kid", "test-ed"},
				{"alg", "EdDSA"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data
		KStringView sData = "eyJhbGciOiJFZERTQSJ9.eyJzdWIiOiJ0ZXN0In0";
		KEd25519Sign Signer;
		auto sSig = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// positive verification
		CHECK ( Keys.VerifySignature("test-ed", "EdDSA", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "EdDSA", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-ed", "EdDSA", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-ed", "EdDSA", "", "tampered.data", sSig) );
	}
#endif // DEKAF2_HAS_ED25519

	SECTION("RS256: JWK parse + VerifySignature")
	{
		// use the known 1024-bit RSA test key from krsasign_tests.cpp
		constexpr KStringView sPrivPEM = R"(-----BEGIN PRIVATE KEY-----
MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBALc4tscsMrcwPqCu
n1wrahZhE4suegJntid9AkrCK6bfkIAK9O4AzexALlMQlSjjHZ9j2Elp14rJWK81
OseD+3wPoE0tiVx7pCWhfsCUkFPb0tAteeYOIzpqdpeE5D//sdZh8HN5hfKrWTZd
IA9FSnuEDyH0a7uli61GhT8C66qvAgMBAAECgYB9gdcKtocDH4Q3E4dMXtzr+ZGm
rK6dSSfpAuP4C+xVAh386ASBqIFmzUwuUFSszm7zSTTWjS89/dDHLEJYe1tfpqR/
TdVEUIW9XQ64tm9pycXFiPGnHhJrUYVp9jzc1isuR97u2jgPNA7/3ScBJbB8/A6h
4xSUKbWGriaeq7q6iQJBANszgaL+pfp4Jfr2jafj70lUNlijBoG3Hjswh44IEOR3
bhFhL3cP/Vh4WzQlIDYygspItU/axC4MvZh2xGNUOBMCQQDV+u3MssnzfRC2oSph
n8t9LPOVd4uJMoZI5XRVx0FvEMEVm0PjlSAetJsEoLsVOFuOsf4qTZotxUp0DK8J
zw51AkB3Rm6bD6+nO+uGxNRN7/SL1TwBPSxUNx1HHeAVBASVHPuSj2xxgAzeMBeI
p08Azrlmcuvd+O9ZE2uzY6T3W6NrAkBqxeFvKS+4fgme9+CsAg6KEaoiRRqthTaY
nVZljx3Ji/StEWLY5wq2B6zqrEFuH0cgdxS6iyqJ+E5khge5v0YZAkBJsTVyAJZ/
iwYs1jTW/WRuWYILO9L0Knjhd//IBe1JZJokCcfdeTjTLjYBOXcX7+34t5dIUPXO
EBrURx/EsHSk
-----END PRIVATE KEY-----
)";

		// modulus and exponent from the above key (hex)
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));

		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "test-rsa"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data with the private key
		KStringView sData = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ0ZXN0In0";
		KRSAKey PrivKey(sPrivPEM);
		REQUIRE_FALSE ( PrivKey.empty() );

		KRSAVerify::Digest Algorithm = KRSASign::SHA256;
		KRSASign Signer(Algorithm, sData);
		auto sSig = Signer.Sign(PrivKey);
		REQUIRE_FALSE ( sSig.empty() );

		// positive verification
		CHECK ( Keys.VerifySignature("test-rsa", "RS256", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "RS256", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-rsa", "RS256", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-rsa", "RS256", "", "tampered.data", sSig) );
	}

	SECTION("GetRSAKey backward compat")
	{
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));

		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "test-rsa"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		REQUIRE ( Keys.size() == 1 );

		// should find the RSA key
		const auto& Key = Keys.GetRSAKey("test-rsa", "RS256", "", "sig");
		CHECK_FALSE ( Key.empty() );

		// wrong kid returns empty
		const auto& Empty = Keys.GetRSAKey("wrong", "RS256", "", "sig");
		CHECK ( Empty.empty() );
	}

	SECTION("mixed key types")
	{
		// EC key
		KECKey ECKeyPair(true);
		auto sPubRaw = ECKeyPair.GetPublicKeyRaw();
		auto sEcX = KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32));
		auto sEcY = KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32));

		// RSA key
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));
		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   sEcX},
				{"y",   sEcY},
				{"kid", "ec-key"},
				{"alg", "ES256"},
				{"use", "sig"}
			},
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "rsa-key"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 2 );

		// EC signature
		KStringView sData = "header.payload";
		KECSign ECSigner;
		auto sECSig = ECSigner.Sign(ECKeyPair, sData);
		REQUIRE_FALSE ( sECSig.empty() );
		CHECK ( Keys.VerifySignature("ec-key", "ES256", "", sData, sECSig) );

		// EC key should not verify with RSA kid
		CHECK_FALSE ( Keys.VerifySignature("rsa-key", "ES256", "", sData, sECSig) );
	}

	SECTION("unsupported algorithm")
	{
		KECKey ECKeyPair(true);
		auto sPubRaw = ECKeyPair.GetPublicKeyRaw();

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32))},
				{"y",   KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32))},
				{"kid", "test-ec"},
				{"alg", "ES384"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		// key is loaded (kty=EC is accepted), but alg=ES384 won't match ES256 dispatch
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES384", "", "data", "sig") );
	}
}
