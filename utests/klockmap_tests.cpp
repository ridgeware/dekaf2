#include "catch.hpp"
#include <dekaf2/klockmap.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KLockMap")
{
	SECTION("Integer")
	{
		KLockMap<> Locks;

		{
			auto Lock1 = Locks.Create(1234);
			auto Lock2 = Locks.Create(4321);
		}
		{
			auto Lock1 = Locks.Create(4321);
		}
		{
			auto Lock1 = Locks.Create(1234);
		}
	}

	SECTION("KString")
	{
		KLockMap<KString> Locks;

		{
			auto Lock1 = Locks.Create("1234");
			CHECK ( Locks.WouldBlock("1234") == true );
			CHECK ( Locks.WouldBlock("4321") == false);
			auto Lock2 = Locks.Create("4321");
			CHECK ( Locks.WouldBlock("1234") == true );
			CHECK ( Locks.WouldBlock("4321") == true );
		}

		CHECK ( Locks.WouldBlock("1234") == false);
		CHECK ( Locks.WouldBlock("4321") == false);

		{
			auto Lock1 = Locks.Create("4321");
		}

		{
			auto Lock1 = Locks.Create("1234");
		}
	}
}
