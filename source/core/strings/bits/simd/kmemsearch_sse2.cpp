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
// |/|   distribute, sublicense, and/or sell copies of the Software,       |\|
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

#include <dekaf2/core/strings/bits/simd/kmemsearch_sse2.h>

#if DEKAF2_HAS_SSE2_MEMSEARCH

#include <dekaf2/core/types/kbit.h>
#include <emmintrin.h>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN
namespace detail {
namespace sse2   {

namespace {

// Build a 16-bit movemask from a SSE2 comparison result where each lane is
// either 0x00 or 0xFF. Bit i of the result is 1 iff lane i of cmp is 0xFF.
// The trailing / leading bit indices then directly map to byte offsets.
//
// First match byte index:  kBitCountRightZero(mask)
// Last  match byte index:  15 - kBitCountLeftZero(uint16_t(mask))
DEKAF2_ALWAYS_INLINE
uint32_t MoveMask(__m128i cmp) noexcept
{
	return static_cast<uint32_t>(_mm_movemask_epi8(cmp)) & 0xFFFFu;
}

} // anon

//-----------------------------------------------------------------------------
std::size_t kFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept
//-----------------------------------------------------------------------------
{
	if (!iScanLen)
	{
		return npos;
	}

	const uint8_t* const pBase   = reinterpret_cast<const uint8_t*>(pData);
	const __m128i        vNeedle = _mm_set1_epi8(static_cast<char>(needle));

	std::size_t i = 0;

	while (i + 16 <= iScanLen)
	{
		__m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBase + i));
		// eq_mask: bit j = 1 iff byte[j] == needle
		uint32_t eq_mask  = MoveMask(_mm_cmpeq_epi8(chunk, vNeedle));
		// neq_mask: bit j = 1 iff byte[j] != needle
		uint32_t neq_mask = (~eq_mask) & 0xFFFFu;

		if (neq_mask)
		{
			return i + static_cast<std::size_t>(kBitCountRightZero(neq_mask));
		}

		i += 16;
	}

	// scalar tail: up to 15 remaining bytes
	while (i < iScanLen)
	{
		if (pBase[i] != static_cast<uint8_t>(needle))
		{
			return i;
		}
		++i;
	}

	return npos;

} // kFindNotChar

//-----------------------------------------------------------------------------
std::size_t kRFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept
//-----------------------------------------------------------------------------
{
	if (!iScanLen)
	{
		return npos;
	}

	const uint8_t* const pBase   = reinterpret_cast<const uint8_t*>(pData);
	const __m128i        vNeedle = _mm_set1_epi8(static_cast<char>(needle));

	std::size_t n = iScanLen;

	while (n >= 16)
	{
		n -= 16;

		__m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBase + n));
		uint32_t eq_mask  = MoveMask(_mm_cmpeq_epi8(chunk, vNeedle));
		uint32_t neq_mask = (~eq_mask) & 0xFFFFu;

		if (neq_mask)
		{
			// highest set bit within the 16-bit mask
			int idx = 15 - kBitCountLeftZero(static_cast<uint16_t>(neq_mask));
			return n + static_cast<std::size_t>(idx);
		}
	}

	// scalar tail: the first 0..15 bytes of the scan window
	while (n)
	{
		--n;
		if (pBase[n] != static_cast<uint8_t>(needle))
		{
			return n;
		}
	}

	return npos;

} // kRFindNotChar

} // end of namespace sse2
} // end of namespace detail
DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SSE2_MEMSEARCH
