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
std::thread::id KThreads::AddLocked(KThreadSafe<Storage>::UniqueLocked& Threads, std::thread newThread)
//-----------------------------------------------------------------------------
{
	auto id = newThread.get_id();

	kDebug(2, "adding thread with id {}", id);

	Threads->emplace(id, std::move(newThread));

	return id;

} // AddLocked

//-----------------------------------------------------------------------------
std::thread::id KThreads::Add(std::thread newThread)
//-----------------------------------------------------------------------------
{
	// create the lock first, compilers do not have to follow left-to-right argument evaluation
	auto Threads = m_Threads.unique();

	return AddLocked(Threads, std::move(newThread));

} // Add

//-----------------------------------------------------------------------------
bool KThreads::RemoveInt(std::thread::id ThreadID, bool bWarnIfFailed)
//-----------------------------------------------------------------------------
{
	std::thread Thread;

	{
		auto Threads = m_Threads.unique();

		auto it = Threads->find(ThreadID);

		if (it == Threads->end())
		{
			// do not warn if this is called from Create() - a Join()
			// may have come in between
			if (bWarnIfFailed)
			{
				kDebug(2, "cannot remove thread with id {} - not found", ThreadID);
			}
			return false;
		}

		Thread = std::move(it->second);

		Threads->erase(it);
	}

	m_Decay.unique()->push_back(std::move(Thread));

	kDebug(2, "removed thread with id {}", ThreadID);
	return true;

} // RemoveInt

//-----------------------------------------------------------------------------
void KThreads::DecayInt()
//-----------------------------------------------------------------------------
{
	auto Threads = m_Decay.unique();

	std::size_t iSize = Threads->size();
	auto iCount = iSize;

	for (auto& Thread : *Threads)
	{
		if (Thread.joinable())
		{
			kDebug(2, "now waiting for {} decaying threads to finish: {}", iCount, Thread.get_id());
			Thread.join();
		}

		--iCount;
	}

	if (iSize)
	{
		Threads->clear();
		kDebug(1, "all decaying threads finished");
	}

} // DecayInt

//-----------------------------------------------------------------------------
void KThreads::JoinInt()
//-----------------------------------------------------------------------------
{
	std::thread Thread;
	std::size_t iSize { 0 };

	for (;;)
	{
		{
			auto Threads = m_Threads.unique();

			auto it = Threads->begin();

			if (it == Threads->end())
			{
				if (iSize)
				{
					// mute output if there was no thread to begin with
					kDebug(1, "all threads finished");
				}
				return;
			}

			Thread = std::move(it->second);
			iSize  = Threads->size();
			Threads->erase(it);
		}

		if (Thread.joinable())
		{
			kDebug(2, "now waiting for {} threads to finish: {}", iSize, Thread.get_id());
			Thread.join();
		}
	}

} // JoinInt

//-----------------------------------------------------------------------------
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
void KThreads::Join()
//-----------------------------------------------------------------------------
{
	JoinInt();
	DecayInt();

} // Join

//-----------------------------------------------------------------------------
size_t KThreads::size() const
//-----------------------------------------------------------------------------
{
	return m_Threads.shared()->size();

} // size

//-----------------------------------------------------------------------------
bool KThreads::empty() const
//-----------------------------------------------------------------------------
{
	return m_Threads.shared()->empty();

} // size

} // end of namespace dekaf2

