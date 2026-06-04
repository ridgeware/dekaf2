#include "catch.hpp"

#include <dekaf2/crypto/encoding/kbase32.h>
#include <dekaf2/crypto/auth/ktotp.h>
#include <dekaf2/time/clock/ktime.h>
#include <ctime>
#include <vector>

using namespace dekaf2;

TEST_CASE("KBase32")
{
	SECTION("RFC 4648 test vectors (encode, padded)")
	{
		CHECK ( KBase32::Encode("")       == ""                 );
		CHECK ( KBase32::Encode("f")      == "MY======"         );
		CHECK ( KBase32::Encode("fo")     == "MZXQ===="         );
		CHECK ( KBase32::Encode("foo")    == "MZXW6==="         );
		CHECK ( KBase32::Encode("foob")   == "MZXW6YQ="         );
		CHECK ( KBase32::Encode("fooba")  == "MZXW6YTB"         );
		CHECK ( KBase32::Encode("foobar") == "MZXW6YTBOI======" );
	}

	SECTION("unpadded encode")
	{
		CHECK ( KBase32::Encode("foo",    /*bPadding=*/false) == "MZXW6"      );
		CHECK ( KBase32::Encode("foobar", /*bPadding=*/false) == "MZXW6YTBOI" );
	}

	SECTION("decode (padding/whitespace ignored, case-insensitive)")
	{
		CHECK ( KBase32::Decode("MZXW6YTBOI======") == "foobar" );
		CHECK ( KBase32::Decode("MZXW6YTBOI")       == "foobar" ); // unpadded
		CHECK ( KBase32::Decode("mzxw6ytboi")       == "foobar" ); // lower-case
		CHECK ( KBase32::Decode("MZXW 6YTB OI")     == "foobar" ); // whitespace
		CHECK ( KBase32::Decode("MY======")         == "f"      );
		CHECK ( KBase32::Decode("!!!!").empty()                 ); // invalid -> empty
	}

	SECTION("round-trips arbitrary bytes")
	{
		KString sAll;
		for (int i = 0; i < 256; ++i) sAll += static_cast<KString::value_type>(i);
		CHECK ( KBase32::Decode(KBase32::Encode(sAll)) == sAll );
	}
}

TEST_CASE("KTOTP")
{
	// RFC 6238 Appendix B reference values (8 digits, 30s period)
	SECTION("RFC 6238 reference vectors")
	{
		// the published shared secrets (ASCII), per algorithm
		const KString sSha1   = "12345678901234567890";
		const KString sSha256 = "12345678901234567890123456789012";
		const KString sSha512 = "1234567890123456789012345678901234567890123456789012345678901234";

		struct Vec { std::time_t t; const char* sha1; const char* sha256; const char* sha512; };
		const std::vector<Vec> vecs = {
			{          59, "94287082", "46119246", "90693936" },
			{  1111111109, "07081804", "68084774", "25091201" },
			{  1111111111, "14050471", "67062674", "99943326" },
			{  1234567890, "89005924", "91819424", "93441116" },
			{  2000000000, "69279037", "90698825", "38618901" },
			{ 20000000000, "65353130", "77737706", "47863826" },
		};

		KTOTP t1(KBase32::Encode(sSha1));   t1.Digits(8).UseAlgorithm(KTOTP::Algorithm::SHA1);
		KTOTP t256(KBase32::Encode(sSha256)); t256.Digits(8).UseAlgorithm(KTOTP::Algorithm::SHA256);
		KTOTP t512(KBase32::Encode(sSha512)); t512.Digits(8).UseAlgorithm(KTOTP::Algorithm::SHA512);

		for (const auto& v : vecs)
		{
			KUnixTime when { v.t };
			CHECK ( t1.FormatCode(when)   == v.sha1   );
			CHECK ( t256.FormatCode(when) == v.sha256 );
			CHECK ( t512.FormatCode(when) == v.sha512 );
		}
	}

	SECTION("generate, verify, clock-skew window")
	{
		KTOTP t = KTOTP::Generate();
		CHECK ( !t.empty() );
		CHECK ( !t.Secret().empty() );
		CHECK ( KBase32::Decode(t.Secret()).size() == 20 ); // 160-bit secret round-trips

		KUnixTime now { std::time_t(1700000000) };
		KString sCode = t.FormatCode(now);
		CHECK ( sCode.size() == 6 );

		CHECK (  t.Verify(sCode, 1, now) );                 // exact step
		CHECK (  t.Verify(t.FormatCode(KUnixTime{std::time_t(1700000000 - 30)}), 1, now) ); // prev step, within skew
		CHECK (  t.Verify(t.FormatCode(KUnixTime{std::time_t(1700000000 + 30)}), 1, now) ); // next step, within skew
		CHECK ( !t.Verify(t.FormatCode(KUnixTime{std::time_t(1700000000 - 90)}), 1, now) ); // outside skew
		CHECK ( (!t.Verify("000000", 1, now) || t.Code(now) == 0) ); // wrong code (unless it really is 0)
		CHECK ( !t.Verify("12345",  1, now) );              // wrong length
		CHECK ( !t.Verify("12x456", 1, now) );              // non-digit
		// spaces are tolerated
		KString sSpaced; for (std::size_t i=0;i<sCode.size();++i){ sSpaced+=sCode[i]; if(i==2) sSpaced+=' '; }
		CHECK (  t.Verify(sSpaced, 1, now) );
	}

	SECTION("otpauth provisioning URI")
	{
		KTOTP t("JBSWY3DPEHPK3PXP");
		KString sURI = t.URI("kssod", "alice");
		CHECK ( sURI.starts_with("otpauth://totp/kssod:alice?") );
		CHECK ( sURI.contains("secret=JBSWY3DPEHPK3PXP") );
		CHECK ( sURI.contains("issuer=kssod") );
		CHECK ( sURI.contains("digits=6") );
		CHECK ( sURI.contains("period=30") );
		CHECK ( sURI.contains("algorithm=SHA1") );
	}

	SECTION("out-of-band helpers: numeric OTP, backup codes, hashing")
	{
		CHECK ( KTOTP::GenerateNumericCode(6).size() == 6 );
		CHECK ( KTOTP::GenerateNumericCode(8).size() == 8 );
		for (char c : KTOTP::GenerateNumericCode(6)) CHECK ( (c >= '0' && c <= '9') );

		auto codes = KTOTP::GenerateBackupCodes(10, 5);
		CHECK ( codes.size() == 10 );
		CHECK ( codes[0].size() == 11 );      // xxxxx-xxxxx
		CHECK ( codes[0][5] == '-' );

		// hashing normalizes case, dashes and spaces; different codes differ
		CHECK ( KTOTP::HashCode("ABCDE-FGHIJ") == KTOTP::HashCode("abcdefghij") );
		CHECK ( KTOTP::HashCode("abc de")      == KTOTP::HashCode("abcde")      );
		CHECK ( KTOTP::HashCode("123456")      != KTOTP::HashCode("123457")     );
		CHECK ( KTOTP::HashCode("x").size()    == 64 );  // SHA-256 hex
	}
}
