#include "catch.hpp"

#include <dekaf2/kregex.h>
#include <dekaf2/kparallel.h>
#include <dekaf2/ktimer.h>
#include <dekaf2/kwriter.h>
#include <re2/re2.h>
#include <vector>

using namespace dekaf2;

static constexpr KStringView sString { "736f6d6520626f72696e6720616e64206c656e6774687920737472696e6720776974686f757420616e79207265616c206d65616e696e67" };

TEST_CASE("KRegex")
{
	SECTION("Growth")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(3);

		CHECK ( KRegex::GetCacheSize() == 0 );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		CHECK ( KRegex::GetCacheSize() == 3 );
		KRegex::SetMaxCacheSize(5);
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::GetCacheSize() == 4 );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("Shrink")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(3);

		CHECK ( KRegex::GetCacheSize() == 0 );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		CHECK ( KRegex::GetCacheSize() == 3 );
		KRegex::SetMaxCacheSize(2);
		CHECK ( KRegex::GetCacheSize() == 2 );
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::GetCacheSize() == 2 );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("MRU")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(3);

		CHECK ( KRegex::GetCacheSize() == 0 );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		CHECK ( KRegex::GetCacheSize() == 3 );
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::GetCacheSize() == 3 );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("MT")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(10);

		std::atomic_uint32_t iErrors { 0 };

		if (KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == false) { ++iErrors; }
		if (KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == true ) { ++iErrors; }
		if (KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == true ) { ++iErrors; }

		KRunThreads().Create([&iErrors]()
		{
			for (int i = 0; i < 1000; ++i)
			{
				if (KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == false) { ++iErrors; }
				if (KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == true ) { ++iErrors; }
				if (KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == true ) { ++iErrors; }
			}
		});

		CHECK ( iErrors == 0 );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("MT with overflow")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(2);

		std::atomic_uint32_t iErrors { 0 };

		if (KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == false) { ++iErrors; }
		if (KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == true ) { ++iErrors; }
		if (KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == true ) { ++iErrors; }

		KRunThreads().Create([&iErrors]()
		{
			for (int i = 0; i < 1000; ++i)
			{
				if (KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == false) { ++iErrors; }
				if (KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == true ) { ++iErrors; }
				if (KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == true ) { ++iErrors; }
			}
		});

		CHECK ( iErrors == 0 );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("Performance")
	{
		KStopWatch Watch1;
		for (int i = 0; i < 1000; ++i)
		{
			re2::RE2("^[0-9A-Fa-f]*$").Match(re2::StringPiece(sString.data(), sString.size()), 0, sString.size(), re2::RE2::UNANCHORED, nullptr, 0);
		}
		Watch1.halt();

		re2::RE2 Regex2("^[0-9A-Fa-f]*$");
		KStopWatch Watch2;
		for (int i = 0; i < 1000; ++i)
		{
			Regex2.Match(re2::StringPiece(sString.data(), sString.size()), 0, sString.size(), re2::RE2::UNANCHORED, nullptr, 0);
		}
		Watch2.halt();

		KStopWatch Watch3;
		for (int i = 0; i < 1000; ++i)
		{
			KRegex("^[0-9A-Fa-f]*$").Match(sString.data());
		}
		Watch3.halt();

		KStopWatch Watch4;
		KRegex Regex4("^[0-9A-Fa-f]*$");
		for (int i = 0; i < 1000; ++i)
		{
			Regex4.Match(sString.data());
		}
		Watch4.halt();

		INFO ("check that cached KRegex is at least 10 times faster than uncached regex use");
		CHECK ( Watch1.elapsed() > 10 * Watch3.elapsed() );

		INFO ("check that KRegex is faster than cached KRegex use");
		CHECK ( Watch3.elapsed() > Watch4.elapsed() );

//		KOut.FormatLine("elapsed 1: {} ns", Watch1.elapsed().count());
//		KOut.FormatLine("elapsed 2: {} ns", Watch2.elapsed().count());
//		KOut.FormatLine("elapsed 3: {} ns", Watch3.elapsed().count());
//		KOut.FormatLine("elapsed 4: {} ns", Watch4.elapsed().count());
	}
}
