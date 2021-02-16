/*
//-----------------------------------------------------------------------------//
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
*/

#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <iterator>
#include "kcppcompat.h"
#include "../ksplit.h"
#include "../klog.h"
#include "../kstream.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Key, class Value, bool Unique>
class KProps_map
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using map_type =
	    typename std::conditional<
	        Unique,
	        std::unordered_map<Key, Value>,
	        std::unordered_multimap<Key, Value>
	    >::type;
	using map_value_type = typename map_type::value_type;

	// we need a default constructed map_value_type to
	// be returned in case of index overflows in operator[]
	// As we also want to return them for non-const return
	// values, we declare them non-const, which adds a risk
	// of someone writing to them - but as they are only
	// fallback values for a index range overflow we do not
	// care. The better solution would be to throw an exception
	// for the index overflow case.
	static const map_value_type s_EmptyMapValue;
	static map_value_type s_EmptyMapValue_v;

	map_type m_map;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_map.clear();
	}

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <class Key, class Value, bool Sequential, bool Unique>
class KProps_base : public KProps_map<Key, Value, Unique>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	// map, no vector

//----------
public:
//----------

	using base_type      = KProps_map<Key, Value, Unique>;
	using map_type       = typename base_type::map_type;
	using map_value_type = typename base_type::map_value_type;

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	inline typename map_type::iterator AddToVector(typename map_type::iterator item)
	//-----------------------------------------------------------------------------
	{
		return item;
	}

	//-----------------------------------------------------------------------------
	inline void RemoveFromVector(size_t iCount, const Key& key)
	//-----------------------------------------------------------------------------
	{
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		base_type::clear();
	}

	// public standard iterator interface
	// iterates over map
	using range       = typename std::pair<typename map_type::iterator, typename map_type::iterator>;
	using const_range = typename std::pair<typename map_type::const_iterator, typename map_type::const_iterator>;
	using size_type   = typename map_type::size_type;

	//-----------------------------------------------------------------------------
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.size();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.empty();
	}

	//-----------------------------------------------------------------------------
	/// test if the container is non-empty
	explicit operator bool() const
	//-----------------------------------------------------------------------------
	{
		return !empty();
	}

	using iterator       = typename map_type::iterator;
	using const_iterator = typename map_type::const_iterator;

	//-----------------------------------------------------------------------------
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.begin();
	}

	//-----------------------------------------------------------------------------
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.end();
	}

	//-----------------------------------------------------------------------------
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.begin();
	}

	//-----------------------------------------------------------------------------
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.end();
	}

	//-----------------------------------------------------------------------------
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.cbegin();
	}

	//-----------------------------------------------------------------------------
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.cend();
	}

}; // KProps_base

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Key, class Value, bool Unique>
class KProps_base<Key, Value, true, Unique> : public KProps_map<Key, Value, Unique>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	// map and vector

//----------
public:
//----------

	using base_type      = KProps_map<Key, Value, Unique>;
	using map_type       = typename base_type::map_type;
	using map_value_type = typename base_type::map_value_type;
	using MapValueRef    =
	    typename std::conditional<
	        std::is_const<base_type>::value,
	        std::reference_wrapper<const map_value_type>,
	        std::reference_wrapper<map_value_type>
	    >::type;

//----------
protected:
//----------

	using vector_type = std::vector<MapValueRef>;

	vector_type m_vector;

//----------
public:
//----------

	class const_iterator;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	// we need our own iterator implementation to force the
	// conversion from std::reference_wrapper<map_value_type>
	// to map_value_type& (which makes it look as if we iterate
	// over a map and not a vector). This is done with
	// operator*() and operator->() below
	class iterator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class const_iterator;

	//----------
	public:
	//----------

		typedef iterator self_type;
		typedef map_value_type value_type;
		typedef map_value_type& reference;
		typedef map_value_type* pointer;
		typedef typename vector_type::iterator base_iterator;
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

		//-----------------------------------------------------------------------------
		iterator() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		iterator(const base_iterator& it)
		//-----------------------------------------------------------------------------
		    : m_it{it}
		{}

		//-----------------------------------------------------------------------------
		iterator(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		iterator(self_type&& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		self_type& operator=(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		self_type& operator=(self_type&& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		self_type& operator++()
		//-----------------------------------------------------------------------------
		{
			++m_it;
			return *this;
		}
		// prefix

		//-----------------------------------------------------------------------------
		self_type operator++(int dummy)
		//-----------------------------------------------------------------------------
		{
			self_type i = *this;
			++m_it;
			return i;
		} // postfix

		//-----------------------------------------------------------------------------
		reference operator*()
		//-----------------------------------------------------------------------------
		{
			return m_it->get();
		}

		//-----------------------------------------------------------------------------
		pointer operator->()
		//-----------------------------------------------------------------------------
		{
			return &(m_it->get());
		}

		//-----------------------------------------------------------------------------
		bool operator==(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_it == rhs.m_it;
		}

		//-----------------------------------------------------------------------------
		bool operator!=(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_it != rhs.m_it;
		}

	//----------
	private:
	//----------

		base_iterator m_it;

	}; // iterator

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	// we also add a const_iterator
	class const_iterator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		typedef const_iterator self_type;
		typedef map_value_type value_type;
		typedef map_value_type& reference;
		typedef map_value_type* pointer;
		typedef typename vector_type::const_iterator base_iterator;
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

		//-----------------------------------------------------------------------------
		const_iterator() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		const_iterator(const base_iterator& it)
		//-----------------------------------------------------------------------------
		    : m_it{it}
		{}

		//-----------------------------------------------------------------------------
		const_iterator(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		const_iterator(const iterator& other)
		//-----------------------------------------------------------------------------
		{
			m_it = other.m_it;
		}

		//-----------------------------------------------------------------------------
		self_type& operator=(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		self_type& operator=(self_type&& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		self_type& operator=(const iterator& other)
		//-----------------------------------------------------------------------------
		{
			m_it = other.m_it;
			return *this;
		}

		//-----------------------------------------------------------------------------
		self_type& operator=(iterator&& other)
		//-----------------------------------------------------------------------------
		{
			m_it = std::move(other.m_it);
			return *this;
		}

		//-----------------------------------------------------------------------------
		self_type& operator++() // prefix
		//-----------------------------------------------------------------------------
		{
			++m_it;
			return *this;
		}

		//-----------------------------------------------------------------------------
		self_type operator++(int dummy) // postfix
		//-----------------------------------------------------------------------------
		{
			self_type i = *this;
			++m_it;
			return i;
		}

		//-----------------------------------------------------------------------------
		const value_type& operator*() const
		//-----------------------------------------------------------------------------
		{
			return m_it->get();
		}

		//-----------------------------------------------------------------------------
		const value_type* operator->() const
		//-----------------------------------------------------------------------------
		{
			return &(m_it->get());
		}

		//-----------------------------------------------------------------------------
		bool operator==(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_it == rhs.m_it;
		}

		//-----------------------------------------------------------------------------
		bool operator!=(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_it != rhs.m_it;
		}

	//----------
	private:
	//----------

		base_iterator m_it;

	}; // const_iterator

protected:

	//-----------------------------------------------------------------------------
	inline typename vector_type::iterator AddToVector(typename map_type::iterator item)
	//-----------------------------------------------------------------------------
	{
		m_vector.emplace_back(*item);
		return m_vector.begin() + (m_vector.size() - 1);
	}

	//-----------------------------------------------------------------------------
	void RemoveFromVector(size_t iCount, const Key& key)
	//-----------------------------------------------------------------------------
	{
		if (iCount)
		{
			for (size_t ii = m_vector.size(); ii-- > 0; )
			{
				if (m_vector[ii].get().first == key)
				{
					m_vector.erase(m_vector.begin() + ii);
					if (!--iCount)
					{
						// no more elements with this key
						break;
					}
				}
			}

			if (iCount)
			{
				kWarning("did not find {} instances of the removed key", iCount);
			}
		}
	}

public:

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		base_type::clear();
		m_vector.clear();
	}

	// ranges point into the map, not into the vector (a range groups a number of elements with the same
	// key, of which the vector does not know
	using range       = std::pair<typename map_type::iterator, typename map_type::iterator>;
	using const_range = std::pair<typename map_type::const_iterator, typename map_type::const_iterator>;

	using size_type   = typename map_type::size_type;

	// public standard iterator interface, using our custom iterators
	// iterates over vector, but returns pointers and refs to map

	//-----------------------------------------------------------------------------
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.size();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.empty();
	}

	//-----------------------------------------------------------------------------
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return iterator(m_vector.begin());
	}

	//-----------------------------------------------------------------------------
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return iterator(m_vector.end());
	}

	//-----------------------------------------------------------------------------
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return const_iterator(m_vector.begin());
	}

	//-----------------------------------------------------------------------------
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return const_iterator(m_vector.end());
	}

	//-----------------------------------------------------------------------------
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return const_iterator(m_vector.cbegin());
	}

	//-----------------------------------------------------------------------------
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return const_iterator(m_vector.cend());
	}

};

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
class KProps : public KProps_base<Key, Value, Sequential, Unique>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type      = KProps_base<Key, Value, Sequential, Unique>;
	using const_range    = typename base_type::const_range;
	using range          = typename base_type::range;
	using iterator       = typename base_type::iterator;
	using const_iterator = typename base_type::const_iterator;
	using map_iterator   = typename base_type::map_type::iterator;
	using map_value_type = typename base_type::map_value_type;

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
	/// append a KProps struct to another one
	KProps& operator+=(const KProps& other)
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
	KProps& operator+=(KProps&& other)
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
					iNewElements += kSplit(*this, sLine, chPairDelim, svDelim);
				}
			}

		}

		return size();

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
	template <class K, class V = Value, bool Uq = Unique, typename std::enable_if<Uq == true>::type* = nullptr>
	/// Set a new value for existing key/value pairs. If the key/value pair is not existing it is created.
	iterator Set(K&& key, V&& newValue = V{})
	//-----------------------------------------------------------------------------
	{
		auto it = base_type::m_map.find(key);
		if (it != base_type::m_map.end())
		{
			it->second = std::forward<V>(newValue);
			// TODO generate reasonable return iterator (has to be of vector type if Sequential..)
//			return range.first;
			return base_type::end();
		}
		else
		{
			return Add(std::forward<K>(key), std::forward<V>(newValue));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for non-unique instances
	/// Set a new value for existing key/value pairs. If the key/value pair is not existing it is created.
	template <class K, class V = Value, bool Uq = Unique, typename std::enable_if<Uq == false>::type* = nullptr>
	iterator Set(K&& key, V&& newValue = V{})
	//-----------------------------------------------------------------------------
	{
		auto range = base_type::m_map.equal_range(key);
		if (range.first != range.second)
		{
			for (auto it = range.first; it != range.second; ++it)
			{
				// we cannot forward as we may have multiple values to insert to
				it->second = newValue;
			}
			// TODO generate reasonable return iterator (has to be of vector type if Sequential..)
//			return range.first;
			return base_type::end();
		}
		else
		{
			return Add(std::forward<K>(key), std::forward<V>(newValue));
		}
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding and SFINAE for unique instances
	/// Set a new value for existing key/value pairs. If the key/value pair is not existing it is created.
	template <class K, class V1, class V2, bool Uq = Unique, typename std::enable_if<Uq == true>::type* = nullptr>
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
	template <class K, class V1, class V2, bool Uq = Unique, typename std::enable_if<Uq == false>::type* = nullptr>
	iterator Set(K&& key, V1&& value, V2&& newValue)
	//-----------------------------------------------------------------------------
	{
		bool replaced{false};

		auto ret   = base_type::end();
		auto range = base_type::m_map.equal_range(key);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == value)
			{
				// we cannot forward as we may have multiple values to insert to
				it->second = newValue;
				// TODO generate reasonable return iterator (has to be of vector type if Sequential..)
//				ret = it;
				replaced = true;
			}
		}

		if (replaced)
		{
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
	template<class K, class V, bool Sq = Sequential, bool Uq = Unique, typename std::enable_if<Sq == true || Uq == false>::type* = nullptr>
	iterator Replace(K&& key, V&& value, map_iterator it)
	//-----------------------------------------------------------------------------
	{
		return Set(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding for non-Sequential Unique instances
	/// Replace the value of an existing key
	template<class K, class V, bool Sq = Sequential, bool Uq = Unique, typename std::enable_if<Sq == false && Uq == true>::type* = nullptr>
	iterator Replace(K&& key, V&& value, map_iterator it)
	//-----------------------------------------------------------------------------
	{
		it->second = std::forward<V>(value);
		return it;
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding for Unique instances
	/// Add a new Key/Value pair. If the key is already existing, its value is replaced.
	template<class K, class V = Value, bool Uq = Unique, typename std::enable_if<Uq == true>::type* = nullptr>
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
#if (__cplusplus > 201402L)
		// C++17 or later
		// we have to use try_emplace and not emplace, as otherwise key and value would be consumed
		// by an aborted emplace and would be empty for a subsequent Replace
		auto pair = base_type::m_map.try_emplace(std::forward<K>(key), std::forward<V>(value));
		if (pair.second)
		{
			return base_type::AddToVector(pair.first);
		}
		else
		{
			return Replace(std::forward<K>(key), std::forward<V>(value), pair.first);
		}
#else
		// C++11/14 do not know try_emplace, so we have to do value copies for the
		// emplace anyway, as we could not call Replace() otherwise
		auto pair = base_type::m_map.emplace(key, value);
		if (pair.second)
		{
			return base_type::AddToVector(pair.first);
		}
		else
		{
			return Replace(std::forward<K>(key), std::forward<V>(value), pair.first);
		}
#endif
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding for non Unique instances
	/// Add a new Key/Value pair.
	template<class K, class V = Value, bool Uq = Unique, typename std::enable_if<Uq == false>::type* = nullptr>
	iterator Add(K&& key, V&& value = V{})
	//-----------------------------------------------------------------------------
	{
		auto item = base_type::m_map.emplace(std::forward<K>(key), std::forward<V>(value));
		return base_type::AddToVector(item);
	}

	//-----------------------------------------------------------------------------
	// SFINAE for Unique instances
	/// remove Key/Value pairs with a given key. Returns count of removed elements.
	template<bool Uq = Unique, typename std::enable_if<Uq == true>::type* = nullptr>
	size_t Remove(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto it = base_type::m_map.find(key);
		if (it == base_type::m_map.end())
		{
			return 0;
		}

		base_type::RemoveFromVector(1, key);
		base_type::m_map.erase(it);

		return 1;
	}

	//-----------------------------------------------------------------------------
	// SFINAE for non Unique instances
	/// remove Key/Value pairs with a given key. Returns count of removed elements.
	template<bool Uq = Unique, typename std::enable_if<Uq == false>::type* = nullptr>
	size_t Remove(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto range = base_type::m_map.equal_range(key);
		auto iErased = std::distance(range.first, range.second);

		if (iErased)
		{
			base_type::RemoveFromVector(iErased, key);
			base_type::m_map.erase(range.first, range.second);
		}

		return iErased;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding, SFINAE for non-sequential case
	/// returns iterator on the element with the given key.
	template<class K, bool Seq = Sequential, typename std::enable_if<Seq == false>::type* = nullptr>
	iterator find(K&& key)
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding, SFINAE for non-sequential case
	/// returns const_iterator on the element with the given key.
	template<class K, bool Seq = Sequential, typename std::enable_if<Seq == false>::type* = nullptr>
	const_iterator find(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding, SFINAE for sequential case
	/// returns iterator on the element with the given key.
	template<class K, bool Seq = Sequential, typename std::enable_if<Seq == true>::type* = nullptr>
	typename base_type::map_type::iterator find(K&& key)
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding, SFINAE for sequential case
	/// returns const_iterator on the element with the given key.
	template<class K, bool Seq = Sequential, typename std::enable_if<Seq == true>::type* = nullptr>
	const typename base_type::map_type::iterator find(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.find(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	Value& Get(K&& key)
	//-----------------------------------------------------------------------------
	{
		auto it = base_type::m_map.find(std::forward<K>(key));
		if (it != base_type::m_map.end())
		{
			return it->second;
		}
		return base_type::s_EmptyMapValue_v.second;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns value of the element with the given key. Returns default constructed
	/// value if not found.
	template<class K>
	const Value& Get(K&& key) const
	//-----------------------------------------------------------------------------
	{
		auto it = base_type::m_map.find(std::forward<K>(key));
		if (it != base_type::m_map.end())
		{
			return it->second;
		}
		return base_type::s_EmptyMapValue.second;
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns iterator range of the elements with the given key.
	template<class K>
	const_range GetMulti(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.equal_range(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	template<class K>
	/// Returns iterator range of the elements with the given key.
	range GetMulti(K&& key)
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.equal_range(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	/// Returns count of elements with the given key.
	template<class K>
	size_t Count(K&& key) const
	//-----------------------------------------------------------------------------
	{
		return base_type::m_map.count(std::forward<K>(key));
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
	template<bool Seq = Sequential, typename std::enable_if<Seq == true>::type* = nullptr>
	map_value_type& at(size_t index)
	//-----------------------------------------------------------------------------
	{
		if (index < base_type::size())
		{
			return base_type::m_vector[index];
		}
		else
		{
			kWarning("called for index {}, which is out of range [0,{})", index, base_type::size());
			return base_type::s_EmptyMapValue_v;
		}
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<bool Seq = Sequential, typename std::enable_if<Seq == true>::type* = nullptr>
	const map_value_type& at(size_t index) const
	//-----------------------------------------------------------------------------
	{
		if (index < base_type::size())
		{
			return base_type::m_vector[index];
		}
		else
		{
			kWarning("called for index {}, which is out of range [0,{})", index, base_type::size());
			return base_type::s_EmptyMapValue;
		}
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for non-integral Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<class T = Key, bool Seq = Sequential, typename std::enable_if<!std::is_integral<T>::value && Seq == true, int>::type = 0 >
	map_value_type& operator[](size_t index)
	//-----------------------------------------------------------------------------
	{
		return at(index);
	}

	//-----------------------------------------------------------------------------
	// SFINAE, only active for non-integral Sequential instances
	/// Gets the element at index position. Returns empty element if out of range.
	template<class T = Key, bool Seq = Sequential, typename std::enable_if<!std::is_integral<T>::value && Seq == true, int>::type >
	const map_value_type& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		return at(index);
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	/// Gets the element with the given key. Returns empty element if not found.
	template<class K, class T = Key, typename std::enable_if<std::is_integral<T>::value || !std::is_integral<K>::value, int>::type >
	const Value& operator[](K&& key) const
	//-----------------------------------------------------------------------------
	{
		return Get(std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	// SFINAE && perfect forwarding, only active for non-integral K, or if the Key is integral
	/// Gets the element with the given key. Returns empty element if not found.
	template<class K, class T = Key, typename std::enable_if<std::is_integral<T>::value || !std::is_integral<K>::value, int>::type >
	Value& operator[](K&& key)
	//-----------------------------------------------------------------------------
	{
		auto it = base_type::m_map.find(key);
		if (it != base_type::m_map.end())
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
		base_type::clear();
	}

	//-----------------------------------------------------------------------------
	/// Returns count of all stored elements.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return base_type::size();
	}

	//-----------------------------------------------------------------------------
	/// Returns true if no elements are stored.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return base_type::empty();
	}

	//-----------------------------------------------------------------------------
	/// Inserts one element at the end. (InsertIterator interface)
	void push_back(const map_value_type& element)
	//-----------------------------------------------------------------------------
	{
		Add(element.first, element.second);
	}

	//-----------------------------------------------------------------------------
	/// Move-inserts one element at the end. (InsertIterator interface)
	void push_back(map_value_type&& element)
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
	void insert(const map_value_type& element)
	//-----------------------------------------------------------------------------
	{
		push_back(element);
	}

	//-----------------------------------------------------------------------------
	/// Move-inserts one element at the end. (InsertIterator interface)
	void insert(map_value_type&& element)
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
		push_back(map_value_type(std::forward<K>(key), std::forward<V>(value)));
	}

}; // KProps

template<class Key, class Value, bool Unique>
const typename KProps_map<Key, Value, Unique>::map_value_type
KProps_map<Key, Value, Unique>::s_EmptyMapValue = map_value_type();

template<class Key, class Value, bool Unique>
typename KProps_map<Key, Value, Unique>::map_value_type
KProps_map<Key, Value, Unique>::s_EmptyMapValue_v = map_value_type();

} // end of namespace dekaf2

