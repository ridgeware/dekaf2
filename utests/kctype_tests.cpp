#include "catch.hpp"

#include <cctype>
#include <cwctype>
#include <dekaf2/kctype.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

TEST_CASE("KCType")
{
	// These tests cannot work on Windows, which is basically the reason
	// why kctype.h was written.. And actually they do not work on any
	// system without failures, as every lib implements a different subset
	// of character properties..

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

	// tests that have to work on all systems

	SECTION("signed chars")
	{
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsUpper = ch >= 'A' && ch <= 'Z';
			CHECK ( KASCII::kIsUpper(ch) == bIsUpper );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsLower = ch >= 'a' && ch <= 'z';
			CHECK ( KASCII::kIsLower(ch) == bIsLower );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsAlpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
			CHECK ( KASCII::kIsAlpha(ch) == bIsAlpha );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsDigit = ch >= '0' && ch <= '9';
			CHECK ( KASCII::kIsDigit(ch) == bIsDigit );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsXDigit = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
			CHECK ( KASCII::kIsXDigit(ch) == bIsXDigit );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsBlank = ch == ' ' || ch == '\t';
			CHECK ( KASCII::kIsBlank(ch) == bIsBlank );
		}
		for (char ch = -0x7f; ch < 0x7f; ++ch)
		{
			bool bIsSpace = (ch >= 0x09 && ch <= 0x0e) || ch == 0x20;
			CHECK ( KASCII::kIsSpace(ch) == bIsSpace );
		}
	}

	SECTION("unsigned chars")
	{
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsUpper = ch >= 'A' && ch <= 'Z';
			CHECK ( KASCII::kIsUpper(ch) == bIsUpper );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsLower = ch >= 'a' && ch <= 'z';
			CHECK ( KASCII::kIsLower(ch) == bIsLower );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsAlpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
			CHECK ( KASCII::kIsAlpha(ch) == bIsAlpha );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsDigit = ch >= '0' && ch <= '9';
			CHECK ( KASCII::kIsDigit(ch) == bIsDigit );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsXDigit = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
			CHECK ( KASCII::kIsXDigit(ch) == bIsXDigit );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsBlank = ch == ' ' || ch == '\t';
			CHECK ( KASCII::kIsBlank(ch) == bIsBlank );
		}
		for (unsigned char ch = 0; ch < 0xff; ++ch)
		{
			bool bIsSpace = (ch >= 0x09 && ch <= 0x0e) || ch == 0x20;
			CHECK ( KASCII::kIsSpace(ch) == bIsSpace );
		}
	}
}

#endif // Windows
