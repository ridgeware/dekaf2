#include "catch.hpp"

#include <dekaf2/kregex.h>
#include <dekaf2/kparallel.h>
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

}
