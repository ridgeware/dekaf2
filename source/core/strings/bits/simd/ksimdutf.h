/*
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

/// @file ksimdutf.h
/// Private header for the dekaf2 sources that call simdutf. It makes simdutf available as
/// namespace dekaf2::simdutf, so that the sources call simdutf:: in either build mode:
/// - with the vendored simdutf, its sources are compiled in the namespace dekaf2::simdutf.
///   dekaf2 then does not define the global simdutf symbols, which any other simdutf in
///   the same program defines as well.
/// - with a system simdutf (DEKAF2_USE_SYSTEM_SIMDUTF), dekaf2::simdutf is an alias of
///   the namespace ::simdutf.
/// The vendored implementation is compiled once, in bits/simd/kutf.cpp, which defines
/// DEKAF2_SIMDUTF_IMPLEMENTATION before it includes this header. The header is not installed.

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_WITH_SIMDUTF

#if DEKAF2_IS_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wpedantic"
// the conversion helpers of the C API, which is left out (see below)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if DEKAF2_IS_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
// the conversion helpers of the C API, which is left out (see below)
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#if DEKAF2_USE_SYSTEM_SIMDUTF

	#include <simdutf.h>

	DEKAF2_NAMESPACE_BEGIN
	namespace simdutf = ::simdutf;
	DEKAF2_NAMESPACE_END

#else

	// dekaf2 does not use the encoding detection of simdutf. Without it, simdutf also leaves
	// out its C API, whose functions have C linkage and would stay global in any namespace.
	#define SIMDUTF_FEATURE_DETECT_ENCODING 0

	// every simdutf token of the vendored sources becomes DEKAF2_NAMESPACE_NAME::simdutf, in
	// the namespace definitions as well as in the qualified names
	#define simdutf DEKAF2_NAMESPACE_NAME::simdutf

	#include "../../../../../from/simdutf/simdutf.h"

	#ifdef DEKAF2_SIMDUTF_IMPLEMENTATION
		#include "../../../../../from/simdutf/simdutf.cpp"
	#endif

	#undef simdutf

#endif

#if DEKAF2_IS_CLANG
#pragma clang diagnostic pop
#endif

#if DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif

#endif // DEKAF2_WITH_SIMDUTF
