
#include "catch.hpp"
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/dekaf2.h>
#include <atomic>
#include <future>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KTimer")
//-----------------------------------------------------------------------------
{
	SECTION("KTimer")
	{
		std::size_t iCalled = 0;

		KTimer Timer(chrono::milliseconds(50));

		auto ID = Timer.CallEvery(chrono::milliseconds(50), [&](KUnixTime tp)
		{
	//		kDebug(1, "the current timepoint is: {}", KTimer::ToTimeT(tp));
			++iCalled;

		}, false);

		kSleep(chrono::milliseconds(500));

		CHECK ( Timer.Cancel(ID) );
		CHECK ( iCalled >= 2 );
		CHECK ( iCalled <= 11 );
	}

	SECTION("the threads of two timers have different names")
	{
		KTimer Timer1(chrono::milliseconds(10));
		KTimer Timer2(chrono::milliseconds(10));

		std::promise<KString> Name1;
		std::promise<KString> Name2;

		// called in the thread of the timer
		Timer1.CallOnce(chrono::milliseconds(0), [&](KUnixTime) { Name1.set_value(kGetThreadName()); }, false);
		Timer2.CallOnce(chrono::milliseconds(0), [&](KUnixTime) { Name2.set_value(kGetThreadName()); }, false);

		auto Future1 = Name1.get_future();
		auto Future2 = Name2.get_future();

		REQUIRE ( Future1.wait_for(std::chrono::seconds(5)) == std::future_status::ready );
		REQUIRE ( Future2.wait_for(std::chrono::seconds(5)) == std::future_status::ready );

		auto sName1 = Future1.get();
		auto sName2 = Future2.get();

		// empty on a platform that cannot name its threads
		if (!sName1.empty())
		{
			CHECK ( sName1.starts_with("ktimer") );
			CHECK ( sName2.starts_with("ktimer") );
			CHECK ( sName1 != sName2 );
		}
	}

	SECTION("Cancel waits for a running callback")
	{
		KTimer Timer(chrono::milliseconds(10));

		std::atomic<bool> bInCallback { false };
		std::atomic<int>  iCalls      { 0     };

		auto ID = Timer.CallEvery(chrono::milliseconds(50), [&](KUnixTime)
		{
			bInCallback = true;
			++iCalls;
			kSleep(chrono::milliseconds(150));
			bInCallback = false;

		}, true);

		REQUIRE ( ID != KTimer::InvalidID );

		// wait until a callback is inside its sleep
		for (int i = 0; i < 500 && !bInCallback; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}
		REQUIRE ( bInCallback == true );

		CHECK ( Timer.Cancel(ID) );
		// Cancel drained the running callback before returning
		CHECK ( bInCallback == false );

		// and no callback runs anymore
		auto iAtCancel = iCalls.load();
		kSleep(chrono::milliseconds(250));
		CHECK ( iCalls == iAtCancel );
	}

	SECTION("Cancel from inside the callback does not deadlock")
	{
		KTimer Timer(chrono::milliseconds(10));

		std::atomic<int>          iCalls        { 0 };
		std::atomic<bool>         bCancelResult { false };
		std::atomic<KTimer::ID_t> ID            { KTimer::InvalidID };

		ID = Timer.CallEvery(chrono::milliseconds(30), [&](KUnixTime)
		{
			++iCalls;
			bCancelResult = Timer.Cancel(ID);

		}, true);

		REQUIRE ( ID != KTimer::InvalidID );

		for (int i = 0; i < 500 && iCalls == 0; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}
		REQUIRE ( iCalls >= 1 );
		CHECK   ( bCancelResult == true );

		// the self-cancel took effect: no further executions
		kSleep(chrono::milliseconds(200));
		CHECK ( iCalls == 1 );
	}

	SECTION("destructor waits for detached callbacks")
	{
		std::atomic<bool> bStarted { false };
		std::atomic<bool> bDone    { false };

		{
			KTimer Timer(chrono::milliseconds(10));

			Timer.CallOnce(chrono::milliseconds(0), [&](KUnixTime)
			{
				bStarted = true;
				kSleep(chrono::milliseconds(200));
				bDone = true;

			}, true);

			for (int i = 0; i < 500 && !bStarted; ++i)
			{
				kSleep(chrono::milliseconds(10));
			}
			REQUIRE ( bStarted == true );
		}

		// the destructor waited for the callback thread
		CHECK ( bDone == true );
	}
}
