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

/// @file kmemsearch_neon.h
/// ARM NEON accelerated implementations of memrchr, memmem, and
/// find_last_not_of(char). These are used on platforms that do not ship
/// with glibc (which already has excellent SIMD implementations of its own):
/// macOS/iOS on Apple Silicon, FreeBSD/OpenBSD on ARM64, etc.
///
/// Only ARM64 (aarch64) with NEON is supported here; 32-bit ARM uses the
/// scalar fallback.

#include <dekaf2/core/init/kdefinitions.h>
#include <cstddef>

// NEON is mandatory on ARMv8/AArch64, so we can enable it unconditionally on
// every ARM64 target. MSVC does not advertise __ARM_NEON, it simply assumes
// NEON is available whenever _M_ARM64 is set; we accept either indicator.
//
// This top-level gate controls the declarations and definitions of ALL
// NEON-backed helpers below (kFindNotChar, kRFindNotChar, kFindFirstOf,
// kFindLastOf, kMemRChr, kMemMem). These are compiled on every ARM64
// platform, including ARM64 Linux with glibc, because the libc has no
// counterpart for the non-memchr helpers (there is no "memnotchr" in glibc).
#if DEKAF2_ARM64 && (defined(__ARM_NEON) || defined(_M_ARM64))
	#define DEKAF2_HAS_NEON 1
#else
	#define DEKAF2_HAS_NEON 0
#endif

// Subset gate: only when we actually need to override libc's memrchr and
// memmem. glibc ships tuned NEON versions of both, so we leave those alone
// and only route kMemRChr/kMemMem into dekaf2's private memrchr/memmem in
// kstring_view.cpp on the non-glibc platforms (Apple libc, musl, ...).
#if DEKAF2_HAS_NEON && !defined(__GLIBC__)
	#define DEKAF2_HAS_NEON_MEMSEARCH 1
#else
	#define DEKAF2_HAS_NEON_MEMSEARCH 0
#endif

#if DEKAF2_HAS_NEON

DEKAF2_NAMESPACE_BEGIN
namespace detail  {
namespace neon    {

/// reverse memchr: find the last occurrence of character c within the first n
/// bytes of s. Returns nullptr if not found.
DEKAF2_NODISCARD DEKAF2_PUBLIC
void* kMemRChr(const void* s, int c, std::size_t n) noexcept;

/// find the first occurrence of needle within haystack, using a NEON-based
/// first-and-last-byte filter. Returns nullptr if not found. An empty needle
/// matches at the start of the haystack.
DEKAF2_NODISCARD DEKAF2_PUBLIC
void* kMemMem(const void* haystack,
              std::size_t  iHaystackSize,
              const void*  needle,
              std::size_t  iNeedleSize) noexcept;

/// return the offset of the first byte in haystack[0 .. iScanLen) that is NOT
/// equal to needle, or SIZE_MAX (KStringView::npos) if every byte equals
/// needle (or the range is empty).
/// @param pData      haystack pointer
/// @param iScanLen   number of bytes to scan, starting at pData
/// @param needle     the byte to exclude
/// @return the offset (0-based within pData) of the first non-matching byte,
///         or SIZE_MAX if not found.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::size_t kFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept;

/// return the offset of the last byte in haystack[0 .. pos] that is NOT equal
/// to needle, or SIZE_MAX (KStringView::npos) if every byte equals needle
/// (or the range is empty).
/// @param pData      haystack pointer
/// @param iScanLen   number of bytes to scan, starting at pData (i.e. the
///                   caller has already clamped pos+1 to the haystack size)
/// @param needle     the byte to exclude
/// @return the offset (0-based within pData) of the last non-matching byte,
///         or SIZE_MAX if not found.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::size_t kRFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept;

/// Forward scan: find the first byte in haystack[pos..end) that matches any
/// (or, if bNot, matches none) of up to 16 needle bytes. Uses NEON
/// broadcast-and-compare: each 16-byte haystack window is compared against
/// every needle broadcast, the results OR'd together, then reduced through
/// the nibble-mask. For N=1..16 this is 2-8x faster than a scalar table
/// lookup on Apple Silicon.
///
/// @param pHaystack    start of the haystack
/// @param iHaystackLen total haystack length in bytes
/// @param pos          starting offset for the search
/// @param pNeedles     pointer to the needle bytes (1..16 bytes)
/// @param iNeedleCount number of needle bytes (must be in [1, 16])
/// @param bNot         true for find_first_not_of semantics (invert match)
/// @return the offset of the first matching byte, or SIZE_MAX if none.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::size_t kFindFirstOf(const char*  pHaystack,
                         std::size_t  iHaystackLen,
                         std::size_t  pos,
                         const char*  pNeedles,
                         std::size_t  iNeedleCount,
                         bool         bNot) noexcept;

/// Backward scan: find the last byte in haystack[0..pos] that matches any
/// (or, if bNot, matches none) of up to 16 needle bytes.
///
/// @param pHaystack    start of the haystack
/// @param iHaystackLen total haystack length in bytes
/// @param pos          inclusive end-offset for the search (SIZE_MAX = scan entire buffer)
/// @param pNeedles     pointer to the needle bytes (1..16 bytes)
/// @param iNeedleCount number of needle bytes (must be in [1, 16])
/// @param bNot         true for find_last_not_of semantics (invert match)
/// @return the offset of the last matching byte, or SIZE_MAX if none.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::size_t kFindLastOf(const char*  pHaystack,
                        std::size_t  iHaystackLen,
                        std::size_t  pos,
                        const char*  pNeedles,
                        std::size_t  iNeedleCount,
                        bool         bNot) noexcept;

} // end of namespace neon
} // end of namespace detail
DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_NEON
