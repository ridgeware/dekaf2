/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018-2019, Ridgeware, Inc.
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

#include "kdefinitions.h"

// frozen constexpr need at least C++14
#ifndef DEKAF2_HAS_CPP_14

// make it easier to code a C++11 replacement for frozen maps and sets by
// already including the necessary header (note: KUnorderedMap/Set does
// not work well as boost::multiindex does not support appropriate initializers)
#include <unordered_map>
#include <unordered_set>

#else

// set the define to tell that we can use frozen maps and sets
#define DEKAF2_HAS_FROZEN 1

// convenience header

#include <frozen/algorithm.h>
#include <frozen/string.h>
#include <frozen/map.h>
#include <frozen/set.h>
#include <frozen/unordered_map.h>
#include <frozen/unordered_set.h>
#include "kdefinitions.h"
#include "kstringview.h"
#include "bits/khash.h"

namespace frozen {

// add hash specialization for KStringView(Z) to frozen:: - the easier
// the better, the compiler will find the seed needed for a perfect hash anyway

template <>
struct elsa<DEKAF2_PREFIX KStringView>
{
	constexpr std::size_t operator()(DEKAF2_PREFIX KStringView value) const
	{
		// a fast and simple hash
		std::size_t hash = 5381;
		for (const auto ch : value)
		{
		  hash = hash * 33 + static_cast<std::size_t>(ch);
		}
		return hash;
	}

	constexpr std::size_t operator()(DEKAF2_PREFIX KStringView value, std::size_t seed) const
	{
		// this is FNV1a 32 bit, seeded
		std::size_t hash = (static_cast<std::size_t>(2166136261U) ^ seed) * static_cast<std::size_t>(16777619U);
		for (const auto ch : value)
		{
			hash = (hash ^ static_cast<std::size_t>(ch)) * static_cast<std::size_t>(16777619U);
		}
		return hash;
	}
};

template <>
struct elsa<DEKAF2_PREFIX KStringViewZ>
{
	constexpr std::size_t operator()(DEKAF2_PREFIX KStringViewZ value) const
	{
		// a fast and simple hash
		std::size_t hash = 5381;
		for (const auto ch : value)
		{
		  hash = hash * 33 + static_cast<std::size_t>(ch);
		}
		return hash;
	}

	constexpr std::size_t operator()(DEKAF2_PREFIX KStringViewZ value, std::size_t seed) const
	{
		// this is FNV1a 32 bit, seeded
		std::size_t hash = (static_cast<std::size_t>(2166136261U) ^ seed) * static_cast<std::size_t>(16777619U);
		for (const auto ch : value)
		{
			hash = (hash ^ static_cast<std::size_t>(ch)) * static_cast<std::size_t>(16777619U);
		}
		return hash;
	}
};

}

#endif // of DEKAF2_HAS_CPP_14
