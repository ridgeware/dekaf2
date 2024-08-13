/*
 //
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

#pragma once

/// @file kformat.h
/// provides format include files and format defines depending on configuration.
/// sets the following defines:
/// DEKAF2_HAS_STD_FORMAT if std::format() is used, otherwise fmt::format() is used
/// DEKAF2_HAS_FMT_FORMAT if fmt::format() is used, otherwise std::format() is used
/// DEKAF2_FORMAT_NAMESPACE the namespace where format() is found, either std or fmt
/// DEKAF2_FORMAT_INLINE if the format functions shall be called without packing the parameters
/// DEKAF2_HAS_FORMAT_RUNTIME is set if the format lib supports the runtime() method
/// DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION is set if the format lib causes an error
/// with format strings like "{}>" or "{}<" - this is a really serious bug and we
/// try to work around it

#include "../kdefinitions.h"

#undef DEKAF2_HAS_STD_FORMAT
#undef DEKAF2_FORMAT_NAMESPACE
// the code is slightly smaller without format() inlining, but of course
// also slightly slower.. leave the decision to the lib detecting
// code below (also see the notes about parameter forwarding)
//#define DEKAF2_FORMAT_INLINE 1

#ifndef DEKAF2_FORCE_FMTLIB_OVER_STD_FORMAT
	#if defined(DEKAF2_HAS_CPP_20)
		#if DEKAF2_HAS_INCLUDE(<format>)
			#include <format>
			#if defined(__cpp_lib_format) || DEKAF2_IS_MACOS
				#define DEKAF2_HAS_STD_FORMAT 1
			#endif
		#endif
	#endif
#endif

#ifdef DEKAF2_HAS_STD_FORMAT
	#define DEKAF2_FORMAT_NAMESPACE std
	#if __cpp_lib_format >= 202311L
		#define DEKAF2_HAS_FORMAT_RUNTIME 1
	#endif
	#if defined(DEKAF2_LIBCXX_VERSION_MAJOR)
		// a format string like "{}>" or "{}<" causes an error in AppleClang's
		// implementation of the format library - IMHO this is a really serious bug
		#define DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION 1
	#endif
#elif DEKAF2_HAS_INCLUDE(<fmt/format.h>)
	#define DEKAF2_FORMAT_NAMESPACE fmt
	#define DEKAF2_HAS_FMT_FORMAT 1
	#include <fmt/format.h>
	#include <fmt/chrono.h>
	#define DEKAF2_HAS_FORMAT_RUNTIME 1
#else
	#error "no formatting library found"
#endif

DEKAF2_NAMESPACE_BEGIN

template<typename... Args>
using KFormatString = DEKAF2_FORMAT_NAMESPACE::format_string<Args...>;

DEKAF2_NAMESPACE_END
