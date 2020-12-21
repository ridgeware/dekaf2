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

/// @file kcache.h
/// a generic cache with LRU removal

#include "bits/kcppcompat.h"
#include "dekaf2.h"
#include "ksharedref.h"
#include "kmru.h"


namespace dekaf2 {

namespace detail {

template<class Value>
struct LoadByConstruction
{
	template<class Key, typename... Args>
	Value operator()(Key&& key, Args&& ...args)
	{
		return Value(std::forward<Key>(key), std::forward<Args>(args)...);
	}
};

} // of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic cache.
/// For cache size management it uses a Least Recently Used removal strategy.
/// To load a new value per default it calls the constructor of Value() with
/// the new Key, but you can also add a class with call operator Key to the
/// template.
template<class Key, class Value, class Load = detail::LoadByConstruction<Value> >
class KCache
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using map_type = KMRUMap<Key, Value>;
	map_type m_map;

	//-----------------------------------------------------------------------------
	/// Creates a new cache entry.
	template<class K = Key, class V = Value>
	Value& Create(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.insert(detail::KMutablePair<Key, Value>(std::forward<K>(key), std::forward<V>(value)));
		return it->second;
	}

//----------
public:
//----------

	enum { DEFAULT_MAX_CACHE_SIZE = 1000 };

	//-----------------------------------------------------------------------------
	KCache(size_t iMaxSize = DEFAULT_MAX_CACHE_SIZE)
	//-----------------------------------------------------------------------------
	    : m_map(iMaxSize)
	{
	}

	KCache(const KCache&) = delete;
	KCache& operator=(const KCache&) = delete;
	KCache(KCache&&) = delete;
	KCache& operator=(KCache&&) = delete;

	//-----------------------------------------------------------------------------
	/// Add a new key value pair to the cache.
	template<class K = Key, class V = Value>
	Value& Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return Create(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key)). Alternatively,
	/// additional parameters can be given in args... which will be supplied
	/// to a Load type.
	template<class K = Key, typename...Args>
	Value& Get(K&& key, Args&&...args)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.find(std::forward<K>(key));

		if (it != m_map.end())
		{
			return it->second;
		}

		// Create a copy of the key and move it into Create(), as the loader could
		// also consume the key. Do not use a temporary, as we do not know which parameter
		// is consumed first.
		Key kk(key);
		return Create(std::move(kk), Load()(std::forward<K>(key), std::forward<Args>(args)...));
	}

	//-----------------------------------------------------------------------------
	// cannot be const, as the rank is changed..
	/// Get a pointer on a value for a key from the cache. If the key does not exist,
	/// a nullptr will be returned.
	template<class K = Key>
	Value* Find(const K& key)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.find(key);

		if (it != m_map.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	//-----------------------------------------------------------------------------
	/// Erase a key and its corresponding value from the cache.
	template<class K = Key>
	bool Erase(const K& key)
	//-----------------------------------------------------------------------------
	{
		return m_map.erase(key);
	}

	//-----------------------------------------------------------------------------
	/// Erase a vector of keys and their corresponding values from the cache.
	template<class K = Key>
	void Erase(const std::vector<K>& keys)
	//-----------------------------------------------------------------------------
	{
		for (const auto& key : keys)
		{
			m_map.erase(key);
		}
	}

	//-----------------------------------------------------------------------------
	/// Set a new maximum cache size. When the cache was filled with more elements,
	/// it is reduced by the amount of excess elements that were the least recently
	/// used.
	void SetMaxSize(size_t iMaxSize)
	//-----------------------------------------------------------------------------
	{
		m_map.SetMaxSize(iMaxSize);
	}

	//-----------------------------------------------------------------------------
	/// Returns the maximum cache size.
	size_t GetMaxSize() const
	//-----------------------------------------------------------------------------
	{
		return m_map.GetMaxSize();
	}

	//-----------------------------------------------------------------------------
	/// Clears the cache.
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_map.clear();
	}

	//-----------------------------------------------------------------------------
	/// Returns count of cached elements.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_map.size();
	}

	//-----------------------------------------------------------------------------
	/// Returns true if no cached elements.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_map.empty();
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	template<class K = Key>
	Value& operator[](K&& key)
	//-----------------------------------------------------------------------------
	{
		return Get(std::forward<K>(key));
	}

}; // KCache


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KSharedCache wraps the Value type into a shared pointer (KSharedRef)
/// and enables shared locking
template<class Key, class Value, class Load = detail::LoadByConstruction<KSharedRef<Value, true> > >
class KSharedCache : public KCache<Key, KSharedRef<Value, true>, Load>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using value_type = KSharedRef<Value, true>;
	using base_type  = KCache<Key, value_type, Load>;

	//-----------------------------------------------------------------------------
	KSharedCache(size_t iMaxSize = base_type::DEFAULT_MAX_CACHE_SIZE)
	//-----------------------------------------------------------------------------
	: base_type(iMaxSize)
	{
	}

	//-----------------------------------------------------------------------------
	/// Add a new key value pair to the cache.
	template<class K = Key, class V = Value>
	value_type Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);

		return base_type::Set(std::forward<K>(key), value_type(std::forward<V>(value)));
	}

	//-----------------------------------------------------------------------------
	// need to reimplement Get() from scratch to accomodate the share logic
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key)). Alternatively,
	/// additional parameters can be given in args... which will be supplied
	/// to a Load type.
	template<class K = Key, typename...Args>
	value_type Get(K&& key, Args&&...args)
	//-----------------------------------------------------------------------------
	{
		// have to use a unique lock even for reads as the
		// rank is changed!
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::Get(std::forward<K>(key), std::forward<Args>(args)...);
	}

	//-----------------------------------------------------------------------------
	// cannot be const, as the rank is changed..
	/// Get a value for a key from the cache. If the key does not exist,
	/// a default constructed value will be returned.
	template<class K = Key>
	value_type Find(const K& key)
	//-----------------------------------------------------------------------------
	{
		// have to use a unique lock even for reads as the
		// rank is changed!
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		auto ptr = base_type::Find(key);

		if (!ptr)
		{
			return value_type{};
		}
		else
		{
			return *ptr;
		}
	}

	//-----------------------------------------------------------------------------
	/// Erase a key and its corresponding value from the cache.
	template<class K = Key>
	bool Erase(const K& key)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::Erase(key);
	}

	//-----------------------------------------------------------------------------
	/// Erase a vector of keys and their corresponding values from the cache.
	template<class K = Key>
	void Erase(const std::vector<K>& keys)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		base_type::Erase(keys);
	}

	//-----------------------------------------------------------------------------
	/// Set a new maximum cache size. When the cache was filled with more elements,
	/// it is reduced by the amount of excess elements that were the least recently
	/// used.
	void SetMaxSize(size_t iMaxSize)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::SetMaxSize(iMaxSize);
	}

	//-----------------------------------------------------------------------------
	/// Returns the maximum cache size.
	size_t GetMaxSize() const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::GetMaxSize();
	}

	//-----------------------------------------------------------------------------
	/// Clears the cache.
	void clear()
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		base_type::clear();
	}

	//-----------------------------------------------------------------------------
	/// Returns count of cached elements.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::size();
	}

	//-----------------------------------------------------------------------------
	/// Returns true if no cached elements.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex);

		return base_type::empty();
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	template<class K = Key>
	value_type operator[](K&& key)
	//-----------------------------------------------------------------------------
	{
		return Get(std::forward<K>(key));
	}

//----------
private:
//----------

	mutable std::shared_mutex m_Mutex;

}; // KSharedCache

} // of namespace dekaf2

