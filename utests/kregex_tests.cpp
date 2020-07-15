#include "catch.hpp"

#include <dekaf2/kregex.h>
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

		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		KRegex::SetMaxCacheSize(5);
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("Shrink")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(3);

		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		KRegex::SetMaxCacheSize(2);
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}

	SECTION("MRU")
	{
		auto iOrigSize = KRegex::GetMaxCacheSize();
		KRegex::ClearCache();
		KRegex::SetMaxCacheSize(3);

		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Fa-f]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-6A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-4A-Ca-c]*$") == false );
		CHECK ( KRegex::Matches(sString, "^[0-9A-Fa-f]*$") == true  );

		KRegex::SetMaxCacheSize(iOrigSize);
		KRegex::ClearCache();
	}
}
