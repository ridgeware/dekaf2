#include "catch.hpp"

#include <cctype>
#include <cwctype>
#include <dekaf2/core/types/kctype.h>

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
			bool bIsSpace = (ch >= 0x09 && ch <= 0x0d) || ch == 0x20;
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
			bool bIsSpace = (ch >= 0x09 && ch <= 0x0d) || ch == 0x20;
			CHECK ( KASCII::kIsSpace(ch) == bIsSpace );
		}
	}

	SECTION("ASCII consistency with libc")
	{
		for (int ch = 0; ch < 128; ++ch)
		{
			INFO ( "codepoint " << ch );
			CHECK ( KASCII::kIsSpace (ch) == (std::isspace (ch) != 0) );
			CHECK ( KASCII::kIsBlank (ch) == (std::isblank (ch) != 0) );
			CHECK ( KASCII::kIsUpper (ch) == (std::isupper (ch) != 0) );
			CHECK ( KASCII::kIsLower (ch) == (std::islower (ch) != 0) );
			CHECK ( KASCII::kIsAlpha (ch) == (std::isalpha (ch) != 0) );
			CHECK ( KASCII::kIsDigit (ch) == (std::isdigit (ch) != 0) );
			CHECK ( KASCII::kIsXDigit(ch) == (std::isxdigit(ch) != 0) );
			CHECK ( KASCII::kIsAlNum (ch) == (std::isalnum (ch) != 0) );
			CHECK ( KASCII::kIsPunct (ch) == (std::ispunct (ch) != 0) );
			CHECK ( KASCII::kToLower (static_cast<char>(ch)) == static_cast<char>(std::tolower(ch)) );
			CHECK ( KASCII::kToUpper (static_cast<char>(ch)) == static_cast<char>(std::toupper(ch)) );

			// KASCII treats BL (blank: HT 0x09) as printable and non-control,
			// while libc considers HT non-printable and a control character.
			// This is an intentional KASCII design choice, so only check
			// kIsPrint and kIsCntrl for non-blank control characters.
			if (!KASCII::kIsBlank(ch))
			{
				CHECK ( KASCII::kIsPrint(ch) == (std::isprint(ch) != 0) );
				CHECK ( KASCII::kIsCntrl(ch) == (std::iscntrl(ch) != 0) );
			}
		}

		// verify the known KASCII divergence for HT explicitly
		CHECK ( KASCII::kIsPrint('\t') == true  );  // KASCII: blank is printable
		CHECK ( (std::isprint('\t') != 0) == false );  // libc: HT is not printable
		CHECK ( KASCII::kIsCntrl('\t') == false );  // KASCII: blank is not control
		CHECK ( (std::iscntrl('\t') != 0) == true  );  // libc: HT is control
	}

	SECTION("IsIdeographic")
	{
		// positives - one representative codepoint per covered script/block
		CHECK ( KCodePoint(0x4F60 ).IsIdeographic() );  // 你 CJK unified ideograph
		CHECK ( KCodePoint(0x3042 ).IsIdeographic() );  // Hiragana A
		CHECK ( KCodePoint(0x30AB ).IsIdeographic() );  // Katakana KA
		CHECK ( KCodePoint(0xD55C ).IsIdeographic() );  // Hangul syllable HAN
		CHECK ( KCodePoint(0x3105 ).IsIdeographic() );  // Bopomofo B
		CHECK ( KCodePoint(0x31A0 ).IsIdeographic() );  // Bopomofo extended
		CHECK ( KCodePoint(0x3005 ).IsIdeographic() );  // ideographic iteration mark
		CHECK ( KCodePoint(0x2E80 ).IsIdeographic() );  // CJK radical (range start)
		CHECK ( KCodePoint(0x3400 ).IsIdeographic() );  // CJK extension A (range start)
		CHECK ( KCodePoint(0x4DBF ).IsIdeographic() );  // CJK extension A (range end)
		CHECK ( KCodePoint(0xF900 ).IsIdeographic() );  // CJK compatibility ideograph
		CHECK ( KCodePoint(0xFF76 ).IsIdeographic() );  // halfwidth Katakana KA
		CHECK ( KCodePoint(0x20000).IsIdeographic() );  // CJK extension B (plane 2)
		CHECK ( KCodePoint(0x2A700).IsIdeographic() );  // CJK extension C (plane 2)

		// negatives - punctuation and symbols delimit, they are not ideographic
		CHECK_FALSE ( KCodePoint('A'    ).IsIdeographic() );  // latin
		CHECK_FALSE ( KCodePoint('0'    ).IsIdeographic() );  // ascii digit
		CHECK_FALSE ( KCodePoint(' '    ).IsIdeographic() );  // space
		CHECK_FALSE ( KCodePoint(0x03B1 ).IsIdeographic() );  // Greek alpha
		CHECK_FALSE ( KCodePoint(0x0410 ).IsIdeographic() );  // Cyrillic A
		CHECK_FALSE ( KCodePoint(0x3001 ).IsIdeographic() );  // ideographic comma (punctuation)
		CHECK_FALSE ( KCodePoint(0x3002 ).IsIdeographic() );  // ideographic full stop (punctuation)
		CHECK_FALSE ( KCodePoint(0xFF01 ).IsIdeographic() );  // fullwidth exclamation mark
		CHECK_FALSE ( KCodePoint(0x4DC0 ).IsIdeographic() );  // Yijing hexagram (gap between ext A and unified)
		CHECK_FALSE ( KCodePoint(0x2FE0 ).IsIdeographic() );  // gap above the CJK radicals block
		CHECK_FALSE ( KCodePoint(0x1F600).IsIdeographic() );  // emoji
		CHECK_FALSE ( KCodePoint(0x40000).IsIdeographic() );  // above the covered planes

		// exhaustive equivalence with the original flat OR over the same ranges,
		// over the whole Unicode space - proves the bracketed tree did not slip a
		// boundary (e.g. the Yijing gap, or the ext A upper bound)
		auto Reference = [](uint32_t cp)
		{
			return (cp >= 0x2E80  && cp <= 0x2FDF)
			    || (cp >= 0x3005  && cp <= 0x3007)
			    || (cp >= 0x3031  && cp <= 0x3035)
			    || (cp >= 0x3040  && cp <= 0x30FF)
			    || (cp >= 0x3100  && cp <= 0x312F)
			    || (cp >= 0x31A0  && cp <= 0x31BF)
			    || (cp >= 0x31F0  && cp <= 0x31FF)
			    || (cp >= 0x3400  && cp <= 0x4DBF)
			    || (cp >= 0x4E00  && cp <= 0x9FFF)
			    || (cp >= 0xAC00  && cp <= 0xD7A3)
			    || (cp >= 0xF900  && cp <= 0xFAFF)
			    || (cp >= 0xFF66  && cp <= 0xFF9D)
			    || (cp >= 0x20000 && cp <= 0x3FFFF);
		};

		int64_t iFirstMismatch = -1;

		for (uint32_t cp = 0; cp <= 0x110000 && iFirstMismatch < 0; ++cp)
		{
			if (KCodePoint(cp).IsIdeographic() != Reference(cp))
			{
				iFirstMismatch = cp;
			}
		}

		INFO ( "first IsIdeographic mismatch at codepoint (decimal, -1 == none): " << iFirstMismatch );
		CHECK ( iFirstMismatch == -1 );
	}

	SECTION("kIsIdeographic")
	{
		// positives - one representative codepoint per covered script/block
		CHECK ( kIsIdeographic(0x4F60 ) );  // 你 CJK unified ideograph
		CHECK ( kIsIdeographic(0x3042 ) );  // Hiragana A
		CHECK ( kIsIdeographic(0x30AB ) );  // Katakana KA
		CHECK ( kIsIdeographic(0xD55C ) );  // Hangul syllable HAN
		CHECK ( kIsIdeographic(0x3105 ) );  // Bopomofo B
		CHECK ( kIsIdeographic(0x31A0 ) );  // Bopomofo extended
		CHECK ( kIsIdeographic(0x3005 ) );  // ideographic iteration mark
		CHECK ( kIsIdeographic(0x2E80 ) );  // CJK radical (range start)
		CHECK ( kIsIdeographic(0x3400 ) );  // CJK extension A (range start)
		CHECK ( kIsIdeographic(0x4DBF ) );  // CJK extension A (range end)
		CHECK ( kIsIdeographic(0xF900 ) );  // CJK compatibility ideograph
		CHECK ( kIsIdeographic(0xFF76 ) );  // halfwidth Katakana KA
		CHECK ( kIsIdeographic(0x20000) );  // CJK extension B (plane 2)
		CHECK ( kIsIdeographic(0x2A700) );  // CJK extension C (plane 2)

		// negatives - punctuation and symbols delimit, they are not ideographic
		CHECK_FALSE ( kIsIdeographic('A'    ) );  // latin
		CHECK_FALSE ( kIsIdeographic('0'    ) );  // ascii digit
		CHECK_FALSE ( kIsIdeographic(' '    ) );  // space
		CHECK_FALSE ( kIsIdeographic(0x03B1 ) );  // Greek alpha
		CHECK_FALSE ( kIsIdeographic(0x0410 ) );  // Cyrillic A
		CHECK_FALSE ( kIsIdeographic(0x3001 ) );  // ideographic comma (punctuation)
		CHECK_FALSE ( kIsIdeographic(0x3002 ) );  // ideographic full stop (punctuation)
		CHECK_FALSE ( kIsIdeographic(0xFF01 ) );  // fullwidth exclamation mark
		CHECK_FALSE ( kIsIdeographic(0x4DC0 ) );  // Yijing hexagram (gap between ext A and unified)
		CHECK_FALSE ( kIsIdeographic(0x2FE0 ) );  // gap above the CJK radicals block
		CHECK_FALSE ( kIsIdeographic(0x1F600) );  // emoji
		CHECK_FALSE ( kIsIdeographic(0x40000) );  // above the covered planes
	}

	SECTION("IsMark")
	{
		// positives - one representative per mark category
		CHECK ( KCodePoint(0x0301).IsMark() );  // combining acute accent (Mn)
		CHECK ( KCodePoint(0x0E31).IsMark() );  // Thai mai han-akat (Mn)
		CHECK ( KCodePoint(0x0E49).IsMark() );  // Thai mai tho, tone mark (Mn)
		CHECK ( KCodePoint(0x093F).IsMark() );  // Devanagari vowel sign I (Mc)
		CHECK ( KCodePoint(0x094D).IsMark() );  // Devanagari virama (Mn)
		CHECK ( KCodePoint(0x064E).IsMark() );  // Arabic fatha (Mn)
		CHECK ( KCodePoint(0x05B8).IsMark() );  // Hebrew qamats (Mn)
		CHECK ( KCodePoint(0x3099).IsMark() );  // combining dakuten (Mn)
		CHECK ( KCodePoint(0x20DD).IsMark() );  // combining enclosing circle (Me)

		CHECK ( kIsMark(0x0301) );
		CHECK ( kIsMark(0x093F) );

		// negatives
		CHECK_FALSE ( KCodePoint('a'   ).IsMark() );  // latin letter
		CHECK_FALSE ( KCodePoint('0'   ).IsMark() );  // digit
		CHECK_FALSE ( KCodePoint(' '   ).IsMark() );  // space
		CHECK_FALSE ( KCodePoint(0x0E01).IsMark() );  // Thai ko kai (Lo, a base letter)
		CHECK_FALSE ( KCodePoint(0x0939).IsMark() );  // Devanagari HA (Lo)
		CHECK_FALSE ( KCodePoint(0x200D).IsMark() );  // zero width joiner (Cf, not a mark)
		CHECK_FALSE ( KCodePoint(0x00B4).IsMark() );  // spacing acute accent (Sk, not combining)

		CHECK_FALSE ( kIsMark('a') );
		CHECK_FALSE ( kIsMark(0x200D) );
	}

}

#endif // Windows
