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

#include "../../kstringview.h"

namespace dekaf2 {
namespace detail {

namespace no_sse {

/// non-sse version of kFindFirstOf, nonetheless using a very fast std:: algorithm
/// @param haystack the string to search in
/// @param needles  the characters to find
/// @param bNot     whether to search for existing (false) or non-existing (true) needles
DEKAF2_PUBLIC std::size_t kFindFirstOf (KStringView haystack, KStringView needles, bool bNot);

/// non-sse version of kFindLastOf, nonetheless using a very fast std:: algorithm
/// @param haystack the string to search in
/// @param needles  the characters to find
/// @param bNot     whether to search for existing (false) or non-existing (true) needles
DEKAF2_PUBLIC std::size_t kFindLastOf  (KStringView haystack, KStringView needles, bool bNot);

} // end of namespace no_sse

namespace sse {

#if !DEKAF2_FIND_FIRST_OF_USE_SIMD

/// non-sse version of kFindFirstOf
/// @param haystack the string to search in
/// @param needles  the characters to find
DEKAF2_PUBLIC inline std::size_t kFindFirstOf    (KStringView haystack, KStringView needles)
{
	return dekaf2::detail::no_sse::kFindFirstOf(haystack, needles, false);
}

/// non-sse version of kFindFirstNotOf
/// @param haystack the string to search in
/// @param needles  the characters not to find
DEKAF2_PUBLIC inline std::size_t kFindFirstNotOf (KStringView haystack, KStringView needles)
{
	return dekaf2::detail::no_sse::kFindFirstOf(haystack, needles, true);
}

/// non-sse version of kFindLastOf
/// @param haystack the string to search in
/// @param needles  the characters to find
DEKAF2_PUBLIC inline std::size_t kFindLastOf     (KStringView haystack, KStringView needles)
{
	return dekaf2::detail::no_sse::kFindLastOf(haystack, needles, false);
}

/// non-sse version of kFindLastNotOf
/// @param haystack the string to search in
/// @param needles  the characters not to find
DEKAF2_PUBLIC inline std::size_t kFindLastNotOf  (KStringView haystack, KStringView needles)
{
	return dekaf2::detail::no_sse::kFindLastOf(haystack, needles, true);
}

#else // !DEKAF2_FIND_FIRST_OF_USE_SIMD

/// sse version of kFindFirstOf
/// @param haystack the string to search in
/// @param needles  the characters to find
DEKAF2_PUBLIC std::size_t kFindFirstOf    (KStringView haystack, KStringView needles);

/// sse version of kFindFirstNotOf
/// @param haystack the string to search in
/// @param needles  the characters not to find
DEKAF2_PUBLIC std::size_t kFindFirstNotOf (KStringView haystack, KStringView needles);

/// sse version of kFindLastOf
/// @param haystack the string to search in
/// @param needles  the characters to find
DEKAF2_PUBLIC std::size_t kFindLastOf     (KStringView haystack, KStringView needles);

/// sse version of kFindLastNotOf
/// @param haystack the string to search in
/// @param needles  the characters not to find
DEKAF2_PUBLIC std::size_t kFindLastNotOf  (KStringView haystack, KStringView needles);

#endif

} // end of namespace sse

} // end of namespace detail
} // end of namespace dekaf2
