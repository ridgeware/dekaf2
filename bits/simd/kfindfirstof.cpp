/*
 *  The code for the SSE42 find_first_of() is taken from Facebook folly.
 *  The code for the non-SSE version is new.
 *  All code for the three siblings of find_first_of() is new.
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

//  Essentially, two versions of this file: one with an SSE42 implementation
//  and one with a fallback implementation. We determine which version to use by
//  testing for the presence of the required headers.
//
//  TODO: Maybe this should be done by the build system....

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
inline size_t nextAlignedIndex(const char* arr)
//-----------------------------------------------------------------------------
{
	auto firstPossible = reinterpret_cast<uintptr_t>(arr) + 1;
	return 1 + // add 1 because the index starts at 'arr'
	        ((firstPossible + 15) & ~0xF) // round up to next multiple of 16
	        - firstPossible;
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

	auto arr2  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	auto arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size()), 0);
	if (index < 16)
	{
		return size_t(index);
	}

	// Now, we can do aligned loads hereafter...
	size_t i = nextAlignedIndex(haystack.data());
	for (; i < haystack.size(); i += 16)
	{
		arr1  = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
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

	auto arr2  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	auto arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size()), 0b00010000);
	if (index < 16)
	{
		return size_t(index);
	}

	// Now, we can do aligned loads hereafter...
	size_t i = nextAlignedIndex(haystack.data());
	for (; i < haystack.size(); i += 16)
	{
		arr1  = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, static_cast<int>(haystack.size() - i), 0b00010000);
		if (index < 16)
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
	int useSize = std::min(16, static_cast<int>(haystack.size()));
	size_t haystackSize = haystack.size();

	__m128i arr2  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	__m128i arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + haystackSize - useSize));
	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, useSize , 0b01110000);

	if (index < useSize)
	{
		return size_t(index + haystackSize - useSize);
	}
	else if (DEKAF2_UNLIKELY(haystackSize <= 16))
	{
		return KStringView::npos;
	}

	// Now, we can do aligned loads hereafter...
	size_t i = nextAlignedIndex(haystack.data() + haystackSize - useSize);
	for (; i < haystackSize; i -= 16) // i > 0, i is unsigned
	{
		// nextAlignedIndex is returning an index that is not aligned...
		//arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, haystackSize - i, 0b01110000);
		if (index < std::min(16, static_cast<int>(haystackSize - i)))
		{
			return i + static_cast<size_t>(index);
		}
	}

	// Load last compare
	arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, useSize, 0b01110000);
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
	// Account for haystack < 16 ONCE
	int useSize = std::min(16, static_cast<int>(haystack.size()));
	size_t haystackSize = haystack.size();

	__m128i arr2  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	// do an unaligned load for first block of haystack
	__m128i arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + haystackSize - useSize));

	auto index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, useSize, 0b01000000);
	if (index < useSize)
	{
		return size_t(index + haystackSize - useSize);
	}
	else if (DEKAF2_UNLIKELY(haystackSize <= 16))
	{
		return KStringView::npos;
	}

	// Now, we can do aligned loads hereafter...
	const char * checkIndex = haystack.data() + haystackSize - useSize;
	size_t i = nextAlignedIndex(checkIndex);
	for (; i < haystackSize ; i -= 16) // i > 0, i is unsigned
	{
		// nextAlignedIndex is returning an index that is not aligned...
		//arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + i));
		index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, haystackSize - i, 0b01000000);
		if (index < 16)
		{
			return i + static_cast<size_t>(index);
		}
	}

	// Load last piece
	arr1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data()));
	index = _mm_cmpestri(arr2, static_cast<int>(needles.size()), arr1, haystackSize, 0b01000000);
	if (index < 16)
	{
		//return i + static_cast<size_t>(index);
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
		arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	// This load is safe because needles.size() >= 16
	auto arr2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	auto b    = _mm_cmpestri(arr2, 16, arr1, static_cast<int>(haystack.size() - blockStartIdx), 0);

	size_t j = nextAlignedIndex(needles.data());
	for (; j < needles.size(); j += 16)
	{
		arr2 = _mm_load_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

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
		arr1 = _mm_load_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}
	else
	{
		arr1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack.data() + blockStartIdx));
	}

	// This load is safe because needles.size() >= 16
	__m128i arr2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data()));
	__m128i mask = _mm_cmpestrm(arr2, 16, arr1, static_cast<int>(haystack.size() - blockStartIdx), 0);

	size_t j = nextAlignedIndex(needles.data());
	for (; j < (needles.size() - 16); j += 16)
	{
		arr2 = _mm_load_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

		mask |= _mm_cmpestrm(
		                 arr2,
		                 static_cast<int>(needles.size() - j),
		                 arr1,
		                 static_cast<int>(haystack.size() - blockStartIdx),
		                 0);
	}

	j = needles.size() - 16;

	arr2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needles.data() + j));

	mask |= _mm_cmpestrm(
	            arr2,
	            static_cast<int>(needles.size() - j),
	            arr1,
	            static_cast<int>(haystack.size() - blockStartIdx),
	            0);

	uint16_t* val = reinterpret_cast<uint16_t*>(&mask);
	if (val)
	{
		auto b = 32 - portableCLZ(*val);
		if (b < std::min(16UL, haystack.size() - blockStartIdx))
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
	//return kFindLastOfNoSSE(haystack, needles, false);
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}
	else if (// must bail if we can't even SSE-load a single segment of haystack
	    (haystack.size() < 16 &&
	    page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		// We can't safely SSE-load haystack. Use a different approach.
		return kFindLastOfNoSSE(haystack, needles, false);
	}
	else if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastOfNeedles16(haystack, needles);
	}

	auto ret = scanHaystackBlock<false>(haystack, needles, haystack.size());
	if (ret != KStringView::npos)
	{
		return ret;
	}

	size_t i = nextAlignedIndex(haystack.data() + haystack.size() - 16);
	for (; i < haystack.size(); i -= 16) // i > 0, i is unsigned
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
size_t kFindLastNotOfSSE(
        const KStringView haystack,
        const KStringView needles)
//-----------------------------------------------------------------------------
{
	//return kFindLastOfNoSSE(haystack, needles, true);
	if (DEKAF2_UNLIKELY(needles.empty() || haystack.empty()))
	{
		return KStringView::npos;
	}
	else if (// must bail if we can't even SSE-load a single segment of haystack
	    (haystack.size() < 16 &&
	     page_for(haystack.end() - 1) != page_for(haystack.data() + 15)) ||
	    // can't load needles into SSE register if it could cross page boundary
	    page_for(needles.end() - 1) != page_for(needles.data() + 15))
	{
		// Haystack crosses page boundary, SSE is slower across 2 pages
		return kFindLastOfNoSSE(haystack, needles, true);
	}
	else if (DEKAF2_LIKELY(needles.size() <= 16))
	{
		// For a 16 byte or less needle you don't need to cycle through it
		return kFindLastNotOfNeedles16(haystack, needles);
	}

	auto ret = scanHaystackBlockNot<false>(haystack, needles, haystack.size());
	if (ret != KStringView::npos)
	{
		return ret;
	}

	size_t i = nextAlignedIndex(haystack.data() + haystack.size() - 16);
	for (; i < haystack.size(); i -= 16) // i > 0, i is unsigned
	{
		ret = scanHaystackBlockNot<true>(haystack, needles, i);
		if (ret != std::string::npos)
		{
			return ret;
		}
	}

	return KStringView::npos;
}

} // end of namespace detail
} // end of namespace dekaf2

#endif
