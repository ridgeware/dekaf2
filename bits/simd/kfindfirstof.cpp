/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

/*
 *  The code for the SSE42 find_first_of() is taken (and modified) from Facebook folly.
 *  The code for the non-SSE version is from dekaf2.
 *  All code for the three siblings of find_first_of() is from dekaf2.
 */

// Original copyright note for the find_first_of_function():
/*
 * Copyright 2017 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <array>
#include "kfindfirstof.h"
#include "../kcppcompat.h"

namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
size_t kFindFirstOfNoSSE(KStringView haystack, KStringView needles, bool bNot)
//-----------------------------------------------------------------------------
{
	std::array<bool, 256> table {};

	for (auto c : needles)
	{
		table[static_cast<unsigned char>(c)] = true;
	}

	auto it = std::find_if(haystack.begin(),
	                       haystack.end(),
	                       [&table, bNot](const char c)
	{
		return table[static_cast<unsigned char>(c)] != bNot;
	});

	if (it == haystack.end())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(it - haystack.begin());
	}
}

//-----------------------------------------------------------------------------
size_t kFindLastOfNoSSE(KStringView haystack, KStringView needles, bool bNot)
//-----------------------------------------------------------------------------
{
	std::array<bool, 256> table {};

	for (auto c : needles)
	{
		table[static_cast<unsigned char>(c)] = true;
	}

	auto it = std::find_if(haystack.rbegin(),
						   haystack.rend(),
						   [&table, bNot](const char c)
	{
		return table[static_cast<unsigned char>(c)] != bNot;
	});

	if (it == haystack.rend())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>((it.base() - 1) - haystack.begin());
	}
}

} // end of namespace detail
} // end of namespace dekaf


// MSC does not offer a test for __SSE4_2__ support in the compiler,
// so we simply always assume it is there for MSC

#if (!defined(__SSE4_2__)) && (!defined _MSC_VER)

namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
size_t kFindFirstOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	return kFindFirstOfNoSSE(haystack, needles, false);
}

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	return kFindFirstOfNoSSE(haystack, needles, true);
}

//-----------------------------------------------------------------------------
size_t kFindLastOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	return kFindLastOfNoSSE(haystack, needles, false);
}

//-----------------------------------------------------------------------------
size_t kFindLastNotOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	return kFindLastOfNoSSE(haystack, needles, true);
}

} // end of namespace detail
} // end of namespace dekaf2

#else

#include <cstdint>
#include <limits>
#include <string>

#include <emmintrin.h>
#include <nmmintrin.h>
#include <smmintrin.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
size_t portableCLZ(uint32_t value)
//-----------------------------------------------------------------------------
{
	if (!value)
	{
		return 8 * sizeof(value);
	}
#ifdef __GNUC__
	return static_cast<size_t>(__builtin_clz(value));
#elif _MSC_VER
	unsigned long clz;
	_BitScanReverse(&clz, value);
	return (8 * sizeof(value) - 1) - static_cast<size_t>(clz);
#else
	static_assert(false, "portableCLZ: compiler not supported")
#endif
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
size_t portableCTZ(uint32_t value)
//-----------------------------------------------------------------------------
{
	if (!value)
	{
		return 8 * sizeof(value);
	}
#ifdef __GNUC__
	return static_cast<size_t>(__builtin_ctz(value));
#elif _MSC_VER
	unsigned long ctz;
	_BitScanForward(&ctz, value);
	return static_cast<size_t>(ctz);
#else
	static_assert(false, "portableCTZ: compiler not supported")
#endif
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
void OperatorOrEqual(__m128i& a, __m128i b)
//-----------------------------------------------------------------------------
{
#ifdef _MSC_VER
	a.m128i_i64[0] |= b.m128i_i64[0];
	a.m128i_i64[1] |= b.m128i_i64[1];
#else
	a |= b;
#endif
}

static constexpr size_t kMinPageSize = 4096;

//-----------------------------------------------------------------------------
template <typename T>
DEKAF2_ALWAYS_INLINE
uintptr_t PageFor(T* addr)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<uintptr_t>(addr) / kMinPageSize;
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
const char* EndAsCharPtr(const KStringView& sv)
//-----------------------------------------------------------------------------
{
	return sv.data() + sv.size();
}

//-----------------------------------------------------------------------------
template <typename T>
DEKAF2_ALWAYS_INLINE
T* align16(T* addr)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(addr) & (~0xf));
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
size_t nextAlignedIndex(const char* buffer)
//-----------------------------------------------------------------------------
{
	auto firstPossible = reinterpret_cast<uintptr_t>(buffer) + 1;
	return 1 + // add 1 because the index starts at 'buffer'
			((firstPossible + 15) & ~0xF) // round up to next multiple of 16
			- firstPossible;
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
const char* prevAlignedPointer(const char* buffer)
//-----------------------------------------------------------------------------
{
	return align16(buffer - 1) - 16;
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool UnalignedPageOverflow16(const KStringView& sv)
//-----------------------------------------------------------------------------
{
	return PageFor(EndAsCharPtr(sv) - 1) != PageFor(sv.data() + 16);
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool UnalignedPageOverflow16Reverse(const KStringView& sv)
//-----------------------------------------------------------------------------
{
	auto Addr = EndAsCharPtr(sv) - 1;
	return PageFor(Addr) != PageFor(Addr - 16);
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool UnalignedPageOverflow(const KStringView& sv)
//-----------------------------------------------------------------------------
{
	return sv.size() < 16 && UnalignedPageOverflow16(sv);
}

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool UnalignedPageOverflowReverse(const KStringView& sv)
//-----------------------------------------------------------------------------
{
	return sv.size() < 16 && UnalignedPageOverflow16Reverse(sv);
}

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
// caller must guarantee that short haystacks and needles don't cross page boundary
template<bool bNot, int iOperation>
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t kFindFirstOfNeedles16(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// do an unaligned load for first block of haystack
	auto arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data()));

	// compare first block with needles
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size(), iOperation);

	if (index < 16)
	{
		if (!bNot || index < static_cast<int>(haystack.size()))
		{
			return index;
		}

		return KStringView::npos;
	}

	if (haystack.size() > 16)
	{
		// prepare for aligned loads
		size_t i = nextAlignedIndex(haystack.data());

		// and loop through the blocks

		for (; i < haystack.size(); i += 16)
		{
			arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
			index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size() - i, iOperation);

			if (index < 16)
			{
				if (!bNot || index < static_cast<int>(haystack.size() - i))
				{
					return i + index;
				}
				break;
			}
		}
	}

	// not found
	return KStringView::npos;

} // kFindFirstNotOfNeedles16


//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
// caller must guarantee that short haystacks and needles don't cross page boundary
template<bool bNot, int iOperation>
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t kFindLastOfNeedles16(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// load first unaligned block
	auto arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(EndAsCharPtr(haystack) - 16));
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, 16, iOperation);

	if (index < 16)
	{
		// this time we need to take care that index does not point into something BELOW
		// our string
		size_t inv = 16 - index;

		if (inv <= haystack.size())
		{
			return haystack.size() - inv;
		}

		return KStringView::npos;
	}

	if (haystack.size() > 16)
	{
		// prepare for aligned loads

		const char* p = prevAlignedPointer(EndAsCharPtr(haystack));

		for (;;)
		{
			arr1  = _mm_load_si128(reinterpret_cast<const __m128i*>(p));
			index = _mm_cmpestri(arr2, needles.size(), arr1, 16, iOperation);

			if (index < 16)
			{
				size_t inv = 16 - index;
				size_t size = p + 16 - haystack.data();

				// need to check for underflow in last round..
				if (inv <= size)
				{
					// we're inside the string
					return size - inv;
				}
				// out, go away
				break;
			}

			if (p <= haystack.data())
			{
				break;
			}

			p -= 16;
		}
	}

	return KStringView::npos;

} // kFindLastOfNeedles16


//-----------------------------------------------------------------------------
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// needle. If bAligned, then haystack must be 16byte aligned.
// If !bAligned, then caller must ensure that it is safe to load the
// block.
template <bool bAligned>
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t scanHaystackBlock(
        KStringView haystack,
        KStringView needles,
        size_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	__m128i arr1;

	if (bAligned)
	{
		arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	// factoring this out and adding the min to 16 makes a big difference, nearly twice the speed
	int useSize = std::min(16, static_cast<int>(haystack.size() - blockStartIdx));

	// This load is safe because needles.size() >= 16
	auto arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	auto b = _mm_cmpestri(arr2, 16, arr1, useSize, 0);

	size_t j = nextAlignedIndex(needles.data());

	for (; j < needles.size(); j += 16)
	{
		arr2 = _mm_load_si128(reinterpret_cast<const __m128i*>(needles.data() + j));
		auto index = _mm_cmpestri(arr2, needles.size() - j, arr1, useSize, 0);
		b = std::min(index, b);
	}

	if (b < useSize)
	{
		return blockStartIdx + b;
	}

	return KStringView::npos;

} // scanHaystackBlock

//-----------------------------------------------------------------------------
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// not of needle. If bAligned, then haystack must be 16byte aligned.
// If !bAligned, then caller must ensure that it is safe to load the
// block.
template <bool bAligned>
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t scanHaystackBlockNot(
        KStringView haystack,
        KStringView needles,
        size_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	__m128i arr1;

	if (bAligned)
	{
		arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	size_t useSize = std::min(16, static_cast<int>(haystack.size() - blockStartIdx));

	// This load is safe because needles.size() >= 16
	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// Must initialize mask before looping with |=
	__m128i mask = _mm_cmpestrm(arr2, 16, arr1, useSize, 0b00000000);

	size_t j = nextAlignedIndex(needles.data());

	for (; j < needles.size(); j += 16)
	{
		arr2 = _mm_load_si128(reinterpret_cast<const __m128i*>(needles.data() + j));
		OperatorOrEqual(mask, _mm_cmpestrm(arr2, needles.size() - j, arr1, useSize, 0b00000000));
	}

	uint16_t val = *reinterpret_cast<uint16_t*>(&mask);

	// What this does is find the first 0, which means
	// we need to count trailing 1's
	// GCC only provides counting trailing (or leading) 0's

	auto b = portableCTZ(~val);

	if (b < useSize)
	{
		return blockStartIdx + b;
	}

	return KStringView::npos;

} // scanHaystackBlockNot

//-----------------------------------------------------------------------------
// this implementation still can overrun memory pages and cause a bus error
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// needle.
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t reverseScanHaystackBlock(
        KStringView haystack,
        KStringView needles,
        size_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	// Calculate once, instead of multiple times
	auto haystackSize = haystack.size();
	const char * needleData = needles.data();
	auto needleSize = needles.size();
	// Calculating this once, instead of in every loop
	int useSize = std::min(16, static_cast<int>(haystackSize - blockStartIdx));

	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));

	// This load is safe because needles.size() > 16
	__m128i arr2;// = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + needleSize - 16));
	// Initialize b before looping
	int b = 16;// = _mm_cmpestri(arr2, 16, arr1, useSize, 0b01000000);

	// maximum reasonable needle size for a 8 bit value type is 256,
	// converting to int (for signedness) is sufficient
	int j = static_cast<int>(needleSize) - 16;

	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		auto index = _mm_cmpestri(arr2, 16, arr1, useSize, 0b01000000);
		if (index >= useSize) continue;
		if (b >= useSize) // if b is initially 16, then I won't get the largest value smaller
		{
			b = index;
		}
		else
		{
			b = std::max(index, b);
		}
	}

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	auto index = _mm_cmpestri(arr2, 16, arr1, useSize, 0b01000000);

	if (index < useSize)
	{
		if (b >= useSize) // if b is initially 16, then I won't get the largest value smaller
		{
			b = index;
		}
		else
		{
			b = std::max(index, b);
		}
	}

	if (b < 16)
	{
		return blockStartIdx + static_cast<size_t>(b);
	}

	return KStringView::npos;

}

//-----------------------------------------------------------------------------
// this implementation still can overrun memory pages and cause a bus error
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t reverseScanHaystackBlockNot(
        KStringView haystack,
        KStringView needles,
        size_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	size_t haystackSize = haystack.size();
	const char * needleData = needles.data();
	size_t needleSize = needles.size();
	size_t useSize = std::min(16, static_cast<int>(haystackSize - blockStartIdx));
	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));

	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + needleSize - 16));
	// Ensure all bytes loaded into needle are from the intended data
	// We need to set the mask with an = before we can do |= safely.
	// So the first call must not be in the loop to prevent if statement
	// Testing revealed that whatever junk is in the memory location
	// before initialization affects the value of the mask.
	__m128i mask = _mm_cmpestrm(arr2, 16, arr1, useSize, 0);

	// maximum reasonable needle size for a 8 bit value type is 256,
	// converting to int (for signedness) is sufficient
	int j = static_cast<int>(needleSize) - 32;
	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));
		OperatorOrEqual(mask, _mm_cmpestrm(arr2, 16, arr1, useSize, 0));
	}

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	OperatorOrEqual(mask, _mm_cmpestrm(arr2, 16, arr1, useSize, 0));

	uint16_t val = *reinterpret_cast<uint16_t*>(&mask);

	// What this does is find the last 0, which means
	// we need to count leading 1's
	// GCC only provides counting leading (or trailing) 0's

	val = ~val; // Invert bits

	if (useSize != 16)
	{
		// If we are NOT looking at a full 16 chars, we have some erroneous leading 1's
		val = val << (16 - useSize);
		val = val >> (16 - useSize);
	}

	auto b = 32 - (portableCLZ(val) + 1); // CLZ + 1 will be the last thing that was a 0

	if (b < useSize) // b can only be valid if the index is within the haystack
	{
		return blockStartIdx + static_cast<size_t>(b);
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindFirstOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(UnalignedPageOverflow(haystack)))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindFirstOfNoSSE(haystack, needles, false);
	}

	if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		if (UnalignedPageOverflow16(needles))
		{
			return kFindFirstOfNoSSE(haystack, needles, true);
		}

		// we can save some unnecessary load instructions by optimizing for
		// the common case of needles.size() <= 16
		return kFindFirstOfNeedles16<false, 0>(haystack, needles);
	}

	auto ret = scanHaystackBlock<false>(haystack, needles, 0);

	if (ret != KStringView::npos)
	{
		return ret;
	}

	size_t i = nextAlignedIndex(haystack.data());

	for (; i < haystack.size(); i += 16)
	{
		ret = scanHaystackBlock<true>(haystack, needles, i);

		if (ret != KStringView::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;

} // kFindFirstOfSSE

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(UnalignedPageOverflow(haystack)))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindFirstOfNoSSE(haystack, needles, true);
	}

	if (needles.size() <= 16)
	{
		if (UnalignedPageOverflow16(needles))
		{
			return kFindFirstOfNoSSE(haystack, needles, true);
		}

		// we can save some unnecessary load instructions by optimizing for
		// the common case of needles.size() <= 16
		return kFindFirstOfNeedles16<true, 0b00010000>(haystack, needles);
	}

	auto ret = scanHaystackBlockNot<false>(haystack, needles, 0);

	if (ret != KStringView::npos)
	{
		return ret;
	}

	size_t i = nextAlignedIndex(haystack.data());

	for (; i < haystack.size(); i += 16)
	{
		ret = scanHaystackBlockNot<true>(haystack, needles, i);

		if (ret != KStringView::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;

} // kFindFirstNotOfSSE

//-----------------------------------------------------------------------------
size_t kFindLastOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{	
	size_t haystackSize = haystack.size();

	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}

	if (UnalignedPageOverflowReverse(haystack))
	{
		kFindLastOfNoSSE(haystack, needles, false);
	}

	if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastOfNeedles16<false, 0b01000000>(haystack, needles);
	}

	// Account for haystack < 16
	size_t useSize = std::min(16, static_cast<int>(haystackSize));

	size_t ret;

	if (haystackSize > useSize)
	{
		auto i = haystackSize - useSize;
		for (;;)
		{
			ret = reverseScanHaystackBlock(haystack, needles, i);

			if (ret != KStringView::npos)
			{
				return ret;
			}

			if (i <= 16)
			{
				break;
			}

			i -= 16;
		}
	}

	// Scan the first haystack block
	ret = reverseScanHaystackBlock(haystack, needles, 0);

	if (ret != KStringView::npos)
	{
		return ret;
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindLastNotOfSSE(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	size_t haystackSize = haystack.size(); // Significantly faster to call this only once 30% reduction in run time

	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}

	if (UnalignedPageOverflowReverse(haystack))
	{
		kFindLastOfNoSSE(haystack, needles, true);
	}

	if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastOfNeedles16<true, 0b01110000>(haystack, needles);
	}

	// Account for haystack < 16
	size_t useSize = std::min(16, static_cast<int>(haystackSize));

	size_t ret;

	if (haystackSize > useSize)
	{
		auto i = haystackSize - useSize;
		for (;;)
		{
			ret = reverseScanHaystackBlockNot(haystack, needles, i);

			if (ret != KStringView::npos)
			{
				return ret;
			}

			if (i <= 16)
			{
				break;
			}

			i -= 16;
		}
	}

	// Scan the first haystack block
	ret = reverseScanHaystackBlockNot(haystack, needles, 0);

	if (ret != KStringView::npos)
	{
		return ret;
	}


	return KStringView::npos;
}

} // end of namespace detail
} // end of namespace dekaf2

#endif
