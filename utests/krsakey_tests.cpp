#include "catch.hpp"

#include <dekaf2/krsakey.h>
#include <dekaf2/kjson.h>

using namespace dekaf2;

TEST_CASE("KRSAKey")
{
	constexpr KStringView sPubKey = R"(-----BEGIN PUBLIC KEY-----
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC3OLbHLDK3MD6grp9cK2oWYROL
LnoCZ7YnfQJKwium35CACvTuAM3sQC5TEJUo4x2fY9hJadeKyVivNTrHg/t8D6BN
LYlce6QloX7AlJBT29LQLXnmDiM6anaXhOQ//7HWYfBzeYXyq1k2XSAPRUp7hA8h
9Gu7pYutRoU/AuuqrwIDAQAB
-----END PUBLIC KEY-----
)";

	constexpr KStringView sPrivKey = R"(-----BEGIN PRIVATE KEY-----
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

	SECTION("CreateNew")
	{
		KRSAKey Key(1024);
		auto sPublicKey  = Key.GetPEM(false);
		auto sPrivateKey = Key.GetPEM(true);
		CHECK ( sPublicKey.size()  >= 200 );
		CHECK ( sPrivateKey.size() >= 800 );
		CHECK ( Key.empty() == false );
		CHECK ( Key.IsPrivateKey() == true );
	}

	SECTION("FromPrivPEM")
	{
		KRSAKey Key(sPrivKey);
		auto sPublicKey = Key.GetPEM(false);
		CHECK ( sPublicKey == sPubKey );
		CHECK ( Key.empty() == false );
		CHECK ( Key.IsPrivateKey() == true );
	}

	SECTION("FromPubPEM")
	{
		KRSAKey Key(sPubKey);
		auto sPublicKey = Key.GetPEM(false);
		CHECK ( sPublicKey == sPubKey );
		CHECK ( Key.empty() == false );
		CHECK ( Key.IsPrivateKey() == false );
	}

	SECTION("MoveConstructPrivateKey")
	{
		// move ctor must transfer m_bIsPrivateKey
		KRSAKey Key(sPrivKey);
		CHECK ( Key.IsPrivateKey() == true );
		CHECK ( Key.empty() == false );

		KRSAKey Moved(std::move(Key));
		CHECK ( Moved.IsPrivateKey() == true  );
		CHECK ( Moved.empty()        == false );
		CHECK ( Key.empty()          == true  );
	}

	SECTION("MoveConstructPublicKey")
	{
		KRSAKey Key(sPubKey);
		CHECK ( Key.IsPrivateKey() == false );
		CHECK ( Key.empty() == false );

		KRSAKey Moved(std::move(Key));
		CHECK ( Moved.IsPrivateKey() == false );
		CHECK ( Moved.empty()        == false );
		CHECK ( Key.empty()          == true  );
	}

	SECTION("MoveAssignPrivateKey")
	{
		// move assign must free existing key and transfer m_bIsPrivateKey
		KRSAKey Key1(sPrivKey);
		KRSAKey Key2(sPubKey);
		CHECK ( Key1.IsPrivateKey() == true  );
		CHECK ( Key2.IsPrivateKey() == false );

		Key2 = std::move(Key1);
		CHECK ( Key2.IsPrivateKey() == true  );
		CHECK ( Key2.empty()        == false );
		CHECK ( Key1.empty()        == true  );
	}

	SECTION("MoveAssignOverExisting")
	{
		// move assign over an existing key must not leak
		KRSAKey Key1(1024);
		KRSAKey Key2(1024);
		auto sPEM1 = Key1.GetPEM(false);

		Key2 = std::move(Key1);
		CHECK ( Key2.empty()        == false );
		CHECK ( Key2.IsPrivateKey() == true  );
		CHECK ( Key2.GetPEM(false)  == sPEM1 );
		CHECK ( Key1.empty()        == true  );
	}

	SECTION("CreateFromJWKPrivateKey")
	{
		// test Create(Parameters) with full private key
		KJSON jwk = kjson::Parse(R"({
			"n":  "tzi2xywyt zA-oK6fXCtqFmETiy56Ame2J30CSsIrpt-QgAr07gDN7EAuUxCVKOMdn2PYSWnXislYrzU6x4P7fA-gTS2JXHukJaF-wJSQU9vS0C155g4jOmp2l4TkP_-x1mHwc3mF8qtZNl0gD0VKe4QPIfRru6WLrUaFPwLrqq8",
			"e":  "AQAB",
			"d":  "fYHXCraHAx-ENxOHTF7c6_mRpqyunUkn6QLj-AvsVQId_OgEgaiBZs1MLlBUrM5u80k01o0vPf3QxyxCWHtbX6akf03VRFCF vV0OuLZvacnFxYjxpx4Sa1GFafY83NYrLkfe7to4DzQO_90nASWwfPwOoeMUlCm1hq4mnqu6uo",
			"p":  "2zOBov6l-ngl-vaNp-PvSVQ2WKMGgbceOzCHjggQ5HduEWEvdw_9WHhbNCUgNjKCyki1T9rELgy9mHbEY1Q4Ew",
			"q":  "1frtzDLJ830QtqEqYZ_LfSzzlXeLiYomhkjldFXHQW8QwRWbQ-OVIBq0mwSguyk4W46x_ipNmi3FSnQMrwnPDnU",
			"dp": "d0Zumw-vp zvrhsTUTe_0i9U8AT0sVDcdRx3gFQQElRz7ko9scYAM3jAXiKdPAM65Zrrr3fjvWRNrs2Ok91uja w",
			"dq": "asXhbykvuH4JnvfgrAIOihGqIkUarYU2mJ1WZY8dyYv0rRaC2OcKtgeso qxBbh9HIHcUuosqifiORHB7m_Rhlk",
			"qi": "SbE1cgCWf4sGLNY01v1kblliCzvS9Cp44Xf_yAXtSWSaJAnH3Xk40y42ATl3F-_t-LeXSFD1zhAa1Ecf xLB0pA"
		})");

		KRSAKey Key(jwk);
		CHECK ( Key.empty()        == false );
		CHECK ( Key.IsPrivateKey() == true  );
		auto sPub = Key.GetPEM(false);
		CHECK ( sPub.size() >= 200 );
		auto sPriv = Key.GetPEM(true);
		CHECK ( sPriv.size() >= 800 );
	}

	SECTION("CreateFromJWKPublicOnly")
	{
		// public-key-only from Parameters must work
		KJSON jwk = kjson::Parse(R"({
			"n": "tzi2xywyt zA-oK6fXCtqFmETiy56Ame2J30CSsIrpt-QgAr07gDN7EAuUxCVKOMdn2PYSWnXislYrzU6x4P7fA-gTS2JXHukJaF-wJSQU9vS0C155g4jOmp2l4TkP_-x1mHwc3mF8qtZNl0gD0VKe4QPIfRru6WLrUaFPwLrqq8",
			"e": "AQAB"
		})");

		KRSAKey Key(jwk);
		CHECK ( Key.empty()        == false );
		CHECK ( Key.IsPrivateKey() == false );
		auto sPub = Key.GetPEM(false);
		CHECK ( sPub.size() >= 200 );
	}

	SECTION("GetPEMEncryptedPrivateKey")
	{
		// GetPEM with password must actually encrypt the private key
		KRSAKey Key(sPrivKey);
		CHECK ( Key.IsPrivateKey() == true );

		auto sUnencrypted = Key.GetPEM(true);
		auto sEncrypted   = Key.GetPEM(true, "testpassword");

		CHECK ( sUnencrypted.size() > 0 );
		CHECK ( sEncrypted.size()   > 0 );
		// encrypted PEM must be different from unencrypted
		CHECK ( sEncrypted != sUnencrypted );
		// encrypted PEM must contain ENCRYPTED marker
		CHECK ( sEncrypted.find("ENCRYPTED") != KString::npos );

		// verify we can read it back with the password
		KRSAKey Key2(sEncrypted, "testpassword");
		CHECK ( Key2.empty()        == false );
		CHECK ( Key2.IsPrivateKey() == true  );
		CHECK ( Key2.GetPEM(false)  == Key.GetPEM(false) );
	}
}
