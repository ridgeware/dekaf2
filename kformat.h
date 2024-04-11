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

#include "kcompatibility.h"

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

#ifdef DEKAF2_HAS_STD_FORMAT
	#define DEKAF2_FORMAT_NAMESPACE std
#elif DEKAF2_HAS_INCLUDE(<fmt/format.h>)
	#define DEKAF2_FORMAT_NAMESPACE fmt
	#define DEKAF2_HAS_FMT_FORMAT 1
	#include <fmt/format.h>
	#include <fmt/chrono.h>
#else
	#error "no formatting library found"
#endif

#include "kstringview.h"
#include "kstring.h"
#include <ostream>
#include <locale>
#include <cstdio>

// fmt v10.0 doesn't support enum to int conversion anymore - add a generic conversion
template<typename Enum, typename std::enable_if<std::is_enum<Enum>::value, int>::type = 0>
constexpr typename std::underlying_type<Enum>::type format_as(Enum e)
{
	return std::to_underlying(e);
}

DEKAF2_NAMESPACE_BEGIN

// add a generic enum to int conversion to namespace dekaf2 as well
template<typename Enum, typename std::enable_if<std::is_enum<Enum>::value, int>::type = 0>
constexpr typename std::underlying_type<Enum>::type format_as(Enum e)
{
	return std::to_underlying(e);
}

namespace dekaf2_format = DEKAF2_FORMAT_NAMESPACE;

namespace detail {

DEKAF2_PUBLIC
KString kFormat(KStringView sFormat, const dekaf2_format::format_args& args) noexcept;

DEKAF2_PUBLIC
KString kFormat(const std::locale& locale, KStringView sFormat, const dekaf2_format::format_args& args) noexcept;

} // end of namespace detail

//-----------------------------------------------------------------------------
// C++20
/// format no-op
DEKAF2_NODISCARD inline DEKAF2_PUBLIC
KString kFormat(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return KString(sFormat);
}

//-----------------------------------------------------------------------------
// C++20
/// formats a KString using Python syntax
template<class... Args, typename std::enable_if<sizeof...(Args) != 0, int>::type = 0>
DEKAF2_NODISCARD
KString kFormat(KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return detail::kFormat(sFormat, dekaf2_format::make_format_args(std::forward<Args>(args)...));
}

//-----------------------------------------------------------------------------
// C++20
/// formats a KString using Python syntax, using locale specification for decimal points and time formatting (month and day names)
template<class... Args>
DEKAF2_NODISCARD
KString kFormat(const std::locale& locale, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return detail::kFormat(locale, sFormat, dekaf2_format::make_format_args(std::forward<Args>(args)...));
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
/// formats to stdout using Python syntax
template<class... Args>
bool kPrint(const std::locale& locale, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(kFormat(locale, sFormat, std::forward<Args>(args)...));
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
/// formats to stdout using Python syntax
template<class... Args>
bool kPrintLine(const std::locale& locale, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(kFormat(locale, sFormat, std::forward<Args>(args)...));
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
/// formats a std::ostream using Python syntax, with locale
template<class... Args>
std::ostream& kPrint(const std::locale& locale, std::ostream& os, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
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

//-----------------------------------------------------------------------------
// C++23
/// formats a std::ostream using Python syntax, with locale
template<class... Args>
std::ostream& kPrintLine(const std::locale& locale, std::ostream& os, KStringView sFormat, Args&&... args) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(os, kFormat(locale, sFormat, std::forward<Args>(args)...));
}

DEKAF2_NAMESPACE_END
