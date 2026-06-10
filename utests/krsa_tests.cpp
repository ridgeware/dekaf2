#include "catch.hpp"

#include <dekaf2/crypto/rsa/krsa.h>
#include <dekaf2/crypto/encoding/kencode.h>

using namespace dekaf2;

TEST_CASE("KRSA")
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

	constexpr KStringView sPlaintext         = "-- The quick brown fox jumps over the lazy dog! 0123456789";
	constexpr KStringView sOneCiphertextOAEP = "IuuwzvJV+oUmWFbheOTCa/YwzNYk6Neyp6nFQscKIGckHTqhmh7BxIxGkzs3CGYwb4/pfRlYlvb/oPqSOdqevkoexLU9km72rBQzgbhI+RhdWbfmZIwfo9vOZDTVsULAAEtKaJ4b4kMsIT6VA4dJHomO2gB8qQ19kLxTPrPmMCk=";
	constexpr KStringView sOneCiphertext     = "GzA53QWIY4ueFio66iPAqNMeygg6X37kD6BSy8ckkBJCxEu6hD5NiN9//XWgCcR8mFTQYgH0HpdrSdeVfUwvLo5SDkXlINSvllpo7H2Hzn+H0GnHDG+Pk0yJ+zxB31TgesH8zISRIVJINlQxKH+d7A1gfrHqp7LJVcfVGezk9Dw=";


	SECTION("Encrypt-Decrypt with OAEP")
	{
		KRSAKey PubKey(sPubKey);
		CHECK ( PubKey.empty()        == false );
		CHECK ( PubKey.IsPrivateKey() == false );

		KRSAEncrypt RSAEnc(PubKey, true);
		auto sCiphertext = KEncode::Base64(RSAEnc.Encrypt(sPlaintext), false);

		KRSAKey PrivKey(sPrivKey);
		CHECK ( PrivKey.empty()        == false );
		CHECK ( PrivKey.IsPrivateKey() == true  );

		KRSADecrypt RSADec(PrivKey, true);
		// first use the static ciphertext
		auto sPlain = RSADec.Decrypt(KDecode::Base64(sOneCiphertextOAEP));
		CHECK ( sPlain == sPlaintext );
		// now the dynamic
		sPlain = RSADec.Decrypt(KDecode::Base64(sCiphertext));
		CHECK ( sPlain == sPlaintext );
		CHECK ( sCiphertext != sOneCiphertextOAEP );
	}

	SECTION("Encrypt-Decrypt without OAEP")
	{
		KRSAKey PubKey(sPubKey);
		CHECK ( PubKey.empty()        == false );
		CHECK ( PubKey.IsPrivateKey() == false );

		KRSAEncrypt RSAEnc(PubKey, false);
		auto sCiphertext = KEncode::Base64(RSAEnc.Encrypt(sPlaintext), false);

		KRSAKey PrivKey(sPrivKey);
		CHECK ( PrivKey.empty()        == false );
		CHECK ( PrivKey.IsPrivateKey() == true  );

		KRSADecrypt RSADec(PrivKey, false);
		// first use the static ciphertext
		auto sPlain = RSADec.Decrypt(KDecode::Base64(sOneCiphertext));
		CHECK ( sPlain == sPlaintext );
		// now the dynamic
		sPlain = RSADec.Decrypt(KDecode::Base64(sCiphertext));
		CHECK ( sPlain == sPlaintext );
		CHECK ( sCiphertext != sOneCiphertext );
	}

	SECTION("a moved KRSAEncrypt/KRSADecrypt does not double-free and still works")
	{
		// regression: KRSAEncrypt/KRSADecrypt own a raw EVP_PKEY_CTX*, free it in the
		// dtor, but used to declare neither move nor deleted-copy -> std::move bound to
		// the implicit SHALLOW copy, so source and destination shared one ctx and
		// EVP_PKEY_CTX_free ran twice on it (double free / SIGABRT). After the fix the
		// move ctor/assignment transfer the ctx and null the source.
		KRSAKey PubKey(sPubKey);
		KRSAKey PrivKey(sPrivKey);
		REQUIRE_FALSE ( PubKey.empty()  );
		REQUIRE_FALSE ( PrivKey.empty() );

		KString sCipher;
		{
			KRSAEncrypt Src(PubKey, true);
			KRSAEncrypt Enc(std::move(Src));   // before fix: shallow copy -> shared ctx
			CHECK_FALSE ( Enc.HasError() );
			sCipher = Enc.Encrypt(sPlaintext);
			// both Src and Enc destruct here; before the fix they free the same ctx
		}
		CHECK_FALSE ( sCipher.empty() );

		KString sPlain;
		{
			KRSADecrypt Src(PrivKey, true);
			KRSADecrypt Dec(std::move(Src));   // before fix: shallow copy -> shared ctx
			CHECK_FALSE ( Dec.HasError() );
			sPlain = Dec.Decrypt(sCipher);
		}
		CHECK ( sPlain == sPlaintext );

		// move-ASSIGNMENT path too (used to fall back to the implicit shallow copy-assign)
		{
			KRSAEncrypt A(PubKey, true);
			KRSAEncrypt B(PubKey, true);
			B = std::move(A);
			CHECK_FALSE ( B.HasError() );
			CHECK_FALSE ( B.Encrypt(sPlaintext).empty() );
		}
	}
}
