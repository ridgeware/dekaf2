/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2025, Ridgeware, Inc.
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

/// @file klockmap.h
/// a hashed map of any key to lock any mutex, using templates

#include "kassociative.h"
#include "kthreadsafe.h"
#include <memory>
#include <mutex>

DEKAF2_NAMESPACE_BEGIN


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Provides an execution barrier that only lets one of multiple equal keys
/// pass. All other instances with the same key have to wait until
/// the first one finishes, then the next, and so on.
template<typename Key = std::size_t, typename Mutex = std::mutex>
class DEKAF2_PUBLIC KLockMap
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct ObjectAndControl
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		void        inc() { ++iCounter; }
		std::size_t dec() { mutex.unlock(); return --iCounter; }

		std::size_t iCounter { 0 };
		Mutex       mutex;

	}; // ObjectAndControl

	using LockMap = KUnorderedMap<Key, std::unique_ptr<ObjectAndControl>>;
	KThreadSafe<LockMap> m_MutexMap;

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// Lock a key
	template<typename K>
	LockMap::iterator LockKey(K&& key)
	//-----------------------------------------------------------------------------
	{
		typename LockMap::iterator it;

		{
			auto Mutexes = m_MutexMap.unique();

			it = Mutexes->find(std::forward<K>(key));

			if (it == Mutexes->end())
			{
				it = Mutexes->emplace(key, std::make_unique<ObjectAndControl>()).first;
			}

			it->second->inc();
		}

		it->second->mutex.lock();

		return it;
	}

	//-----------------------------------------------------------------------------
	/// Unlock a key
	void UnlockKey(LockMap::iterator it)
	//-----------------------------------------------------------------------------
	{
		auto Mutexes = m_MutexMap.unique();

		if (!it->second->dec())
		{
			Mutexes->erase(it);
		}
	}

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Lock
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	friend class KLockMap;

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		/// destructor unlocks the key and removes the map entry if this was the last lock on the key
		~Lock()
		//-----------------------------------------------------------------------------
		{
			m_Locks.UnlockKey(m_it);
		}

	//----------
	private:
	//----------

		//-----------------------------------------------------------------------------
		/// constructor
		template<typename K>
		Lock(KLockMap& locks, K&& key)
		//-----------------------------------------------------------------------------
			: m_Locks(locks)
			, m_it(m_Locks.LockKey(std::forward<K>(key)))
		{
		}

		KLockMap& m_Locks;
		KLockMap::LockMap::iterator m_it;

	}; // Lock

	//-----------------------------------------------------------------------------
	/// creates a new lock for the given key
	template<typename K>
	Lock Create(K&& key)
	//-----------------------------------------------------------------------------
	{
		return Lock(*this, std::forward<K>(key));
	}

	//-----------------------------------------------------------------------------
	/// tests if a key would block right now
	template<typename K>
	bool WouldBlock(K&& key) const
	//-----------------------------------------------------------------------------
	{
		auto Mutexes = m_MutexMap.shared();
		return Mutexes->find(std::forward<K>(key)) != Mutexes->end();
	}

	//-----------------------------------------------------------------------------
	/// returns the current size of the mutex map
	std::size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_MutexMap.shared()->size();
	}

	//-----------------------------------------------------------------------------
	/// returns true if the mutex map is right now empty
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_MutexMap.shared()->empty();
	}

}; // KLockMap

DEKAF2_NAMESPACE_END
