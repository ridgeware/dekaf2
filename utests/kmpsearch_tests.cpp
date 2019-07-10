#include "catch.hpp"

#include <dekaf2/kmpsearch.h>
#include <dekaf2/kinstringstream.h>

using namespace dekaf2;

TEST_CASE("KMPSearch") {

// frozen::KMPSearch does not work in older gcc and below C++17 because it thinks that
// std::array is not constexpr
#if (defined(DEKAF2_NO_GCC) || DEKAF2_GCC_VERSION >= 70000) && defined(DEKAF2_HAS_CPP_17)

	SECTION("kfrozen::KMPSearch char* found search")
	{
		constexpr const char sHaystack[] { "abcderfghrsijklmnopqrstuvwxyz" };

		constexpr auto KMP = kfrozen::CreateKMPSearch("rst");
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
		CHECK ( sHaystack[iFound + 3] == 'u' );
	}

	SECTION("kfrozen::KMPSearch char* unfound search")
	{
		constexpr const char sHaystack[] { "abcderfghrsijklmnopqrstuvwxyz" };

		constexpr auto KMP = kfrozen::CreateKMPSearch("ret");
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound == KStringView::npos );
	}

	SECTION("kfrozen::KMPSearch found search")
	{
		constexpr KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		constexpr KStringView sNeedle   { "rst" };

		constexpr kfrozen::KMPSearch<sNeedle.size()> KMP(sNeedle);
		constexpr auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
		CHECK ( sHaystack[iFound + 3] == 'u' );
	}

	SECTION("kfrozen::KMPSearch unfound search")
	{
		constexpr KStringView sHaystack { "abcderfghrsijklmnopqrstuvwxyz" };
		constexpr KStringView sNeedle   { "ret" };

		KInStringStream iss(sHaystack);

		constexpr kfrozen::KMPSearch<sNeedle.size()> KMP(sNeedle);
		bool bFound = KMP.Match(iss);

		CHECK ( bFound == false );
	}
#endif

	SECTION("KMPSearch char* found search")
	{
		const char sHaystack[] = "abcderfghrsijklmnopqrstuvwxyz";
		const char sNeedle[]   = "rst";

		KMPSearch KMP(sNeedle);
		auto iFound = KMP.Match(sHaystack);

		CHECK ( iFound != KStringView::npos );
		CHECK ( sHaystack[iFound] == 'r' );
		CHECK ( sHaystack[iFound + 3] == 'u' );
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
		CHECK ( sHaystack[iFound + 3] == 'u' );
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
