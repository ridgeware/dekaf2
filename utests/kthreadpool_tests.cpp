#include "catch.hpp"

#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/core/strings/kstring.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>
#include <thread>
#include <algorithm>
#include <array>
#include <stdexcept>

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

struct PostTarget
{
	void Add(int i) { iSum += i; }

	std::atomic<int> iSum { 0 };
};

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

	SECTION("shutdown callback tells why a thread ends")
	{
		// a callback that took every ending thread for a stopping pool misread
		// the first shrink after a burst - the Reason tells them apart
		std::mutex Mutex;
		std::vector<KThreadPool::Abort> Reasons;

		auto Count = [&](KThreadPool::Abort Reason)
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			return std::count(Reasons.begin(), Reasons.end(), Reason);
		};

		{
			KThreadPool Pool;
			Pool.register_shutdown_callback([&](KThreadPool::Diagnostics Diag)
			{
				std::lock_guard<std::mutex> Lock(Mutex);
				Reasons.push_back(Diag.Reason);
			});

			CHECK ( Pool.resize(4) );
			CHECK ( Pool.resize(2) );

			// the two threads that go end on their own
			for (int i = 0; i < 200 && Count(KThreadPool::Abort::Resize) < 2; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			CHECK ( Count(KThreadPool::Abort::Resize) == 2 );
			CHECK ( Count(KThreadPool::Abort::None)   == 0 );

			Pool.stop(false);

			CHECK ( Count(KThreadPool::Abort::None)   == 2 );
			CHECK ( Count(KThreadPool::Abort::Kill)   == 0 );
		}

		{
			std::lock_guard<std::mutex> Lock(Mutex);
			Reasons.clear();
		}

		{
			KThreadPool Pool;
			Pool.register_shutdown_callback([&](KThreadPool::Diagnostics Diag)
			{
				std::lock_guard<std::mutex> Lock(Mutex);
				Reasons.push_back(Diag.Reason);
			});

			CHECK ( Pool.resize(3) );

			for (int i = 0; i < 30; ++i)
			{
				Pool.push([]() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
			}

			Pool.stop(true);

			CHECK ( Count(KThreadPool::Abort::Kill)   == 3 );
			CHECK ( Count(KThreadPool::Abort::Resize) == 0 );
			CHECK ( Count(KThreadPool::Abort::None)   == 0 );
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

	SECTION("a finished task releases its captured objects")
	{
		// The worker that ran a task must drop the task - and all it captured - right away,
		// not keep it until it runs its next task: on a quiet pool that may be hours later, and
		// a captured object can be a network connection with its open socket (seen in
		// production as a websocket in CLOSE_WAIT until the next request came in)
		KThreadPool Pool(1);

		auto pCaptured = std::make_shared<int>(42);
		std::weak_ptr<int> wCaptured = pCaptured;

		{
			auto Future = Pool.push([pCaptured]() {});
			pCaptured.reset();

			CHECK ( Future.wait_for(std::chrono::seconds(2)) == std::future_status::ready );

			// libstdc++ keeps the callable in the state the task shares with the future -
			// the captured objects can only go once the future is gone, too
		}

		// the future becomes ready inside the task call - the worker drops the task just after
		for (int iWait = 0; iWait < 100 && !wCaptured.expired(); ++iWait)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		CHECK ( wCaptured.expired() );
	}

	SECTION("a burst of pushes grows the pool for each task without a thread")
	{
		// The queue is paused, so no thread takes a task, and every push finds the
		// threads of the earlier ones either starting or idle. With PrestartNone the
		// pool must have exactly one thread per task afterwards - not fewer (tasks
		// left without a thread until a busy one finishes), and not more (a growth
		// that ignores the policy).
		constexpr std::size_t iTasks = 6;

		KThreadPool Pool(8, KThreadPool::PrestartNone, KThreadPool::ShrinkNever);
		Pool.pause(true);

		std::atomic<std::size_t> iRunning  { 0 };
		std::atomic<std::size_t> iTogether { 0 };

		for (std::size_t i = 0; i < iTasks; ++i)
		{
			Pool.push([&iRunning, &iTogether]()
			{
				// every task waits until all of them run at the same time
				++iRunning;
				auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(2);
				while (iRunning < iTasks && std::chrono::steady_clock::now() < tEnd)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				if (iRunning >= iTasks) ++iTogether;
			});
		}

		CHECK ( Pool.size() == iTasks );

		Pool.pause(false);
		Pool.stop(false);

		CHECK ( iTogether == iTasks );
	}

	SECTION("a burst of pushes keeps to the growth policy")
	{
		// PrestartSome starts ten idle threads on top of the needed ones - at the
		// most, a burst may leave ten threads more than tasks
		constexpr std::size_t iTasks = 20;

		KThreadPool Pool(50, KThreadPool::PrestartSome, KThreadPool::ShrinkNever);
		Pool.pause(true);

		std::atomic<std::size_t> iDone { 0 };

		for (std::size_t i = 0; i < iTasks; ++i)
		{
			Pool.push([&iDone]() { ++iDone; });
		}

		CHECK ( Pool.size() >= iTasks      );
		CHECK ( Pool.size() <= iTasks + 10 );

		Pool.pause(false);
		Pool.stop(false);

		CHECK ( iDone == iTasks );
	}

	SECTION("a capped tag grows the pool only for the tasks within its cap")
	{
		// A task of a tag at its concurrency cap waits for a free slot of its tag,
		// not for a thread. With the queue paused, ten tasks of a tag with a cap of
		// two need two threads, and every untagged task needs one more.
		KThreadPool Pool(32, KThreadPool::PrestartNone, KThreadPool::ShrinkNever);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxConcurrency = 2;
		Pool.ConfigureTag(1, Cfg);
		Pool.pause(true);

		std::atomic<std::size_t> iDone { 0 };

		for (int i = 0; i < 10; ++i)
		{
			Pool.PostTagged(1, [&iDone]() { ++iDone; });
		}

		CHECK ( Pool.size() == 2 );

		for (int i = 0; i < 3; ++i)
		{
			Pool.Post([&iDone]() { ++iDone; });
		}

		CHECK ( Pool.size() == 5 );

		Pool.pause(false);
		Pool.stop(false);

		CHECK ( iDone == 13 );
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

	SECTION("a shrink ends the idle threads, not the busy ones")
	{
		// Ten of twenty threads run a task when the pool shrinks by ten. The idle
		// threads have to end: a busy thread would go on with its task outside of the
		// pool, which could then grow back to its maximum size on top of it.
		constexpr std::size_t iBusy = 10;

		KThreadPool Pool(2 * iBusy, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

		std::atomic<bool>        bRelease { false };
		std::atomic<std::size_t> iRunning { 0 };
		std::atomic<std::size_t> iDone    { 0 };

		auto Task = [&bRelease, &iRunning, &iDone]()
		{
			++iRunning;

			while (!bRelease)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			++iDone;
		};

		for (std::size_t i = 0; i < iBusy; ++i)
		{
			Pool.push(Task);
		}

		CHECK ( WaitUntil([&]() { return iRunning == iBusy && Pool.n_idle() == iBusy; }) );

		// one thread less than the size forces the shrink, and ShrinkImmediately
		// then ends all idle threads
		Pool.resize(2 * iBusy - 1, KThreadPool::PrestartNone, KThreadPool::ShrinkImmediately);

		CHECK ( Pool.size()   == iBusy );
		CHECK ( Pool.n_idle() == 0     );

		// the threads that the shrink ended are gone, and all tasks still run
		CHECK ( WaitUntil([&]() { return Pool.get_diagnostics().iTotalThreads == iBusy; }) );
		CHECK ( iRunning == iBusy );
		CHECK ( iDone    == 0     );

		// back at the old maximum size, the pool has room for ten more tasks
		Pool.resize(2 * iBusy, KThreadPool::PrestartNone, KThreadPool::ShrinkNever);

		for (std::size_t i = 0; i < iBusy + 5; ++i)
		{
			Pool.push(Task);
		}

		CHECK ( WaitUntil([&]() { return iRunning == 2 * iBusy; }) );
		CHECK ( Pool.size()     == 2 * iBusy );
		CHECK ( Pool.n_queued() == 5         );
		CHECK ( iRunning        == 2 * iBusy );

		bRelease = true;
		Pool.stop(false);

		CHECK ( iDone == 2 * iBusy + 5 );
	}

	SECTION("a burst of pushes after a shrink grows the pool")
	{
		// The threads that a shrink ends have to leave the idle count with the shrink,
		// not when they wake up: until then a push would take them for threads that
		// take a waiting task, and not grow the pool.
		constexpr std::size_t iTasks = 30;

		KThreadPool Pool(40, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

		CHECK ( WaitUntil([&]() { return Pool.n_idle() == 40; }) );

		// no thread takes a task, as in the burst tests above
		Pool.pause(true);

		// one thread less than the size forces the shrink, and ShrinkSome then ends
		// all idle threads but ten
		Pool.resize(39, KThreadPool::PrestartNone, KThreadPool::ShrinkSome);

		CHECK ( Pool.size()   == 10 );
		CHECK ( Pool.n_idle() == 10 );

		std::atomic<std::size_t> iDone { 0 };

		for (std::size_t i = 0; i < iTasks; ++i)
		{
			Pool.push([&iDone]() { ++iDone; });
		}

		// ten tasks for the idle threads, and one new thread for each other task
		CHECK ( Pool.size() == iTasks );

		Pool.pause(false);
		Pool.stop(false);

		CHECK ( iDone == iTasks );
	}

	SECTION("restart keeps the maximum size and the policies")
	{
		KThreadPool Pool(20, KThreadPool::PrestartNone, KThreadPool::ShrinkNever);
		Pool.pause(true);

		std::atomic<std::size_t> iDone { 0 };

		for (std::size_t i = 0; i < 5; ++i)
		{
			Pool.push([&iDone]() { ++iDone; });
		}

		// the threads end, the tasks stay in the paused queue
		Pool.stop(true);

		CHECK ( Pool.size()     == 0 );
		CHECK ( Pool.n_queued() == 5 );

		CHECK ( Pool.restart() );

		// PrestartNone starts one thread for each waiting task
		CHECK ( Pool.size()                        == 5  );
		CHECK ( Pool.get_diagnostics().iMaxThreads == 20 );

		Pool.pause(false);
		Pool.stop(false);

		CHECK ( iDone == 5 );
	}

	SECTION("concurrent stops")
	{
		// Several threads stop the pool at the same time, as it happens when an
		// application shuts down. All calls have to return, with all tasks run.
		for (int iteration = 0; iteration < 50; ++iteration)
		{
			std::atomic<int> iCounter { 0 };

			KThreadPool Pool(8, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

			for (int i = 0; i < 20; ++i)
			{
				Pool.push([&iCounter]() { ++iCounter; });
			}

			std::vector<std::thread> Stoppers;

			for (int i = 0; i < 4; ++i)
			{
				Stoppers.emplace_back([&Pool]() { Pool.stop(false); });
			}

			for (auto& Stopper : Stoppers)
			{
				Stopper.join();
			}

			CHECK ( Pool.is_stopped() );
			CHECK ( iCounter == 20 );
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

	SECTION("coalesced run rejected by a bounded tag breaks its future")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxQueue      = 1;
		Cfg.OverflowPolicy = KThreadPool::Overflow::Reject;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int> iRan { 0 };

		Pool.pause(true);

		// fills the bounded queue (depth 1)
		auto fTagged = Pool.PushTagged(1, [&]() { ++iRan; });

		// the coalesce run cannot be scheduled - its future must break, not pend forever
		auto fCoalesced = Pool.PushCoalesced(1, 42, [&]() { ++iRan; });

		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { return iRan.load() == 1; }) );

		fTagged.get(); // the queued task completed normally

		bool bBroken { false };
		try { fCoalesced.get(); }
		catch (const std::future_error&) { bBroken = true; }
		CHECK ( bBroken );

		// the key's state was dropped - a later submission starts fresh and runs
		auto fRetry = Pool.PushCoalesced(1, 42, [&]() { ++iRan; });
		fRetry.get();
		CHECK ( iRan.load() == 2 );
	}

	SECTION("DropOldest eviction runs user destructors outside the pool mutex")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxQueue      = 1;
		Cfg.OverflowPolicy = KThreadPool::Overflow::DropOldest;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int> iDtorPushRan { 0 };
		std::atomic<int> iSurvivorRan { 0 };

		Pool.pause(true);

		{
			// a guard whose destructor pushes back into the pool - if the evicted task were
			// destroyed under the pool mutex, this push would deadlock
			std::shared_ptr<void> Guard(nullptr, [&Pool, &iDtorPushRan](void*)
			{
				Pool.push([&iDtorPushRan]() { ++iDtorPushRan; });
			});

			Pool.PushTagged(1, [Guard = std::move(Guard)]() {});
		}

		// evicts the guard-holding task -> its destructor pushes to DefaultTag
		Pool.PushTagged(1, [&]() { ++iSurvivorRan; });

		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { return iSurvivorRan.load() == 1 && iDtorPushRan.load() == 1; }) );
	}

	SECTION("capped tag keeps draining after a shrink aborts the finishing worker")
	{
		// regression for a lost wakeup: a worker aborted by a shrink right after finishing a
		// capped tag's task must still wake a waiting worker for the freed concurrency slot -
		// otherwise the tag's queued work stalls until the next push
		KThreadPool Pool(2, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxConcurrency = 1;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int>  iDone { 0 };
		std::atomic<bool> bFirstRunning { false };

		Pool.PushTagged(1, [&]()
		{
			bFirstRunning = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			++iDone;
		});

		REQUIRE ( WaitUntil([&]() { return bFirstRunning.load(); }) );

		for (int i = 0; i < 3; ++i)
		{
			Pool.PushTagged(1, [&]() { ++iDone; });
		}

		// shrink while the capped task runs - one of the two workers gets aborted (possibly
		// the one that is just finishing the capped task)
		Pool.resize(1);

		CHECK ( WaitUntil([&]() { return iDone.load() == 4; }) );
	}

	SECTION("a tag weight of 0 is clamped to 1 and dispatches normally")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iWeight = 0;
		Pool.ConfigureTag(1, Cfg);
		Pool.ConfigureTag(2, KThreadPool::TagConfig{});

		std::atomic<int> iRan { 0 };

		Pool.pause(true);
		for (int i = 0; i < 4; ++i)
		{
			Pool.PushTagged(1, [&]() { ++iRan; });
			Pool.PushTagged(2, [&]() { ++iRan; });
		}
		Pool.pause(false);

		REQUIRE ( WaitUntil([&]() { return iRan.load() == 8; }) );
	}
}

TEST_CASE("KThreadPool Post")
{
	SECTION("callables with and without arguments, in the order of the calls")
	{
		KThreadPool Pool(1);

		// only the one worker writes to Order
		std::vector<int> Order;
		PostTarget       Target;

		CHECK ( Pool.Post([&Order]() { Order.push_back(1); })         );
		CHECK ( Pool.Post([&Order](int i) { Order.push_back(i); }, 2) );
		CHECK ( Pool.Post(&PostTarget::Add, &Target, 5)               );

		// a move-only capture
		auto pValue = std::make_unique<int>(3);
		CHECK ( Pool.Post([&Order, pValue = std::move(pValue)]() { Order.push_back(*pValue); }) );

		// a capture larger than the storage of a task, which goes to the heap
		std::array<int, 64> Large {};
		Large[63] = 4;
		CHECK ( Pool.Post([&Order, Large]() { Order.push_back(Large[63]); }) );

		// the one worker runs the tasks in order - when this one is done, all are
		Pool.push([]() {}).get();

		CHECK ( Order == (std::vector<int>{ 1, 2, 3, 4 }) );
		CHECK ( Target.iSum == 5 );
	}

	SECTION("an exception does not stop the worker")
	{
		KThreadPool Pool(1);
		std::atomic<int> iRan { 0 };

		Pool.Post([]() { throw std::runtime_error("posted task throws"); });
		Pool.Post([&iRan]() { ++iRan; });

		CHECK ( WaitUntil([&]() { return iRan.load() == 1; }) );
	}

	SECTION("a finished task releases its captured objects")
	{
		KThreadPool Pool(1);

		auto pCaptured = std::make_shared<int>(42);
		std::weak_ptr<int> wCaptured = pCaptured;

		Pool.Post([pCaptured]() {});
		pCaptured.reset();

		CHECK ( WaitUntil([&]() { return wCaptured.expired(); }) );
	}

	SECTION("a bounded tag rejects")
	{
		KThreadPool Pool(1);

		KThreadPool::TagConfig Cfg;
		Cfg.iMaxQueue      = 2;
		Cfg.OverflowPolicy = KThreadPool::Overflow::Reject;
		Pool.ConfigureTag(1, Cfg);

		std::atomic<int> iRan { 0 };

		Pool.pause(true);
		CHECK       ( Pool.PostTagged(1, [&iRan]() { ++iRan; }) );
		CHECK       ( Pool.PostTagged(1, [&iRan]() { ++iRan; }) );
		CHECK_FALSE ( Pool.PostTagged(1, [&iRan]() { ++iRan; }) );
		Pool.pause(false);

		CHECK ( WaitUntil([&]() { return iRan.load() == 2; }) );
	}

	SECTION("an idle pool wakes up for a task")
	{
		KThreadPool Pool(2);
		std::atomic<int> iRan { 0 };

		for (int i = 1; i <= 3; ++i)
		{
			// let the workers become idle
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			Pool.Post([&iRan]() { ++iRan; });
			CHECK ( WaitUntil([&]() { return iRan.load() == i; }) );
		}
	}

	SECTION("all tasks of several producers run on several workers")
	{
		KThreadPool Pool(4);
		std::atomic<int> iRan { 0 };
		std::vector<std::thread> Producers;

		for (int p = 0; p < 4; ++p)
		{
			Producers.emplace_back([&Pool, &iRan]()
			{
				for (int i = 0; i < 10000; ++i)
				{
					Pool.Post([&iRan]() { ++iRan; });
				}
			});
		}

		for (auto& Producer : Producers)
		{
			Producer.join();
		}

		CHECK ( WaitUntil([&]() { return iRan.load() == 40000; }) );
	}
}
