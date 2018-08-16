/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kassociative.h
/// provides unordered maps and sets based on boost::multi_index to overcome
/// the weakness of current std::multimaps/sets to allow for a template
/// iterator find(T& key), which e.g. with string keys and lookups through
/// string literals always forces a string allocation. Strangely, these
/// containers seem to have been overlooked when the ordered versions
/// became extended with a template iterator find(T& key). As boost::multi_index
/// supports promotion of the find argument we use it as a replacement for the
/// std templates

#include <set>
#include <map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include "bits/kmutable_pair.h"

namespace dekaf2
{

//---------------------------------------------------------------------------
// typedefs of std::map/multimap/set/multiset
//---------------------------------------------------------------------------

template<
	class Key,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<Key>
>
using KSet = std::set<Key, Compare, Allocator>;

template<
	class Key,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<Key>
>
using KMultiSet = std::multiset<Key, Compare, Allocator>;

template<
	class Key,
	class Value,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, Value> >
>
using KMap = std::map<Key, Value, Compare, Allocator>;

template<
	class Key,
	class Value,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, Value> >
>
using KMultiMap = std::multimap<Key, Value, Compare, Allocator>;

//---------------------------------------------------------------------------
// boost::multi_index versions of std::unordered_map/unordered_multimap/unordered_set/unordered_multiset
//---------------------------------------------------------------------------

template<
	class Key,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator<Key>
>
using KUnorderedSet = boost::multi_index::multi_index_container<
	Key,
	boost::multi_index::indexed_by<
		boost::multi_index::hashed_unique<
			boost::multi_index::identity<
				Key
			>,
			Hash,
			KeyEqual,
			Allocator
		>
	>
>;

template<
	class Key,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator<Key>
>
using KUnorderedMultiSet = boost::multi_index::multi_index_container<
	Key,
	boost::multi_index::indexed_by<
		boost::multi_index::hashed_non_unique<
			boost::multi_index::identity<
				Key
			>,
			Hash,
			KeyEqual,
			Allocator
		>
	>
>;

template<
	class Key,
	class Value,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator< std::pair<const Key, Value> >
>
using KUnorderedMap = boost::multi_index::multi_index_container<
	detail::KMutablePair<Key, Value>,
	boost::multi_index::indexed_by<
		boost::multi_index::hashed_unique<
			boost::multi_index::member<
				detail::KMutablePair<Key, Value>,
				Key,
				&detail::KMutablePair<Key, Value>::first
			>,
			Hash,
			KeyEqual,
			Allocator
		>
	>
>;

template<
	class Key,
	class Value,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator< std::pair<const Key, Value> >
>
using KUnorderedMultiMap = boost::multi_index::multi_index_container<
	detail::KMutablePair<Key, Value>,
	boost::multi_index::indexed_by<
		boost::multi_index::hashed_non_unique<
			boost::multi_index::member<
				detail::KMutablePair<Key, Value>,
				Key,
				&detail::KMutablePair<Key, Value>::first
			>,
			Hash,
			KeyEqual,
			Allocator
		>
	>
>;

} // end of namespace dekaf2
