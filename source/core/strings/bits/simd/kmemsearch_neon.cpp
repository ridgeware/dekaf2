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
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
// |/|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |/|
// |\|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |\|
// |/|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |/|
// |\|                                                                     |\|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#include <dekaf2/core/strings/bits/simd/kmemsearch_neon.h>

#if DEKAF2_HAS_NEON

#include <arm_neon.h>
#include <dekaf2/core/types/kbit.h>
#include <cstring>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN
namespace detail {
namespace neon   {

namespace {

//-----------------------------------------------------------------------------
// Nibble-per-byte movemask for NEON. Given a 16-byte comparison result with
// lanes that are either 0x00 or 0xFF, produce a 64-bit value where each
// 4-bit nibble corresponds to one input lane: 0xF for a match, 0x0 for a
// non-match.
//
// Trick: 'vshrn_n_u16(cmp, 4)' treats the 16 bytes as 8 u16 values, shifts
// each u16 right by 4, and narrows to 8 bits. For a comparison result, each
// 2-byte pair (b0, b1) becomes a single byte ((b0>>4) | (b1<<4)) & 0xFF,
// which interleaves the two matching flags into the low and high nibble.
//
// See https://danlark.org/2022/04/20/changing-stdsort-at-googles-scale-and-beyond/
// and the equivalent pattern in glibc / simdjson / folly.
//
// First match byte index:  kBitCountRightZero(mask) >> 2
// Last  match byte index:  (63 - kBitCountLeftZero(mask)) >> 2
//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
uint64_t NibbleMask(uint8x16_t cmp) noexcept
{
	return vget_lane_u64(
	           vreinterpret_u64_u8(
	               vshrn_n_u16(vreinterpretq_u16_u8(cmp), 4)),
	           0);
}

} // anon

//-----------------------------------------------------------------------------
void* kMemRChr(const void* s, int c, std::size_t n) noexcept
//-----------------------------------------------------------------------------
{
	if (!n)
	{
		return nullptr;
	}

	const uint8_t* const pBase = static_cast<const uint8_t*>(s);
	const uint8_t        target = static_cast<uint8_t>(c);
	const uint8x16_t     vTarget = vdupq_n_u8(target);

	// we scan from high address towards pBase, in 16-byte chunks.
	// pEnd is the one-past-end marker, reduced by 16 each step.
	const uint8_t* pEnd = pBase + n;

	while (n >= 16)
	{
		pEnd -= 16;
		n    -= 16;

		uint8x16_t chunk = vld1q_u8(pEnd);
		uint8x16_t cmp   = vceqq_u8(chunk, vTarget);
		uint64_t   mask  = NibbleMask(cmp);

		if (mask)
		{
			// last match within this 16-byte window
			int idx = (63 - kBitCountLeftZero(mask)) >> 2;
			return const_cast<uint8_t*>(pEnd + idx);
		}
	}

	// tail: up to 15 remaining bytes at [pBase, pBase + n)
	// scan from end towards start
	while (n)
	{
		--n;
		if (pBase[n] == target)
		{
			return const_cast<uint8_t*>(pBase + n);
		}
	}

	return nullptr;

} // kMemRChr

namespace {

//-----------------------------------------------------------------------------
// NEON two-byte filter body, called when we've decided that memchr+last-byte
// is losing (i.e. the first byte of the needle is dense in the haystack).
// pStart must point into the original haystack and iBytesLeft must be the
// number of bytes from pStart to the end of the haystack.
//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
void* kMemMemNeonFilter(const uint8_t* pStart,
                        std::size_t    iBytesLeft,
                        const uint8_t* pNeedle,
                        std::size_t    iNeedleSize) noexcept
{
	// classical first-and-last-byte filter (used by glibc, musl, folly, ...):
	//
	//   at every window of 16 candidate positions, check whether
	//     haystack[i]               == needle[0]    (first byte)
	//   AND
	//     haystack[i + N - 1]       == needle[N-1]  (last byte)
	//   simultaneously in SIMD. Only windows where the AND mask is non-zero
	//   proceed to a byte-wise memcmp of the middle bytes.
	//
	// this filters out >99% of the work when needle[0] is common but
	// needle[N-1] is not (the classic pathological case for a memchr+memcmp
	// strategy).

	const uint8x16_t vFirst = vdupq_n_u8(pNeedle[0]);
	const uint8x16_t vLast  = vdupq_n_u8(pNeedle[iNeedleSize - 1]);

	// iMax is the highest valid starting offset, inclusive
	const std::size_t iMax = iBytesLeft - iNeedleSize;

	std::size_t i = 0;

	if (iMax >= 15)
	{
		const std::size_t iNeonEnd = iMax - 14;

		for (; i < iNeonEnd; i += 16)
		{
			uint8x16_t hFirst = vld1q_u8(pStart + i);
			uint8x16_t hLast  = vld1q_u8(pStart + i + iNeedleSize - 1);
			uint8x16_t cmp    = vandq_u8(vceqq_u8(hFirst, vFirst),
			                             vceqq_u8(hLast,  vLast));
			uint64_t   mask   = NibbleMask(cmp);

			while (mask)
			{
				int bit = kBitCountRightZero(mask);
				int idx = bit >> 2;

				if (iNeedleSize == 2 ||
				    std::memcmp(pStart  + i + idx + 1,
				                pNeedle + 1,
				                iNeedleSize - 2) == 0)
				{
					return const_cast<uint8_t*>(pStart + i + idx);
				}

				mask &= ~(static_cast<uint64_t>(0xF) << bit);
			}
		}
	}

	// tail: the last (iMax - i + 1) positions
	for (; i <= iMax; ++i)
	{
		if (pStart[i]                   == pNeedle[0] &&
		    pStart[i + iNeedleSize - 1] == pNeedle[iNeedleSize - 1] &&
		    (iNeedleSize == 2 ||
		     std::memcmp(pStart  + i + 1,
		                 pNeedle + 1,
		                 iNeedleSize - 2) == 0))
		{
			return const_cast<uint8_t*>(pStart + i);
		}
	}

	return nullptr;

} // kMemMemNeonFilter

} // anon

//-----------------------------------------------------------------------------
void* kMemMem(const void* haystack,
              std::size_t  iHaystackSize,
              const void*  needle,
              std::size_t  iNeedleSize) noexcept
//-----------------------------------------------------------------------------
{
	// empty needle matches at the start of any haystack (including a null one)
	if (!iNeedleSize)
	{
		return const_cast<void*>(haystack);
	}

	if (!haystack || !needle || iNeedleSize > iHaystackSize)
	{
		return nullptr;
	}

	const uint8_t* const pHaystack = static_cast<const uint8_t*>(haystack);
	const uint8_t* const pNeedle   = static_cast<const uint8_t*>(needle);

	// single-byte needle: Apple's memchr is already NEON-accelerated and
	// faster than any inner-loop we could write here.
	if (iNeedleSize == 1)
	{
		return std::memchr(const_cast<uint8_t*>(pHaystack), pNeedle[0], iHaystackSize);
	}

	// adaptive strategy for iNeedleSize >= 2:
	//
	// Phase 1: use Apple's memchr (~43 GB/s on M-series) to locate the first
	//          byte, then quickly reject the position via a last-byte check
	//          before falling back to memcmp on the middle bytes. This is
	//          the fastest path when the first byte is rare, because the
	//          memchr scan is single-pass and we only verify on actual hits.
	//
	// Phase 2: if we accumulate too many consecutive first-byte false starts,
	//          the first byte must be dense in the haystack. Memchr is then
	//          being restarted very often, which kills throughput. In that
	//          case we switch to the NEON first-and-last-byte filter, which
	//          processes 16 candidate positions per iteration regardless of
	//          match density.
	//
	// This keeps the normal-case performance equivalent to Apple's memchr
	// while still delivering >100x speedups in the pathological case where
	// the first needle byte is everywhere in the haystack.

	const uint8_t fb = pNeedle[0];
	const uint8_t lb = pNeedle[iNeedleSize - 1];

	const uint8_t* pCur      = pHaystack;
	std::size_t    remaining = iHaystackSize - iNeedleSize + 1;

	// empirically chosen threshold: 8 false starts in a row is enough to
	// distinguish "rare first byte" from "dense first byte" while keeping
	// the phase 2 switch cost negligible in the common case.
	constexpr int kSwitchThreshold = 8;
	int           iMisses         = 0;

	while (remaining)
	{
		const uint8_t* pFound =
		    static_cast<const uint8_t*>(std::memchr(pCur, fb, remaining));

		if (DEKAF2_UNLIKELY(!pFound))
		{
			return nullptr;
		}

		// last-byte quick reject: a single byte compare often eliminates the
		// position before we pay the cost of memcmp on the middle bytes.
		if (pFound[iNeedleSize - 1] == lb)
		{
			if (iNeedleSize == 2 ||
			    std::memcmp(pFound + 1, pNeedle + 1, iNeedleSize - 2) == 0)
			{
				return const_cast<uint8_t*>(pFound);
			}
		}

		if (DEKAF2_UNLIKELY(++iMisses >= kSwitchThreshold))
		{
			// first byte is dense -> switch to NEON two-byte filter for the
			// rest of the haystack. We resume at pFound, including it, so no
			// candidate position is lost.
			const std::size_t iOffset = static_cast<std::size_t>(pFound - pHaystack);
			return kMemMemNeonFilter(pFound,
			                         iHaystackSize - iOffset,
			                         pNeedle,
			                         iNeedleSize);
		}

		const std::size_t iAdvance = static_cast<std::size_t>(pFound - pCur) + 1;
		pCur      += iAdvance;
		remaining -= iAdvance;
	}

	return nullptr;

} // kMemMem

//-----------------------------------------------------------------------------
std::size_t kFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept
//-----------------------------------------------------------------------------
{
	if (!iScanLen)
	{
		return npos;
	}

	const uint8_t* const pBase   = reinterpret_cast<const uint8_t*>(pData);
	const uint8x16_t     vNeedle = vdupq_n_u8(static_cast<uint8_t>(needle));

	std::size_t i = 0;

	while (i + 16 <= iScanLen)
	{
		uint8x16_t chunk = vld1q_u8(pBase + i);
		// eq  : 0xFF if byte == needle, 0x00 otherwise
		uint8x16_t eq    = vceqq_u8(chunk, vNeedle);
		// neq : 0xFF if byte != needle, 0x00 otherwise
		uint8x16_t neq   = vmvnq_u8(eq);
		uint64_t   mask  = NibbleMask(neq);

		if (mask)
		{
			int idx = kBitCountRightZero(mask) >> 2;
			return i + static_cast<std::size_t>(idx);
		}

		i += 16;
	}

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

	const uint8_t* const pBase  = reinterpret_cast<const uint8_t*>(pData);
	const uint8x16_t     vNeedle = vdupq_n_u8(static_cast<uint8_t>(needle));

	std::size_t n = iScanLen;

	while (n >= 16)
	{
		n -= 16;

		uint8x16_t chunk = vld1q_u8(pBase + n);
		// eq  : 0xFF if byte == needle, 0x00 otherwise
		uint8x16_t eq    = vceqq_u8(chunk, vNeedle);
		// neq : 0xFF if byte != needle, 0x00 otherwise
		uint8x16_t neq   = vmvnq_u8(eq);
		uint64_t   mask  = NibbleMask(neq);

		if (mask)
		{
			int idx = (63 - kBitCountLeftZero(mask)) >> 2;
			return n + static_cast<std::size_t>(idx);
		}
	}

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

namespace {

//-----------------------------------------------------------------------------
// Build an OR-reduced comparison mask of a 16-byte haystack chunk against
// up to 16 needle broadcasts. Each lane of the returned vector is 0xFF if the
// corresponding haystack byte matches at least one needle, 0x00 otherwise.
//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
uint8x16_t CompareMatchAny(uint8x16_t chunk,
                           const uint8x16_t* vNeedles,
                           std::size_t iNeedleCount) noexcept
{
	uint8x16_t cmp = vceqq_u8(chunk, vNeedles[0]);

	for (std::size_t k = 1; k < iNeedleCount; ++k)
	{
		cmp = vorrq_u8(cmp, vceqq_u8(chunk, vNeedles[k]));
	}

	return cmp;
}

//-----------------------------------------------------------------------------
// Scalar per-byte match against up to 16 needles. Returns true iff byte is
// in the needle set.
//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool ScalarMatchAny(uint8_t byte, const char* pNeedles, std::size_t iNeedleCount) noexcept
{
	for (std::size_t k = 0; k < iNeedleCount; ++k)
	{
		if (byte == static_cast<uint8_t>(pNeedles[k]))
		{
			return true;
		}
	}

	return false;
}

} // anon

//-----------------------------------------------------------------------------
std::size_t kFindFirstOf(const char*  pHaystack,
                         std::size_t  iHaystackLen,
                         std::size_t  pos,
                         const char*  pNeedles,
                         std::size_t  iNeedleCount,
                         bool         bNot) noexcept
//-----------------------------------------------------------------------------
{
	if (pos >= iHaystackLen || iNeedleCount == 0)
	{
		// empty needle set: find_first_of -> npos; find_first_not_of -> pos (first position past start)
		// but this is only called from KFindSetOfChars with iNeedleCount >= 1, so treat as npos.
		return npos;
	}

	// pre-broadcast all needle bytes; iNeedleCount is guaranteed <= 16 by the
	// caller.
	uint8x16_t vNeedles[16];

	for (std::size_t k = 0; k < iNeedleCount; ++k)
	{
		vNeedles[k] = vdupq_n_u8(static_cast<uint8_t>(pNeedles[k]));
	}

	const uint8_t* const pBase = reinterpret_cast<const uint8_t*>(pHaystack);
	std::size_t          i     = pos;

	// NEON main loop: 16 bytes per iteration
	while (i + 16 <= iHaystackLen)
	{
		uint8x16_t chunk = vld1q_u8(pBase + i);
		uint8x16_t cmp   = CompareMatchAny(chunk, vNeedles, iNeedleCount);

		if (bNot)
		{
			cmp = vmvnq_u8(cmp);
		}

		uint64_t mask = NibbleMask(cmp);

		if (mask)
		{
			int idx = kBitCountRightZero(mask) >> 2;
			return i + static_cast<std::size_t>(idx);
		}

		i += 16;
	}

	// Scalar tail: up to 15 remaining bytes
	while (i < iHaystackLen)
	{
		bool match = ScalarMatchAny(pBase[i], pNeedles, iNeedleCount);

		if (match != bNot)
		{
			return i;
		}

		++i;
	}

	return npos;

} // kFindFirstOf

//-----------------------------------------------------------------------------
std::size_t kFindLastOf(const char*  pHaystack,
                        std::size_t  iHaystackLen,
                        std::size_t  pos,
                        const char*  pNeedles,
                        std::size_t  iNeedleCount,
                        bool         bNot) noexcept
//-----------------------------------------------------------------------------
{
	if (iHaystackLen == 0 || iNeedleCount == 0)
	{
		return npos;
	}

	// clamp pos to the last valid offset
	std::size_t iEnd = (pos >= iHaystackLen) ? iHaystackLen : pos + 1;

	uint8x16_t vNeedles[16];

	for (std::size_t k = 0; k < iNeedleCount; ++k)
	{
		vNeedles[k] = vdupq_n_u8(static_cast<uint8_t>(pNeedles[k]));
	}

	const uint8_t* const pBase = reinterpret_cast<const uint8_t*>(pHaystack);
	std::size_t          n     = iEnd;

	// NEON main loop: process 16 bytes from the tail towards the start
	while (n >= 16)
	{
		n -= 16;

		uint8x16_t chunk = vld1q_u8(pBase + n);
		uint8x16_t cmp   = CompareMatchAny(chunk, vNeedles, iNeedleCount);

		if (bNot)
		{
			cmp = vmvnq_u8(cmp);
		}

		uint64_t mask = NibbleMask(cmp);

		if (mask)
		{
			int idx = (63 - kBitCountLeftZero(mask)) >> 2;
			return n + static_cast<std::size_t>(idx);
		}
	}

	// Scalar tail: the first 0..15 bytes of the scan window
	while (n)
	{
		--n;

		bool match = ScalarMatchAny(pBase[n], pNeedles, iNeedleCount);

		if (match != bNot)
		{
			return n;
		}
	}

	return npos;

} // kFindLastOf

} // end of namespace neon
} // end of namespace detail
DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_NEON
