#include "catch.hpp"

#include <dekaf2/time/duration/kcircuitbreaker.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace dekaf2;

namespace {

// a breaker that opens after the given count of failures, for 10 seconds, then for
// 20, and for 40 from then on - without a random share
KCircuitBreaker::Options BreakerOptions(uint32_t iFailures)
{
	KBackoff::Options OpenTime(chrono::seconds(10), chrono::seconds(40));
	OpenTime.dJitter = 0.0;

	return KCircuitBreaker::Options(iFailures, OpenTime);
}

// fails the given count of calls
void Fail(KCircuitBreaker& Breaker, uint32_t iCalls, KSteadyTime Now)
{
	for (uint32_t i = 0; i < iCalls; ++i)
	{
		auto Call = Breaker.Try(Now);

		if (Call)
		{
			Call.Failure(Now);
		}
	}
}

} // end of anonymous namespace

TEST_CASE("KCircuitBreaker")
{
	using State = KCircuitBreaker::State;

	KSteadyTime tStart = KSteadyTime::now();

	auto After = [&tStart](int iSeconds) -> KSteadyTime
	{
		return tStart + chrono::seconds(iSeconds);
	};

	SECTION("defaults")
	{
		KCircuitBreaker Breaker;

		CHECK ( Breaker.GetState()                    == State::Closed       );
		CHECK ( Breaker.GetOptions().iFailures        == 5                   );
		CHECK ( Breaker.GetOptions().OpenTime.Initial == chrono::seconds(5)  );
		CHECK ( Breaker.GetOptions().OpenTime.Max     == chrono::minutes(1)  );
		CHECK ( Breaker.GetOpenings()                 == 0                   );
		CHECK ( Breaker.GetRefused()                  == 0                   );
		CHECK ( Breaker.GetOptions().sName.empty() );
		CHECK_FALSE ( Breaker.GetOpenUntil().ok() );

		KCircuitBreaker Named(KCircuitBreaker::Options(3, KBackoff::Options(chrono::seconds(1), chrono::seconds(2)), "payments"));

		CHECK ( Named.GetOptions().sName     == "payments" );
		CHECK ( Named.GetOptions().iFailures == 3          );
	}

	SECTION("a closed breaker lets the calls pass")
	{
		KCircuitBreaker Breaker(BreakerOptions(3));

		for (int i = 0; i < 100; ++i)
		{
			auto Call = Breaker.Try(tStart);
			CHECK ( Call );
			Call.Success();
			// the report does not change what the call was told
			CHECK ( Call );
		}

		CHECK ( Breaker.GetState() == State::Closed );
	}

	SECTION("failures in a row open it")
	{
		KCircuitBreaker Breaker(BreakerOptions(3));

		Fail(Breaker, 2, tStart);
		CHECK ( Breaker.GetState() == State::Closed );

		// a success in between starts the count again
		Breaker.Try(tStart).Success();
		Fail(Breaker, 2, tStart);
		CHECK ( Breaker.GetState() == State::Closed );

		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetState()     == State::Open );
		CHECK ( Breaker.GetOpenings()  == 1           );
		CHECK ( Breaker.GetOpenUntil() == After(10)   );

		// calls are refused for the open time
		CHECK_FALSE ( Breaker.Try(After(0)) );
		CHECK_FALSE ( Breaker.Try(After(9)) );
		CHECK ( Breaker.GetRefused() == 2           );
		CHECK ( Breaker.GetState()   == State::Open );
	}

	SECTION("a limit of zero or one opens with the first failure")
	{
		for (uint32_t iFailures : { 0, 1 })
		{
			KCircuitBreaker Breaker(BreakerOptions(iFailures));

			Fail(Breaker, 1, tStart);
			CHECK ( Breaker.GetState() == State::Open );
		}
	}

	SECTION("a probe that succeeds closes it")
	{
		KCircuitBreaker Breaker(BreakerOptions(1));

		Fail(Breaker, 1, tStart);
		REQUIRE ( Breaker.GetState() == State::Open );

		{
			auto Probe = Breaker.Try(After(10));
			CHECK ( Probe );
			CHECK ( Breaker.GetState() == State::HalfOpen );

			// all other calls are still refused
			CHECK_FALSE ( Breaker.Try(After(10)) );
			CHECK_FALSE ( Breaker.Try(After(100)) );

			Probe.Success();
		}

		CHECK ( Breaker.GetState()    == State::Closed );
		CHECK ( Breaker.GetOpenings() == 1             );
		CHECK_FALSE ( Breaker.GetOpenUntil().ok() );
		CHECK ( Breaker.Try(After(10)) );

		// the next time it opens, the open time starts with the initial one again
		Fail(Breaker, 1, After(100));
		CHECK ( Breaker.GetOpenings()  == 2          );
		CHECK ( Breaker.GetOpenUntil() == After(110) );
	}

	SECTION("a probe that fails keeps it open for a longer time")
	{
		KCircuitBreaker Breaker(BreakerOptions(1));

		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetOpenUntil() == After(10) );

		// the probe fails after the first open time: 20 seconds
		Fail(Breaker, 1, After(10));
		CHECK ( Breaker.GetState()     == State::Open );
		CHECK ( Breaker.GetOpenUntil() == After(30)   );
		CHECK_FALSE ( Breaker.Try(After(29)) );

		// then 40 seconds, the limit
		Fail(Breaker, 1, After(30));
		CHECK ( Breaker.GetOpenUntil() == After(70) );

		Fail(Breaker, 1, After(70));
		CHECK ( Breaker.GetOpenUntil() == After(110) );

		// a probe that failed is no new opening
		CHECK ( Breaker.GetOpenings() == 1 );
	}

	SECTION("a call without a report changes nothing")
	{
		KCircuitBreaker Breaker(BreakerOptions(2));

		// while closed: neither a failure nor a success
		Fail(Breaker, 1, tStart);

		for (int i = 0; i < 10; ++i)
		{
			auto Call = Breaker.Try(tStart);
			CHECK ( Call );
		}

		CHECK ( Breaker.GetState() == State::Closed );
		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetState() == State::Open );

		// a probe without a report leaves the breaker open, and the next call is the probe
		{
			auto Probe = Breaker.Try(After(10));
			CHECK ( Probe );
			CHECK ( Breaker.GetState() == State::HalfOpen );
		}

		CHECK ( Breaker.GetState()     == State::Open );
		CHECK ( Breaker.GetOpenUntil() == After(10)   );

		auto Probe = Breaker.Try(After(11));
		CHECK ( Probe );
		CHECK ( Breaker.GetState() == State::HalfOpen );
		Probe.Success();
		CHECK ( Breaker.GetState() == State::Closed );
	}

	SECTION("only the first report of a call counts")
	{
		KCircuitBreaker Breaker(BreakerOptions(2));

		auto Call = Breaker.Try(tStart);
		Call.Failure(tStart);
		Call.Failure(tStart);
		Call.Failure(tStart);
		CHECK ( Breaker.GetState() == State::Closed );

		// a success after the failure does not take it back
		Call.Success();
		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetState() == State::Open );
	}

	SECTION("a moved call reports once")
	{
		KCircuitBreaker Breaker(BreakerOptions(2));

		auto First = Breaker.Try(tStart);
		KCircuitBreaker::Call Second(std::move(First));
		CHECK ( Second );

		First.Failure(tStart);
		CHECK ( Breaker.GetState() == State::Closed );
		Second.Failure(tStart);
		First.Failure(tStart);
		CHECK ( Breaker.GetState() == State::Closed );

		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetState() == State::Open );
	}

	SECTION("a late report does not count")
	{
		KCircuitBreaker Breaker(BreakerOptions(2));

		// three calls start while the breaker is closed
		auto Slow1 = Breaker.Try(tStart);
		auto Slow2 = Breaker.Try(tStart);
		auto Slow3 = Breaker.Try(tStart);

		Fail(Breaker, 2, tStart);
		REQUIRE ( Breaker.GetState() == State::Open );

		// a late failure does not extend the open time, a late success does not close
		Slow1.Failure(After(5));
		Slow2.Success();
		CHECK ( Breaker.GetState()     == State::Open );
		CHECK ( Breaker.GetOpenUntil() == After(10)   );

		// and neither one decides for the probe
		auto Probe = Breaker.Try(After(10));
		REQUIRE ( Probe );
		Slow3.Failure(After(10));
		CHECK ( Breaker.GetState() == State::HalfOpen );
		Probe.Success();
		CHECK ( Breaker.GetState() == State::Closed );
	}

	SECTION("Reset")
	{
		KCircuitBreaker Breaker(BreakerOptions(2));

		Fail(Breaker, 2, tStart);
		REQUIRE ( Breaker.GetState() == State::Open );

		Breaker.Reset();
		CHECK ( Breaker.GetState() == State::Closed );
		CHECK_FALSE ( Breaker.GetOpenUntil().ok() );
		CHECK ( Breaker.Try(tStart) );

		// the failures before the reset are forgotten, also those of calls under way
		auto Before = Breaker.Try(tStart);
		Fail(Breaker, 1, tStart);
		Breaker.Reset();
		Before.Failure(tStart);
		Fail(Breaker, 1, tStart);
		CHECK ( Breaker.GetState() == State::Closed );

		// a probe that reports after the reset does not open it again
		Fail(Breaker, 1, tStart);
		REQUIRE ( Breaker.GetState() == State::Open );
		auto Probe = Breaker.Try(After(10));
		REQUIRE ( Probe );
		Breaker.Reset();
		Probe.Failure(After(10));
		CHECK ( Breaker.GetState() == State::Closed );
	}

	SECTION("one probe among many threads")
	{
		KCircuitBreaker Breaker(BreakerOptions(1));

		Fail(Breaker, 1, tStart);
		REQUIRE ( Breaker.GetState() == State::Open );

		constexpr std::size_t    iThreads { 16 };
		std::atomic<bool>        bStart   { false };
		std::atomic<bool>        bEnd     { false };
		std::atomic<std::size_t> iAsked   { 0 };
		std::atomic<std::size_t> iAllowed { 0 };
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < iThreads; ++i)
		{
			Threads.emplace_back([&]()
			{
				while (!bStart.load())
				{
					std::this_thread::yield();
				}

				// all threads ask after the open time, and keep their call until the end
				auto Call = Breaker.Try(After(10));

				if (Call)
				{
					++iAllowed;
				}

				++iAsked;

				while (!bEnd.load())
				{
					std::this_thread::yield();
				}

				Call.Success();
			});
		}

		bStart = true;

		while (iAsked.load() < iThreads)
		{
			std::this_thread::yield();
		}

		CHECK ( iAllowed.load()      == 1               );
		CHECK ( Breaker.GetState()   == State::HalfOpen );
		CHECK ( Breaker.GetRefused() == iThreads - 1    );

		bEnd = true;

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		CHECK ( Breaker.GetState() == State::Closed );
	}

	SECTION("calls from many threads while the service fails and recovers")
	{
		// opens after 3 failures, for a time of zero: every call after that is a probe
		KBackoff::Options OpenTime(KDuration::zero(), KDuration::zero());
		KCircuitBreaker Breaker(KCircuitBreaker::Options(3, OpenTime));

		constexpr std::size_t    iThreads { 8 };
		constexpr std::size_t    iCalls   { 2000 };
		std::atomic<bool>        bFailing { true };
		std::atomic<std::size_t> iDone    { 0 };
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < iThreads; ++i)
		{
			Threads.emplace_back([&]()
			{
				for (std::size_t iCall = 0; iCall < iCalls; ++iCall)
				{
					auto Call = Breaker.Try();

					if (Call)
					{
						if (bFailing.load())
						{
							Call.Failure();
						}
						else
						{
							Call.Success();
						}
					}
				}

				++iDone;
			});
		}

		// let the threads run against the failing service for a while, then let it recover
		while (Breaker.GetOpenings() == 0 && iDone.load() < iThreads)
		{
			std::this_thread::yield();
		}

		bFailing = false;

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		// one more call settles a probe that a thread may have failed at its very end
		Breaker.Try().Success();
		Breaker.Try().Success();

		CHECK ( Breaker.GetOpenings() >= 1             );
		CHECK ( Breaker.GetState()    == State::Closed );
	}
}
