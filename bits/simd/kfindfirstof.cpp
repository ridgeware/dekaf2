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
 * NOTE ON THE LACK OF ALIGNED LOADS IN THE DEKAF2 CODE
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


#ifndef __SSE4_2__

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


namespace dekaf2 {
namespace detail {

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
size_t portableCLZ(uint32_t value)
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
DEKAF2_ALWAYS_INLINE
size_t portableCTZ(uint32_t value)
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

static constexpr size_t kMinPageSize = 4096;

//-----------------------------------------------------------------------------
template <typename T>
static inline uintptr_t page_for(T* addr)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<uintptr_t>(addr) / kMinPageSize;
}

//-----------------------------------------------------------------------------
template <typename T>
static inline T* align16(T* addr)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(addr) & (~0xf));
}

//-----------------------------------------------------------------------------
static inline size_t nextAlignedIndex(const char* buffer)
//-----------------------------------------------------------------------------
{
	auto firstPossible = reinterpret_cast<uintptr_t>(buffer) + 1;
	return 1 + // add 1 because the index starts at 'buffer'
			((firstPossible + 15) & ~0xF) // round up to next multiple of 16
			- firstPossible;
}

//-----------------------------------------------------------------------------
static inline const char* prevAlignedPointer(const char* buffer)
//-----------------------------------------------------------------------------
{
	return align16(buffer - 1) - 16;
}


//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
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
	if (// must bail if we can't even SSE-load a single segment of haystack
		(haystack.size() < 16 && page_for(haystack.data()) != page_for(haystack.data() + 15))
		// can't load needles into SSE register if it could cross page boundary
		|| page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindFirstOfNoSSE(haystack, needles, false);
	}

	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// do an unaligned load for first block of haystack
	auto arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data()));

	// compare first block with needles
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size(), 0);

	if (index < 16)
	{
		return index;
	}

	if (haystack.size() > 16)
	{

		// prepare for aligned loads
		size_t i = nextAlignedIndex(haystack.data());

		// and loop through the blocks

		for (; i < haystack.size(); i += 16)
		{
			arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
			index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size() - i, 0);

			if (index < 16)
			{
				return i + index;
			}
		}

	}

	// not found
	return KStringView::npos;

} // kFindFirstOfNeedles16

//-----------------------------------------------------------------------------
// helper method for case where needles.size() <= 16
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t kFindFirstNotOfNeedles16(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	if (// must bail if we can't even SSE-load a single segment of haystack
		(haystack.size() < 16 && page_for(haystack.data()) != page_for(haystack.data() + 15))
		// can't load needles into SSE register if it could cross page boundary
		|| page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindFirstOfNoSSE(haystack, needles, true);
	}

	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// do an unaligned load for first block of haystack
	auto arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data()));

	// compare first block with needles
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size(), 0b00010000);

	if (index < 16)
	{
		if (index < static_cast<int>(haystack.size()))
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
			index = _mm_cmpestri(arr2, needles.size(), arr1, haystack.size() - i, 0b00010000);

			if (index < 16)
			{
				if (index < static_cast<int>(haystack.size() - i))
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
	if (// must bail if we can't even SSE-load a single segment of haystack
		(haystack.size() < 16
		 && page_for(haystack.end() - 1)
		 != page_for(haystack.end() - 1 - 15))
		// can't load needles into SSE register if it could cross page boundary
		|| page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindLastOfNoSSE(haystack, needles, false);
	}

	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// load first unaligned block
	auto arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.end() - 16));
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, 16, 0b01000000);

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

		const char* p = prevAlignedPointer(haystack.end());

		for (;;)
		{
			arr1  = _mm_load_si128(reinterpret_cast<const __m128i*>(p));
			index = _mm_cmpestri(arr2, needles.size(), arr1, 16, 0b01000000);

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
// helper method for case where needles.size() <= 16
#ifdef NDEBUG
DEKAF2_ALWAYS_INLINE
#else
DEKAF2_NO_ASAN
#endif
size_t kFindLastNotOfNeedles16(
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	if (// must bail if we can't even SSE-load a single segment of haystack
		(haystack.size() < 16
		 && page_for(haystack.end() - 1)
		 != page_for(haystack.end() - 1 - 15))
		// can't load needles into SSE register if it could cross page boundary
		|| page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		return kFindLastOfNoSSE(haystack, needles, true);
	}

	// load needles
	auto arr2  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needles.data()));

	// load first unaligned block
	auto arr1  = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.end() - 16));
	auto index = _mm_cmpestri(arr2, needles.size(), arr1, 16, 0b01110000);

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

		const char* p = prevAlignedPointer(haystack.end());

		for (;;)
		{
			arr1  = _mm_load_si128(reinterpret_cast<const __m128i*>(p));
			index = _mm_cmpestri(arr2, needles.size(), arr1, 16, 0b01110000);

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

} // kFindLastNotOfNeedles16


//-----------------------------------------------------------------------------
// Scans a 16-byte block of haystack (starting at blockStartIdx) to find first
// needle.
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
	// Calculate these once
	//size_t haystackSize = haystack.size();
	//size_t needleSize = needles.size();
	const char * needleData = needles.data();
	int useSize = std::min(16, static_cast<int>(haystack.size() - blockStartIdx));

	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));

	// This load is safe because needles.size() >= 16
	__m128i arr2;
	// Need to set b once before looping
	int b = 16;
	int j = needles.size() - 16;
	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		auto index = _mm_cmpestri(
		                 arr2, 16,
		                 arr1, useSize,
		                 0);
		b = std::min(index, b); // with inline more efficient than if block
	}

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	auto index = _mm_cmpestri(
	                 arr2, 16,
	                 arr1, useSize,
	                 0);
	b = std::min(index, b);

	if (b < useSize) {
		return blockStartIdx + static_cast<size_t>(b);
	}

	return KStringView::npos;

}

//-----------------------------------------------------------------------------
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
	// Calculate these once
	const char * needleData = needles.data();
	size_t needleSize = needles.size();
	//size_t haystackSize = haystack.size(); // Only more efficient if used more than once
	size_t useSize = std::min(16, static_cast<int>(haystack.size() - blockStartIdx));

	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	__m128i arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + needleSize - 16));
	// Must initialize mask before looping with |=
	__m128i mask = _mm_cmpestrm(arr2, 16,
	                            arr1, useSize,
	                            0b00000000);

	// Ensure all bytes loaded into needle are from the intended data
	// When getting raw mask back here, size of needle is ignored.
	// Reverse iteration allows the use of a constant for the end condition
	// And constant size of needle. Reverse iter of needle doesn't change logic
	int j = (needleSize - 32);
	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		mask |= _mm_cmpestrm(
		                 arr2, 16,
		                 arr1, useSize,
		                 0b00000000);
	}

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	mask |= _mm_cmpestrm(
	            arr2, 16,
	            arr1, useSize,
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

		if (b < useSize)
		{
			return blockStartIdx + static_cast<size_t>(b);
		}
	}

	return KStringView::npos;

}

//-----------------------------------------------------------------------------
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
	size_t haystackSize = haystack.size();
	const char * needleData = needles.data();
	size_t needleSize = needles.size();
	// Calculating this once, instead of in every loop
	int useSize = std::min(16, static_cast<int>(haystackSize - blockStartIdx));

	__m128i arr1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));

	// This load is safe because needles.size() > 16
	__m128i arr2;// = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + needleSize - 16));
	// Initialize b before looping
	int b = 16;// = _mm_cmpestri(arr2, 16, arr1, useSize, 0b01000000);

	// Reverse iteration allows the use of a constant for the end condition
	// And constant size of needle. Reverse iter of needle doesn't change logic
	int j = needleSize - 16;
	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		auto index = _mm_cmpestri(
		                 arr2, 16,
		                 arr1, useSize,
		                 0b01000000);
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

	auto index = _mm_cmpestri(
	                 arr2, 16,
	                 arr1, useSize,
	                 0b01000000);

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
	__m128i mask = _mm_cmpestrm(arr2, 16,
	                            arr1, useSize,
	                            0);

	// Reverse iteration allows the use of a constant for the end condition
	// And constant size of needle. Reverse iter of needle doesn't change logic
	int j = needleSize - 32;
	for (; j > 0; j -= 16)
	{
		arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData + j));

		mask |= _mm_cmpestrm(
		                 arr2, 16,
		                 arr1, useSize,
		                 0);
	}

	arr2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(needleData));

	mask |= _mm_cmpestrm(
	            arr2, 16,
	            arr1, useSize,
	            0);

	uint16_t* val = reinterpret_cast<uint16_t*>(&mask);
	if (val)
	{

		// What this does is find the last 0, which means
		// we need to count leading 1's
		// GCC only provides counting leading (or trailing) 0's
		// The trailing side according to the GCC implementation is before our data
		// The leading is after , this is because of the Little Endian architecture.
		// For leading 0's the first 16 aren't even "ours",
		// we use a uint16_t to look at the mask and clz takes uint_32t.

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
        KStringView haystack,
        KStringView needles)
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
	size_t haystackSize = haystack.size();
	for (size_t i = 0; i < haystackSize; i += 16)
	{
		size_t ret = scanHaystackBlock(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfSSE(
        KStringView haystack,
        KStringView needles)
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

	size_t haystackSize = haystack.size();
	for (size_t i = 0; i < haystackSize; i += 16)
	{
		auto ret = scanHaystackBlockNot(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;
}

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
        KStringView haystack,
        KStringView needles)
//-----------------------------------------------------------------------------
{
	size_t haystackSize = haystack.size(); // Significantly faster to call this only once 30% reduction in run time

	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}
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
