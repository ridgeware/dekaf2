#include "catch.hpp"

#include <dekaf2/time/duration/kbackoff.h>
#include <algorithm>
#include <limits>

using namespace dekaf2;

TEST_CASE("KBackoff")
{
	SECTION("without random share the time doubles up to the limit")
	{
		KBackoff::Options Options(chrono::seconds(1), chrono::seconds(10));
		Options.dJitter = 0.0;

		KBackoff Backoff(Options);

		CHECK ( Backoff.GetFailures() == 0 );
		CHECK ( Backoff.Next() == chrono::seconds(1)  );
		CHECK ( Backoff.Next() == chrono::seconds(2)  );
		CHECK ( Backoff.Next() == chrono::seconds(4)  );
		CHECK ( Backoff.Next() == chrono::seconds(8)  );
		CHECK ( Backoff.Next() == chrono::seconds(10) );
		CHECK ( Backoff.Next() == chrono::seconds(10) );
		CHECK ( Backoff.GetFailures() == 6 );

		Backoff.Reset();

		CHECK ( Backoff.GetFailures() == 0 );
		CHECK ( Backoff.Next() == chrono::seconds(1) );
	}

	SECTION("the time for a count of failed attempts")
	{
		KBackoff::Options Options(chrono::milliseconds(100), chrono::seconds(5));
		Options.dJitter = 0.0;
		Options.dFactor = 3.0;

		KBackoff Backoff(Options);

		CHECK ( Backoff.GetWait(0) == KDuration::zero() );
		CHECK ( Backoff.GetWait(1) == chrono::milliseconds(100)  );
		CHECK ( Backoff.GetWait(2) == chrono::milliseconds(300)  );
		CHECK ( Backoff.GetWait(3) == chrono::milliseconds(900)  );
		CHECK ( Backoff.GetWait(4) == chrono::milliseconds(2700) );
		CHECK ( Backoff.GetWait(5) == chrono::seconds(5) );
		// a count that would overflow any integer
		CHECK ( Backoff.GetWait(std::numeric_limits<uint32_t>::max()) == chrono::seconds(5) );
		// asking does not count
		CHECK ( Backoff.GetFailures() == 0 );
	}

	SECTION("a factor of one keeps the time")
	{
		KBackoff::Options Options(chrono::seconds(5), chrono::minutes(1));
		Options.dJitter = 0.0;
		Options.dFactor = 1.0;

		KBackoff Backoff(Options);

		CHECK ( Backoff.GetWait(1)    == chrono::seconds(5) );
		CHECK ( Backoff.GetWait(1000) == chrono::seconds(5) );
	}

	SECTION("an initial time above the limit is the limit")
	{
		KBackoff::Options Options(chrono::minutes(5), chrono::minutes(1));
		Options.dJitter = 0.0;

		KBackoff Backoff(Options);

		CHECK ( Backoff.Next() == chrono::minutes(1) );
	}

	SECTION("no wait after the first failed attempt")
	{
		KBackoff::Options Options(chrono::seconds(1), chrono::seconds(10));
		Options.dJitter         = 0.0;
		Options.bImmediateFirst = true;

		KBackoff Backoff(Options);

		CHECK ( Backoff.Next() == KDuration::zero()   );
		CHECK ( Backoff.Next() == chrono::seconds(1)  );
		CHECK ( Backoff.Next() == chrono::seconds(2)  );
		CHECK ( Backoff.Next() == chrono::seconds(4)  );
		CHECK ( Backoff.GetWait(0) == KDuration::zero() );
	}

	SECTION("the random share")
	{
		// with the default the time is between its half and the whole
		KBackoff Default(chrono::seconds(8), chrono::minutes(1));

		// with a share of one it is anywhere below the whole
		KBackoff::Options FullOptions(chrono::seconds(8), chrono::minutes(1));
		FullOptions.dJitter = 1.0;

		KBackoff Full(FullOptions);

		CHECK ( Default.GetMaxWait(1) == chrono::seconds(8)  );
		CHECK ( Default.GetMaxWait(2) == chrono::seconds(16) );

		KDuration DefaultMin { KDuration::max()  };
		KDuration DefaultMax { KDuration::zero() };
		KDuration FullMin    { KDuration::max()  };
		KDuration FullMax    { KDuration::zero() };

		for (int i = 0; i < 2000; ++i)
		{
			auto Wait = Default.GetWait(1);
			DefaultMin = std::min(DefaultMin, Wait);
			DefaultMax = std::max(DefaultMax, Wait);

			Wait    = Full.GetWait(1);
			FullMin = std::min(FullMin, Wait);
			FullMax = std::max(FullMax, Wait);
		}

		// never outside of the range
		CHECK ( DefaultMin >  chrono::seconds(4) );
		CHECK ( DefaultMax <= chrono::seconds(8) );
		CHECK ( FullMin    >= KDuration::zero()  );
		CHECK ( FullMax    <= chrono::seconds(8) );

		// and spread over it: 2000 draws that all fall into one tenth of the range do not happen
		CHECK ( DefaultMin < chrono::milliseconds(4400) );
		CHECK ( DefaultMax > chrono::milliseconds(7600) );
		CHECK ( FullMin    < chrono::milliseconds(800)  );
		CHECK ( FullMax    > chrono::milliseconds(7200) );
	}

	SECTION("options that are no numbers do no harm")
	{
		KBackoff::Options Options(chrono::seconds(1), chrono::seconds(10));
		Options.dFactor = std::numeric_limits<double>::quiet_NaN();
		Options.dJitter = std::numeric_limits<double>::infinity();

		KBackoff Backoff(Options);

		for (int i = 0; i < 100; ++i)
		{
			auto Wait = Backoff.Next();
			CHECK ( Wait >= KDuration::zero()   );
			CHECK ( Wait <= chrono::seconds(10) );
		}

		Options.dFactor = std::numeric_limits<double>::infinity();
		Options.dJitter = std::numeric_limits<double>::quiet_NaN();

		KBackoff Other(Options);

		for (int i = 0; i < 100; ++i)
		{
			auto Wait = Other.Next();
			CHECK ( Wait >= KDuration::zero()   );
			CHECK ( Wait <= chrono::seconds(10) );
		}
	}
}
