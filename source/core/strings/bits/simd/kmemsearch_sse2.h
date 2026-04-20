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

/// @file kmemsearch_sse2.h
/// X86 SSE2 accelerated implementations of find_first_not_of(char) and
/// find_last_not_of(char). The libc (glibc, musl, Windows CRT, Apple libc)
/// ships highly optimized memchr/memrchr/memmem, but none of them has a
/// "find the first byte that does NOT equal x" primitive, so we roll our own
/// to close that gap on X86.
///
/// SSE2 is baseline on every x86_64 CPU and on modern 32-bit MSVC / clang
/// builds (/arch:SSE2, -msse2), so we enable this path unconditionally
/// whenever the compiler advertises SSE2 support.

#include <dekaf2/core/init/kdefinitions.h>
#include <cstddef>

// Enabled whenever SSE2 is advertised. This is baseline on every x86_64
// target (the x86_64 ABI requires SSE2), and on 32-bit builds whenever the
// compiler promises SSE2 (-msse2 for GCC/Clang, /arch:SSE2 for MSVC). On
// non-x86 architectures the gate is naturally false because none of these
// predefined macros exist.
#if defined(__SSE2__) \
    || defined(_M_X64) \
    || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
	#define DEKAF2_HAS_SSE2_MEMSEARCH 1
#else
	#define DEKAF2_HAS_SSE2_MEMSEARCH 0
#endif

#if DEKAF2_HAS_SSE2_MEMSEARCH

DEKAF2_NAMESPACE_BEGIN
namespace detail {
namespace sse2   {

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

/// return the offset of the last byte in haystack[0 .. iScanLen) that is NOT
/// equal to needle, or SIZE_MAX (KStringView::npos) if every byte equals
/// needle (or the range is empty).
/// @param pData      haystack pointer
/// @param iScanLen   number of bytes to scan, starting at pData (i.e. the
///                   caller has already clamped pos+1 to the haystack size)
/// @param needle     the byte to exclude
/// @return the offset (0-based within pData) of the last non-matching byte,
///         or SIZE_MAX if not found.
DEKAF2_NODISCARD DEKAF2_PUBLIC
std::size_t kRFindNotChar(const char* pData, std::size_t iScanLen, char needle) noexcept;

} // end of namespace sse2
} // end of namespace detail
DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SSE2_MEMSEARCH
