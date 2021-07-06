/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017-2021, Ridgeware, Inc.
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

/// @file kthreads.h
/// keep track of running threads

#include <thread>
#include <utility>
#include <unordered_map>
#include "kthreadsafe.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for generated threads. Maintains a list of started threads
/// (and hence keeps them alive) and ensures they have joined at destruction.
class KThreads
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	using Storage = std::unordered_map<std::thread::id, std::thread>;

	KThreadSafe<Storage> m_Threads;

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads while in unique lock
	std::thread::id AddUniquelyLocked(KThreadSafe<Storage>::UniqueLocked& Threads, std::thread newThread);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Destructor waits for all threads to join
	~KThreads()
	//-----------------------------------------------------------------------------
	{
		Join();
	}

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads
	std::thread::id Add(std::thread newThread);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// create one thread, calling function f with arguments args, removing thread from join list at destruction
	template<class Function, class... Args>
	std::thread::id Create(Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		// create the lock first, compilers do not have to follow left-to-right argument evaluation
		auto Threads = m_Threads.unique();

		auto id = AddUniquelyLocked(Threads, std::thread([this](Function&& f, Args&&... args)
		{
			// call the callable
			(f)(std::forward<Args>(args)...);

			// and remove the thread id
			Remove(std::this_thread::get_id());

		}, std::forward<Function>(f), std::forward<Args>(args)...));

		return id;

	} // Create

	//-----------------------------------------------------------------------------
	/// remove a thread from the list of started threads
	bool Remove(std::thread::id ThreadID);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for all threads to join
	void Join();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for one thread to join
	bool Join(std::thread::id ThreadID);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return count of started threads
	size_t size() const;
	//-----------------------------------------------------------------------------

}; // KThreads

} // of namespace dekaf2

