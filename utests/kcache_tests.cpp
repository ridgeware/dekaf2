#include "catch.hpp"

#include <dekaf2/containers/associative/kcache.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/threading/execution/kparallel.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace dekaf2;

struct Loader
{
	template<class K>
	KString operator()(K&& key)
	{
		KString sReverse(std::forward<K>(key));
		std::reverse(sReverse.begin(), sReverse.end());
		return sReverse;
	}
};

TEST_CASE("KCache")
{
	SECTION("Construct")
	{
		KCache<KString, KString, Loader> MyCache;
		KString sString = "abcdefg";
		CHECK ( MyCache.Get(std::move(sString)) == "gfedcba" );
		CHECK ( MyCache.size() == 1 );
		CHECK ( MyCache.Get("abcdefg") == "gfedcba" );
		CHECK ( MyCache.size() == 1 );
		CHECK ( *MyCache.Find("abcdefg") == "gfedcba" );

		MyCache.clear();
	}

	SECTION("Growth")
	{
		KCache<KString, KString, Loader> MyCache;

		auto iOrigSize = MyCache.GetMaxSize();
		MyCache.clear();
		MyCache.SetMaxSize(3);

		CHECK ( MyCache.size() == 0 );
		CHECK ( MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 3 );
		MyCache.SetMaxSize(5);
		CHECK ( MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( MyCache.size() == 4 );

		MyCache.SetMaxSize(iOrigSize);
		MyCache.clear();
		CHECK ( MyCache.size() == 0 );
		CHECK ( MyCache.empty() == true );

		MyCache.clear();
	}

	SECTION("Shrink")
	{
		KCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(3);

		CHECK ( MyCache.size() == 0 );
		CHECK ( MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 3 );
		MyCache.SetMaxSize(2);
		CHECK ( MyCache.size() == 2 );
		MyCache.SetMaxSize(3);
		CHECK ( MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 2 );
		CHECK ( MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( MyCache.size() == 3 );

		MyCache.clear();
	}
}

TEST_CASE("KSharedCache")
{
	SECTION("Growth")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		auto iOrigSize = MyCache.GetMaxSize();
		MyCache.clear();
		MyCache.SetMaxSize(3);

		CHECK ( MyCache.size() == 0 );
		CHECK ( *MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( *MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( *MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( *MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( *MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 3 );
		MyCache.SetMaxSize(5);
		CHECK ( *MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( MyCache.size() == 4 );

		MyCache.SetMaxSize(iOrigSize);
		MyCache.clear();
		CHECK ( MyCache.size() == 0 );
		CHECK ( MyCache.empty() == true );

		MyCache.clear();
	}

	SECTION("Shrink")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(3);

		CHECK ( MyCache.size() == 0 );
		CHECK ( *MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( *MyCache.Get("abccefg") == "gfeccba"  );
		CHECK ( *MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( *MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( *MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 3 );
		MyCache.SetMaxSize(2);
		CHECK ( MyCache.size() == 2 );
		MyCache.SetMaxSize(3);
		CHECK ( *MyCache.Get("abcdefg") == "gfedcba"  );
		CHECK ( *MyCache.Get("1234567") == "7654321"  );
		CHECK ( MyCache.size() == 2 );
		CHECK ( *MyCache.Get("bbcdefg") == "gfedcbb"  );
		CHECK ( MyCache.size() == 3 );

		MyCache.clear();
	}

	SECTION("MT")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(10);

		std::atomic<uint32_t> iErrors { 0 };

		if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		KRunThreads(20).Create([&iErrors,&MyCache]()
		{
			for (int i = 0; i < 500; ++i)
			{
				if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
				if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
				if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }
			}
		});

		if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		CHECK ( iErrors == 0 );

		MyCache.clear();
	}

	SECTION("MT with overflow")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(2);

		std::atomic<uint32_t> iErrors { 0 };

		if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		KRunThreads(20).Create([&iErrors,&MyCache]()
		{
			for (int i = 0; i < 500; ++i)
			{
				if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
				if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
				if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }
			}
		});

		if (*MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (*MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (*MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		CHECK ( iErrors == 0 );

		MyCache.clear();
	}

	SECTION("Overflow")
	{
		KSharedCache<KString, KString, Loader> MyCache(2);

		auto s1 = MyCache.Get("1234567890123456789012345678901234567890");
		auto s2 = MyCache.Get("2345678901234567890123456789012345678901");
		auto s3 = MyCache.Get("3456789012345678901234567890123456789012");
		auto s4 = MyCache.Get("4567890123456789012345678901234567890123");

		CHECK ( MyCache.size() == 2 );
		CHECK ( s1->size() == 40 );
		CHECK ( s2->size() == 40 );
		CHECK ( s3->size() == 40 );
		CHECK ( s4->size() == 40 );
		CHECK ( (s1 == "0987654321098765432109876543210987654321") );
		CHECK ( (s2 == "1098765432109876543210987654321098765432") );
		CHECK ( (s3 == "2109876543210987654321098765432109876543") );
		CHECK ( (s4 == "3210987654321098765432109876543210987654") );
	}
}

namespace {

//-----------------------------------------------------------------------------
std::atomic<int>& CountingLoads()
//-----------------------------------------------------------------------------
{
	static std::atomic<int> s_iLoads { 0 };
	return s_iLoads;
}

// counts its loads, and calls an optional callable before returning the key as the value
struct CountingLoader
{
	KString operator()(KStringView sKey)
	{
		++CountingLoads();
		return sKey;
	}

	template<class Callable>
	KString operator()(KStringView sKey, Callable&& fnDuringLoad)
	{
		++CountingLoads();
		fnDuringLoad();
		return sKey;
	}
};

} // end of anonymous namespace

TEST_CASE("KSharedCache eviction and locking")
{
	SECTION("a used element gets a second chance")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(3);

		auto iLoads = CountingLoads().load();

		CHECK ( *MyCache.Get("a") == "a" );
		CHECK ( *MyCache.Get("b") == "b" );
		CHECK ( *MyCache.Get("c") == "c" );
		CHECK ( CountingLoads() == iLoads + 3 );

		// marks "a" as used
		CHECK ( *MyCache.Get("a") == "a" );
		CHECK ( CountingLoads() == iLoads + 3 );

		// "a" is the oldest element, but was used, therefore "b" is removed
		CHECK ( *MyCache.Get("d") == "d" );
		CHECK ( CountingLoads() == iLoads + 4 );
		CHECK ( MyCache.size() == 3 );

		CHECK ( *MyCache.Get("a") == "a" );
		CHECK ( *MyCache.Get("c") == "c" );
		CHECK ( *MyCache.Get("d") == "d" );
		CHECK ( CountingLoads() == iLoads + 4 );

		CHECK ( *MyCache.Get("b") == "b" );
		CHECK ( CountingLoads() == iLoads + 5 );
		CHECK ( MyCache.size() == 3 );
	}

	SECTION("Find does not load")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(3);

		auto iLoads = CountingLoads().load();

		CHECK ( *MyCache.Find("a") == "" );
		CHECK ( MyCache.size() == 0 );
		CHECK ( *MyCache.Get("a") == "a" );
		CHECK ( *MyCache.Find(KStringView("a")) == "a" );
		CHECK ( CountingLoads() == iLoads + 1 );
	}

	SECTION("Set keeps an existing value")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(3);

		CHECK ( *MyCache.Set("a", KString("first"))  == "first" );
		CHECK ( *MyCache.Set("a", KString("second")) == "first" );
		CHECK ( *MyCache.Get("a") == "first" );
		CHECK ( MyCache.size() == 1 );
	}

	SECTION("maximum size 0")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(0);

		CHECK ( *MyCache.Get("a") == "a" );
		CHECK ( *MyCache.Set("b", KString("b")) == "b" );
		CHECK ( MyCache.size() == 0 );
	}

	SECTION("a load does not block hits on other keys")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(10);

		CHECK ( *MyCache.Get("fast") == "fast" );

		std::atomic<bool> bHit { false };
		bool bHitDuringLoad { false };
		std::thread Hitter;

		auto sSlow = MyCache.Get("slow", [&]()
		{
			// this runs inside the load of "slow" - another thread looks up "fast"
			Hitter = std::thread([&MyCache, &bHit]()
			{
				if (*MyCache.Get("fast") == "fast")
				{
					bHit = true;
				}
			});

			for (int i = 0; i < 500 && !bHit; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			bHitDuringLoad = bHit;
		});

		Hitter.join();

		CHECK ( *sSlow == "slow" );
		CHECK ( bHitDuringLoad == true );
		CHECK ( MyCache.size() == 2 );
	}

	SECTION("concurrent requests for a missing key load it once")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(10);

		constexpr std::size_t iThreads = 8;

		auto iLoads = CountingLoads().load();
		std::atomic<std::size_t> iStarted { 0 };
		std::vector<KSharedCache<KString, KString, CountingLoader>::value_type> Values(iThreads, KString());
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < iThreads; ++i)
		{
			Threads.emplace_back([&, i]()
			{
				++iStarted;

				Values[i] = MyCache.Get("key", [&iStarted]()
				{
					// wait until all threads have started, or for one second, and
					// give them the time to request the key while it is loading
					for (int j = 0; j < 100 && iStarted < iThreads; ++j)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				});
			});
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		CHECK ( CountingLoads() == iLoads + 1 );
		CHECK ( MyCache.size() == 1 );

		auto Cached = MyCache.Get("key");

		for (const auto& Value : Values)
		{
			CHECK ( *Value == "key" );
			CHECK ( &Value.get() == &Cached.get() );
		}
	}

	SECTION("a waiting thread loads the key if the loading thread fails")
	{
		KSharedCache<KString, KString, CountingLoader> MyCache(10);

		constexpr std::size_t iThreads = 8;

		auto iLoads = CountingLoads().load();
		std::atomic<std::size_t> iStarted    { 0 };
		std::atomic<std::size_t> iExceptions { 0 };
		std::atomic<bool>        bFail       { true };
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < iThreads; ++i)
		{
			Threads.emplace_back([&]()
			{
				++iStarted;

				try
				{
					auto Value = MyCache.Get("key", [&iStarted, &bFail]()
					{
						for (int j = 0; j < 100 && iStarted < iThreads; ++j)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(100));

						// the first load fails
						if (bFail.exchange(false))
						{
							throw std::runtime_error("load failed");
						}
					});

					if (*Value != "key")
					{
						++iExceptions;
					}
				}
				catch (const std::runtime_error&)
				{
					++iExceptions;
				}
			});
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		// only the thread with the failing load got the exception
		CHECK ( iExceptions == 1 );
		CHECK ( CountingLoads() == iLoads + 2 );
		CHECK ( MyCache.size() == 1 );
		CHECK ( *MyCache.Get("key") == "key" );
		CHECK ( CountingLoads() == iLoads + 2 );
	}

	SECTION("MT with eviction")
	{
		KSharedCache<KString, KString, Loader> MyCache(8);

		std::vector<KString> Keys;

		for (int i = 0; i < 50; ++i)
		{
			Keys.push_back(KString("key") + KString::to_string(i));
		}

		std::atomic<uint32_t> iErrors { 0 };

		KRunThreads(10).Create([&iErrors, &MyCache, &Keys]()
		{
			for (int i = 0; i < 2000; ++i)
			{
				const auto& sKey = Keys[static_cast<std::size_t>(i * 7) % Keys.size()];
				KString sReverse(sKey);
				std::reverse(sReverse.begin(), sReverse.end());

				if (*MyCache.Get(sKey) != sReverse) { ++iErrors; }
				// a hot key that is used all the time
				if (*MyCache.Get("hot") != "toh") { ++iErrors; }
			}
		});

		CHECK ( iErrors == 0 );
		CHECK ( MyCache.size() <= 8 );
	}
}
