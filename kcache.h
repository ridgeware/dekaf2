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


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic cache.
/// For cache size management it uses a Least Recently Used removal strategy.
/// To load a new value it calls the constructor of Value() with the new Key.
template<class Key, class Value>
class KCache
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	enum { DEFAULT_MAX_CACHE_SIZE = 1000 };
	using map_type = KMRUMap<Key, Value>;
	map_type m_map;
	size_t m_iMaxSize;

	//-----------------------------------------------------------------------------
	/// Creates a new cache entry.
	template<class K, class V>
	Value& Create(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.insert(std::make_pair(std::forward<K>(key), std::forward<V>(value)));
		return it->second;
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KCache(size_t iMaxSize = DEFAULT_MAX_CACHE_SIZE)
	//-----------------------------------------------------------------------------
	    : m_map(iMaxSize)
	{
	}

	KCache(const KCache&) = delete;
	KCache& operator=(const KCache&) = delete;
	KCache(const KCache&&) = delete;
	KCache& operator=(const KCache&&) = delete;

	//-----------------------------------------------------------------------------
	/// Add a new key value pair to the cache.
	template<class K, class V>
	Value& Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return Create(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	Value& Get(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.find(key);
		if (it != m_map.end())
		{
			return it->second;
		}

		return Create(key, Value(key));
	}

	//-----------------------------------------------------------------------------
	/// Erase a key and its corresponding value from the cache.
	bool Erase(const Key& key)
	//-----------------------------------------------------------------------------
	{
		return m_map.erase(key);
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
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	Value& operator[](const Key& key)
	//-----------------------------------------------------------------------------
	{
		return Get(key);
	}

}; // KCache



//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KSharedCache wraps the Value type into a shared pointer (KSharedRef)
/// and enables shared locking when Dekaf is set into multithreading mode
template<class Key, class Value>
class KSharedCache : public KCache<Key, KSharedRef<Value, true>>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using value_type = KSharedRef<Value, true>;
	using base_type  = KCache<Key, value_type>;

	//-----------------------------------------------------------------------------
	/// Add a new key value pair to the cache.
	template<class K, class V>
	value_type& Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		return base_type::Set(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
	// need to reimplement Get() from scratch to accomodate the share logic
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	value_type& Get(const Key& key)
	//-----------------------------------------------------------------------------
	{
		if (!Dekaf().GetMultiThreading())
		{
			// we can use the lock free version
			return base_type::Get(key);
		}
		// we will use shared and unique locks
		{
			// check in a readlock if key is existing
			std::shared_lock<std::shared_mutex> Lock(m_Mutex);

			auto it = base_type::m_map.find(key);
			if (it != base_type::m_map.end())
			{
				return it->second;
			}
		}

		// acquire a write lock
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		// we call the base_type to search again exclusively,
		// and to create if still not found
		return base_type::Get(key);
	}

	//-----------------------------------------------------------------------------
	/// Erase a key and its corresponding value from the cache.
	bool Erase(const Key& key)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		return base_type::Erase(key);
	}

	//-----------------------------------------------------------------------------
	/// Set a new maximum cache size. When the cache was filled with more elements,
	/// it is reduced by the amount of excess elements that were the least recently
	/// used.
	void SetMaxSize(size_t iMaxSize)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		return base_type::SetMaxSize(iMaxSize);
	}

	//-----------------------------------------------------------------------------
	/// Returns the maximum cache size.
	size_t GetMaxSize() const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		return base_type::GetMaxSize();
	}

	//-----------------------------------------------------------------------------
	/// Clears the cache.
	void clear()
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		base_type::clear();
	}

	//-----------------------------------------------------------------------------
	/// Returns count of cached elements.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex, std::defer_lock);
		if (Dekaf().GetMultiThreading())
		{
			Lock.lock();
		}
		return base_type::size();
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist, a new
	/// value will be created and the key value pair will be inserted into the
	/// cache. For this to be possible, the Value type needs to be constructible
	/// from the Key type (so, have a constructor Value(Key) ).
	value_type& operator[](const Key& key)
	//-----------------------------------------------------------------------------
	{
		return Get(key);
	}

//----------
private:
//----------

	mutable std::shared_mutex m_Mutex;

}; // KSharedCache

} // of namespace dekaf2

