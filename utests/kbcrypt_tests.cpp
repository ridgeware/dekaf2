#include "catch.hpp"
#include <dekaf2/kbcrypt.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KBCrypt")
{
	SECTION("Construct 50ms")
	{
		KBCrypt BCrypt(chrono::milliseconds(50));
		auto i = BCrypt.GetWorkload();
		CHECK ( i >= 4  );
		CHECK ( i <= 31 );

		KStringViewZ sPassword = "Secret123!";
		KStopTime Timer;
		auto sHash = BCrypt.GenerateHash(sPassword);
		auto Elapsed = Timer.elapsed();

		CHECK ( sHash.size() == 60 );
		CHECK ( Elapsed < chrono::milliseconds(50 + 10));

		auto bValid = BCrypt.ValidatePassword(sPassword, sHash);
		CHECK ( bValid );
	}
}
