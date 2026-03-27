#include "catch.hpp"

#include <dekaf2/kconnectionlimiter.h>
#include <dekaf2/kexception.h>

using namespace dekaf2;

TEST_CASE("KConnectionLimiter")
{
	SECTION("disabled by default")
	{
		KConnectionLimiter limiter;
		CHECK ( limiter.IsEnabled() == false );
		auto guard = limiter.Acquire("192.168.1.1");
		CHECK ( static_cast<bool>(guard) == false ); // empty guard from disabled limiter
		CHECK ( limiter.GetKeyCount() == 0 );
	}

	SECTION("enabled with limit")
	{
		KConnectionLimiter limiter(5);
		CHECK ( limiter.IsEnabled() == true );
	}

	SECTION("acquire and release")
	{
		KConnectionLimiter limiter(3);

		{
			auto g1 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g1) == true );
			CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );

			auto g2 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g2) == true );
			CHECK ( limiter.GetConnectionCount("10.0.0.1") == 2 );

			auto g3 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g3) == true );
			CHECK ( limiter.GetConnectionCount("10.0.0.1") == 3 );

			// 4th connection: over limit
			auto g4 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g4) == false );
			CHECK ( KStringView(limiter.GetLastError()).contains("connection limit exceeded") );
			// count should still be 3, not 4
			CHECK ( limiter.GetConnectionCount("10.0.0.1") == 3 );
		}

		// all guards destroyed: count should be 0, key removed
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 0 );
		CHECK ( limiter.GetKeyCount() == 0 );
	}

	SECTION("different keys are independent")
	{
		KConnectionLimiter limiter(2);

		auto g1 = limiter.Acquire("10.0.0.1");
		auto g2 = limiter.Acquire("10.0.0.2");
		auto g3 = limiter.Acquire("10.0.0.3");
		CHECK ( static_cast<bool>(g1) == true );
		CHECK ( static_cast<bool>(g2) == true );
		CHECK ( static_cast<bool>(g3) == true );
		CHECK ( limiter.GetKeyCount() == 3 );
		CHECK ( limiter.GetTotalConnections() == 3 );

		// second connection per key is still fine
		auto g4 = limiter.Acquire("10.0.0.1");
		CHECK ( static_cast<bool>(g4) == true );
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 2 );

		// third connection for same key: over limit
		auto g5 = limiter.Acquire("10.0.0.1");
		CHECK ( static_cast<bool>(g5) == false );

		// other keys still have room
		auto g6 = limiter.Acquire("10.0.0.2");
		CHECK ( static_cast<bool>(g6) == true );
	}

	SECTION("guard move semantics")
	{
		KConnectionLimiter limiter(2);

		auto g1 = limiter.Acquire("10.0.0.1");
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );

		// move construct
		auto g2 = std::move(g1);
		CHECK ( static_cast<bool>(g1) == false ); // moved-from is invalid
		CHECK ( static_cast<bool>(g2) == true );
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 ); // still 1, not 2

		// move assign
		KConnectionLimiter::Guard g3;
		g3 = std::move(g2);
		CHECK ( static_cast<bool>(g2) == false );
		CHECK ( static_cast<bool>(g3) == true );
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );
	}

	SECTION("guard destruction releases slot")
	{
		KConnectionLimiter limiter(1);

		{
			auto g1 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g1) == true );
			CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );

			// second one fails
			auto g2 = limiter.Acquire("10.0.0.1");
			CHECK ( static_cast<bool>(g2) == false );
		}

		// after destruction, slot is free again
		auto g3 = limiter.Acquire("10.0.0.1");
		CHECK ( static_cast<bool>(g3) == true );
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );
	}

	SECTION("SetThrowOnError throws KException")
	{
		KConnectionLimiter limiter(1);
		limiter.SetThrowOnError(true);

		auto g1 = limiter.Acquire("10.0.0.1");
		CHECK ( static_cast<bool>(g1) == true );
		CHECK_THROWS_AS ( limiter.Acquire("10.0.0.1"), const KException& );
		// count should still be 1 (failed acquire doesn't increment)
		CHECK ( limiter.GetConnectionCount("10.0.0.1") == 1 );
	}

	SECTION("GetTotalConnections")
	{
		KConnectionLimiter limiter(10);

		auto g1 = limiter.Acquire("10.0.0.1");
		auto g2 = limiter.Acquire("10.0.0.1");
		auto g3 = limiter.Acquire("10.0.0.2");
		CHECK ( limiter.GetTotalConnections() == 3 );
		CHECK ( limiter.GetKeyCount() == 2 );
	}
}
