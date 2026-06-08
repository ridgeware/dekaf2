#include "catch.hpp"

#include <dekaf2/crypto/random/krandom.h>
#include <algorithm>
#include <array>

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

	SECTION("kGetRandom yields non-degenerate, distinct output (CSPRNG fallback regression, INFO-1)")
	{
		// guards against a broken/constant/looping fallback being reintroduced.
		// Asserts behaviour, not the backend, so it holds on every platform
		// (arc4random / getrandom / the OpenSSL RAND_bytes fallback alike).
		std::array<unsigned char, 64> a{}, b{};
		CHECK ( kGetRandom(a.data(), a.size()) );
		CHECK ( kGetRandom(b.data(), b.size()) );

		// a no-op fill would leave the zero-initialised buffer untouched
		CHECK_FALSE ( std::all_of(a.begin(), a.end(), [](unsigned char c){ return c == 0; }) );
		// two independent draws must differ (a constant generator would repeat)
		CHECK ( a != b );

		// a large request must be filled completely in one call - exceeds both
		// getrandom()'s 256-byte syscall chunk and the former 4-byte fill loop
		KString sBig = kGetRandom(4096);
		CHECK ( sBig.size() == 4096 );
		CHECK_FALSE ( std::all_of(sBig.begin(), sBig.end(), [](char c){ return c == 0; }) );
	}
}
