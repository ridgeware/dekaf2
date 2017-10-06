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

/*
 * NOTE ON THE LACK OF ALIGNED LOADS
 *
 * Originally we looked for aligned memory locations and did aligned loading
 * whenever possible. However, in performance testing, the same code with all
 * ifs still in there was just as fast if not faster if all load calls were
 * swapped for unaligned loads.
 *
 * Therefore it seems for our purposes aligned loading doesn't gain us anything.
 * To make the code simpler, easier to understand, and more maintainable we
 * opted to just use unaligned loading.
 */

#include <algorithm>
#include "kfindfirstof.h"
#include "../kcppcompat.h"

namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
size_t kFindFirstOfNoSSE(KStringView haystack, KStringView needles, bool bNot)
//-----------------------------------------------------------------------------
{
	bool table[256];
	std::memset(table, false, 256);

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
	bool table[256];
	std::memset(table, false, 256);

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


#ifndef __SSE__
//#ifndef __SSE4_2__

namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
size_t kFindFirstOfSSE42(
        const KStringView haystack,
        const KStringView needles,
        bool bNot)
//-----------------------------------------------------------------------------
{
	return kFindFirstOfNoSSE(haystack, needles, bNot);
}

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfSSE42(
        const KStringView haystack,
        const KStringView needles,
        bool bNot)
//-----------------------------------------------------------------------------
{
	return kFindFirstNotOfNoSSE(haystack, needles, bNot);
}

//-----------------------------------------------------------------------------
size_t kFindLastOfSSE42(
        const KStringView haystack,
        const KStringView needles,
        bool bNot)
//-----------------------------------------------------------------------------
{
	return kFindLastOfNoSSE(haystack, needles, bNot);
}

//-----------------------------------------------------------------------------
size_t kFindLastNotOfSSE42(
        const KStringView haystack,
        const KStringView needles,
        bool bNot)
//-----------------------------------------------------------------------------
{
	return kFindLastNotOfNoSSE(haystack, needles, bNot);
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


namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
inline size_t portableCLZ(uint32_t value)
//-----------------------------------------------------------------------------
{
	if (!value) return 8 * sizeof(value);
#ifdef __GNUC__
	return static_cast<size_t>(__builtin_clz(value));
#elif _MSVC
	uint32_t clz = 0;
	if (_BitScanForward(&clz, value))
	{
		return (8 * sizeof(value) - 1) - static_cast<size_t>(clz);
	}
	else
	{
		return 8 * sizeof(value);
	}
#else
	abort();
#endif
}

//-----------------------------------------------------------------------------
inline size_t portableCTZ(uint32_t value)
//-----------------------------------------------------------------------------
{
	if (!value) return 8 * sizeof(value);
#ifdef __GNUC__
	return static_cast<size_t>(__builtin_ctz(value));
#elif _MSVC
	uint32_t ctz = 0;
	if (_BitScanReverse(&ctz, value))
	{
		return (8 * sizeof(value) - 1) - static_cast<size_t>(ctz);
	}
	else
	{
		return 8 * sizeof(value);
	}
#else
	abort();
#endif
}

// It's okay if pages are bigger than this (as powers of two), but they should
// not be smaller.
static constexpr size_t kMinPageSize = 4096;
static_assert(kMinPageSize >= 16, "kMinPageSize must be at least SSE register size");

//-----------------------------------------------------------------------------
template <typename T>
inline uintptr_t page_for(T* addr)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<uintptr_t>(addr) / kMinPageSize;
}

//-----------------------------------------------------------------------------
inline size_t nextAlignedIndex(const char* arr, size_t startIndex = 0)
//-----------------------------------------------------------------------------
{
	auto firstPossible = reinterpret_cast<uintptr_t>(arr) + 1;
	return 1 + // add 1 because the index starts at 'arr'
	        ((firstPossible + 15) & ~0xF) // round up to next multiple of 16
	        - firstPossible + startIndex;
}

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
size_t kFindFirstOfNeedles16(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	if (// must bail if we can't even SSE-load a single segment of haystack
	    (haystack.size() < 16 &&
	     page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindFirstOfNoSSE(haystack, needles, false);
	}

	__m128i arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	__m128i arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size()), 0);
	if (index < 16)
	{
		return size_t(index);
	}

	// Now, we can do aligned loads hereafter...
	size_t i = nextAlignedIndex(haystack.data());
	for (; i < haystack.size(); i += 16)
	{
		arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size() - i), 0);
		if (index < 16)
		{
			return i + static_cast<size_t>(index);
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
size_t kFindFirstNotOfNeedles16(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	if (// must bail if we can't even SSE-load a single segment of haystack
	    (haystack.size() < 16 &&
	     page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindFirstOfNoSSE(haystack, needles, true);
	}

	__m128i arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	__m128i arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size()), 0b00010000);
	if (index < std::min(16, static_cast<int>(haystack.size())))
	{
		return size_t(index);
	}

	// Now, we can do aligned loads hereafter...
	size_t i = nextAlignedIndex(haystack.data());
	for (; i < haystack.size(); i += 16)
	{
		arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size() - i), 0b00010000);
		if (index < std::min(16, static_cast<int>(haystack.size() - i)))
		{
			return i + static_cast<size_t>(index);
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
size_t kFindLastNotOfNeedles16(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	// Account for haystack < 16 ONCE
	size_t haystackSize = haystack.size(); // Significantly faster to call this only once 30% reduction in run time
	size_t needleSize = needles.size(); // Additional 25% reduction
	const char* haystackData = haystack.data(); // Additional 25% reduction
	int useSize = std::min(16, static_cast<int>(haystackSize));


	__m128i arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	__m128i arr1;
	long index;

	int i = haystackSize - useSize;
	// if i = 0 either haystack < 16 or mutliple of 16
	// then avoid extra comparison
	for (; i > 0; i -= 16)
	{
		arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystackData + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needleSize),
		                     arr1, haystackSize - i,
		                     0b01110000);
		if (index < std::min(16, static_cast<int>(haystackSize - i)))
		{
			return i + static_cast<size_t>(index);
		}
	}

	// Load first block of haystack
	arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystackData));
	index = _mm_cmpestri(arr2, static_cast<int>(needleSize),
	                     arr1, useSize,
	                     0b01110000);
	if (index < useSize)
	{
		return static_cast<size_t>(index);
	}

	return KStringView::npos;

}

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
size_t kFindLastOfNeedles16(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	//  Calculate these values once
	int useSize = std::min(16, static_cast<int>(haystack.size()));
	int needleSize = needles.size();
	const char * haystackData = haystack.data();
	size_t haystackSize = haystack.size();

	__m128i arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	__m128i arr1;
	long index;

	// Load Haystack from the back
	// If haystack size < 16 or a multiple of 16 The last
	// (and possibly only) check is done outside the loop
	int i = haystackSize - useSize;
	for (; i > 0; i -= 16)
	{
		arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystackData + i));
		index = _mm_cmpestri(arr2, needleSize, arr1, haystackSize - i, 0b01000000);
		if (index < useSize)
		{
			return i + static_cast<size_t>(index);
		}
	}

	// Load first bit of haystack
	arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystackData));
	index = _mm_cmpestri(arr2, needleSize, arr1, useSize, 0b01000000);
	if (index < useSize)
	{
		return static_cast<size_t>(index);
	}

	return KStringView::npos;

}


//-----------------------------------------------------------------------------
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// needle. If HAYSTACK_ALIGNED, then haystack must be 16byte aligned.
// If !HAYSTACK_ALIGNED, then caller must ensure that it is safe to load the
// block.
template <bool HAYSTACK_ALIGNED>
size_t scanHaystackBlock(
        const KStringView haystack,
        const KStringView needles,
        uint64_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	__m128i arr1;
	if (HAYSTACK_ALIGNED)
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	// This load is safe because needles.size() >= 16
	auto arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	auto b    = _mm_cmpestri(arr2, 16, arr1, static_cast<int>(haystack.size() - blockStartIdx), 0);

	size_t j = nextAlignedIndex(needles.data());
	for (; j < needles.size(); j += 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

		auto index = _mm_cmpestri(
		                 arr2,
		                 static_cast<int>(needles.size() - j),
		                 arr1,
		                 static_cast<int>(haystack.size() - blockStartIdx),
		                 0);
		b = std::min(index, b);
	}

	if (b < 16) {
		return blockStartIdx + static_cast<size_t>(b);
	}
	return KStringView::npos;
}

//-----------------------------------------------------------------------------
template <bool HAYSTACK_ALIGNED>
size_t scanHaystackBlockNot(
        const KStringView haystack,
        const KStringView needles,
        uint64_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	__m128i arr1;
	if (HAYSTACK_ALIGNED)
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	// This load is safe because needles.size() >= 16
	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	__m128i mask = _mm_cmpestrm(arr2, 16, arr1, static_cast<int>(haystack.size() - blockStartIdx), 0b00000000);

	size_t haystackSize = haystack.size();
	haystackSize = haystackSize;
	size_t j = nextAlignedIndex(needles.data());
	for (; j < (needles.size() - 16); j += 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

		mask |= _mm_cmpestrm(
		                 arr2,
		                 static_cast<int>(needles.size() - j),
		                 arr1,
		                 static_cast<int>(haystack.size() - blockStartIdx),
		                 0b00000000);
	}
	// Ensure all bytes loaded into needle are from the intended data
	j = needles.size() - 16;

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

	mask |= _mm_cmpestrm(
	            arr2,
	            static_cast<int>(needles.size() - j),
	            arr1,
	            static_cast<int>(haystack.size() - blockStartIdx),
	            0b00000000);

	uint16_t* val = reinterpret_cast<uint16_t*>(&mask);
	if (val)
	{
		// What this does is find the first 0, which means
		// we need to count trailing 1's
		// GCC only provides counting trailing (or leading) 0's
		// The trailing side according to the GCC implementation is before our data
		// The leading is after , this is because of the Little Endian architechture.
		// For leading 0's the first 16 aren't even "ours",
		// we use a uint16_t to look at the mask and clz takes uint32_t.

		*val = ~*val;
		auto b = portableCTZ(*val);

		if (b < std::min(16UL, haystack.size() - blockStartIdx))
		{
			return blockStartIdx + static_cast<size_t>(b);
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// needle. If HAYSTACK_ALIGNED, then haystack must be 16byte aligned.
// If !HAYSTACK_ALIGNED, then caller must ensure that it is safe to load the
// block.
size_t reverseScanHaystackBlock(
        const KStringView haystack,
        const KStringView needles,
        uint64_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	// Calculate once, instead of multiple times
	size_t haystackSize = haystack.size();
	const char * needleData = needles.data();
	size_t needleSize = needles.size();
	// Calculating this once, instead of in every loop
	int useSize = std::min(16, static_cast<int>(haystackSize - blockStartIdx));

	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));

	// This load is safe because needles.size() > 16
	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	int b = _mm_cmpestri(arr2, 16, arr1, useSize, 0b01000000);

	size_t j = 16;
	for (; j < needleSize; j += 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		auto index = _mm_cmpestri(
		                 // This could be 16 if I take the last load out of the loop
		                 arr2, static_cast<int>(needleSize - j),
		                 arr1, useSize,
		                 0b01000000);
		if (index >= useSize) continue;
		if (DEKAF2_UNLIKELY(b >= useSize)) // if b is initially 16, then I won't get the largest value smaller
		{
			b = index;
		}
		else
		{
			b = std::max(index, b);
		}
	}

	if (b < 16) {
		return blockStartIdx + static_cast<size_t>(b);
	}
	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t reverseScanHaystackBlockNot(
        const KStringView haystack,
        const KStringView needles,
        uint64_t blockStartIdx)
//-----------------------------------------------------------------------------
{
	size_t haystackSize = haystack.size();
	const char * needleData = needles.data();
	size_t needleSize = needles.size();
	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));


	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));
	// We need to set the mask with an = before we can do |= safely.
	// So the first call must not be in the loop for efficiency
	// Testing revealed that whatever junk is in the memory location
	// before initialization effects the value of the mask.
	__m128i mask = _mm_cmpestrm(arr2, 16,
	                            arr1, static_cast<int>(haystackSize - blockStartIdx),
	                            0);

	size_t j = 16;
	for (; j < (needleSize - 16); j += 16)
	{
		// This load is safe because needles.size() > 16
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		mask |= _mm_cmpestrm(
		                 arr2, 16, //static_cast<int>(needleSize - j),
		                 arr1, static_cast<int>(haystackSize - blockStartIdx),
		                 0);
	}

	// Ensure all bytes loaded into needle are from the intended data
	j = needles.size() - 16;

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

	mask |= _mm_cmpestrm(
	            arr2, 16, //static_cast<int>(needles.size() - j),
	            arr1, static_cast<int>(haystackSize - blockStartIdx),
	            0);

	uint16_t* val = reinterpret_cast<uint16_t*>(&mask);
	if (val)
	{

		// What this does is find the last 0, which means
		// we need to count leading 1's
		// GCC only provides counting leading (or trailing) 0's
		// The trailing side according to the GCC implementation is before our data
		// The leading is after , this is because of the Little Endian architechture.
		// For leading 0's the first 16 aren't even "ours",
		// we use a uint16_t to look at the mask and clz takes uint_32t.
		size_t useSize = static_cast<size_t>(std::min(16, static_cast<int>(haystackSize - blockStartIdx)));

		*val = ~*val; // Invert bits
		if (useSize != 16)
		{
			// If we are NOT looking at a full 16 chars, we have some erroneous leading 1's
			*val = *val << (16 - useSize);
			*val = *val >> (16 - useSize);
		}
		auto b = 32 - (portableCLZ(*val) + 1); // CLZ + 1 will be the last thing that was a 0

		if (b < useSize) // b can only be valid if the index is within the haystack
		{
			return blockStartIdx + static_cast<size_t>(b);
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindFirstOfSSE(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return std::string::npos;
	}
	else if (needles.size() <= 16)
	{
		// we can save some unnecessary load instructions by optimizing for
		// the common case of needles.size() <= 16
		return kFindFirstOfNeedles16(haystack, needles);
	}

	if (haystack.size() < 16 &&
	    page_for(haystack.end() - 1) != page_for(haystack.data() + 16))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindFirstOfNoSSE(haystack, needles, false);
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
		if (ret != std::string::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfSSE(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return std::string::npos;
	}
	else if (needles.size() <= 16)
	{
		// we can save some unnecessary load instructions by optimizing for
		// the common case of needles.size() <= 16
		return kFindFirstNotOfNeedles16(haystack, needles);
	}

	if (haystack.size() < 16 &&
	    page_for(haystack.end() - 1) != page_for(haystack.data() + 16))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindFirstOfNoSSE(haystack, needles, true);
	}

	auto ret = scanHaystackBlockNot<false>(haystack, needles, 0);
	if (ret != KStringView::npos)
	{
		return ret;
	}

	size_t i = nextAlignedIndex(haystack.data());
	for (; i < (haystack.size()); i += 16)
	{
		ret = scanHaystackBlockNot<true>(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindLastOfSSE(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{	
	size_t haystackSize = haystack.size();
	//return kFindLastOfNoSSE(haystack, needles, false);
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}
	/*
	else if (// must bail if we can't even SSE-load a single segment of haystack
		( haystackSize < 16 &&
	    page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindLastOfNoSSE(haystack, needles, false);
	}
	*/
	else if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastOfNeedles16(haystack, needles);
	}

	// Account for haystack < 16
	int useSize = std::min(16, static_cast<int>(haystackSize));

	size_t ret;
	// if i = 0 either haystack < 16 or mutliple of 16
	// then avoid extra call to reverseScanHaystackBlockNot
	int i = haystackSize - useSize;
	for (; i > 0; i -= 16) // i > 0, i is unsigned
	{
		ret = reverseScanHaystackBlock(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
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
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	size_t haystackSize = haystack.size(); // Significantly faster to call this only once 30% reduction in run time

	//return kFindLastOfNoSSE(haystack, needles, true);
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}
	/*
	else if (// must bail if we can't even SSE-load a single segment of haystack
		(haystackSize < 16 &&
	     page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		// Haystack crosses page boundary, SSE is slower across 2 pages
		return kFindLastOfNoSSE(haystack, needles, true);
	}
	*/
	else if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastNotOfNeedles16(haystack, needles);
	}

	// Account for haystack < 16
	int useSize = std::min(16, static_cast<int>(haystackSize));

	size_t ret;

	// if i = 0 either haystack < 16 or mutliple of 16
	// then avoid extra call to reverseScanHaystackBlockNot
	int i = haystackSize - useSize;
	for (; i > 0; i -= 16)
	{
		ret = reverseScanHaystackBlockNot(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
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
