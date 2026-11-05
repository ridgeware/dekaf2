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

#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/containers/memory/ksharedref.h>
#include <dekaf2/containers/associative/kmru.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <atomic>
#include <future>
#include <unordered_map>
#include <vector>


DEKAF2_NAMESPACE_BEGIN

/// @addtogroup containers_associative
/// @{

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


namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The element value of KSharedCache: a shared reference on the cached value and
/// a flag that marks the element as used. A cache hit sets the flag while holding
/// the shared lock. The eviction reads and resets the flag while holding the
/// exclusive lock.
template<class Value>
class KSharedCacheEntry
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using value_type = KSharedRef<Value, true>;

	//-----------------------------------------------------------------------------
	explicit KSharedCacheEntry(value_type value)
	//-----------------------------------------------------------------------------
	: m_Value(std::move(value))
	{
	}

	//-----------------------------------------------------------------------------
	KSharedCacheEntry(const KSharedCacheEntry& other)
	//-----------------------------------------------------------------------------
	: m_Value(other.m_Value)
	, m_bUsed(other.m_bUsed.load(std::memory_order_relaxed))
	{
	}

	//-----------------------------------------------------------------------------
	KSharedCacheEntry(KSharedCacheEntry&& other) noexcept
	//-----------------------------------------------------------------------------
	: m_Value(std::move(other.m_Value))
	, m_bUsed(other.m_bUsed.load(std::memory_order_relaxed))
	{
	}

	KSharedCacheEntry& operator=(const KSharedCacheEntry&) = delete;
	KSharedCacheEntry& operator=(KSharedCacheEntry&&) = delete;

	//-----------------------------------------------------------------------------
	/// returns the shared reference on the cached value
	const value_type& Get() const
	//-----------------------------------------------------------------------------
	{
		return m_Value;
	}

	//-----------------------------------------------------------------------------
	/// marks the element as used, multiple threads may call this concurrently
	void SetUsed() const
	//-----------------------------------------------------------------------------
	{
		// write only if the flag is not yet set, so that repeated hits on the same
		// element do not write to a cache line that other threads read
		if (!m_bUsed.load(std::memory_order_relaxed))
		{
			m_bUsed.store(true, std::memory_order_relaxed);
		}
	}

	//-----------------------------------------------------------------------------
	/// returns true if the element was marked as used, and resets the mark
	bool ResetUsed() const
	//-----------------------------------------------------------------------------
	{
		return m_bUsed.exchange(false, std::memory_order_relaxed);
	}

//----------
private:
//----------

	value_type                m_Value;
	mutable std::atomic<bool> m_bUsed { false };

}; // KSharedCacheEntry

} // of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A thread safe cache that returns its values as shared pointers (KSharedRef),
/// so that a value stays valid after the cache has removed it.
/// @code
///   struct FileLoader
///   {
///       KString operator()(KStringViewZ sFileName) { return kReadAll(sFileName); }
///   };
///
///   static KSharedCache<KString, KString, FileLoader> s_Files(100);
///   auto pContent = s_Files.Get("/etc/hosts"); // reads the file only if it is not cached
/// @endcode
/// Other than KCache, KSharedCache does not remove the least recently used
/// element. When the cache is full, it removes the oldest element that was not
/// used since it was inserted or checked the last time. It does not know which
/// of the used elements was used least recently: if all elements were used, it
/// removes the element that was inserted or checked first, even if it was used last.
/// If multiple threads request the same missing key at the same time, one of them
/// loads the value, and the others wait for it. If that load fails, the next waiting
/// thread loads the value. Requests for other keys do not wait for the load.
///
/// Implementation note: a cache hit only sets a flag in the element instead of
/// moving the element in the list, so that hits run in parallel under a shared
/// lock, and do not write to the element once the flag is set. Exact LRU order
/// would require a write to the list, and an access counter a write to the element,
/// on every hit. A new value is loaded without holding the lock.
template<class Key, class Value, class Load = detail::LoadByConstruction<KSharedRef<Value, true> > >
class KSharedCache : protected KCache<Key, detail::KSharedCacheEntry<Value>, Load>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using entry_type   = detail::KSharedCacheEntry<Value>;
	using base_type    = KCache<Key, entry_type, Load>;
	using map_type     = typename base_type::map_type;
	using element_type = typename map_type::element_type;

	// m_map is a member of a dependent base class, which unqualified lookup does not search
	using base_type::m_map;

//----------
public:
//----------

	using value_type   = KSharedRef<Value, true>;

	using base_type::DEFAULT_MAX_CACHE_SIZE;

	//-----------------------------------------------------------------------------
	KSharedCache(size_t iMaxSize = base_type::DEFAULT_MAX_CACHE_SIZE)
	//-----------------------------------------------------------------------------
	: base_type(iMaxSize)
	{
	}

	//-----------------------------------------------------------------------------
	/// Add a new key value pair to the cache. If the key already exists, the
	/// existing value is kept and returned.
	template<class K = Key, class V = Value>
	value_type Set(K&& key, V&& value)
	//-----------------------------------------------------------------------------
	{
		value_type NewValue(std::forward<V>(value));

		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		return Insert(std::forward<K>(key), NewValue);
	}

	//-----------------------------------------------------------------------------
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
		{
			// a cache hit only marks the element as used and does not change
			// the order of the elements, therefore a shared lock is sufficient
			std::shared_lock<std::shared_mutex> Lock(m_Mutex);

			const map_type& Map = m_map;
			auto it = Map.peek(key);

			if (it != Map.end())
			{
				it->second.SetUsed();
				return it->second.Get();
			}
		}

		// copy the key first, as the loader could consume it
		Key kk(key);
		std::promise<void> LoadDone;

		for (;;)
		{
			std::unique_lock<std::shared_mutex> Lock(m_Mutex);

			// another thread may have inserted the key since the shared lock was released
			const map_type& Map = m_map;
			auto it = Map.peek(kk);

			if (it != Map.end())
			{
				it->second.SetUsed();
				return it->second.Get();
			}

			auto Loading = m_Loading.find(kk);

			if (Loading == m_Loading.end())
			{
				// no other thread loads this key, this thread loads it
				m_Loading.emplace(kk, LoadDone.get_future().share());
				break;
			}

			// Another thread loads this key. Wait for it without holding the lock, then
			// check the cache again. If the other thread failed, this thread loads the key.
			auto OtherLoadDone = Loading->second;
			Lock.unlock();
			OtherLoadDone.wait();
		}

		// removes the key from m_Loading and wakes up the waiting threads if the loader throws
		KScopeGuard Guard([this, &kk, &LoadDone]()
		{
			{
				std::unique_lock<std::shared_mutex> Lock(m_Mutex);
				m_Loading.erase(kk);
			}
			LoadDone.set_value();
		});

		// load the new value without holding the lock
		value_type NewValue = Load()(std::forward<K>(key), std::forward<Args>(args)...);

		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		// insert the value and remove the key from m_Loading in one step, so that
		// other threads find the key either loading or in the cache
		auto CachedValue = Insert(kk, NewValue);
		m_Loading.erase(kk);

		Lock.unlock();
		Guard.release();
		LoadDone.set_value();

		return CachedValue;
	}

	//-----------------------------------------------------------------------------
	/// Get a value for a key from the cache. If the key does not exist,
	/// a default constructed value will be returned.
	template<class K = Key>
	value_type Find(const K& key) const
	//-----------------------------------------------------------------------------
	{
		std::shared_lock<std::shared_mutex> Lock(m_Mutex);

		const map_type& Map = m_map;
		auto it = Map.peek(key);

		if (it == Map.end())
		{
			return value_type{};
		}

		it->second.SetUsed();

		return it->second.Get();
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
	/// it is reduced by the amount of excess elements that were inserted or moved
	/// to the top the longest time ago.
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

	//-----------------------------------------------------------------------------
	/// Inserts a new element, the caller has to hold the exclusive lock. Returns
	/// the value of the existing element if the key is already known.
	template<class K>
	value_type Insert(K&& key, const value_type& value)
	//-----------------------------------------------------------------------------
	{
		// an element that was used since it was inserted or checked the
		// last time stays in the cache
		auto fnWasUsed = [](const element_type& element)
		{
			return element.second.ResetUsed();
		};

		auto it = m_map.insert(element_type(std::forward<K>(key), entry_type(value)), fnWasUsed);

		// the map is empty after the insertion if its maximum size is 0
		if (it == m_map.end())
		{
			return value;
		}

		return it->second.Get();
	}

	// the keys that are loaded right now, with the signal that the load is done
	std::unordered_map<Key, std::shared_future<void>> m_Loading;
	mutable std::shared_mutex                         m_Mutex;

}; // KSharedCache


/// @}

DEKAF2_NAMESPACE_END

