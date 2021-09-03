#include "catch.hpp"

#include <dekaf2/kcache.h>
#include <dekaf2/kwriter.h>
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
		KSharedCache<KString, KString, Loader> MyCache;

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

	SECTION("MT")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(10);

		std::atomic_uint32_t iErrors { 0 };

		if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		KRunThreads(20).Create([&iErrors,&MyCache]()
		{
			for (int i = 0; i < 500; ++i)
			{
				if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
				if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
				if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }
			}
		});

		if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		CHECK ( iErrors == 0 );

		MyCache.clear();
	}

	SECTION("MT with overflow")
	{
		KSharedCache<KString, KString, Loader> MyCache;

		MyCache.clear();
		MyCache.SetMaxSize(2);

		std::atomic_uint32_t iErrors { 0 };

		if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		KRunThreads(20).Create([&iErrors,&MyCache]()
		{
			for (int i = 0; i < 500; ++i)
			{
				if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
				if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
				if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }
			}
		});

		if (MyCache.Get("abcdefg") != "gfedcba") { ++iErrors; }
		if (MyCache.Get("abccefg") != "gfeccba") { ++iErrors; }
		if (MyCache.Get("bbcdefg") != "gfedcbb") { ++iErrors; }

		CHECK ( iErrors == 0 );

		MyCache.clear();
	}
}
