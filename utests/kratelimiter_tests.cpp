#include "catch.hpp"

#include <dekaf2/kratelimiter.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kduration.h>

using namespace dekaf2;

TEST_CASE("KRateLimiter")
{
	SECTION("disabled by default")
	{
		KRateLimiter limiter;
		CHECK ( limiter.IsEnabled() == false );
		// must return true when disabled
		CHECK ( limiter.Check("192.168.1.1") == true );
		CHECK ( limiter.GetBucketCount() == 0 );
	}

	SECTION("enabled with rate")
	{
		KRateLimiter limiter(10.0, 5);
		CHECK ( limiter.IsEnabled() == true );
	}

	SECTION("burst allows initial requests")
	{
		// 2 req/s, burst of 3
		KRateLimiter limiter(2.0, 3);
		auto tNow = KSteadyTime::now();

		// first request creates bucket with burst-1 = 2 tokens remaining
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		// consume remaining 2 tokens
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		// 4th request at same time: no tokens left
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );
		CHECK ( KStringView(limiter.GetLastError()).contains("rate limit exceeded") );
	}

	SECTION("tokens refill over time")
	{
		// 2 req/s, burst of 2
		KRateLimiter limiter(2.0, 2);
		auto tNow = KSteadyTime::now();

		// exhaust all tokens: first request gets burst-1=1, then consume it
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		// now exhausted
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );

		// advance 1 second: should refill 2 tokens (rate=2/s), capped at burst=2
		auto tLater = tNow + KDuration(chrono::seconds(1));
		CHECK ( limiter.Check("10.0.0.1", tLater) == true );
		CHECK ( limiter.Check("10.0.0.1", tLater) == true );
		// third at same time: exhausted again
		CHECK ( limiter.Check("10.0.0.1", tLater) == false );
	}

	SECTION("different keys are independent")
	{
		KRateLimiter limiter(1.0, 1);
		auto tNow = KSteadyTime::now();

		// each key gets its own burst
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		CHECK ( limiter.Check("10.0.0.2", tNow) == true );
		CHECK ( limiter.Check("10.0.0.3", tNow) == true );
		CHECK ( limiter.GetBucketCount() == 3 );

		// second request on same key at same time fails
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );
		// but other keys still have their own state
		CHECK ( limiter.Check("10.0.0.2", tNow) == false );
	}

	SECTION("error message on rate limit")
	{
		KRateLimiter limiter(1.0, 1);
		auto tNow = KSteadyTime::now();

		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );
		CHECK ( KStringView(limiter.GetLastError()).contains("rate limit exceeded") );
	}

	SECTION("SetThrowOnError throws KException")
	{
		KRateLimiter limiter(1.0, 1);
		limiter.SetThrowOnError(true);
		auto tNow = KSteadyTime::now();

		CHECK_NOTHROW ( limiter.Check("10.0.0.1", tNow) );
		CHECK_THROWS_AS ( limiter.Check("10.0.0.1", tNow), const KException& );
	}

	SECTION("cleanup removes stale buckets")
	{
		KRateLimiter limiter(10.0, 5);
		auto tNow = KSteadyTime::now();

		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		CHECK ( limiter.Check("10.0.0.2", tNow) == true );
		CHECK ( limiter.GetBucketCount() == 2 );

		// advance 2 minutes - well past the 60s stale timeout and 60s cleanup interval
		auto tLater = tNow + KDuration(chrono::seconds(120));
		limiter.Cleanup(tLater);
		CHECK ( limiter.GetBucketCount() == 0 );
	}

	SECTION("burst minimum is 1")
	{
		// burst=0 with rate>0 should be corrected to 1
		KRateLimiter limiter(5.0, 0);
		CHECK ( limiter.IsEnabled() == true );
		auto tNow = KSteadyTime::now();

		// should allow at least the initial request
		CHECK ( limiter.Check("10.0.0.1", tNow) == true );
		// but not a second one at the same time
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );
	}

	SECTION("half second refill")
	{
		// 10 req/s, burst of 1 -> one token every 100ms
		KRateLimiter limiter(10.0, 1);
		auto tNow = KSteadyTime::now();

		CHECK ( limiter.Check("10.0.0.1", tNow) == true  );
		CHECK ( limiter.Check("10.0.0.1", tNow) == false );

		// advance 500ms -> refill 5 tokens, capped at burst=1
		auto tLater = tNow + KDuration(chrono::milliseconds(500));
		CHECK ( limiter.Check("10.0.0.1", tLater) == true  );
		CHECK ( limiter.Check("10.0.0.1", tLater) == false );
	}
}
