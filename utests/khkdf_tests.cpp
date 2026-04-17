#include "catch.hpp"

#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/crypto/encoding/khex.h>

using namespace dekaf2;

TEST_CASE("KHKDF")
{
	SECTION("Extract produces 32-byte PRK with SHA256")
	{
		KString sPRK = KHKDF::Extract("salt", "input keying material");
		CHECK ( sPRK.size() == 32 );
	}

	SECTION("Expand produces requested length")
	{
		KString sPRK = KHKDF::Extract("salt", "ikm");

		auto s16  = KHKDF::Expand(sPRK, "info", 16);
		auto s32  = KHKDF::Expand(sPRK, "info", 32);
		auto s64  = KHKDF::Expand(sPRK, "info", 64);

		CHECK ( s16.size() == 16 );
		CHECK ( s32.size() == 32 );
		CHECK ( s64.size() == 64 );
	}

	SECTION("DeriveKey = Extract + Expand")
	{
		KString sPRK    = KHKDF::Extract("salt", "ikm");
		KString sExpand = KHKDF::Expand(sPRK, "info", 42);
		KString sDerived = KHKDF::DeriveKey("salt", "ikm", "info", 42);

		CHECK ( sExpand == sDerived );
	}

	// RFC 5869 Appendix A - Test Case 1
	SECTION("RFC 5869 Test Case 1")
	{
		KString sIKM  = kUnHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
		KString sSalt = kUnHex("000102030405060708090a0b0c");
		KString sInfo = kUnHex("f0f1f2f3f4f5f6f7f8f9");

		KString sPRK = KHKDF::Extract(sSalt, sIKM);
		CHECK ( kHex(sPRK) == "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" );

		KString sOKM = KHKDF::Expand(sPRK, sInfo, 42);
		CHECK ( kHex(sOKM) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865" );
	}

	// RFC 5869 Appendix A - Test Case 2
	SECTION("RFC 5869 Test Case 2")
	{
		KString sIKM  = kUnHex(
			"000102030405060708090a0b0c0d0e0f"
			"101112131415161718191a1b1c1d1e1f"
			"202122232425262728292a2b2c2d2e2f"
			"303132333435363738393a3b3c3d3e3f"
			"404142434445464748494a4b4c4d4e4f");
		KString sSalt = kUnHex(
			"606162636465666768696a6b6c6d6e6f"
			"707172737475767778797a7b7c7d7e7f"
			"808182838485868788898a8b8c8d8e8f"
			"909192939495969798999a9b9c9d9e9f"
			"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf");
		KString sInfo = kUnHex(
			"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
			"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
			"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
			"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
			"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

		KString sPRK = KHKDF::Extract(sSalt, sIKM);
		CHECK ( kHex(sPRK) == "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244" );

		KString sOKM = KHKDF::Expand(sPRK, sInfo, 82);
		CHECK ( kHex(sOKM) ==
			"b11e398dc80327a1c8e7f78c596a4934"
			"4f012eda2d4efad8a050cc4c19afa97c"
			"59045a99cac7827271cb41c65e590e09"
			"da3275600c2f09b8367793a9aca3db71"
			"cc30c58179ec3e87c14c01d5c1f3434f"
			"1d87" );
	}

	// RFC 5869 Appendix A - Test Case 3 (zero-length salt and info)
	SECTION("RFC 5869 Test Case 3")
	{
		KString sIKM = kUnHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");

		KString sPRK = KHKDF::Extract(KStringView{}, sIKM);
		CHECK ( kHex(sPRK) == "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04" );

		KString sOKM = KHKDF::Expand(sPRK, KStringView{}, 42);
		CHECK ( kHex(sOKM) == "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8" );
	}

	SECTION("different salts produce different PRKs")
	{
		auto sPRK1 = KHKDF::Extract("salt1", "same ikm");
		auto sPRK2 = KHKDF::Extract("salt2", "same ikm");
		CHECK ( sPRK1 != sPRK2 );
	}

	SECTION("different info produces different OKM")
	{
		auto sPRK  = KHKDF::Extract("salt", "ikm");
		auto sOKM1 = KHKDF::Expand(sPRK, "info1", 32);
		auto sOKM2 = KHKDF::Expand(sPRK, "info2", 32);
		CHECK ( sOKM1 != sOKM2 );
	}

	SECTION("empty salt is valid")
	{
		auto sPRK = KHKDF::Extract(KStringView{}, "some ikm");
		CHECK ( sPRK.size() == 32 );
	}

	SECTION("empty info is valid")
	{
		auto sPRK = KHKDF::Extract("salt", "ikm");
		auto sOKM = KHKDF::Expand(sPRK, KStringView{}, 32);
		CHECK ( sOKM.size() == 32 );
	}
}
