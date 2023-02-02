#include "catch.hpp"

#include <dekaf2/kpool.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kthreads.h>
#include <dekaf2/ksystem.h>

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
					kSleepRandomMilliseconds(1, 10);
					auto p = Pool.get();
					p->sString = "hello";
					kSleepRandomMilliseconds(1, 10);
				}
			}));
		}

		Threads.Join();

		CHECK ( Pool.used() == 0  );
		CHECK ( Pool.size() <= 20 );
	}

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
			kMilliSleep(1);
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
}
