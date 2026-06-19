#include "catch.hpp"

#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/core/strings/kstring.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace dekaf2;

namespace {

// spin until pred() is true or a timeout elapses (keeps the fair-scheduling tests non-flaky)
template<typename Pred>
bool WaitUntil(Pred pred, int iMaxMillis = 5000)
{
	for (int i = 0; i < iMaxMillis && !pred(); i += 5)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return pred();
}

} // anonymous namespace

TEST_CASE("KThreadPool")
{
	SECTION("basics")
	{
		KThreadPool Queue(10);

		struct MyClass
		{
			MyClass(int i) : m_i(i) {}
			int Add(int i) const { return m_i + i; }
			void Inc(int i) { m_i += i; }
			void Dec() { --m_i; }
			std::atomic<int> m_i;
		};

		int i1 = 0;
		int i2 = 0;

		auto future1 = Queue.push([&i1]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			i1 = 1;
		});

		MyClass C(10);

		auto future3 = Queue.push(&MyClass::Inc, &C, 3);
		auto future4 = Queue.push(&MyClass::Dec, &C);

		auto w1 = future1.wait_for(std::chrono::seconds(2));
		auto w3 = future3.wait_for(std::chrono::seconds(2));
		auto w4 = future4.wait_for(std::chrono::seconds(2));

		CHECK ( w1 == std::future_status::ready   );
		CHECK ( w3 == std::future_status::ready   );
		CHECK ( w4 == std::future_status::ready   );

		CHECK ( i1 == 1 );
		CHECK ( future1.valid() );

		CHECK ( Queue.resize(5) );

#ifndef DEKAF2_IS_WINDOWS
		// VS 2017 has issues compiling this - it is clearly
		// not an implementation problem of dekaf2, therefore
		// we simply drop the test for windows..
		auto future2 = Queue.push([&i2]() -> KString
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			i2 = 2;
			return "done";
		});

		auto w2 = future2.wait_for(std::chrono::seconds(2));
		auto future5 = Queue.push(&MyClass::Add, &C, 5);
		auto w5 = future5.wait_for(std::chrono::seconds(2));

		CHECK ( w2 == std::future_status::ready   );
		CHECK ( w5 == std::future_status::ready   );
		CHECK ( i2 == 2 );
		CHECK ( future2.valid() );
		CHECK ( future5.valid() );
		CHECK ( future2.get() == "done" );
		CHECK ( future5.get() == 17 );
#endif
	}

	SECTION("forced stop")
	{
		KThreadPool Queue(10);

		std::atomic<int> iCounter { 0 };

		for (int i = 0; i < 1000; ++i)
		{
			Queue.push([&iCounter]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				++iCounter;
			});
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Queue.resize(2);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Queue.stop(true);

		CHECK ( Queue.n_queued() > 0 );
		CHECK ( iCounter < 1000 );

		Queue.clear();
		CHECK ( Queue.n_queued() == 0 );
	}

	SECTION("stepwise growth and shrink")
	{
		std::atomic<int> iCounter { 0 };

		{
			KThreadPool Queue(50, KThreadPool::PrestartSome, KThreadPool::ShrinkSome);

			for (int i = 0; i < 1000; ++i)
			{
				Queue.push([&iCounter]()
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					++iCounter;
				});
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(150));

			Queue.push([&iCounter](){ ++iCounter; });

			CHECK ( Queue.size() == 50 );
		}

		CHECK ( iCounter == 1001 );
	}

	SECTION("stop(false) drains queue completely")
	{
		// regression test: push followed by immediate stop(false) must not lose tasks
		for (int iteration = 0; iteration < 50; ++iteration)
		{
			std::atomic<int> iCounter { 0 };

			KThreadPool Pool(4, KThreadPool::PrestartSome, KThreadPool::ShrinkNever);

			// push a single task and immediately stop
			Pool.push([&iCounter]() { ++iCounter; });
			Pool.stop(false);

			CHECK ( iCounter == 1 );
		}
	}

	SECTION("stop(false) drains multiple tasks")
	{
		std::atomic<int> iCounter { 0 };

		KThreadPool Pool(4, KThreadPool::PrestartSome, KThreadPool::ShrinkNever);

		for (int i = 0; i < 100; ++i)
		{
			Pool.push([&iCounter]() { ++iCounter; });
		}

		Pool.stop(false);

		CHECK ( iCounter == 100 );
	}

	SECTION("stop race stress")
	{
		// regression test for a lost thread wakeup: stop() used to set the abort and
		// interrupt flags without holding the condition variable's mutex, so a worker
		// could evaluate the wait predicate to false, miss the notify_all(), and sleep
		// forever - hanging stop() in join(). The race window opens when workers are
		// idle (or about to go idle) at the moment stop() is called, so we cycle pools
		// with mostly idle threads through immediate stops
		for (int iteration = 0; iteration < 100; ++iteration)
		{
			std::atomic<int> iCounter { 0 };

			KThreadPool Pool(4, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

			// one task for four threads - three stay idle, the fourth races
			// stop() on its way back into the idle wait
			Pool.push([&iCounter]() { ++iCounter; });

			Pool.stop(iteration % 2 == 0); // alternate kill and drain

			if (iteration % 2 != 0)
			{
				CHECK ( iCounter == 1 ); // drained stop must have run the task
			}
		}
	}

	SECTION("resize shrink does not lose idle threads")
	{
		// regression test for the same lost wakeup through resize(): shrink() sets
		// eAbort::Resize on (typically idle) threads and detaches them - if the wakeup
		// is lost, the detached thread never finishes, ma_iDetachedThreadsToFinish
		// never drops to zero, and the next stop() hangs in the detached wait
		for (int iteration = 0; iteration < 50; ++iteration)
		{
			KThreadPool Pool(8, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

			// all eight threads are idle - force-shrink six of them away
			Pool.resize(2);

			Pool.stop(false); // must not hang waiting for the detached threads

			CHECK ( Pool.is_stopped() );
		}
	}
}

TEST_CASE("KThreadPool fair scheduling")
{
	SECTION("weighted DRR dispatch order")
	{
		// one worker so the pop order is exactly the scheduler's dispatch order
		KThreadPool Pool(1);

		KThreadPool::TagConfig HeavyCfg; HeavyCfg.iWeight = 3;
		KThreadPool::TagConfig LightCfg; LightCfg.iWeight = 1;
		Pool.ConfigureTag(1, HeavyCfg);
		Pool.ConfigureTag(2, LightCfg);

		std::vector<int> Order;
		std::mutex       OrderMutex;

		// fill both tags while paused, so all tasks are queued before any runs
		Pool.pause(true);

		for (int i = 0; i < 6; ++i)
		{
			Pool.PushTagged(1, [&]() { std::lock_guard<std::mutex> L(OrderMutex); Order.push_back(1); });
		}
		for (int i = 0; i < 6; ++i)
		{
			Pool.PushTagged(2, [&]() { std::lock_guard<std::mutex> L(OrderMutex); Order.push_back(2); });
		}

		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { std::lock_guard<std::mutex> L(OrderMutex); return Order.size() == 12; }) );

		std::lock_guard<std::mutex> L(OrderMutex);

		auto iOnes = std::count(Order.begin(), Order.end(), 1);
		auto iTwos = std::count(Order.begin(), Order.end(), 2);
		CHECK ( iOnes == 6 );
		CHECK ( iTwos == 6 );

		// while both tags are active, weight 3:1 must hold - the first 4 dispatched are 3x tag1, 1x tag2
		auto iOnesFirst4 = std::count(Order.begin(), Order.begin() + 4, 1);
		CHECK ( iOnesFirst4 == 3 );
	}

	SECTION("bounded queue, Reject -> broken futures")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxQueue      = 2;
		Cfg.OverflowPolicy = KThreadPool::Overflow::Reject;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int> iRan { 0 };
		std::vector<std::future<void>> Futures;

		Pool.pause(true);
		for (int i = 0; i < 5; ++i)
		{
			Futures.push_back(Pool.PushTagged(1, [&]() { ++iRan; }));
		}
		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { return iRan.load() == 2; }) );

		int iCompleted { 0 };
		int iRejected  { 0 };

		for (auto& f : Futures)
		{
			try { f.get(); ++iCompleted; }
			catch (const std::future_error&) { ++iRejected; }
		}

		CHECK ( iCompleted == 2 );   // only two fit the bounded queue
		CHECK ( iRejected  == 3 );   // the rest were rejected -> broken futures
		CHECK ( iRan.load() == 2 );
	}

	SECTION("bounded queue, DropOldest")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxQueue      = 2;
		Cfg.OverflowPolicy = KThreadPool::Overflow::DropOldest;
		Pool.ConfigureTag(1, Cfg);

		std::vector<int> Ran;
		std::mutex       RanMutex;

		Pool.pause(true);
		// push ids 1..4; with DropOldest and depth 2, ids 1 and 2 get dropped, 3 and 4 survive
		for (int id = 1; id <= 4; ++id)
		{
			Pool.PushTagged(1, [&, id]() { std::lock_guard<std::mutex> L(RanMutex); Ran.push_back(id); });
		}
		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { std::lock_guard<std::mutex> L(RanMutex); return Ran.size() == 2; }) );

		std::lock_guard<std::mutex> L(RanMutex);
		CHECK ( std::find(Ran.begin(), Ran.end(), 3) != Ran.end() );
		CHECK ( std::find(Ran.begin(), Ran.end(), 4) != Ran.end() );
		CHECK ( std::find(Ran.begin(), Ran.end(), 1) == Ran.end() );
		CHECK ( std::find(Ran.begin(), Ran.end(), 2) == Ran.end() );
	}

	SECTION("PushTagged returns the result via future")
	{
		KThreadPool Pool(2);

		auto future = Pool.PushTagged(7, [](int a, int b) { return a + b; }, 20, 22);
		CHECK ( future.get() == 42 );
	}

	SECTION("per-tag concurrency cap")
	{
		// plenty of workers, but the tag may only run 2 of its tasks at once
		KThreadPool Pool(8, KThreadPool::PrestartAll);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxConcurrency = 2;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int> iConcurrent    { 0 };
		std::atomic<int> iMaxConcurrent { 0 };
		std::atomic<int> iDone          { 0 };

		for (int i = 0; i < 10; ++i)
		{
			Pool.PushTagged(1, [&]()
			{
				int iCur = ++iConcurrent;

				// record the high-water mark of concurrent tasks for this tag
				int iPrev = iMaxConcurrent.load();
				while (iCur > iPrev && !iMaxConcurrent.compare_exchange_weak(iPrev, iCur)) {}

				std::this_thread::sleep_for(std::chrono::milliseconds(40));  // hold the slot

				--iConcurrent;
				++iDone;
			});
		}

		REQUIRE ( WaitUntil([&]() { return iDone.load() == 10; }, 10000) );
		CHECK   ( iMaxConcurrent.load() <= 2 );

		// an uncapped tag must still run freely in parallel alongside the capped one
		std::atomic<int> iOther { 0 };
		std::vector<std::future<void>> Others;
		for (int i = 0; i < 5; ++i)
		{
			Others.push_back(Pool.PushTagged(2, [&]() { ++iOther; }));
		}
		for (auto& f : Others) { f.get(); }
		CHECK ( iOther.load() == 5 );
	}

	SECTION("coalesced debounce-rerun")
	{
		KThreadPool Pool(2);

		std::atomic<int>  iRuns    { 0 };
		std::atomic<bool> bStarted { false };
		std::atomic<bool> bRelease { false };

		auto Work = [&]()
		{
			++iRuns;
			bStarted = true;
			// the first run blocks so we can pile up submissions while it executes;
			// the rerun does not block (bRelease is already set by then)
			while (!bRelease.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
		};

		// first submission schedules a run for key 1
		auto f1 = Pool.PushCoalesced(1, 1, Work);

		REQUIRE ( WaitUntil([&]() { return bStarted.load(); }) );

		// while that run is executing, five more submissions for the same key must collapse
		// into exactly one follow-up run (debounce)
		std::vector<std::shared_future<void>> Fs;
		for (int i = 0; i < 5; ++i)
		{
			Fs.push_back(Pool.PushCoalesced(1, 1, Work));
		}

		bRelease = true;

		f1.get();
		for (auto& f : Fs) { f.get(); }

		// the original run plus exactly one coalesced rerun = 2, never 6
		CHECK ( iRuns.load() == 2 );
	}

	SECTION("coalesce keys are independent")
	{
		KThreadPool Pool(4);

		std::atomic<int> iKeyA { 0 };
		std::atomic<int> iKeyB { 0 };

		auto fa = Pool.PushCoalesced(1, 100, [&]() { ++iKeyA; });
		auto fb = Pool.PushCoalesced(1, 200, [&]() { ++iKeyB; });

		fa.get();
		fb.get();

		CHECK ( iKeyA.load() == 1 );
		CHECK ( iKeyB.load() == 1 );
	}

	SECTION("member functions and void / non-void returns")
	{
		struct Worker
		{
			std::atomic<int> iCalls { 0 };
			void Bump()             { ++iCalls; }                 // void member
			int  Add(int a, int b)  { ++iCalls; return a + b; }   // non-void member
		};

		Worker W;
		KThreadPool Pool(2);

		// member function, void return -> takes the single-packaged_task void overload
		auto fv = Pool.PushTagged(1, &Worker::Bump, &W);
		fv.get();

		// member function, non-void return -> result delivered through the future
		auto fr = Pool.PushTagged(1, &Worker::Add, &W, 40, 2);
		CHECK ( fr.get() == 42 );

		// member function via PushCoalesced (it std::binds, so member pointers just work)
		auto fc = Pool.PushCoalesced(2, 99, &Worker::Bump, &W);
		fc.get();

		// a plain void lambda likewise takes the void overload
		std::atomic<bool> bRan { false };
		Pool.PushTagged(1, [&]() { bRan = true; }).get();

		CHECK ( W.iCalls.load() == 3 );
		CHECK ( bRan.load()     == true );
	}
}
