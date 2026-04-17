#include "catch.hpp"

#include <dekaf2/containers/memory/kpool.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/system/os/ksystem.h>

using namespace dekaf2;

struct MyType
{
	KString sString;
	uint32_t iPopped { 0 };
	uint32_t iPushed { 0 };
};

class MyControl : public KPoolControl<MyType>
{
public:

	void Popped(MyType* value, bool bIsNew)
	{
		++value->iPopped;
	}

	void Pushed(MyType* value)
	{
		++value->iPushed;
	}
};

TEST_CASE("KPool")
{
	SECTION("Unique")
	{
		MyControl Control;
		KPool<MyType> Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );
		CHECK ( Pool.used() == 0 );

		{
			auto p = Pool.get();
			CHECK ( Pool.size() == 1 );
			CHECK ( Pool.used() == 1 );
			p->sString = "hello world";
			// KPool internally uses lambda deleters, because they reduce to one single
			// pointer added to the type. Using std::function would add 7 words to the
			// size of a KPool<>::unique_ptr. We want to check that this really worked.
			CHECK ( sizeof(p) == sizeof(void*) * 2 );
			// It is important (and difficult to code) that KPool<type>::unique_ptr
			// returns the same type as KPool<type>.get(). Verify it worked..
			CHECK ( (std::is_same<decltype(p), KPool<MyType>::unique_ptr>::value) == true );
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );
		CHECK ( Pool.used()  == 0     );

		{
			auto p = Pool.get();
			CHECK ( Pool.size()  == 1     );
			CHECK ( Pool.used()  == 1     );
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

	SECTION("Shared")
	{
		KPool<MyType> Pool;

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			CHECK ( sizeof(p) == sizeof(void*) * 2 );
			CHECK ( (std::is_same<decltype(p), KPool<MyType>::shared_ptr>::value) == true );
			CHECK ( (std::is_same<decltype(p), std::shared_ptr<MyType>>::value) == true );
			p->sString = "hello world";
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.get();
			CHECK ( p->sString == "hello world" );
		}
	}

	SECTION("Shared and Shared access")
	{
		MyControl Control;
		KSharedPool<MyType> Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			p->sString = "hello world";
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

	SECTION("multithreaded")
	{
		MyControl Control;
		KSharedPool<MyType> Pool(Control, 50);

		CHECK ( Pool.max_size() == 50);
		Pool.max_size(20);
		CHECK ( Pool.max_size() == 20);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		KThreads Threads;

		for (int iCount = 0; iCount < 20; ++iCount)
		{
			Threads.Add(std::thread([&Pool]()
			{
				for (int iLoop = 0; iLoop < 30; ++iLoop)
				{
					kSleep(chrono::milliseconds(kRandom(1, 10)));
					auto p = Pool.get();
					p->sString = "hello";
					kSleep(chrono::milliseconds(kRandom(1, 10)));
				}
			}));
		}

		Threads.Join();

		CHECK ( Pool.used() == 0  );
		CHECK ( Pool.size() <= 20 );
	}

	SECTION("multiple active objects")
	{
		MyControl Control;
		KPool<MyType> Pool(Control);

		auto p1 = Pool.get();
		auto p2 = Pool.get();
		auto p3 = Pool.get();

		CHECK ( Pool.size() == 3 );
		CHECK ( Pool.used() == 3 );

		p1->sString = "one";
		p2->sString = "two";
		p3->sString = "three";

		p2.reset();

		CHECK ( Pool.size() == 3 );
		CHECK ( Pool.used() == 2 );

		p1.reset();
		p3.reset();

		CHECK ( Pool.size() == 3 );
		CHECK ( Pool.used() == 0 );
	}

	SECTION("GetStats")
	{
		MyControl Control;
		KPool<MyType> Pool(Control, 50);

		auto Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      == 0  );
		CHECK ( Stats.iAvailable == 0  );
		CHECK ( Stats.iMaxPool   == 50 );
		CHECK ( Stats.iAbsoluteMax == 50 );

		auto p1 = Pool.get();
		auto p2 = Pool.get();

		Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      == 2 );
		CHECK ( Stats.iAvailable == 0 );

		p1.reset();

		Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      == 1 );
		CHECK ( Stats.iAvailable == 1 );
	}

	SECTION("disable and enable")
	{
		MyControl Control;
		KPool<MyType> Pool(Control);

		{
			auto p = Pool.get();
			p->sString = "pooled";
		}

		CHECK ( Pool.size() == 1 );

		// disable pooling - should clear the pool
		bool bWasDisabled = Pool.disable(true);
		CHECK ( bWasDisabled == false );

		// pool should be empty now
		CHECK ( Pool.empty() );

		{
			// objects should still be created, just not pooled
			auto p = Pool.get();
			p->sString = "not pooled";
			CHECK ( Pool.used() == 1 );
		}

		// object should have been destroyed, not returned to pool
		CHECK ( Pool.empty() );
		CHECK ( Pool.used() == 0 );

		// idempotent disable
		bWasDisabled = Pool.disable(true);
		CHECK ( bWasDisabled == true );

		// re-enable pooling
		bWasDisabled = Pool.disable(false);
		CHECK ( bWasDisabled == true );

		{
			auto p = Pool.get();
			p->sString = "pooled again";
		}

		// object should be back in the pool
		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.get();
			CHECK ( p->sString == "pooled again" );
		}
	}

	SECTION("disable shared")
	{
		MyControl Control;
		KSharedPool<MyType> Pool(Control);

		{
			auto p = Pool.get();
			p->sString = "shared pooled";
		}

		CHECK ( Pool.size() == 1 );

		Pool.disable(true);
		CHECK ( Pool.empty() );

		{
			auto p = Pool.get();
			CHECK ( Pool.used() == 1 );
		}

		CHECK ( Pool.empty() );
		CHECK ( Pool.used() == 0 );

		Pool.disable(false);

		{
			auto p = Pool.get();
			p->sString = "re-enabled";
		}

		CHECK ( Pool.size() == 1 );
	}

	SECTION("put")
	{
		KPool<MyType> Pool;

		auto value = new MyType();
		value->sString = "externally created";

		Pool.put(value);

		CHECK ( Pool.empty() == false );

		auto p = Pool.get();
		CHECK ( p->sString == "externally created" );
	}

	SECTION("max_size contention")
	{
		MyControl Control;
		KSharedPool<MyType> Pool(Control, 5);

		std::atomic<std::size_t> iMaxConcurrent { 0 };
		std::atomic<std::size_t> iCurrent { 0 };

		KThreads Threads;

		for (int iCount = 0; iCount < 10; ++iCount)
		{
			Threads.Add(std::thread([&Pool, &iMaxConcurrent, &iCurrent]()
			{
				for (int iLoop = 0; iLoop < 20; ++iLoop)
				{
					auto p = Pool.get();

					auto iNow = ++iCurrent;
					// track the maximum concurrent usage
					auto iMax = iMaxConcurrent.load();
					while (iNow > iMax && !iMaxConcurrent.compare_exchange_weak(iMax, iNow)) {}

					kSleep(chrono::milliseconds(kRandom(1, 3)));

					--iCurrent;
				}
			}));
		}

		Threads.Join();

		CHECK ( Pool.used() == 0 );
		// the pool limit is 5, but we allow <= so up to 6 concurrent
		// objects may exist (m_iPopped <= m_iMaxSize allows one extra)
		CHECK ( iMaxConcurrent.load() <= 6 );
	}

// gcc 6 has difficulties casting timepoints of different durations
#if !DEKAF2_IS_GCC || DEKAF2_GCC_VERSION_MAJOR > 6

	SECTION("dynamic")
	{
		using PoolResolution = std::chrono::duration<long, std::ratio<1, 1000>>;
		MyControl Control;
		using PoolType = KPool<MyType, 3, PoolResolution>;
		PoolType Pool(Control);

		std::vector<PoolType::unique_ptr> Objects;

		for (int i = 0; i < 100; ++i)
		{
			Objects.push_back(Pool.get());
		}

		auto Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      ==   100 );
		CHECK ( Stats.iAvailable ==     0 );
		CHECK ( Stats.iMaxPool   ==   100 );

		Objects.clear();

		Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      ==     0 );
		CHECK ( Stats.iAvailable ==   100 );
		CHECK ( Stats.iMaxPool   ==   100 );

		for (int i = 0; i < 10; ++i)
		{
			kSleep(chrono::milliseconds(1));
			Objects.push_back(Pool.get());
		}

		Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      ==  10 );
		CHECK ( Stats.iAvailable ==   0 );
		CHECK ( Stats.iMaxPool   ==  10 );

		Objects.clear();

		Stats = Pool.GetStats();

		CHECK ( Stats.iUsed      ==   0 );
		CHECK ( Stats.iAvailable ==  10 );
		CHECK ( Stats.iMaxPool   ==  10 );
	}
#endif
}
