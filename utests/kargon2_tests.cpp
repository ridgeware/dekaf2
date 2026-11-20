#include "catch.hpp"
#include <dekaf2/crypto/auth/kargon2.h>
#include <dekaf2/crypto/auth/bits/kargon2core.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <openssl/opensslv.h>
#include <openssl/opensslconf.h>
#include <array>
#include <vector>

// OpenSSL has Argon2 from version 3.2 on, unless it was built without - it then is the
// second implementation the own one is compared with
#if OPENSSL_VERSION_NUMBER >= 0x30200000L && !defined(OPENSSL_NO_ARGON2)
	#include <openssl/core_names.h>
	#include <openssl/kdf.h>
	#include <openssl/params.h>
	#define KARGON2_TESTS_WITH_OPENSSL 1
#endif

using namespace dekaf2;

namespace {

#ifdef KARGON2_TESTS_WITH_OPENSSL

// Argon2 with OpenSSL
// returns an empty string when the provider in use does not offer Argon2
KString OpenSSLDerive(KArgon2::Variant variant, KStringView sPassword, KStringView sSalt, KArgon2::Cost cost,
                      std::size_t iTagLength = 32, KStringView sSecret = KStringView{}, KStringView sAssociatedData = KStringView{})
{
	KStringViewZ sName;

	switch (variant)
	{
		case KArgon2::Variant::Argon2d:  sName = "ARGON2D";  break;
		case KArgon2::Variant::Argon2i:  sName = "ARGON2I";  break;
		case KArgon2::Variant::Argon2id: sName = "ARGON2ID"; break;
	}

	auto* KDF = ::EVP_KDF_fetch(nullptr, sName.c_str(), nullptr);

	if (!KDF)
	{
		return {};
	}

	auto* Context = ::EVP_KDF_CTX_new(KDF);
	::EVP_KDF_free(KDF);

	if (!Context)
	{
		return {};
	}

	// the parameters are not modified by OpenSSL, but its interface is not const
	auto Octets = [](const char* sKey, KStringView sValue) -> OSSL_PARAM
	{
		return ::OSSL_PARAM_construct_octet_string(sKey, const_cast<char*>(sValue.data()), sValue.size());
	};

	std::array<OSSL_PARAM, 8> Params;
	std::size_t iParam = 0;

	Params[iParam++] = Octets(OSSL_KDF_PARAM_PASSWORD, sPassword);
	Params[iParam++] = Octets(OSSL_KDF_PARAM_SALT,     sSalt);

	if (!sSecret.empty())
	{
		Params[iParam++] = Octets(OSSL_KDF_PARAM_SECRET, sSecret);
	}

	if (!sAssociatedData.empty())
	{
		Params[iParam++] = Octets(OSSL_KDF_PARAM_ARGON2_AD, sAssociatedData);
	}

	Params[iParam++] = ::OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER,           &cost.iIterations);
	Params[iParam++] = ::OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES,   &cost.iParallelism);
	Params[iParam++] = ::OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &cost.iMemoryKiB);
	Params[iParam++] = ::OSSL_PARAM_construct_end();

	KString sTag(iTagLength, '\0');

	auto bDerived = ::EVP_KDF_derive(Context, reinterpret_cast<unsigned char*>(&sTag[0]), sTag.size(), Params.data()) == 1;

	::EVP_KDF_CTX_free(Context);

	if (!bDerived)
	{
		sTag.clear();
	}

	return sTag;
}

#endif // KARGON2_TESTS_WITH_OPENSSL

KArgon2::Cost MakeCost(uint32_t iMemoryKiB, uint32_t iIterations, uint32_t iParallelism)
{
	KArgon2::Cost cost;
	cost.iMemoryKiB   = iMemoryKiB;
	cost.iIterations  = iIterations;
	cost.iParallelism = iParallelism;
	return cost;
}

} // end of anonymous namespace

TEST_CASE("KArgon2")
{
	SECTION("BLAKE2b")
	{
		// the test vector of RFC 7693
		CHECK ( kHex(detail::KArgon2Core::Blake2b("abc", 64)) ==
		        "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
		        "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" );
		CHECK ( kHex(detail::KArgon2Core::Blake2b("", 64)) ==
		        "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419"
		        "d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce" );

		// a shorter digest is another hash, not the start of the long one
		auto sShort = detail::KArgon2Core::Blake2b("abc", 32);
		CHECK ( sShort.size() == 32 );
		CHECK ( sShort != detail::KArgon2Core::Blake2b("abc", 64).substr(0, 32) );

		CHECK ( detail::KArgon2Core::Blake2b("abc",  0).empty() );
		CHECK ( detail::KArgon2Core::Blake2b("abc", 65).empty() );

#if DEKAF2_HAS_BLAKE2
		// against the crypto library, around the block size of 128 bytes
		for (std::size_t iSize : { 0, 1, 127, 128, 129, 255, 256, 257, 1000 })
		{
			INFO ( iSize );
			KString sInput;

			for (std::size_t i = 0; i < iSize; ++i)
			{
				sInput += static_cast<char>(i * 7 + 3);
			}

			CHECK ( detail::KArgon2Core::Blake2b(sInput, 64) == KBLAKE2B(sInput).Digest() );
		}
#endif
	}

	SECTION("test vectors of RFC 9106")
	{
		KString sPassword  (32, '\x01');
		KString sSalt      (16, '\x02');
		KString sSecret    ( 8, '\x03');
		KString sAssociated(12, '\x04');
		auto    cost = MakeCost(32, 3, 4);

		struct Vector
		{
			KArgon2::Variant variant;
			KStringView      sTag;
		};

		std::vector<Vector> Vectors
		{
			{ KArgon2::Variant::Argon2d,  "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb" },
			{ KArgon2::Variant::Argon2i,  "c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8" },
			{ KArgon2::Variant::Argon2id, "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659" },
		};

		for (const auto& V : Vectors)
		{
			INFO ( static_cast<int>(V.variant) );
			CHECK ( kHex(KArgon2::Derive(V.variant, sPassword, sSalt, cost, 32, sSecret, sAssociated)) == V.sTag );
		}
	}

	SECTION("many parameters, and against the crypto library")
	{
		std::size_t iCompared { 0 };

		for (auto variant : { KArgon2::Variant::Argon2d, KArgon2::Variant::Argon2i, KArgon2::Variant::Argon2id })
		{
			for (uint32_t iParallelism : { 1, 2, 3, 4 })
			{
				for (uint32_t iMemoryKiB : { 8, 9, 31, 64, 129, 300 })
				{
					if (iMemoryKiB / 8 < iParallelism)
					{
						continue;
					}

					for (uint32_t iIterations : { 1, 2, 3 })
					{
						// the lengths around the digest size of BLAKE2b, and several of its blocks
						auto iTagLength = std::vector<std::size_t>{ 4, 32, 64, 65, 96, 97, 500 }[iCompared % 7];
						auto sPassword  = KString(iCompared % 5 * 20, 'p');
						auto sSalt      = KString(8 + iCompared % 3 * 8, 's');
						auto sSecret    = (iCompared % 2) ? KString("a secret") : KString();
						auto sData      = (iCompared % 3) ? KString("some data") : KString();
						auto cost       = MakeCost(iMemoryKiB, iIterations, iParallelism);

						INFO ( kFormat("variant {} m {} t {} p {} tag {}", static_cast<int>(variant), iMemoryKiB, iIterations, iParallelism, iTagLength) );

						auto sTag = KArgon2::Derive(variant, sPassword, sSalt, cost, iTagLength, sSecret, sData);

						CHECK ( sTag.size() == iTagLength );

#ifdef KARGON2_TESTS_WITH_OPENSSL
						auto sReference = OpenSSLDerive(variant, sPassword, sSalt, cost, iTagLength, sSecret, sData);

						if (!sReference.empty())
						{
							CHECK ( sTag == sReference );
						}
#endif

						++iCompared;
					}
				}
			}
		}

		CHECK ( iCompared > 100 );
	}

	SECTION("limits of Derive()")
	{
		auto cost = MakeCost(64, 1, 1);

		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", cost).size() == 32 );
		// an empty password is a password
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "", "saltsalt", cost).size() == 32 );
		// the salt has at least 8 bytes, the tag at least 4
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsal",  cost).empty() );
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", cost, 3).empty() );
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", cost, 4).size() == 4 );
		// at least 8 KiB per lane, one pass, one lane
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", MakeCost( 7, 1, 1)).empty() );
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", MakeCost(15, 1, 2)).empty() );
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", MakeCost(64, 0, 1)).empty() );
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", MakeCost(64, 1, 0)).empty() );
		// more lanes than Argon2 allows
		CHECK ( KArgon2::Derive(KArgon2::Variant::Argon2id, "password", "saltsalt", MakeCost(0xffffffff, 1, 0x01000000)).empty() );
	}

	SECTION("hash strings of the reference implementation")
	{
		KArgon2 Argon2;

		// from the tests of the reference implementation
		CHECK ( Argon2.ValidatePassword("password", "$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4") );
		CHECK ( Argon2.ValidatePassword("password", "$argon2id$v=19$m=256,t=2,p=2$c29tZXNhbHQ$bQk8UB/VmZZF4Oo79iDXuL5/0ttZwg2f/5U52iv1cDc") );
		CHECK ( Argon2.ValidatePassword("password", "$argon2i$v=19$m=256,t=2,p=1$c29tZXNhbHQ$iekCn0Y3spW+sCcFanM2xBT63UP2sghkUoHLIUpWRS8") );

		CHECK ( Argon2.ValidatePassword("Password", "$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4") == false );
		CHECK ( Argon2.HasError() == false );
	}

	SECTION("generate and validate")
	{
		// a small cost, as the computation is slow in a debug build
		KArgon2 Argon2(MakeCost(256, 2, 1));

		auto sHash = Argon2.GenerateHash("Secret123!");

		CHECK ( sHash.starts_with("$argon2id$v=19$m=256,t=2,p=1$") );
		// 16 bytes of salt and 32 bytes of tag in base64 without padding
		CHECK ( sHash.size() == 29 + 22 + 1 + 43 );
		CHECK ( sHash.contains('=') == true );
		CHECK ( sHash.substr(29).contains('=') == false );

		CHECK ( Argon2.ValidatePassword("Secret123!", sHash) );
		CHECK ( Argon2.ValidatePassword("Secret123",  sHash) == false );
		CHECK ( Argon2.ValidatePassword("",           sHash) == false );

		// every hash has its own salt
		auto sSecond = Argon2.GenerateHash("Secret123!");
		CHECK ( sSecond != sHash );
		CHECK ( Argon2.ValidatePassword("Secret123!", sSecond) );

		// an object with another cost validates the hash with the cost of the hash
		KArgon2 Other(MakeCost(512, 1, 2));
		CHECK ( Other.ValidatePassword("Secret123!", sHash) );

		// the whole password counts - bcrypt stops after 72 bytes
		KString sLong(100, 'x');
		auto sLongHash = Argon2.GenerateHash(sLong);
		CHECK ( Argon2.ValidatePassword(sLong, sLongHash) );
		CHECK ( Argon2.ValidatePassword(sLong.substr(0, 99), sLongHash) == false );
		CHECK ( Argon2.ValidatePassword(sLong.substr(0, 72), sLongHash) == false );

		// also behind a zero byte
		KString sBinary("ab\0cd", 5);
		auto sBinaryHash = Argon2.GenerateHash(sBinary);
		CHECK ( Argon2.ValidatePassword(sBinary, sBinaryHash) );
		CHECK ( Argon2.ValidatePassword("ab",    sBinaryHash) == false );

		// an empty password
		auto sEmptyHash = Argon2.GenerateHash("");
		CHECK ( Argon2.ValidatePassword("",  sEmptyHash) );
		CHECK ( Argon2.ValidatePassword(" ", sEmptyHash) == false );
	}

	SECTION("the default cost")
	{
		KArgon2 Argon2;

		CHECK ( Argon2.GetCost().iMemoryKiB   == 19 * 1024 );
		CHECK ( Argon2.GetCost().iIterations  == 2 );
		CHECK ( Argon2.GetCost().iParallelism == 1 );

		// one hash only, as the computation is slow in a debug build
		auto sHash = Argon2.GenerateHash("Secret123!");
		CHECK ( sHash.starts_with("$argon2id$v=19$m=19456,t=2,p=1$") );
		CHECK ( sHash.size() == 31 + 22 + 1 + 43 );
	}

	SECTION("invalid hash strings")
	{
		KArgon2 Argon2(MakeCost(64, 1, 1));

		for (KStringView sHash : {
			"",
			"argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2x$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			// the first version of Argon2, with and without its version field
			"$argon2id$v=16$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			// parameters
			"$argon2id$v=19$m=256,t=2$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=x$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=-1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=+1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=99999999999$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=1,x=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			// outside of the limits of Argon2
			"$argon2id$v=19$m=4,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=0,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			// salt and tag
			"$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ",
			"$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$",
			"$argon2id$v=19$m=256,t=2,p=1$$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=1$c29t$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			"$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4$more",
			"$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65 EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4",
			// a hash of bcrypt
			"$2b$08$Ou6m0FwoGd6vhGCnIDhDnuO3wWKiWg6CZxKBiNEPpYQJgk2UoqEnS" })
		{
			INFO ( sHash );
			KArgon2 Fresh(MakeCost(64, 1, 1));
			CHECK ( Fresh.ValidatePassword("password", sHash) == false );
			CHECK ( Fresh.HasError() );
			CHECK ( Fresh.NeedsRehash(sHash) );
		}
	}

	SECTION("NeedsRehash")
	{
		KArgon2 Argon2(MakeCost(256, 2, 1));
		auto sHash = Argon2.GenerateHash("Secret123!");

		CHECK ( Argon2.NeedsRehash(sHash) == false );
		// a hash of a higher cost is good as well
		CHECK ( KArgon2(MakeCost(128, 1, 1)).NeedsRehash(sHash) == false );
		// more memory, passes or lanes are asked for now
		CHECK ( KArgon2(MakeCost(257, 2, 1)).NeedsRehash(sHash) );
		CHECK ( KArgon2(MakeCost(256, 3, 1)).NeedsRehash(sHash) );
		CHECK ( KArgon2(MakeCost(256, 2, 2)).NeedsRehash(sHash) );
		// another variant
		CHECK ( Argon2.NeedsRehash("$argon2i$v=19$m=256,t=2,p=1$c29tZXNhbHQ$iekCn0Y3spW+sCcFanM2xBT63UP2sghkUoHLIUpWRS8") );
		// a short salt
		CHECK ( Argon2.NeedsRehash("$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4") );
		// what bcrypt generated
		CHECK ( Argon2.NeedsRehash(KBCrypt(static_cast<uint16_t>(4)).GenerateHash("Secret123!")) );
	}

	SECTION("cost")
	{
		KArgon2 Argon2;

		CHECK ( Argon2.SetCost(MakeCost(64, 1, 1)) );
		CHECK ( Argon2.GetCost().iMemoryKiB == 64 );

		// at least 8 KiB per lane, one pass and one lane - the cost stays as it was
		CHECK ( Argon2.SetCost(MakeCost( 7, 1, 1)) == false );
		CHECK ( Argon2.SetCost(MakeCost(64, 0, 1)) == false );
		CHECK ( Argon2.SetCost(MakeCost(64, 1, 0)) == false );
		CHECK ( Argon2.SetCost(MakeCost(64, 1, 9)) == false );
		CHECK ( Argon2.GetCost().iMemoryKiB   == 64 );
		CHECK ( Argon2.GetCost().iIterations  == 1  );
		CHECK ( Argon2.GetCost().iParallelism == 1  );

		// by duration: the memory stays, the count of passes is computed
		KArgon2 Timed(chrono::milliseconds(20), 256);
		CHECK ( Timed.GetCostDuration() == chrono::milliseconds(20) );
		auto cost = Timed.GetCost();
		CHECK ( cost.iMemoryKiB   == 256 );
		CHECK ( cost.iIterations  >= 1   );
		CHECK ( cost.iParallelism == 1   );

		auto sHash = Timed.GenerateHash("Secret123!");
		CHECK ( sHash.starts_with(kFormat("$argon2id$v=19$m=256,t={},p=1$", cost.iIterations)) );
		CHECK ( Timed.ValidatePassword("Secret123!", sHash) );
	}
}
