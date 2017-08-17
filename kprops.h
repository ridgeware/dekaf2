/*
//
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
//
*/

#pragma once

#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include "kcppcompat.h"
#include "klog.h"

namespace dekaf2
{

using boost::multi_index_container;
using namespace boost::multi_index;

namespace KProps_detail
{

// this is a helper type to map a std::map into boost::multi_index,
// as the latter is const on all elements (and does not have the
// distinction between key and value)
template<class Key, class Value>
struct mutable_pair
{
	using self_type = mutable_pair<Key, Value>;

	mutable_pair()
	    : first(Key())
	    , second(Value())
	{}

	mutable_pair(const Key& f, const Value& s)
	    : first(f)
	    , second(s)
	{}

	mutable_pair(Key&& f, Value&& s)
	    : first(std::move(f))
	    , second(std::move(s))
	{}

	mutable_pair(const self_type& p)
	    : first(p.first)
	    , second(p.second)
	{}

	mutable_pair(self_type&& p)
	    : first(std::move(p.first))
	    , second(std::move(p.second))
	{}

	mutable_pair(const std::pair<Key, Value>& p)
	    : first(p.first)
	    , second(p.second)
	{}

	mutable_pair(std::pair<Key, Value>&& p)
	    : first(std::move(p.first))
	    , second(std::move(p.second))
	{}

	Key           first;
	mutable Value second;
};

}

template <class Key, class Value, bool Sequential, bool Unique>
class KProps_base
{
protected:
	// non-sequential

	// our index tags (they are just names)
	struct IndexByKey{};
	// make this an alias - there is no sequential index..
	using  IndexBySeq = IndexByKey;

	using Element = KProps_detail::mutable_pair<Key, Value>;

	// the multi_index definition. Looks a bit convoluted because
	// it adapts to Unique and Sequential template arguments..
	using storage_type =
			// the non-sequential variant
			multi_index_container<
				Element,
				indexed_by<
					std::conditional_t<
						Unique,
						// the non-sequential unique variant
						hashed_unique<
							tag<IndexByKey>, member<Element, Key, &Element::first>
						>,
						// the non-sequential non-unique variant
						hashed_non_unique<
							tag<IndexByKey>, member<Element, Key, &Element::first>
						>
					>
				>
		>;

};

template<class Key, class Value, bool Unique>
class KProps_base<Key, Value, true, Unique>
{
protected:
	// sequential

	// our index tags (they are just names)
	struct IndexByKey{};
	struct IndexBySeq{};

	using Element = KProps_detail::mutable_pair<Key, Value>;

	// the multi_index definition. Looks a bit convoluted because
	// it adapts to Unique and Sequential template arguments..
	using storage_type =
			// the sequential variant
			multi_index_container<
				Element,
				indexed_by<
					std::conditional_t<
						Unique,
						// the sequential unique variant
						hashed_unique<
							tag<IndexByKey>, member<Element, Key, &Element::first>
						>,
						// the sequential non-unique variant
						hashed_non_unique<
							tag<IndexByKey>, member<Element, Key, &Element::first>
						>
					>,
					random_access<
						tag<IndexBySeq>
					>
				>
			>;

};

/// The KProps template class is a generic key value store that allows
/// for optimized associative and random (index) access at the same time.
///
/// If sequential access is not needed, it can be switched off by
/// instantiating with the template parameter Sequential set to false.
///
/// Also, per default, multiple keys (or key/value pairs) with different or
/// the same value may exist. To switch to unique keys, set Unique to true.
template <class Key, class Value, bool Sequential = true, bool Unique = false>
class KProps : protected KProps_base<Key, Value, Sequential, Unique>
{
private:
	using self_type      = KProps<Key, Value, Sequential, Unique>;
	using base_type      = KProps_base<Key, Value, Sequential, Unique>;

	using Element        = typename base_type::Element;
	using storage_type   = typename base_type::storage_type;

	using IndexByKey     = typename base_type::IndexByKey;
	using IndexBySeq     = typename base_type::IndexBySeq;

	using IndexByKey_t   = typename index<storage_type, IndexByKey>::type;
	using IndexBySeq_t   = typename index<storage_type, IndexBySeq>::type;

	storage_type m_Storage;

	IndexByKey_t& m_KeyIndex;
	IndexBySeq_t& m_SeqIndex;

	static Element s_EmptyElement;

public:

	// iterators automatically flip to IndexByKey if no sequence, see base class alias
	using iterator           = typename IndexBySeq_t::iterator;
	using const_iterator     = typename IndexBySeq_t::const_iterator;
	using map_iterator       = typename IndexByKey_t::iterator;
	using const_map_iterator = typename IndexByKey_t::const_iterator;
	using range              = std::pair<map_iterator, map_iterator>;
	using const_range        = std::pair<const_map_iterator, const_map_iterator>;

	class Parser
	{
	public:
		Parser() {}
		virtual ~Parser() {}
		virtual bool Get(Key& key, Value& value) = 0;
	};

	class Serializer
	{
	public:
		Serializer() {}
		virtual ~Serializer() {}
		virtual bool Set(const Key& key, const Value& value) = 0;
	};


	KProps()
	    : m_KeyIndex(m_Storage.template get<IndexByKey>())
	    , m_SeqIndex(m_Storage.template get<IndexBySeq>())
	{
	}

	KProps(const self_type& other)
	    : m_Storage(other.m_Storage)
	    , m_KeyIndex(m_Storage.template get<IndexByKey>())
	    , m_SeqIndex(m_Storage.template get<IndexBySeq>())
	{
	}

	KProps(self_type&& other)
	    : m_Storage(std::move(other.m_Storage))
	    , m_KeyIndex(m_Storage.template get<IndexByKey>())
	    , m_SeqIndex(m_Storage.template get<IndexBySeq>())
	{
	}

	self_type& operator=(const self_type& other)
	{
		m_Storage  = other.m_Storage;
		m_KeyIndex = m_Storage.template get<IndexByKey>();
		m_SeqIndex = m_Storage.template get<IndexBySeq>();
		return *this;
	}

	self_type& operator=(self_type&& other)
	{
		m_Storage  = std::move(other.m_Storage);
		m_KeyIndex = m_Storage.template get<IndexByKey>();
		m_SeqIndex = m_Storage.template get<IndexBySeq>();
		return *this;
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == true>* = nullptr>
	iterator begin()
	{
		return m_SeqIndex.begin();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == false>* = nullptr>
	iterator begin()
	{
		return m_KeyIndex.begin();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == true>* = nullptr>
	iterator end()
	{
		return m_SeqIndex.end();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == false>* = nullptr>
	iterator end()
	{
		return m_KeyIndex.end();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == true>* = nullptr>
	const_iterator cbegin() const
	{
		return m_SeqIndex.begin();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == false>* = nullptr>
	const_iterator cbegin() const
	{
		return m_KeyIndex.begin();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == true>* = nullptr>
	const_iterator cend() const
	{
		return m_SeqIndex.end();
	}

	template<bool Seq = Sequential,
	         typename std::enable_if_t<Seq == false>* = nullptr>
	const_iterator cend() const
	{
		return m_KeyIndex.end();
	}

	const_iterator begin() const
	{
		return cbegin();
	}

	const_iterator end() const
	{
		return cend();
	}

	self_type& operator+=(const self_type& other)
	{
		for (auto it = other.begin(); it != other.end(); ++it)
		{
			Add(it->first, it->second);
		}
		return *this;
	}

	self_type& operator+=(self_type&& other)
	{
		for (auto it = other.begin(); it != other.end(); ++it)
		{
			Add(std::move(it->first), std::move(it->second));
		}
		return *this;
	}

	size_t Parse(Parser& parser)
	{
		size_t iCount{0};
		for (;;)
		{
			Key key;
			Value value;
			if (!parser.Get(key, value))
			{
				break;
			}
			if (!Add(std::move(key), std::move(value)))
			{
				KLog().warning("KProps: cannot add new pair");
				continue;
			}
			++iCount;
		}
		return iCount;
	}

	bool Serialize(Serializer& serializer) const
	{
		for (auto it = begin(); it != end(); ++it)
		{
			serializer.Set(it.first, it.second);
		}
		return !empty();
	}

	// perfect forwarding and SFINAE for unique instances
	template <class K, class V = Value, bool Uq = Unique,
	          typename std::enable_if<Uq == true>::type* = nullptr>
	iterator Set(K&& key, V&& newValue = V{})
	{
		auto it = m_KeyIndex.find(key);
		if (it != m_KeyIndex.end())
		{
			it->second = std::forward<V>(newValue);
			// if this is not a sequential instance, IndexBySeq is aliased to IndexByKey
			return m_Storage.template project<IndexBySeq>(it);
		}
		else
		{
			return Add(std::forward<K>(key), std::forward<V>(newValue));
		}
	}

	// perfect forwarding and SFINAE for non-unique instances
	template <class K, class V = Value, bool Uq = Unique,
	          typename std::enable_if<Uq == false>::type* = nullptr>
	iterator Set(K&& key, V&& newValue = V{})
	{
		auto range = m_KeyIndex.equal_range(key);
		if (range.first != range.second)
		{
			for (auto it = range.first; it != range.second; ++it)
			{
				// we cannot forward as we may have multiple values to insert to
				it->second = newValue;
			}
			// if this is not a sequential instance, IndexBySeq is aliased to IndexByKey
			return m_Storage.template project<IndexBySeq>(range.first);
		}
		else
		{
			return Add(std::forward<K>(key), std::forward<V>(newValue));
		}
	}

	// perfect forwarding and SFINAE for unique instances
	template <class K, class V1, class V2, bool Uq = Unique,
	          typename std::enable_if<Uq == true>::type* = nullptr>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	{
		// this is actually the same as a Set(key, newValue) as we can only have one
		// record with this key
		return Set(std::forward<K>(key), std::forward<V2>(newValue));
	}

	// perfect forwarding and SFINAE for non-unique instances
	template <class K, class V1, class V2, bool Uq = Unique,
	          typename std::enable_if<Uq == false>::type* = nullptr>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	{
		bool replaced{false};

		auto ret   = end();
		auto range = m_KeyIndex.equal_range(key);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == value)
			{
				// we cannot forward as we may have multiple values to insert to
				it->second = newValue;

				if (replaced == false)
				{
					// if this is not a sequential instance, IndexBySeq is aliased to IndexByKey
					ret = m_Storage.template project<IndexBySeq>(range.first);
					replaced = true;
				}
			}
		}

		if (replaced)
		{
			// returns an iterator on the first element replaced
			return ret;
		}
		else
		{
			return Add(std::forward<K>(key), std::forward<V2>(newValue));
		}
	}

//----------
protected:
//----------
	// SFINAE && perfect forwarding for Sequential or non-unique instances
	template<class K, class V, bool Sq = Sequential, bool Uq = Unique,
			 typename std::enable_if_t<Sq == true || Uq == false>* = nullptr>
	iterator Replace(K&& key, V&& value)
	{
		return Set(std::forward<K>(key), std::forward<V>(value));
	}

	// SFINAE && perfect forwarding for Unique non-Sequential instances
	template<class V, bool Sq = Sequential, bool Uq = Unique,
			 typename std::enable_if_t<Sq == false && Uq == true>* = nullptr>
	iterator Replace(iterator it, V&& value)
	{
		it->second = std::forward<V>(value);
		return it;
	}

//----------
public:
//----------
	// perfect forwarding and SFINAE for unique sequential instances
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
	         typename std::enable_if_t<Uq == true && Sq == true>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = m_KeyIndex.emplace(key, value);
		if (pair.second)
		{
			return m_Storage.template project<IndexBySeq>(pair.first);
		}
		else
		{
			return Replace(std::forward<K>(key), std::forward<V>(value));
		}
	}

	// perfect forwarding and SFINAE for unique non-sequential instances
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
	         typename std::enable_if_t<Uq == true && Sq == false>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = m_KeyIndex.emplace(key, value);
		if (pair.second)
		{
			return pair.first;
		}
		else
		{
			return Replace(pair.first, std::forward<V>(value));
		}
	}

	// perfect forwarding and SFINAE for non-unique sequential instances
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
	         typename std::enable_if_t<Uq == false && Sq == true>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = m_KeyIndex.emplace(std::forward<K>(key), std::forward<V>(value));
		return m_Storage.template project<IndexBySeq>(pair.first);
	}

	// perfect forwarding and SFINAE for non-unique non-sequential instances
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
	         typename std::enable_if_t<Uq == false && Sq == false>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = m_KeyIndex.emplace(std::forward<K>(key), std::forward<V>(value));
		return pair.first;
	}

	// SFINAE for Unique instances
	template<bool Uq = Unique,
	         typename std::enable_if<Uq == true>::type* = nullptr>
	size_t Remove(const Key& key)
	{
		auto it = m_KeyIndex.find(key);
		if (it == m_KeyIndex.end())
		{
			return 0;
		}

		m_KeyIndex.erase(it);

		return 1;
	}

	// SFINAE for non Unique instances
	template<bool Uq = Unique,
	         typename std::enable_if<Uq == false>::type* = nullptr>
	size_t Remove(const Key& key)
	{
		auto range = m_KeyIndex.equal_range(key);
		auto iErased = std::distance(range.first, range.second);

		if (iErased)
		{
			m_KeyIndex.erase(range.first, range.second);
		}

		return iErased;
	}

	// perfect forwarding
	template<class K>
	typename IndexByKey_t::iterator find(K&& key)
	{
		return m_KeyIndex.find(std::forward<K>(key));
	}

	// perfect forwarding, SFINAE for non-sequential case
	template<class K>
	typename IndexByKey_t::const_iterator find(K&& key) const
	{
		return m_KeyIndex.find(std::forward<K>(key));
	}

	// perfect forwarding
	template<class K>
	Value& Get(K&& key)
	{
		auto it = m_KeyIndex.find(std::forward<K>(key));
		if (it != m_KeyIndex.end())
		{
			return it->second;
		}
		KLog().debug(2, "KProps::Get() called for non-existing key");
		return s_EmptyElement.second;
	}

	// perfect forwarding
	template<class K>
	const Value& Get(K&& key) const
	{
		auto it = m_KeyIndex.find(std::forward<K>(key));
		if (it != m_KeyIndex.end())
		{
			return it->second;
		}
		KLog().debug(2, "KProps::Get() const called for non-existing key");
		return s_EmptyElement.second;
	}

	// perfect forwarding
	template<class K>
	const_range GetMulti(K&& key) const
	{
		return m_KeyIndex.equal_range(std::forward<K>(key));
	}

	// perfect forwarding
	template<class K>
	range GetMulti(K&& key)
	{
		return m_KeyIndex.equal_range(std::forward<K>(key));
	}

	// perfect forwarding
	template<class K>
	size_t Count(K&& key) const
	{
		return m_KeyIndex.count(std::forward<K>(key));
	}

	// perfect forwarding
	template<class K>
	bool IsMulti(K&& key) const
	{
		return Count(std::forward<K>(key)) > 1;
	}

	// SFINAE, only active for Sequential instances
	template<bool Seq = Sequential,
	         typename std::enable_if<Seq == true>::type* = nullptr>
	const Element& at(size_t index) const
	{
		if (index < size())
		{
			return m_SeqIndex.at(index);
		}
		else
		{
			KLog().warning("KProps::at() const called for index {}, which is out of range [0,{})", index, size());
			return s_EmptyElement;
		}
	}

	// SFINAE, only active for non-integral Sequential instances
	template<class T = Key, bool Seq = Sequential,
	         typename = std::enable_if_t<!std::is_integral<T>::value && Seq == true> >
	const Element& operator[](size_t index) const
	{
		return at(index);
	}

	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	template<class K, class T = Key,
	         typename = std::enable_if_t<std::is_integral<T>::value || !std::is_integral<K>::value> >
	const Value& operator[](K&& key) const
	{
		return Get(std::forward<K>(key));
	}

	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	template<class K, class T = Key,
	         typename = std::enable_if_t<std::is_integral<T>::value || !std::is_integral<K>::value> >
	Value& operator[](K&& key)
	{
		auto it = m_KeyIndex.find(key);
		if (it != m_KeyIndex.end())
		{
			return it->second;
		}
		else
		{
			// create new element and return it
			return Add(std::forward<K>(key))->second;
		}
	}

	void clear()
	{
		m_Storage.clear();
	}

	size_t size() const
	{
		return m_Storage.size();
	}

	bool empty() const
	{
		return m_Storage.empty();
	}
};

template<class Key, class Value, bool Sequential, bool Unique>
typename KProps<Key, Value, Sequential, Unique>::Element
KProps<Key, Value, Sequential, Unique>::s_EmptyElement;


} // end of namespace dekaf2

