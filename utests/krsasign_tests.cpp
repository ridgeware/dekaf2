#include "catch.hpp"

#include <dekaf2/krsasign.h>
#include <dekaf2/kencode.h>

using namespace dekaf2;

TEST_CASE("KRSASign")
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

	constexpr KStringView sMessage1 = "-- The quick brown fox jumps over the lazy dog! 0123456789";
	constexpr KStringView sMessage2 = "aselFKJE akgh alkj sdjkaguhi3qkrgel'jlik";
	constexpr KStringView sExpected = "K6acGrV8ZN0wCgKDpvgWNcm4FcUVOnVrYHgnQEjKG8JbEvpFNC4SYLL2DCZfEXoSTw5FWZq2x6+z+gB9xDgNlqB/DcHSBso2w840OyD1qfoCmVdWsKjvwGu7RMEug7xnbN9uSQkdfR60OneEPvFHrSKyPqzMtLyhPDi4KbKdZWc=";

	SECTION("Sign-Verify")
	{
		KRSAKey PrivKey(sPrivKey);
		CHECK ( PrivKey.empty()        == false );
		CHECK ( PrivKey.IsPrivateKey() == true  );

		KRSASign Sign(KRSASign::Digest::SHA256);
		Sign += sMessage1;
		Sign += sMessage2;
		auto sSignature = KEncode::Base64(Sign.Sign(PrivKey), false);

		CHECK ( sSignature == sExpected );

		KRSAKey PubKey(sPubKey);
		CHECK ( PubKey.empty()        == false );
		CHECK ( PubKey.IsPrivateKey() == false );

		KRSAVerify Verify(KRSAVerify::Digest::SHA256);
		Verify += sMessage1;
		Verify += sMessage2;
		bool bValid = Verify.Verify(PubKey, KDecode::Base64(sSignature));

		CHECK ( bValid );
	}
}
