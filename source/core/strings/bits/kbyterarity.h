/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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

#pragma once

/// @file kbyterarity.h
/// the rarity of bytes in common text, to pick the rare bytes of a needle for a
/// substring search

#include <dekaf2/core/init/kdefinitions.h>
#include <cstddef>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
/// The rank of each byte value by its frequency in common text, 0 for the rarest.
/// The frequencies come from a mixed corpus of seven parts with equal weight: log
/// files of the system and of an IDE, C++ source code, English prose (man pages and
/// Markdown), JSON, HTML, European languages (translated program messages in 24
/// languages with Latin, Greek and Cyrillic script) and further scripts (Japanese,
/// Korean, Chinese, Arabic, Hebrew, Hindi, Thai and Vietnamese messages), each
/// language with equal weight in its part.
///
/// The UTF-8 lead bytes 0xC2 to 0xF4 rank above all other bytes, among themselves by
/// their frequency: they are frequent in the texts of their scripts, but rare in
/// ASCII text, so no single frequency fits them. And a lead byte never stands alone,
/// a continuation byte follows it, which selects the character far better - so a
/// needle with a character from 0x80 on is searched for by one of its continuation
/// bytes, or by one of its ASCII bytes if that is rarer.
///
/// Bytes that did not occur - most control characters, and 0xC0, 0xC1 and 0xF5 to
/// 0xFF, which UTF-8 never uses - rank below all others, by their value. Only the
/// order matters: a better ranking saves restarts of memchr, it does not change
/// results. genbyterarity.py in this directory computes the table.
constexpr uint8_t kByteRarity[256]
{
	/* 0x00 */   0,   1,   2,   3,   4,   5,   6,  41,  38, 181, 192,  39,   7,  43,   8,   9,
	/* 0x10 */  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  40,  21,  22,  23,  42,
	/* 0x20 */ 204,  49, 193,  54,  45, 124,  70, 102, 162, 161,  50,  74, 180, 201, 187, 182,
	/* 0x30 */ 184, 178, 174, 166, 160, 165, 163, 141, 164, 148, 183, 127, 140, 157, 142,  47,
	/* 0x40 */  69, 154, 118, 149, 136, 152, 131, 113, 112, 155,  68, 108, 150, 132, 144, 138,
	/* 0x50 */ 156,  67, 137, 169, 159, 120,  91, 103,  77,  64,  63, 147, 134, 146,  55, 167,
	/* 0x60 */  53, 200, 175, 191, 190, 203, 177, 179, 185, 199, 145, 173, 194, 186, 196, 197,
	/* 0x70 */ 189, 126, 195, 198, 202, 188, 176, 168, 158, 171, 139, 128,  86, 129,  44,  24,
	/* 0x80 */ 153, 151, 143, 125, 133, 100,  72, 107, 116,  78,  92,  88,  90,  98,  46,  73,
	/* 0x90 */  66,  71,  48,  51,  82, 109,  57,  84,  75,  93,  83,  59, 104,  95,  65,  60,
	/* 0xA0 */  76, 115,  52,  79, 170, 121,  61, 119,  96,  87, 101,  62,  56, 105,  81,  89,
	/* 0xB0 */ 135, 106,  94,  85,  99, 117,  58,  80, 172, 130, 114, 123, 111, 110, 122,  97,
	/* 0xC0 */  25,  26, 232, 251, 240, 233, 229, 205, 228, 206, 223, 207, 225, 208, 245, 236,
	/* 0xD0 */ 254, 249, 226, 209, 210, 211, 227, 237, 253, 250, 224, 212, 213, 214, 215, 216,
	/* 0xE0 */ 255, 243, 241, 252, 239, 248, 247, 244, 238, 235, 231, 242, 246, 234, 217, 230,
	/* 0xF0 */ 218, 219, 220, 221, 222,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,
};

//-----------------------------------------------------------------------------
/// the positions of two bytes of a needle to search for
struct KRareBytes
{
	std::size_t iRarest; ///< the position of the rarest byte
	std::size_t iSecond; ///< the position of the last byte, or of the first if the last is the rarest
};

//-----------------------------------------------------------------------------
/// Finds the rarest byte of a needle of at least two bytes, and a second byte: the
/// last one, or the first one if the last is the rarest. A substring search scans
/// for the rarest byte with memchr, and rejects most of its hits by the second one
/// before it compares the whole needle. Of equally rare bytes the first one counts.
/// This runs for each search, so the loop has no branches: it keeps the minimum of
/// rank and position in one number.
DEKAF2_NODISCARD inline
KRareBytes kFindRareBytes(const uint8_t* pNeedle, std::size_t iNeedleSize) noexcept
//-----------------------------------------------------------------------------
{
	// the rank in the upper half, the position in the lower one
	uint64_t iMin = static_cast<uint64_t>(kByteRarity[pNeedle[0]]) << 32;

	for (std::size_t i = 1; i < iNeedleSize; ++i)
	{
		auto iKey = (static_cast<uint64_t>(kByteRarity[pNeedle[i]]) << 32) | static_cast<uint32_t>(i);

		iMin = (iKey < iMin) ? iKey : iMin;
	}

	KRareBytes Rare;

	Rare.iRarest = static_cast<uint32_t>(iMin);
	Rare.iSecond = (Rare.iRarest == iNeedleSize - 1) ? 0 : iNeedleSize - 1;

	return Rare;

} // kFindRareBytes

} // end of namespace detail

DEKAF2_NAMESPACE_END
