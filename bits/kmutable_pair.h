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

/// @file kmutable_pair.h
/// provides a helper for boost multi_index

#include <utility>

namespace dekaf2 {
namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A helper type to map a std::map into boost::multi_index,
/// as the latter is const on all elements (and does not have the
/// distinction between key and value)
template<class Key, class Value>
struct KMutablePair
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using self_type = KMutablePair<Key, Value>;

	//-----------------------------------------------------------------------------
	KMutablePair() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KMutablePair(Key f, Value s)
	//-----------------------------------------------------------------------------
	    : first(std::move(f))
	    , second(std::move(s))
	{}

	//-----------------------------------------------------------------------------
	KMutablePair(std::pair<Key, Value> p)
	//-----------------------------------------------------------------------------
	    : first(std::move(p.first))
	    , second(std::move(p.second))
	{}

	//-----------------------------------------------------------------------------
	bool operator==(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		return (other.first == first) && (other.second == second);
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		return !other.operator==(*this);
	}

	//-----------------------------------------------------------------------------
	bool operator<(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		if (first < other.first) return true;

		if (first == other.first)
		{
			return second < other.second;
		}

		return false;
	}

	//-----------------------------------------------------------------------------
	bool operator>(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		return other < *this;
	}

	//-----------------------------------------------------------------------------
	bool operator<=(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		return !(*this > other);
	}

	//-----------------------------------------------------------------------------
	bool operator>=(const KMutablePair& other) const
	//-----------------------------------------------------------------------------
	{
		return !(other < *this);
	}

	Key           first;
	mutable Value second;

}; // KMutablePair

} // end of namespace detail
} // end of namespace dekaf2

