/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

/// @file kfind.h
/// provides high performance string searches and replaces

#include <cstring>
#include "kcppcompat.h"

namespace dekaf2
{

constexpr std::size_t npos = ~0UL;

//------------------------------------------------------------------------------
inline std::size_t kFind(const char* str,
                         std::size_t size,
                         char c,
                         std::size_t pos = 0)
//------------------------------------------------------------------------------
{
	// we keep this inlined as then the compiler can evaluate const expressions
	// (memchr() is actually a compiler-builtin with gcc)
	if (DEKAF2_UNLIKELY(pos > size))
	{
		return npos;
	}
	auto ret = static_cast<const char*>(std::memchr(str+pos, c, size-pos));
	if (DEKAF2_UNLIKELY(ret == nullptr))
	{
		return npos;
	}
	else
	{
		return static_cast<std::size_t>(ret - str);
	}
}

//------------------------------------------------------------------------------
inline std::size_t kRFind(const char* str,
                          std::size_t size,
                          char c,
                          std::size_t pos = npos)
//------------------------------------------------------------------------------
{
#if (DEKAF2_GCC_VERSION > 40600)
	// we keep this inlined as then the compiler can evaluate const expressions
	// (memrchr() is actually a compiler-builtin with gcc)
	if (DEKAF2_UNLIKELY(pos >= size))
	{
		pos = size;
	}
	else
	{
		++pos;
	}
	auto ret = static_cast<const char*>(::memrchr(str, c, pos));
	if (DEKAF2_UNLIKELY(ret == nullptr))
	{
		return npos;
	}
	else
	{
		return static_cast<std::size_t>(ret - str);
	}
#else
	// windows has no memrchr()
	pos = std::min(pos, size-1);
	const value_type* base  = str;
	const value_type* found = base + pos;
	for (; found >= base; --found)
	{
		if (*found == ch)
		{
			return static_cast<size_type>(found - base);
		}
	}
	return npos;
#endif
}

class KString;
class KStringView;

//------------------------------------------------------------------------------
std::size_t kFind(KStringView str,
                  const char* search,
                  std::size_t pos,
                  std::size_t n);
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
std::size_t kReplace(KString& string,
                     KStringView sSearch,
                     KStringView sReplaceWith,
                     bool bReplaceAll = true);
//------------------------------------------------------------------------------

} // end of namespace dekaf2

