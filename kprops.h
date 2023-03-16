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

/// @file kprops.h
/// provides a multi-indexed container used for property sheets

#ifndef BOOST_NO_CXX98_FUNCTION_BASE
	#define BOOST_NO_CXX98_FUNCTION_BASE
#endif
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <algorithm>
#include "bits/kcppcompat.h"
#include "bits/kmutable_pair.h"
#include "ksplit.h"
#include "kjoin.h"
#include "klog.h"
#include "kstream.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// definition of the multi index storage type for non-sequential KProps types
template <class Key, class Value, bool Sequential, bool Unique>
class KPropsBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	// non-sequential

	// our index tags (they are just names)
	struct IndexByKey{};
	// make this an alias - there is no sequential index..
	using  IndexBySeq = IndexByKey;

	using Element = detail::KMutablePair<Key, Value>;

	// the multi_index definition. Looks a bit convoluted because
	// it adapts to Unique and Sequential template arguments..
	using storage_type =
			// the non-sequential variant
			boost::multi_index::multi_index_container<
				Element,
				boost::multi_index::indexed_by<
	                typename std::conditional<
						Unique,
						// the non-sequential unique variant
						boost::multi_index::hashed_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>,
						// the non-sequential non-unique variant
						boost::multi_index::hashed_non_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>
					>::type
				>
			>;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// definition of the multi index storage type for sequential KProps types
template<class Key, class Value, bool Unique>
class KPropsBase<Key, Value, true, Unique>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	// sequential

	// our index tags (they are just names)
	struct IndexByKey{};
	struct IndexBySeq{};

	using Element = detail::KMutablePair<Key, Value>;

	// the multi_index definition. Looks a bit convoluted because
	// it adapts to Unique and Sequential template arguments..
	using storage_type =
			// the sequential variant
			boost::multi_index::multi_index_container<
				Element,
				boost::multi_index::indexed_by<
				    typename std::conditional<
						Unique,
						// the sequential unique variant
						boost::multi_index::hashed_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>,
						// the sequential non-unique variant
						boost::multi_index::hashed_non_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>
					>::type,
					boost::multi_index::random_access<
						boost::multi_index::tag<IndexBySeq>
					>
				>
			>;

}; // KPropsBase

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generic key value store with optimized associative and sequentially
/// indexed access at the same time.
///
/// If sequential access is not needed, it can be switched off by
/// instantiating with the template parameter Sequential set to false.
///
/// Also, per default, multiple keys (or key/value pairs) with different or
/// the same value may exist. To switch to unique keys, set Unique to true.
template <class Key, class Value, bool Sequential = true, bool Unique = false>
class KProps : protected KPropsBase<Key, Value, Sequential, Unique>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using self_type      = KProps<Key, Value, Sequential, Unique>;
	using base_type      = KPropsBase<Key, Value, Sequential, Unique>;

	using Element        = typename base_type::Element;
	using storage_type   = typename base_type::storage_type;

	using IndexByKey     = typename base_type::IndexByKey;
	using IndexBySeq     = typename base_type::IndexBySeq;

	using IndexByKey_t   = typename boost::multi_index::index<storage_type, IndexByKey>::type;
	using IndexBySeq_t   = typename boost::multi_index::index<storage_type, IndexBySeq>::type;

	storage_type m_Storage;

	//-----------------------------------------------------------------------------
	inline const IndexByKey_t& KeyIndex() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.template get<IndexByKey>();
	}

	//-----------------------------------------------------------------------------
	inline IndexByKey_t& KeyIndex()
	//-----------------------------------------------------------------------------
	{
		return m_Storage.template get<IndexByKey>();
	}

	//-----------------------------------------------------------------------------
	inline const IndexBySeq_t& SeqIndex() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.template get<IndexBySeq>();
	}

	//-----------------------------------------------------------------------------
	inline IndexBySeq_t& SeqIndex()
	//-----------------------------------------------------------------------------
	{
		return m_Storage.template get<IndexBySeq>();
	}

	static const Element s_EmptyElement;
	static Element s_EmptyElement_v;

//----------
public:
//----------

	// iterators automatically flip to IndexByKey if no sequence, see base class alias
	using iterator           = typename IndexBySeq_t::iterator;
	using const_iterator     = typename IndexBySeq_t::const_iterator;
	using map_iterator       = typename IndexByKey_t::iterator;
	using const_map_iterator = typename IndexByKey_t::const_iterator;
	using range              = std::pair<map_iterator, map_iterator>;
	using const_range        = std::pair<const_map_iterator, const_map_iterator>;
	using value_type         = Element;
	using key_type           = Key;
	using mapped_type        = Value;

	KProps() = default;
	KProps (std::initializer_list<value_type> il) : m_Storage(il) {}
	template<class _InputIterator>
	KProps (_InputIterator first, _InputIterator last) : m_Storage(first, last) {}

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Parser
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		Parser() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual ~Parser() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual bool Get(Key& key, Value& value) = 0;
		//-----------------------------------------------------------------------------

	}; // Parser

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Serializer
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		Serializer() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual ~Serializer() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual bool Set(const Key& key, const Value& value) = 0;
		//-----------------------------------------------------------------------------

	}; // Serializer

	//-----------------------------------------------------------------------------
	bool operator==(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return (
			size() == other.size()) &&
			std::equal(begin(), end(), other.begin());
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(other);
	}

	//-----------------------------------------------------------------------------
	bool operator<(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	//-----------------------------------------------------------------------------
	bool operator>(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		return other < *this;
	}

	//-----------------------------------------------------------------------------
	bool operator<=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		return !(*this > other);
	}

	//-----------------------------------------------------------------------------
	bool operator>=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		return !(other < *this);
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true, int>::type = 0>
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == false, int>::type = 0>
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true, int>::type = 0>
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == false, int>::type = 0>
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true, int>::type = 0>
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == false, int>::type = 0>
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true, int>::type = 0>
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == false, int>::type = 0>
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// const_iterator
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return cbegin();
	}

	//-----------------------------------------------------------------------------
	/// const_iterator
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return cend();
	}

	//-----------------------------------------------------------------------------
	/// append a KProps struct to another one
	self_type& operator+=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		for (auto& it : other)
		{
			Add(it.first, it.second);
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move a Kprops struct to the end of another one
	self_type& operator+=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		for (auto& it : other)
		{
			Add(std::move(it.first), std::move(it.second));
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	size_t Load(KInStream& Input, KStringView svDelim = "\n", KStringView svPairDelim = "=")
	//-----------------------------------------------------------------------------
	{
		size_t iNewElements { 0 };

		if (Input.InStream().good())
		{
			if (!svDelim.empty())
			{
				Input.SetReaderEndOfLine(svDelim.front());
				Input.SetReaderRightTrim(svDelim);
			}
			KFindSetOfChars Delim(svDelim);
			KString sLine;
			while (Input.ReadLine(sLine))
			{
				// make sure we detect a comment line even with leading spaces
				sLine.TrimLeft();

				if (svDelim != "\n" || (!sLine.empty() && sLine.front() != '#'))
				{
					iNewElements += kSplit(*this, sLine, Delim, svPairDelim);
				}
			}
		}

		return iNewElements;

	} // Load

	//-----------------------------------------------------------------------------
	size_t Load(KStringViewZ sInputFile, KStringView svDelim = "\n", KStringView svPairDelim = "=")
	//-----------------------------------------------------------------------------
	{
		KInFile fin(sInputFile);
		return Load(fin, svDelim, svPairDelim);

	} // Load

	//-----------------------------------------------------------------------------
	size_t Store(KOutStream& Output, KStringView svDelim = "\n", KStringView svPairDelim = "=")
	//-----------------------------------------------------------------------------
	{
		size_t iNewElements { 0 };

		do
		{
			if (svDelim == "\n")
			{
				if (!Output.Write("#! KPROPS")) break;
				if (!Output.Write(svDelim)) break;
			}

			kJoin(Output, *this, svDelim, svPairDelim, true);
			
			if (Output)
			{
				iNewElements = size();
			}

		} while (false);

		return iNewElements;

	} // Store

	//-----------------------------------------------------------------------------
	size_t Store(KStringViewZ sOutputFile, KStringView svDelim = "\n", KStringView svPairDelim = "=")
	//-----------------------------------------------------------------------------
	{
		KOutFile fout(sOutputFile);
		return Store(fout, svDelim, svPairDelim);

	} // Store

	//-----------------------------------------------------------------------------
	/// Parse input and add to the KProps struct
	size_t Parse(Parser& parser)
	//-----------------------------------------------------------------------------
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
			Add(std::move(key), std::move(value));
			++iCount;
		}
		return iCount;
	}

	//-----------------------------------------------------------------------------
	/// serialize a KProps struct
	bool Serialize(Serializer& serializer) const
	//-----------------------------------------------------------------------------
	{
		for (auto& it : *this)
		{
			serializer.Set(it.first, it.second);
		}
		return true;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for sequential instances
	template<class ValueType = value_type, bool Sq = Sequential,
	         typename std::enable_if<Sq == true, int>::type = 0>
	std::pair<iterator, bool> insert(ValueType&& value)
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().push_back(std::forward<ValueType>(value));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-sequential instances
	template<class ValueType = value_type, bool Sq = Sequential,
	         typename std::enable_if<Sq == false, int>::type = 0>
	std::pair<iterator, bool> insert(ValueType&& value)
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().insert(std::forward<ValueType>(value));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique instances
	template<class K, class V, bool Uq = Unique,
	         typename std::enable_if<Uq == true, int>::type = 0>
	std::pair<iterator, bool> insert_or_assign(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it != KeyIndex().end())
		{
			it->second = std::forward<V>(value);
			// if this is not a sequential instance, IndexBySeq is aliased to IndexByKey
			return { m_Storage.template project<IndexBySeq>(it), false };
		}
		else
		{
			return insert(value_type(std::forward<K>(key), std::forward<V>(value)));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	template<class K, class V, bool Uq = Unique,
	         typename std::enable_if<Uq == false, int>::type = 0>
	std::pair<iterator, bool> insert_or_assign(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return insert(value_type(std::forward<K>(key), std::forward<V>(value)));
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element.
	template<class K, class V>
	std::pair<iterator, bool> emplace(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return insert(value_type(std::forward<K>(key), std::forward<V>(value)));
	}

	//-----------------------------------------------------------------------------
	/// returns iterator on the element with the given key.
	template<class K>
	iterator find(const K& key)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		return m_Storage.template project<IndexBySeq>(it);
	}

	//-----------------------------------------------------------------------------
	/// returns const_iterator on the element with the given key.
	template<class K>
	const_iterator find(const K& key) const
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		return m_Storage.template project<IndexBySeq>(it);
	}

	//-----------------------------------------------------------------------------
	/// Returns iterator range of the elements with the given key.
	template<class K>
	const_range equal_range(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().equal_range(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns iterator range of the elements with the given key.
	template<class K>
	range equal_range(const K& key)
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().equal_range(key);
	}

	//-----------------------------------------------------------------------------
	// SFINAE for Unique instances
	/// remove a Key/Value pair with a given key. Returns count of removed elements.
	template<class K, bool Uq = Unique,
			typename std::enable_if<Uq == true, int>::type = 0>
	size_t erase(const K& key)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it == KeyIndex().end())
		{
			return 0;
		}

		KeyIndex().erase(it);

		return 1;
	}

	//-----------------------------------------------------------------------------
	// SFINAE for non Unique instances
	/// remove Key/Value pairs with a given key. Returns count of removed elements.
	template<class K, bool Uq = Unique,
			typename std::enable_if<Uq == false, int>::type = 0>
	size_t erase(const K& key)
	//-----------------------------------------------------------------------------
	{
		auto range = KeyIndex().equal_range(key);
		auto iErased = std::distance(range.first, range.second);

		if (iErased)
		{
			KeyIndex().erase(range.first, range.second);
		}

		return iErased;
	}

	//-----------------------------------------------------------------------------
	/// Returns count of elements with the given key.
	template<class K>
	size_t count(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().count(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns true if at least one element with the given key exists.
	template<class K>
	bool contains(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().find(key) != KeyIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// Deletes all elements.
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_Storage.clear();
	}

	//-----------------------------------------------------------------------------
	/// Returns count of all stored elements.
	size_t size() const noexcept
	//-----------------------------------------------------------------------------
	{
		return m_Storage.size();
	}

	//-----------------------------------------------------------------------------
	/// Returns maximum size.
	size_t max_size() const noexcept
	//-----------------------------------------------------------------------------
	{
		return m_Storage.max_size();
	}

	//-----------------------------------------------------------------------------
	/// Returns true if no elements are stored.
	bool empty() const noexcept
	//-----------------------------------------------------------------------------
	{
		return m_Storage.empty();
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique instances
	/// Set a new value for an existing key. If the key is not existing it is created.
	template <class K, class V = Value, bool Uq = Unique,
			typename std::enable_if<Uq == true, int>::type = 0>
	iterator Set(K&& key, V&& newValue = V{})
	//-----------------------------------------------------------------------------
	{
		return insert_or_assign(std::forward<K>(key), std::forward<V>(newValue)).first;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	/// Set a new value for all existing keys. If the key is not existing it is created.
	template <class K, class V = Value, bool Uq = Unique,
			typename std::enable_if<Uq == false, int>::type = 0>
	iterator Set(K&& key, V&& newValue = V{})
	//-----------------------------------------------------------------------------
	{
		auto range = KeyIndex().equal_range(key);
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
			return insert(value_type(std::forward<K>(key), std::forward<V>(newValue))).first;
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique instances
	/// Set a new value for an existing key. If the key is not existing it is created.
	template <class K, class V1, class V2, bool Uq = Unique,
			typename std::enable_if<Uq == true, int>::type = 0>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	//-----------------------------------------------------------------------------
	{
		// this is actually the same as a Set(key, newValue) as we can only have one
		// record with this key
		return insert_or_assign(std::forward<K>(key), std::forward<V2>(newValue)).first;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	/// Set a new value for existing key/value pairs. If the key/value pair is not existing it is created.
	template <class K, class V1, class V2, bool Uq = Unique,
			typename std::enable_if<Uq == false, int>::type = 0>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	//-----------------------------------------------------------------------------
	{
		bool replaced{false};

		auto ret   = end();
		auto range = KeyIndex().equal_range(key);
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
			return insert(value_type(std::forward<K>(key), std::forward<V2>(newValue))).first;
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Add a new Key/Value pair. If the key is already existing, its value is replaced.
	template<class K, class V = Value>
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		return insert_or_assign(std::forward<K>(key), std::forward<V>(value)).first;
	}

	//-----------------------------------------------------------------------------
	/// remove a Key/Value pair with a given key. Returns count of removed elements.
	template<class K>
	size_t Remove(const K& key)
	//-----------------------------------------------------------------------------
	{
		return erase(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	Value& Get(const K& key)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it != KeyIndex().end())
		{
			return it->second;
		}
		return s_EmptyElement_v.second;
	}

	//-----------------------------------------------------------------------------
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	const Value& Get(const K& key) const
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it != KeyIndex().end())
		{
			return it->second;
		}
		return s_EmptyElement.second;
	}

	//-----------------------------------------------------------------------------
	/// Returns iterator range of the elements with the given key.
	template<class K>
	const_range GetMulti(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return equal_range(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns iterator range of the elements with the given key.
	template<class K>
	range GetMulti(const K& key)
	//-----------------------------------------------------------------------------
	{
		return equal_range(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns count of elements with the given key.
	template<class K>
	size_t Count(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return count(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns true if at least one element with the given key exists.
	template<class K>
	bool Contains(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return contains(key);
	}

	//-----------------------------------------------------------------------------
	/// Returns true if a key appears multiple times.
	template<class K>
	bool IsMulti(const K& key) const
	//-----------------------------------------------------------------------------
	{
		return count(key) > 1;
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true, int>::type = 0>
	const Element& at(size_t index) const
	//-----------------------------------------------------------------------------
	{
		if (index < size())
		{
			return SeqIndex().at(index);
		}
		else
		{
			kWarning("called for index {}, which is out of range [0,{})", index, size());
			return s_EmptyElement;
		}
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for non-integral Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<class T = Key, bool Seq = Sequential,
			typename std::enable_if<!std::is_integral<T>::value && Seq == true, int>::type = 0>
	const Element& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		return at(index);
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for non-integral K, or if the Key is integral
	/// Gets the value with the given key. Returns empty value if not found.
	template<class K, class T = Key,
			typename std::enable_if<std::is_integral<T>::value || !std::is_integral<K>::value, int>::type = 0>
	const Value& operator[](const K& key) const
	//-----------------------------------------------------------------------------
	{
		return Get(key);
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	/// Gets the element with the given key. Returns empty element if not found.
	template<class K, class T = Key,
			typename std::enable_if<std::is_integral<T>::value || !std::is_integral<K>::value, int>::type = 0>
	Value& operator[](K&& key)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it != KeyIndex().end())
		{
			return it->second;
		}
		else
		{
			// create new element and return it
			return insert(value_type(std::forward<K>(key), Value{})).first->second;
		}
	}

	//-----------------------------------------------------------------------------
	/// test if the container is non-empty.
	explicit operator bool() const noexcept
	//-----------------------------------------------------------------------------
	{
		return !empty();
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element at the end.
	template<class E>
	void push_back(E&& element)
	//-----------------------------------------------------------------------------
	{
		insert(std::forward<E>(element));
	}

	//-----------------------------------------------------------------------------
	/// Inserts one key / value pair at the end.
	template<class K, class V>
	void push_back(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		insert(value_type(std::forward<K>(key), std::forward<V>(value)));
	}

}; // KProps

template<class Key, class Value, bool Sequential, bool Unique>
const typename KProps<Key, Value, Sequential, Unique>::Element
KProps<Key, Value, Sequential, Unique>::s_EmptyElement = Element();

template<class Key, class Value, bool Sequential, bool Unique>
typename KProps<Key, Value, Sequential, Unique>::Element
KProps<Key, Value, Sequential, Unique>::s_EmptyElement_v = Element();

class KString;

extern template class KProps<KString, KString, /*order-matters=*/false, /*unique-keys=*/true >;
extern template class KProps<KString, KString, /*order-matters=*/true,  /*unique-keys=*/true >;
extern template class KProps<KString, KString, /*order-matters=*/false, /*unique-keys=*/false>;
extern template class KProps<KString, KString, /*order-matters=*/true,  /*unique-keys=*/false>;

} // end of namespace dekaf2
