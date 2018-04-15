#include "catch.hpp"

#include <dekaf2/kmpsearch.h>
#include <dekaf2/kstringstream.h>

using namespace dekaf2;

TEST_CASE("KMPSearch") {

	SECTION("frozen::KMPSearch char* found search")
	{
		constexpr const char sHaystack[] { "abcderfghrsijklmnopqrstuvwxyz" };

		constexpr auto KMP = frozen::CreateKMPSearch("rst");
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
	}

	SECTION("frozen::KMPSearch char* unfound search")
	{
		constexpr const char sHaystack[] { "abcderfghrsijklmnopqrstuvwxyz" };

		constexpr auto KMP = frozen::CreateKMPSearch("ret");
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound == KStringView::npos );
	}

	SECTION("frozen::KMPSearch found search")
	{
		constexpr KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		constexpr KStringView sNeedle   { "rst" };

		constexpr frozen::KMPSearch<sNeedle.size()> KMP(sNeedle);
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
	}

	SECTION("frozen::KMPSearch unfound search")
	{
		constexpr KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		constexpr KStringView sNeedle   { "ret" };

		KInStringStream iss(sHaystack);

		constexpr frozen::KMPSearch<sNeedle.size()> KMP(sNeedle);
		bool bFound = KMP.Match(iss);

		CHECK ( bFound == false );
	}

	SECTION("KMPSearch char* found search")
	{
		const char sHaystack[] = "abcderfghrsijklmnopqrstuvwxyz";
		const char sNeedle[]   = "rst";

		KMPSearch KMP(sNeedle);
		auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
	}

	SECTION("KMPSearch char * unfound search")
	{
		const char sHaystack[] = "abcderfghrsijklmnopqrstuvwxyz";
		const char sNeedle[]   = "ret";

		KMPSearch KMP(sNeedle);
		auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound == KStringView::npos );
	}

	SECTION("KMPSearch found search")
	{
		KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		KStringView sNeedle   { "rst" };

		KMPSearch KMP(sNeedle);
		auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
	}

	SECTION("KMPSearch unfound search")
	{
		KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		KStringView sNeedle   { "ret" };

		KMPSearch KMP(sNeedle);
		auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound == KStringView::npos );
	}

}
