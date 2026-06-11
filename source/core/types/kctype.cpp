/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#include <dekaf2/core/types/kctype.h>

DEKAF2_NAMESPACE_BEGIN

// we need to patch the unicode table such that 0x09..0x0d are of Type Separator
// (they are Control characters, but we don't care for that type)
// otherwise IsSpace() is incorrect
// The tab character also needs the Category SeparatorSpace, otherwise
// IsBlank() is incorrect

#include "unicodetables.cpp"

// const std::array<int32_t , MAX_CASEFOLDS + 1> CaseFolds;
// const std::array<Property, MAX_TABLE     + 1> CodePoints;

// 0x020000 - 0x03134A Lo with minor gaps
// 0x0E0001 - 0x0E007F Cf
// 0x0E0100 - 0x0E01EF Mn
// 0x0F0000 - 0x0FFFFD Co (private use)
// 0x100000 - 0x10FFFD Co (private use)

//-----------------------------------------------------------------------------
KCodePoint::Property KCodePoint::GetHighUnicodeProperty() const
//-----------------------------------------------------------------------------
{
	static constexpr Property Lo { LetterOther      , Letter , 0 };
	static constexpr Property Cf { OtherFormat      , Other  , 0 };
	static constexpr Property Mn { MarkNonspacing   , Mark   , 0 };
	static constexpr Property Co { OtherPrivateUse  , Other  , 0 };
	static constexpr Property On { OtherNotAssigned , Other  , 0 };

	if (m_CodePoint <= 0x0323AF) return Lo;
	if (m_CodePoint <  0x0E0001) return On;
	if (m_CodePoint <= 0x0E007F) return Cf;
	if (m_CodePoint <  0x0E0100) return On;
	if (m_CodePoint <= 0x0E01EF) return Mn;
	if (m_CodePoint <  0x0F0000) return On;
	if (m_CodePoint <= 0x10FFFD) return Co;

	return On;

} // GetHighUnicodeProperty

//-----------------------------------------------------------------------------
bool KCodePoint::IsIdeographic() const
//-----------------------------------------------------------------------------
{
	const auto cp = m_CodePoint;

	// bracketed range tree, ordered by frequency: a non-CJK codepoint is
	// rejected in one comparison, a CJK unified ideograph confirmed in three
	if (cp < 0x2E80) return false;             // ASCII, Latin, Greek, Cyrillic, Arabic, ... and most symbols

	if (cp <= 0x9FFF)                          // the dense BMP CJK zone
	{
		if (cp >= 0x4E00) return true;         // CJK unified ideographs (the bulk)
		if (cp <= 0x2FDF) return true;         // CJK radicals, Kangxi radicals
		if (cp >= 0x3400) return cp <= 0x4DBF; // CJK extension A (0x4DC0..0x4DFF Yijing -> false)

		return (cp >= 0x3040 && cp <= 0x30FF)  // Hiragana, Katakana
			|| (cp >= 0x3005 && cp <= 0x3007)  // ideographic iteration mark, closing mark, zero
			|| (cp >= 0x3031 && cp <= 0x3035)  // vertical kana repeat marks
			|| (cp >= 0x3100 && cp <= 0x312F)  // Bopomofo
			|| (cp >= 0x31A0 && cp <= 0x31BF)  // Bopomofo extended
			|| (cp >= 0x31F0 && cp <= 0x31FF); // Katakana phonetic extensions
	}

	if (cp <  0xAC00) return false;            // Yi and others
	if (cp <= 0xD7A3) return true;             // Hangul syllables
	if (cp <  0xF900) return false;
	if (cp <= 0xFAFF) return true;             // CJK compatibility ideographs
	if (cp >= 0xFF66 && cp <= 0xFF9D) return true; // halfwidth Katakana
	return (cp >= 0x20000 && cp <= 0x3FFFF);   // CJK extensions B and up (planes 2 and 3)

} // IsIdeographic

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr kutf::codepoint_t KCodePoint::MAX_ASCII;
constexpr kutf::codepoint_t KCodePoint::MAX_CASEFOLDS;
constexpr kutf::codepoint_t KCodePoint::MAX_TABLE;
constexpr std::array<KCodePoint::CTYPE, KCodePoint::MAX_ASCII + 1> KCodePoint::ASCIITable;

#endif

DEKAF2_NAMESPACE_END
