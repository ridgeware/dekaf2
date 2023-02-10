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

#pragma once

/// @file kformat.h
/// provides basic string formatter functionality

#include "bits/kcppcompat.h"

#undef DEKAF2_HAS_STD_FORMAT
#undef DEKAF2_FORMAT_NAMESPACE

#ifndef DEKAF2_FORCE_FMTLIB_OVER_STD_FORMAT
	#if defined(DEKAF2_HAS_CPP_20)
		#if DEKAF2_HAS_INCLUDE(<format>)
			#include <format>
			#if defined(__cpp_lib_format)
				#define DEKAF2_HAS_STD_FORMAT 1
			#endif
		#endif
	#endif
#endif

#include "kstringview.h"
#ifdef DEKAF2_HAS_STD_FORMAT
	#define DEKAF2_FORMAT_NAMESPACE std
#else
	#define DEKAF2_FORMAT_NAMESPACE fmt
	#include <fmt/core.h>
	#include <fmt/printf.h>
	#include <fmt/chrono.h>
#endif
#include <ostream>

namespace dekaf2 {

namespace format = DEKAF2_FORMAT_NAMESPACE;

namespace detail {

DEKAF2_PUBLIC
KString kFormat(KStringView sFormat, format::format_args args) noexcept;

} // end of namespace detail

//-----------------------------------------------------------------------------
// C++20
/// format no-op
inline DEKAF2_PUBLIC
KString kFormat(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return sFormat;
}

//-----------------------------------------------------------------------------
// C++20
/// formats a KString using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
KString kFormat(KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return detail::kFormat(sFormat, format::make_format_args(args...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op to stdout
bool kPrint(KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrint(KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op to stdout, with newline
bool kPrintLine(KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats to stdout using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrintLine(KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for std::FILE*
DEKAF2_PUBLIC
bool kPrint(std::FILE* fp, KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats a std::FILE* using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrint(std::FILE* fp, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(fp, kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for std::FILE*
DEKAF2_PUBLIC
bool kPrintLine(std::FILE* fp, KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats a std::FILE* using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrintLine(std::FILE* fp, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(fp, kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for filedesc
DEKAF2_PUBLIC
bool kPrint(int fd, KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrint(int fd, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(fd, kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for filedesc
DEKAF2_PUBLIC
bool kPrintLine(int fd, KStringView sFormat) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// C++23
/// formats a filedesc using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
bool kPrintLine(int fd, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(fd, kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for std::ostream
inline DEKAF2_PUBLIC
std::ostream& kPrint(std::ostream& os, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return os.write(sFormat.data(), sFormat.size());
}

//-----------------------------------------------------------------------------
// C++23
/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
std::ostream& kPrint(std::ostream& os, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(os, kFormat(sFormat, std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++23
/// format no-op for std::ostream
inline DEKAF2_PUBLIC
std::ostream& kPrintLine(std::ostream& os, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return os.write(sFormat.data(), sFormat.size()).write("\n", 1);
}

//-----------------------------------------------------------------------------
// C++23
/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
std::ostream& kPrintLine(std::ostream& os, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(os, kFormat(sFormat, std::forward<Args>(args)...));
}

} // end of namespace dekaf2

