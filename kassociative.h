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
/// Provides unordered maps and sets based on boost::multi_index to overcome
/// the weakness of current std::unordered_multimaps/sets to allow for a template
/// iterator find(T& key), which e.g. with string keys and lookups through
/// string literals always forces a string allocation. Strangely, these
/// containers seem to have been overlooked when the ordered versions
/// became extended with a template iterator find(T& key). As boost::multi_index
/// supports promotion of the find argument we use it as a replacement for the
/// std templates.
/// Also adds ordered versions based on boost::multi_index, as they tend to be
/// 10-20% faster on insertion than the std:: versions.
/// The map implementations all have a operator[](Key) const that will not add
/// a new element when the key is not found, but instead return a reference to
/// a static, default constructed value.
/// All types have a contains() member that returns true if the searched element
/// is found.

#ifndef BOOST_NO_CXX98_FUNCTION_BASE
	#define BOOST_NO_CXX98_FUNCTION_BASE
#endif
#include "kdefinitions.h"
#include "bits/kmutable_pair.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
// boost::multi_index versions of std::map/multimap/set/multiset
//---------------------------------------------------------------------------

template<
	class Key,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<Key>
>
class KSet : public boost::multi_index::multi_index_container<
	Key,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
			boost::multi_index::identity<
				Key
			>,
			Compare,
			Allocator
		>
	>
>
{
public:

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}

};

template<
	class Key,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<Key>
>
class KMultiSet : public boost::multi_index::multi_index_container<
	Key,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_non_unique<
			boost::multi_index::identity<
				Key
			>,
			Compare,
			Allocator
		>
	>
>
{
public:

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}
};

template<
	class Key,
	class Value,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, Value> >
>
class KMap : public boost::multi_index::multi_index_container<
	detail::KMutablePair<Key, Value>,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
			boost::multi_index::member<
				detail::KMutablePair<Key, Value>,
				Key,
				&detail::KMutablePair<Key, Value>::first
			>,
			Compare,
			Allocator
		>
	>
>
{
public:
	using mapped_type = Value;

	template<typename K>
	const Value& operator[](K&& key) const
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return s_Empty;
		}
	}

	template<typename K>
	Value& operator[](K&& key)
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return this->insert({std::forward<K>(key), Value()}).first->second;
		}
	}

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}

private:
	static const Value s_Empty;
};

template<class Key, class Value, class Compare, class Allocator>
const Value KMap<Key, Value, Compare, Allocator>::s_Empty = Value();

template<
	class Key,
	class Value,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, Value> >
>
class KMultiMap : public boost::multi_index::multi_index_container<
	detail::KMutablePair<Key, Value>,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_non_unique<
			boost::multi_index::member<
				detail::KMutablePair<Key, Value>,
				Key,
				&detail::KMutablePair<Key, Value>::first
			>,
			Compare,
			Allocator
		>
	>
>
{

public:
	using mapped_type = Value;

	template<typename K>
	const Value& operator[](K&& key) const
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return s_Empty;
		}
	}

	template<typename K>
	Value& operator[](K&& key)
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return this->insert({std::forward<K>(key), Value()}).first->second;
		}
	}

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}

private:
	static const Value s_Empty;
};

template<class Key, class Value, class Compare, class Allocator>
const Value KMultiMap<Key, Value, Compare, Allocator>::s_Empty = Value();

//---------------------------------------------------------------------------
// boost::multi_index versions of std::unordered_map/unordered_multimap/unordered_set/unordered_multiset
//---------------------------------------------------------------------------

template<
	class Key,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator<Key>
>
class KUnorderedSet : public boost::multi_index::multi_index_container<
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
>
{
public:

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}
};

template<
	class Key,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator<Key>
>
class KUnorderedMultiSet : public boost::multi_index::multi_index_container<
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
>
{
public:

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}
};

template<
	class Key,
	class Value,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator< std::pair<const Key, Value> >
>
class KUnorderedMap : public boost::multi_index::multi_index_container<
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
>
{
public:
	using mapped_type = Value;

	template<typename K>
	const Value& operator[](K&& key) const
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return s_Empty;
		}
	}

	template<typename K>
	Value& operator[](K&& key)
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return this->insert({std::forward<K>(key), Value()}).first->second;
		}
	}

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}

private:
	static const Value s_Empty;
};

template<class Key, class Value, class Hash, class KeyEqual, class Allocator>
const Value KUnorderedMap<Key, Value, Hash, KeyEqual, Allocator>::s_Empty = Value();

template<
	class Key,
	class Value,
	class Hash = std::hash<Key>,
	class KeyEqual = std::equal_to<Key>,
	class Allocator = std::allocator< std::pair<const Key, Value> >
>
class KUnorderedMultiMap : public boost::multi_index::multi_index_container<
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
>
{
public:
	using mapped_type = Value;

	template<typename K>
	const Value& operator[](K&& key) const
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return s_Empty;
		}
	}

	template<typename K>
	Value& operator[](K&& key)
	{
		auto it = this->find(std::forward<K>(key));
		if (it != this->end())
		{
			return it->second;
		}
		else
		{
			return this->insert({std::forward<K>(key), Value()}).first->second;
		}
	}

	template<typename K>
	bool contains(K&& key) const
	{
		return this->find(std::forward<K>(key)) != this->end();
	}

private:
	static const Value s_Empty;
};

template<class Key, class Value, class Hash, class KeyEqual, class Allocator>
const Value KUnorderedMultiMap<Key, Value, Hash, KeyEqual, Allocator>::s_Empty = Value();

DEKAF2_NAMESPACE_END
