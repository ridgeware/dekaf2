#include "catch.hpp"

#include <cctype>
#include <cwctype>
#include <dekaf2/kctype.h>

#ifndef DEKAF2_IS_WINDOWS

// These tests cannot work on Windows, which is basically the reason
// why kctype.h was written..

using namespace dekaf2;

TEST_CASE("KCType")
{
	/*
	SECTION("IsSpace")
	{
		for (uint32_t cp = 0; cp < 80000; ++cp)
		{
			INFO ( cp );
			CHECK ( kIsSpace(cp) == std::iswspace(cp) );
		}
	}

	SECTION("ToLower")
	{
		for (uint32_t cp = 0; cp < 65535; ++cp)
		{
			INFO ( cp );
			CHECK ( kToLower(cp) == std::towlower(cp) );
		}
	}

	SECTION("ToUpper")
	{
		for (uint32_t cp = 0; cp < 65535; ++cp)
		{
			INFO ( cp );
			CHECK ( kToUpper(cp) == std::towupper(cp) );
		}
	}

	SECTION("IsLower")
	{
		for (uint32_t cp = 0; cp < 65535; ++cp)
		{
			INFO ( cp );
			CHECK ( kIsLower(cp) == std::iswlower(cp) );
		}
	}

	SECTION("IsUpper")
	{
		for (uint32_t cp = 0; cp < 65535; ++cp)
		{
			INFO ( cp );
			CHECK ( kIsUpper(cp) == std::iswupper(cp) );
		}
	}
*/
}

#endif // Windows
