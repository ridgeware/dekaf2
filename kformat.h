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

#include <ostream>
#include "kstringview.h"
#include <fmt/core.h>
#include <fmt/printf.h>

namespace dekaf2 {

namespace detail {

std::ostream& kfFormat(std::ostream& os, KStringView sFormat, fmt::format_args args);
KString kFormat(KStringView sFormat, fmt::format_args args);
std::ostream& kfPrintf(std::ostream& os, KStringView sFormat, fmt::printf_args args);
KString kPrintf(KStringView sFormat, fmt::printf_args args);

} // end of namespace detail



//-----------------------------------------------------------------------------
/// format no-op for std::ostream
inline
std::ostream& kfFormat(std::ostream& os, KStringView sFormat)
//-----------------------------------------------------------------------------
{
	os.write(sFormat.data(), sFormat.size());
	return os;
}

//-----------------------------------------------------------------------------
/// formats a std::ostream using Python syntax
template<class... Args, typename std::enable_if_t<sizeof...(Args) != 0, int> = 0>
std::ostream& kfFormat(std::ostream& os, KStringView sFormat, Args&&... args)
//-----------------------------------------------------------------------------
{
	return detail::kfFormat(os, sFormat, fmt::make_format_args(args...));
}

//-----------------------------------------------------------------------------
/// format no-op
inline
KString kFormat(KStringView sFormat)
//-----------------------------------------------------------------------------
{
	return sFormat;
}

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax
template<class... Args, typename std::enable_if_t<sizeof...(Args) != 0, int> = 0>
KString kFormat(KStringView sFormat, Args&&... args)
//-----------------------------------------------------------------------------
{
	return detail::kFormat(sFormat, fmt::make_format_args(args...));
}

//-----------------------------------------------------------------------------
/// format-noop for a file
inline
std::ostream& kfPrintf(std::ostream& os, KStringView sFormat)
//-----------------------------------------------------------------------------
{
	os.write(sFormat.data(), sFormat.size());
	return os;
}

//-----------------------------------------------------------------------------
/// formats a file using POSIX printf syntax
template<class... Args, typename std::enable_if_t<sizeof...(Args) != 0, int> = 0>
std::ostream& kfPrintf(std::ostream& os, KStringView sFormat, Args&&... args)
//-----------------------------------------------------------------------------
{
	return detail::kfPrintf(os, sFormat, fmt::make_printf_args(args...));
}

//-----------------------------------------------------------------------------
/// format no-op
inline
KString kPrintf(KStringView sFormat)
//-----------------------------------------------------------------------------
{
	return sFormat;
}

//-----------------------------------------------------------------------------
/// formats a KString using POSIX printf syntax
template<class... Args, typename std::enable_if_t<sizeof...(Args) != 0, int> = 0>
KString kPrintf(KStringView sFormat, Args&&... args)
//-----------------------------------------------------------------------------
{
	return detail::kPrintf(sFormat, fmt::make_printf_args(args...));
}

} // end of namespace dekaf2

