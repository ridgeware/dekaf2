/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

/// @file krandom.h
/// random bytes

#include <dekaf2/kdefinitions.h>
#include <dekaf2/kstring.h>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_id
/// @{

/// Returns 32 bit unsigned random number
DEKAF2_NODISCARD DEKAF2_PUBLIC
uint32_t kRandom32();

/// Returns 64 bit unsigned random number
DEKAF2_NODISCARD DEKAF2_PUBLIC
uint64_t kRandom64();

/// Returns 32 bit unsigned random number, prefer using kRandom32() for new code
DEKAF2_NODISCARD DEKAF2_PUBLIC inline
uint32_t kRandom()
{
	return kRandom32();
}

/// Returns 32 bit unsigned random number in range [iMin - iMax].
DEKAF2_NODISCARD DEKAF2_PUBLIC
uint32_t kRandom(uint32_t iMin, uint32_t iMax);

/// Returns cryptographically secure random bytes
DEKAF2_PUBLIC
bool kGetRandom(void* buf, std::size_t iCount);

/// Returns cryptographically secure random bytes
DEKAF2_PUBLIC
KString kGetRandom(std::size_t iCount);

namespace detail {
/// Called at program initialization by dekaf2's init
DEKAF2_PUBLIC
void kSetRandomSeed();
}


/// @}

DEKAF2_NAMESPACE_END
