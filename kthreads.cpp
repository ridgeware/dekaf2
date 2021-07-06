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

#include "kthreads.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
/// add a new thread to the list of started threads while in unique lock
std::thread::id KThreads::AddUniquelyLocked(KThreadSafe<Storage>::UniqueLocked& Threads, std::thread newThread)
//-----------------------------------------------------------------------------
{
	auto id = newThread.get_id();

	kDebug(2, "adding thread with id {}", id);

	Threads->emplace(id, std::move(newThread));

	return id;

} // AddUniquelyLocked

//-----------------------------------------------------------------------------
/// add a new thread to the list of started threads
std::thread::id KThreads::Add(std::thread newThread)
//-----------------------------------------------------------------------------
{
	// create the lock first, compilers do not have to follow left-to-right argument evaluation
	auto Threads = m_Threads.unique();

	return AddUniquelyLocked(Threads, std::move(newThread));

} // Add

//-----------------------------------------------------------------------------
bool KThreads::Remove(std::thread::id ThreadID)
//-----------------------------------------------------------------------------
{
	bool bSuccess = m_Threads.unique()->erase(ThreadID);

	if (bSuccess)
	{
		kDebug(2, "removed thread with id {}", ThreadID);
	}
	else
	{
		kDebug(2, "cannot remove thread with id {}", ThreadID);
	}

	return bSuccess;

} // Remove

//-----------------------------------------------------------------------------
/// wait for all threads to join
void KThreads::Join()
//-----------------------------------------------------------------------------
{
	auto Threads = m_Threads.unique();

	if (!Threads->empty())
	{
		for (auto& it : *Threads)
		{
			kDebug(2, "now waiting for {} threads to finish", Threads->size());

			if (it.second.joinable())
			{
				it.second.join();
			}
		}

		Threads->clear();

		kDebug(1, "all threads finished");
	}

} // Join

//-----------------------------------------------------------------------------
/// wait for one thread to join
bool KThreads::Join(std::thread::id ThreadID)
//-----------------------------------------------------------------------------
{
	std::thread Thread;

	{
		auto Threads = m_Threads.unique();

		auto it = Threads->find(ThreadID);

		if (it == Threads->end())
		{
			kDebug(2, "thread {} not found", ThreadID);
			return false;
		}

		Thread = std::move(it->second);

		Threads->erase(it);
	}

	if (Thread.joinable())
	{
		kDebug(2, "now waiting for thread {} to finish", ThreadID);
		Thread.join();
		kDebug(2, "thread {} finished", ThreadID);
		return true;
	}
	else
	{
		kDebug(2, "thread {} is not joinable", ThreadID);
		return false;
	}

} // Join

//-----------------------------------------------------------------------------
/// return count of started threads
size_t KThreads::size() const
//-----------------------------------------------------------------------------
{
	return m_Threads.shared()->size();

} // size

} // end of namespace dekaf2

