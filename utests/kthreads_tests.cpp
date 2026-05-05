#include "catch.hpp"

#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>

using namespace dekaf2;

namespace {

void ThreadedIntFunction(int i)
{
}

void ThreadedFunction(KString sTest)
{
}

struct Foo
{
	void Bar(KString sTest)
	{
	}
};

}

TEST_CASE("KThreads")
{
	{
		KThreads Threads;

		auto id1 = Threads.Add(std::thread([]()
		{
			kSleep(chrono::milliseconds(50));
		}));

		KString sTest = "Hello";
		Foo F;

		auto id2 = Threads.Add(std::thread(&Foo::Bar, &F, sTest));

		CHECK ( Threads.empty() == false );
		CHECK ( Threads.size() == 2 );
		CHECK ( Threads.Join(id1) );
		CHECK ( Threads.empty() == false );
		CHECK ( Threads.size() == 1 );
		CHECK ( Threads.Join(id2) );
		CHECK ( Threads.empty() == true );
		CHECK ( Threads.size() == 0 );

		Threads.Create([]()
		{
			kSleep(chrono::milliseconds(50));
		});

		Threads.Create(&Foo::Bar, &F, sTest);

		Threads.Join();

		CHECK ( Threads.empty() == true );
		CHECK ( Threads.size() == 0 );

		KString sTest2 = "Hello again";

		Threads.Add(std::thread(&ThreadedFunction, sTest2));

		Threads.Create(&ThreadedIntFunction, 1);
		Threads.Create(&ThreadedFunction, sTest2);
		Threads.Create(&ThreadedFunction, sTest2);
		Threads.Create(&ThreadedFunction, std::move(sTest2));

		Threads.Join();
	}

	SECTION("Stress: many short-lived threads via KThreads::Create")
	{
		// reproduce KParallel-style usage: spawn hardware_concurrency()
		// threads via KThreads::Create, join them all, then do the same
		// thing again. Helps figure out whether short-lived workers
		// generally trip over kSetupThreadSignalHandling()/sigaltstack
		// (independent of kMakeThread/KRunThreads).
		KThreads Threads;

		// match the KParallel test exactly: workers do absolutely nothing
		// and exit immediately. Many parallel exits = many parallel
		// sigaltstack(SS_DISABLE) + munmap calls, which is the suspect
		// scenario.
		KString sTest = "Hello";

		const auto N = std::max(std::thread::hardware_concurrency(), 4u);

		for (unsigned int i = 0; i < N; ++i)
		{
			Threads.Create(&ThreadedFunction, sTest);
		}

		Threads.Join();

		CHECK ( Threads.empty() == true );

		// second wave
		for (unsigned int i = 0; i < N; ++i)
		{
			Threads.Create(&ThreadedFunction, sTest);
		}

		Threads.Join();

		CHECK ( Threads.empty() == true );
	}

	SECTION("kMakeAsync: return value via future")
	{
		auto fut = kMakeAsync([](int a, int b){ return a + b; }, 2, 40);
		CHECK ( fut.get() == 42 );
	}

	SECTION("kMakeAsync: exception propagation via future")
	{
		auto fut = kMakeAsync([]{ throw std::runtime_error("boom"); });

		bool bCaught = false;
		try
		{
			fut.get();
		}
		catch (const std::runtime_error& ex)
		{
			bCaught = true;
			CHECK ( KString(ex.what()) == "boom" );
		}
		CHECK ( bCaught );
	}

	SECTION("kMakeAsync: void return type")
	{
		std::atomic<int> counter { 0 };
		auto fut = kMakeAsync([&]{ counter.fetch_add(1, std::memory_order_relaxed); });
		fut.get();
		CHECK ( counter.load() == 1 );
	}
}
