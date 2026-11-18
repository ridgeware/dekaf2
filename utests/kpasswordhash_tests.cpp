#include "catch.hpp"
#include <dekaf2/crypto/auth/kpasswordhash.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

namespace {

// a small cost, as Argon2 is slow in a debug build
KArgon2::Cost SmallCost(uint32_t iMemoryKiB = 256, uint32_t iIterations = 2)
{
	KArgon2::Cost cost;
	cost.iMemoryKiB   = iMemoryKiB;
	cost.iIterations  = iIterations;
	cost.iParallelism = 1;
	return cost;
}

} // end of anonymous namespace

TEST_CASE("KPasswordHash")
{
	SECTION("algorithm of a hash")
	{
		CHECK ( KPasswordHash::GetAlgorithm("$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4") == KPasswordHash::Algorithm::Argon2 );
		CHECK ( KPasswordHash::GetAlgorithm("$argon2i$v=19$m=256,t=2,p=1$c29tZXNhbHQ$iekCn0Y3spW+sCcFanM2xBT63UP2sghkUoHLIUpWRS8")  == KPasswordHash::Algorithm::Argon2 );
		CHECK ( KPasswordHash::GetAlgorithm("$2a$04$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG") == KPasswordHash::Algorithm::BCrypt );
		CHECK ( KPasswordHash::GetAlgorithm("$2b$10$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG") == KPasswordHash::Algorithm::BCrypt );
		CHECK ( KPasswordHash::GetAlgorithm("$2y$10$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG") == KPasswordHash::Algorithm::BCrypt );

		CHECK ( KPasswordHash::GetAlgorithm("") == KPasswordHash::Algorithm::Unknown );
		CHECK ( KPasswordHash::GetAlgorithm("Secret123!") == KPasswordHash::Algorithm::Unknown );
		// a cut hash of bcrypt, and one of another scheme
		CHECK ( KPasswordHash::GetAlgorithm("$2b$10$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIb") == KPasswordHash::Algorithm::Unknown );
		CHECK ( KPasswordHash::GetAlgorithm("$2x$10$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG") == KPasswordHash::Algorithm::Unknown );
		CHECK ( KPasswordHash::GetAlgorithm("$6$rounds=5000$salt$hash") == KPasswordHash::Algorithm::Unknown );
	}

	SECTION("new hashes are Argon2id")
	{
		KPasswordHash Hasher;
		CHECK ( Hasher.GetAlgorithm() == KPasswordHash::Algorithm::Argon2 );
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost()) );

		auto sHash = Hasher.GenerateHash("Secret123!");

		CHECK ( sHash.starts_with("$argon2id$v=19$m=256,t=2,p=1$") );
		CHECK ( Hasher.ValidatePassword("Secret123!", sHash) );
		CHECK ( Hasher.ValidatePassword("Secret123",  sHash) == false );
		CHECK ( Hasher.HasError() == false );
		CHECK ( Hasher.NeedsRehash(sHash) == false );
	}

	SECTION("hashes of bcrypt validate")
	{
		auto sOld = KBCrypt(static_cast<uint16_t>(4)).GenerateHash("Secret123!");
		REQUIRE ( sOld.size() == 60 );

		KPasswordHash Hasher;
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost()) );

		CHECK ( Hasher.ValidatePassword("Secret123!", sOld) );
		CHECK ( Hasher.ValidatePassword("Secret123",  sOld) == false );
		CHECK ( Hasher.HasError() == false );
		CHECK ( Hasher.NeedsRehash(sOld) );
	}

	SECTION("a hash of bcrypt is replaced at the login")
	{
		auto sStored = KBCrypt(static_cast<uint16_t>(4)).GenerateHash("Secret123!");

		KPasswordHash Hasher;
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost()) );

		KString sNewHash = "stays not";

		// a wrong password replaces nothing
		CHECK ( Hasher.ValidatePassword("wrong", sStored, sNewHash) == false );
		CHECK ( sNewHash.empty() );

		CHECK ( Hasher.ValidatePassword("Secret123!", sStored, sNewHash) );
		CHECK ( sNewHash.starts_with("$argon2id$") );
		CHECK ( Hasher.ValidatePassword("Secret123!", sNewHash) );

		// the new hash stays at the next login
		sStored = sNewHash;
		CHECK ( Hasher.ValidatePassword("Secret123!", sStored, sNewHash) );
		CHECK ( sNewHash.empty() );

		// until the cost is raised
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost(512)) );
		CHECK ( Hasher.NeedsRehash(sStored) );
		CHECK ( Hasher.ValidatePassword("Secret123!", sStored, sNewHash) );
		CHECK ( sNewHash.starts_with("$argon2id$v=19$m=512,t=2,p=1$") );
	}

	SECTION("bcrypt for new hashes")
	{
		// for a system that shall understand Argon2 before it generates it
		KPasswordHash Hasher(KPasswordHash::Algorithm::BCrypt);
		CHECK ( Hasher.GetAlgorithm() == KPasswordHash::Algorithm::BCrypt );
		REQUIRE ( Hasher.BCrypt().SetWorkload(5, false) );

		auto sHash = Hasher.GenerateHash("Secret123!");

		CHECK ( sHash.size() == 60 );
		CHECK ( sHash.starts_with("$2b$05$") );
		CHECK ( Hasher.ValidatePassword("Secret123!", sHash) );
		CHECK ( Hasher.NeedsRehash(sHash) == false );

		// a lower workload is replaced, a higher one stays
		CHECK ( Hasher.NeedsRehash(KBCrypt(static_cast<uint16_t>(4)).GenerateHash("Secret123!")) );
		CHECK ( Hasher.NeedsRehash(KBCrypt(static_cast<uint16_t>(6)).GenerateHash("Secret123!")) == false );

		// a hash of Argon2 validates, and is not replaced by one of bcrypt
		KPasswordHash Other;
		REQUIRE ( Other.Argon2().SetCost(SmallCost()) );
		auto sArgon2 = Other.GenerateHash("Secret123!");

		KString sNewHash;
		CHECK ( Hasher.ValidatePassword("Secret123!", sArgon2, sNewHash) );
		CHECK ( sNewHash.empty() );
		CHECK ( Hasher.NeedsRehash(sArgon2) == false );

		CHECK ( Hasher.SetAlgorithm(KPasswordHash::Algorithm::Unknown) == false );
		CHECK ( Hasher.GetAlgorithm() == KPasswordHash::Algorithm::BCrypt );
		CHECK ( Hasher.SetAlgorithm(KPasswordHash::Algorithm::Argon2) );
		CHECK ( Hasher.NeedsRehash(sHash) );
	}

	SECTION("what is no hash")
	{
		KPasswordHash Hasher;
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost()) );

		for (KStringView sHash : { "", "Secret123!", "$6$rounds=5000$salt$hash", "$argon2id$v=19$m=256" })
		{
			INFO ( sHash );
			KPasswordHash Fresh;
			KString sNewHash = "stays not";
			CHECK ( Fresh.ValidatePassword("Secret123!", sHash, sNewHash) == false );
			CHECK ( Fresh.HasError() );
			CHECK ( sNewHash.empty() );
			CHECK ( Fresh.NeedsRehash(sHash) );
		}

		// an invalid hash before does not make a wrong password an error
		CHECK ( Hasher.ValidatePassword("Secret123!", "$argon2id$v=19$m=256") == false );
		auto sHash = Hasher.GenerateHash("Secret123!");
		Hasher.ClearError();
		CHECK ( Hasher.ValidatePassword("wrong", sHash) == false );
		CHECK ( Hasher.HasError() == false );
	}

	SECTION("cost by duration, and a login without a user")
	{
		KPasswordHash Hasher(chrono::milliseconds(20), KPasswordHash::Algorithm::Argon2, false);

		auto cost = Hasher.Argon2().GetCost();
		CHECK ( cost.iMemoryKiB  == KArgon2::iDefaultMemoryKiB );
		CHECK ( cost.iIterations >= 1 );

		KPasswordHash BCrypt(chrono::milliseconds(5), KPasswordHash::Algorithm::BCrypt, false);
		CHECK ( BCrypt.BCrypt().GetWorkload() >= 4 );

		// it only takes its time
		REQUIRE ( Hasher.Argon2().SetCost(SmallCost()) );
		Hasher.SpendValidationTime("Secret123!");
		BCrypt.SpendValidationTime("Secret123!");
		CHECK ( Hasher.HasError() == false );
		CHECK ( BCrypt.HasError() == false );
	}
}
