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

/// @file kprops.h
/// provides a multi-indexed container used for property sheets

#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <algorithm>
#include "kcppcompat.h"
#include "kmutable_pair.h"
#include "../ksplit.h"
#include "../klog.h"
#include "../kstream.h"

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
					std::conditional_t<
						Unique,
						// the non-sequential unique variant
						boost::multi_index::hashed_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>,
						// the non-sequential non-unique variant
						boost::multi_index::hashed_non_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>
					>
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
					std::conditional_t<
						Unique,
						// the sequential unique variant
						boost::multi_index::hashed_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>,
						// the sequential non-unique variant
						boost::multi_index::hashed_non_unique<
							boost::multi_index::tag<IndexByKey>, boost::multi_index::member<Element, Key, &Element::first>
						>
					>,
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

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Parser
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		Parser() {}
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual ~Parser() {}
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
		Serializer() {}
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual ~Serializer() {}
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		virtual bool Set(const Key& key, const Value& value) = 0;
		//-----------------------------------------------------------------------------

	}; // Serializer

	//-----------------------------------------------------------------------------
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	bool operator==(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return (
			size() == other.size()) &&
			std::equal(begin(), end(), other.begin());
	}

	//-----------------------------------------------------------------------------
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	bool operator!=(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		return !((*this) == other);
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == false>* = nullptr>
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == false>* = nullptr>
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == false>* = nullptr>
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().begin();
	}

	//-----------------------------------------------------------------------------
	/// sequential iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == true>* = nullptr>
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return SeqIndex().end();
	}

	//-----------------------------------------------------------------------------
	/// non-sequential (map) iterator
	template<bool Seq = Sequential,
			typename std::enable_if_t<Seq == false>* = nullptr>
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
	template<class K=int>
	size_t Load(KInStream& Input, const char chPairDelim = '=', KStringView svDelim = "\n")
	//-----------------------------------------------------------------------------
	{
		size_t iNewElements{0};

		if (Input.InStream().good())
		{
			if (!svDelim.empty())
			{
				Input.SetReaderEndOfLine(svDelim.front());
			}
			KString sLine;
			while (Input.ReadLine(sLine))
			{
				sLine.Trim();
				if (svDelim != "\n" || (!sLine.empty() && sLine.front() != '#'))
				{
					iNewElements += kSplitPairs(*this, sLine, chPairDelim, svDelim);
				}
			}

		}

		return iNewElements;

	} // Load

	//-----------------------------------------------------------------------------
	size_t Load(KStringView sInput, const char chPairDelim = '=', KStringView svDelim = "\n")
	//-----------------------------------------------------------------------------
	{
		KInFile fin(sInput);
		return Load(fin, chPairDelim, svDelim);

	} // Load

	//-----------------------------------------------------------------------------
	size_t Store(KOutStream& Output, const char chPairDelim = '=', KStringView svDelim = "\n")
	//-----------------------------------------------------------------------------
	{
		size_t iNewElements{0};

		if (Output.OutStream().good())
		{
			if (svDelim == "\n")
			{
				Output.Write("#! KPROPS");
				Output.Write(svDelim);
			}

			for (const auto& it : *this)
			{
				Output.Write(it.first);
				Output.Write(chPairDelim);
				Output.Write(it.second);
				Output.Write(svDelim);
				++iNewElements;
			}
		}

		return iNewElements;

	} // Store

	//-----------------------------------------------------------------------------
	size_t Store(KStringView sOutput, const char chPairDelim = '=', KStringView svDelim = "\n")
	//-----------------------------------------------------------------------------
	{
		KOutFile fout(sOutput);
		return Store(fout, chPairDelim, svDelim);

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
	// perfect forwarding and SFINAE for unique instances
	/// Set a new value for an existing key. If the key is not existing it is created.
	template <class K, class V = Value, bool Uq = Unique,
			typename std::enable_if<Uq == true>::type* = nullptr>
	iterator Set(K&& key, V&& newValue = V{})
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(key);
		if (it != KeyIndex().end())
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

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	/// Set a new value for an existing key. If the key is not existing it is created.
	template <class K, class V = Value, bool Uq = Unique,
			typename std::enable_if<Uq == false>::type* = nullptr>
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
			return Add(std::forward<K>(key), std::forward<V>(newValue));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique instances
	/// Set a new value for an existing key. If the key is not existing it is created.
	template <class K, class V1, class V2, bool Uq = Unique,
			typename std::enable_if<Uq == true>::type* = nullptr>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	//-----------------------------------------------------------------------------
	{
		// this is actually the same as a Set(key, newValue) as we can only have one
		// record with this key
		return Set(std::forward<K>(key), std::forward<V2>(newValue));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	/// Set a new value for existing key/value pairs. If the key/value pair is not existing it is created.
	template <class K, class V1, class V2, bool Uq = Unique,
			typename std::enable_if<Uq == false>::type* = nullptr>
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
			return Add(std::forward<K>(key), std::forward<V2>(newValue));
		}
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding for Sequential or non-unique instances
	/// Replace the value of an existing key
	template<class K, class V, bool Sq = Sequential, bool Uq = Unique,
			typename std::enable_if_t<Sq == true || Uq == false>* = nullptr>
	iterator Replace(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return Set(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding for Unique non-Sequential instances
	/// Replace the value of an existing key
	template<class V, bool Sq = Sequential, bool Uq = Unique,
			typename std::enable_if_t<Sq == false && Uq == true>* = nullptr>
	iterator Replace(iterator it, V&& value)
	//-----------------------------------------------------------------------------
	{
		it->second = std::forward<V>(value);
		return it;
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique sequential instances
	/// Add a new Key/Value pair. If the key is already existing, its value is replaced.
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
			typename std::enable_if_t<Uq == true && Sq == true>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = KeyIndex().emplace(key, value);
		if (pair.second)
		{
			return m_Storage.template project<IndexBySeq>(pair.first);
		}
		else
		{
			return Replace(std::forward<K>(key), std::forward<V>(value));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique non-sequential instances
	/// Add a new Key/Value pair. If the key is already existing, its value is replaced.
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
			typename std::enable_if_t<Uq == true && Sq == false>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = KeyIndex().emplace(key, value);
		if (pair.second)
		{
			return pair.first;
		}
		else
		{
			return Replace(pair.first, std::forward<V>(value));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique sequential instances
	/// Add a new Key/Value pair.
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
			typename std::enable_if_t<Uq == false && Sq == true>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = KeyIndex().emplace(std::forward<K>(key), std::forward<V>(value));
		return m_Storage.template project<IndexBySeq>(pair.first);
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique non-sequential instances
	/// Add a new Key/Value pair.
	template<class K, class V = Value, bool Uq = Unique, bool Sq = Sequential,
			typename std::enable_if_t<Uq == false && Sq == false>* = nullptr >
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		// boost::multi_index does not know try_emplace, so we have to do value
		// copies for the emplace anyway, as we could not call Replace() otherwise
		auto pair = KeyIndex().emplace(std::forward<K>(key), std::forward<V>(value));
		return pair.first;
	}

	//-----------------------------------------------------------------------------
	// SFINAE for Unique instances
	/// remove a Key/Value pair with a given key. Returns count of removed elements.
	template<bool Uq = Unique,
			typename std::enable_if<Uq == true>::type* = nullptr>
	size_t Remove(const Key& key)
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
	template<bool Uq = Unique,
			typename std::enable_if<Uq == false>::type* = nullptr>
	size_t Remove(const Key& key)
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
	// perfect forwarding
	/// returns iterator on the element with the given key.
	template<class K>
	typename IndexByKey_t::iterator find(K&& key)
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding, SFINAE for non-sequential case
	/// returns const_iterator on the element with the given key.
	template<class K>
	typename IndexByKey_t::const_iterator find(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	Value& Get(K&& key)
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(std::forward<K>(key));
		if (it != KeyIndex().end())
		{
			return it->second;
		}
		kDebug(2, "called for non-existing key");
		return s_EmptyElement_v.second;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	const Value& Get(K&& key) const
	//-----------------------------------------------------------------------------
	{
		auto it = KeyIndex().find(std::forward<K>(key));
		if (it != KeyIndex().end())
		{
			return it->second;
		}
		kDebug(2, "called for non-existing key");
		return s_EmptyElement.second;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns iterator range of the elements with the given key.
	template<class K>
	const_range GetMulti(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().equal_range(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns iterator range of the elements with the given key.
	template<class K>
	range GetMulti(K&& key)
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().equal_range(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns count of elements with the given key.
	template<class K>
	size_t Count(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return KeyIndex().count(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	/// Returns true if at least one element with the given key exists.
	template<class K>
	bool Exists(K&& key) const
	//-----------------------------------------------------------------------------
	{
		// TODO switch to find() once we have unified the end() for sequential
		// and non sequential KProps
		return Count(std::forward<K>(key)) > 0;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns true if a key appears multiple times.
	template<class K>
	bool IsMulti(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return Count(std::forward<K>(key)) > 1;
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<bool Seq = Sequential,
			typename std::enable_if<Seq == true>::type* = nullptr>
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
			typename = std::enable_if_t<!std::is_integral<T>::value && Seq == true> >
	const Element& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		return at(index);
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	/// Gets the element with the given key. Returns empty element if not found.
	template<class K, class T = Key,
			typename = std::enable_if_t<std::is_integral<T>::value || !std::is_integral<K>::value> >
	const Value& operator[](K&& key) const
	//-----------------------------------------------------------------------------
	{
		return Get(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	/// Gets the element with the given key. Returns empty element if not found.
	template<class K, class T = Key,
			typename = std::enable_if_t<std::is_integral<T>::value || !std::is_integral<K>::value> >
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
			return Add(std::forward<K>(key))->second;
		}
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
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.size();
	}

	//-----------------------------------------------------------------------------
	/// Returns true if no elements are stored.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.empty();
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element at the end. (InsertIterator interface)
	void push_back(const Element& element)
	//-----------------------------------------------------------------------------
	{
		Add(element.first, element.second);
	}

	//-----------------------------------------------------------------------------
	/// Move-inserts one element at the end. (InsertIterator interface)
	void push_back(Element&& element)
	//-----------------------------------------------------------------------------
	{
		Add(std::move(element.first), std::move(element.second));
	}

	//-----------------------------------------------------------------------------
	/// Inserts one key at the end. (InsertIterator interface)
	void push_back(const Key& key)
	//-----------------------------------------------------------------------------
	{
		Add(key);
	}

	//-----------------------------------------------------------------------------
	/// Move-inserts one key at the end. (InsertIterator interface)
	void push_back(Key&& key)
	//-----------------------------------------------------------------------------
	{
		Add(std::move(key));
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element at the end. (InsertIterator interface)
	void insert(const Element& element)
	//-----------------------------------------------------------------------------
	{
		push_back(element);
	}

	//-----------------------------------------------------------------------------
	/// Move-inserts one element at the end. (InsertIterator interface)
	void insert(Element&& element)
	//-----------------------------------------------------------------------------
	{
		push_back(std::move(element));
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element at the end. (InsertIterator interface)
	template<class K, class V>
	void emplace(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		push_back(Element(std::forward<K>(key), std::forward<V>(value)));
	}


}; // KProps

template<class Key, class Value, bool Sequential, bool Unique>
const typename KProps<Key, Value, Sequential, Unique>::Element
KProps<Key, Value, Sequential, Unique>::s_EmptyElement = Element();

template<class Key, class Value, bool Sequential, bool Unique>
typename KProps<Key, Value, Sequential, Unique>::Element
KProps<Key, Value, Sequential, Unique>::s_EmptyElement_v = Element();

} // end of namespace dekaf2
