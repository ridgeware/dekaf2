#include "catch.hpp"

#include <dekaf2/util/id/krandom.h>

using namespace dekaf2;

TEST_CASE("KRandom")
{
	SECTION("kRandom32")
	{
		uint32_t i = kRandom32();
		if (i == 0) {}
	}

	SECTION("kRandom64")
	{
		uint64_t i = kRandom64();
		if (i == 0) {}
	}

	SECTION("kRandom")
	{
		auto i = kRandom(17, 43);
		CHECK ( (i >= 17 && i <= 43) == true );
		i = kRandom();
		// don't test for i >= 0, gcc 12 thinks it needs to teach us that
		// is always true and hence an error
		CHECK ( (i <= std::numeric_limits<decltype(i)>::max()) );
	}

	SECTION("kGetRandom 1")
	{
		std::array<char, 60> r;
		CHECK ( kGetRandom(r.data(), r.size()) );
	}

	SECTION("kGetRandom 2")
	{
		CHECK ( kGetRandom(20).size() == 20 );
	}
}
