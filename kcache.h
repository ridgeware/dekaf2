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

#include "kcppcompat.h"
#include <unordered_map>
#include <functional>
#include "dekaf.h"
#include "ksharedref.h"


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KCacheTemplate implements a generic cache.
/// For cache size management it uses a random
/// replacement strategy. To load a new value
/// it calls the constructor of Value() with the
/// new Key.
template<class Key, class Value>
class KCacheTemplate
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------
	enum { DEFAULT_MAX_CACHE_SIZE = 1000 };
	using map_type = std::unordered_map<Key, Value>;
	map_type m_map;
	size_t m_iMaxSize;

	//-----------------------------------------------------------------------------
	// TODO fix to either apply a real random replacement strategy using a vector or
	// a LRU strategy with a queue
	void ApplySizePolicy()
	//-----------------------------------------------------------------------------
	{
		while (m_map.size() >= m_iMaxSize)
		{
			m_map.erase(m_map.begin());
		}
	}

	//-----------------------------------------------------------------------------
	template<class K, class V>
	Value& Create(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		ApplySizePolicy();

		auto pair = m_map.emplace(std::forward<K>(key), std::forward<V>(value));

		return pair.first->second;
	}

//----------
public:
//----------
	//-----------------------------------------------------------------------------
	KCacheTemplate(size_t iMaxSize = DEFAULT_MAX_CACHE_SIZE)
	//-----------------------------------------------------------------------------
	    : m_iMaxSize(iMaxSize)
	{}

	KCacheTemplate(const KCacheTemplate&) = delete;
	KCacheTemplate& operator=(const KCacheTemplate&) = delete;
	KCacheTemplate(const KCacheTemplate&&) = delete;
	KCacheTemplate& operator=(const KCacheTemplate&&) = delete;

	//-----------------------------------------------------------------------------
	template<class K, class V>
	Value& Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		return Create(std::forward<K>(key), std::forward<V>(value));
	}

	//-----------------------------------------------------------------------------
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
	bool Erase(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto it = m_map.find(key);
		if (it != m_map.end())
		{
			m_map.erase(it);
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	void SetMaxSize(size_t iMaxSize)
	//-----------------------------------------------------------------------------
	{
		m_iMaxSize = iMaxSize;
		ApplySizePolicy();
	}

	//-----------------------------------------------------------------------------
	size_t GetMaxSize() const
	//-----------------------------------------------------------------------------
	{
		return m_iMaxSize;
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_map.clear();
	}

	//-----------------------------------------------------------------------------
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_map.size();
	}

}; // KCacheTemplate



//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KSharedCacheTemplate wraps the Value type into a shared pointer (KSharedRef)
/// and enables shared locking when Dekaf is set into multithreading mode
template<class Key, class Value>
class KSharedCacheTemplate : public KCacheTemplate<Key, KSharedRef<Value, true>>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------
	using value_type = KSharedRef<Value, true>;
	using base_type  = KCacheTemplate<Key, value_type>;

	//-----------------------------------------------------------------------------
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

//----------
private:
//----------
	std::shared_mutex m_Mutex;

}; // KSharedCacheTemplate

} // of namespace dekaf2

