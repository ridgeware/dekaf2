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

#include "bits/kcppcompat.h"

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
#include "kstringview.h"
#include "khash.h"

namespace frozen {

// add hash specialization for KStringView(Z) to frozen:: - the easier
// the better, the compiler will find the seed needed for a perfect hash anyway

template <>
struct elsa<dekaf2::KStringView>
{
	constexpr std::size_t operator()(dekaf2::KStringView value) const
	{
		return value.Hash();
	}

	constexpr std::size_t operator()(dekaf2::KStringView value, std::size_t seed) const
	{
		std::size_t hash = seed;
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			hash = (hash ^ static_cast<unsigned char>(value[i])) * 1099511628211;
		}
		return hash;
	}
};

template <>
struct elsa<dekaf2::KStringViewZ>
{
	constexpr std::size_t operator()(dekaf2::KStringViewZ value) const
	{
		return value.Hash();
	}

	constexpr std::size_t operator()(dekaf2::KStringViewZ value, std::size_t seed) const
	{
		std::size_t hash = seed;
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			hash = (hash ^ static_cast<unsigned char>(value[i])) * 1099511628211;
		}
		return hash;
	}
};

}

#endif // of DEKAF2_HAS_CPP_14
