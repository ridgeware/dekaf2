/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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
//
*/

#include <dekaf2/core/strings/bits/kstring_view.h>
#include <dekaf2/core/strings/bits/simd/kfindfirstof.h>
#include <dekaf2/core/strings/bits/simd/kmemsearch_neon.h>
#ifdef DEKAF2_X86_64
#endif

DEKAF2_NAMESPACE_BEGIN

#ifndef __GLIBC__

//-----------------------------------------------------------------------------
void* memrchr(const void* s, int c, size_t n)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NEON_MEMRCHR
	// ARM64 NEON path (Apple Silicon, FreeBSD/OpenBSD ARM64, ...)
	return DEKAF2_PREFIX detail::neon::kMemRChr(s, c, n);
#else

#if DEKAF2_FIND_FIRST_OF_USE_SIMD
	{
		const char* p = static_cast<const char*>(s);
		char ch = static_cast<char>(c);
		size_t pos = DEKAF2_PREFIX detail::sse::kFindLastOf(DEKAF2_PREFIX KStringView(p, n), DEKAF2_PREFIX KStringView(&ch, 1));
		if (pos != DEKAF2_PREFIX KStringView::npos)
		{
			return const_cast<char*>(p + pos);
		}
		return nullptr;
	}
#endif

	auto p = static_cast<const unsigned char*>(s);
	for (p += n; n > 0; --n)
	{
		if (*--p == c)
		{
			return const_cast<unsigned char*>(p);
		}
	}
	return nullptr;

#endif // DEKAF2_HAS_NEON_MEMRCHR

} // memrchr

#endif // of __GLIBC__ (memrchr)

//-----------------------------------------------------------------------------
void* memmem(const void* haystack, size_t iHaystackSize, const void *needle, size_t iNeedleSize)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NEON
	// ARM64 NEON first-and-last-byte filter. Benchmarks on M1 Pro show
	// this wins big over glibc 2.34's Two-Way algorithm for short needles
	// (2B -> ~25x, 8B -> ~4x, 16B -> ~2x faster) and loses to glibc for
	// longer needles (64B -> ~2x slower). We therefore use the NEON path
	// up to the configured cutoff and hand larger needles off to libc's
	// tuned memmem when we are on glibc; on non-glibc targets our NEON
	// path is still the best available option for every size (Apple
	// libc's memmem in particular is ~100x slower than glibc's).
	//
	// The cutoff is configurable via the DEKAF2_MEMMEM_NEON_CUTOFF CMake
	// option because future glibc versions may shift the crossover
	// point; see CMakeLists.txt for details.
#ifndef DEKAF2_MEMMEM_NEON_CUTOFF
	#define DEKAF2_MEMMEM_NEON_CUTOFF 16
#endif
	constexpr std::size_t iNeonCutoff = DEKAF2_MEMMEM_NEON_CUTOFF;

	if (iNeedleSize <= iNeonCutoff)
	{
		return DEKAF2_PREFIX detail::neon::kMemMem(haystack, iHaystackSize, needle, iNeedleSize);
	}
  #ifdef __GLIBC__
	return ::memmem(haystack, iHaystackSize, needle, iNeedleSize);
  #else
	return DEKAF2_PREFIX detail::neon::kMemMem(haystack, iHaystackSize, needle, iNeedleSize);
  #endif
#else
  #ifdef __GLIBC__
	// no NEON, but glibc has an excellent SIMD memmem on every architecture
	return ::memmem(haystack, iHaystackSize, needle, iNeedleSize);
  #else
	// no NEON, no glibc: scalar memchr + memcmp loop. Still faster than
	// Apple's two-way implementation.
	if (!iNeedleSize || !needle || !haystack)
	{
		// an empty needle matches the start of any haystack
		return const_cast<void*>(haystack);
	}

	auto pHaystack = static_cast<const char*>(haystack);
	auto pNeedle   = static_cast<const char*>(needle);

	for(;iNeedleSize <= iHaystackSize;)
	{
		auto pFound = static_cast<const char*>(std::memchr(pHaystack, pNeedle[0], (iHaystackSize - iNeedleSize) + 1));

		if (DEKAF2_UNLIKELY(!pFound))
		{
			return nullptr;
		}

		// due to aligned loads it is faster to compare the full needle again
		if (std::memcmp(pFound, pNeedle, iNeedleSize) == 0)
		{
			return const_cast<char*>(pFound);
		}

		auto iAdvance = static_cast<size_t>(pFound - pHaystack) + 1;

		pHaystack     += iAdvance;
		iHaystackSize -= iAdvance;
	}

	return nullptr;
  #endif
#endif

} // memmem

DEKAF2_NAMESPACE_END
