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

/// @file kstringref.h
/// the string type for output parameters, KStringRef - in a header of its own,
/// so that headers that are included by kstring.h can use it

#include <dekaf2/core/init/kdefinitions.h>
#include <string>

DEKAF2_NAMESPACE_BEGIN

class KString;

#if defined(DEKAF2_IS_APPLE_CLANG) && DEKAF2_CLANG_VERSION < 120000
/// A reference-compatible string type for use as output parameters.
/// On older Apple Clang (< 12) this is KString itself; on all other
/// compilers it is std::string, which converts to/from KString transparently.
/// Use `KStringRef&` in function signatures that write into a caller-provided string.
/// @ingroup core_strings
using KStringRef = KString;
#else
/// A reference-compatible string type for use as output parameters.
/// On older Apple Clang (< 12) this is KString itself; on all other
/// compilers it is std::string, which converts to/from KString transparently.
/// Use `KStringRef&` in function signatures that write into a caller-provided string.
/// @ingroup core_strings
using KStringRef = std::string;
#endif

// Before C++20 a reserve() of a string with less than its capacity is a request to
// shrink the string, which moves its buffer. libstdc++ follows that request up to
// version 10, also in its C++20 mode. Since C++20 reserve() never shrinks.
#if !defined(DEKAF2_HAS_CPP_20) || (defined(DEKAF2_GLIBCXX_VERSION_MAJOR) && DEKAF2_GLIBCXX_VERSION_MAJOR < 11)
	#define DEKAF2_STRING_RESERVE_MAY_SHRINK 1
#endif

namespace detail {

//-----------------------------------------------------------------------------
/// reserve() for a std::string (or any string type) that never shrinks the string, as
/// reserve() itself does since C++20. Use it instead of sString.reserve(): a
/// string that was given room before keeps it, and views into it stay valid.
/// KString::reserve() does the same.
template<class String>
DEKAF2_CONSTEXPR_STRING
void kReserve(String& sString, typename String::size_type iCapacity)
//-----------------------------------------------------------------------------
{
#if DEKAF2_STRING_RESERVE_MAY_SHRINK
	if (iCapacity <= sString.capacity())
	{
		return;
	}
#endif

	sString.reserve(iCapacity);

} // kReserve

} // end of namespace detail

DEKAF2_NAMESPACE_END
