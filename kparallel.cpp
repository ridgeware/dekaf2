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

#include "kparallel.h"

namespace dekaf2
{

bool                KRunThreads::s_bHasThreadsStarted{false};
thread_local size_t KRunThreads::s_iThreadNum{0};
std::atomic_size_t  KRunThreads::s_iThreadIdCount{0};

//-----------------------------------------------------------------------------
void KBlockOnID::Data::lock(size_t ID)
//-----------------------------------------------------------------------------
{
	lockmap_t::iterator it;

	{
		std::lock_guard<std::mutex> lock(m_map_mutex);

		it = m_id_mutexes.find(ID);
		if (it == m_id_mutexes.end())
		{
			unique_mutex_t umu(new(std::mutex));
			it = m_id_mutexes.emplace(ID, std::move(umu)).first;
		}
	}

	it->second->lock();
}

//-----------------------------------------------------------------------------
bool KBlockOnID::Data::unlock(size_t ID)
//-----------------------------------------------------------------------------
{
	lockmap_t::iterator it;
	bool bFoundLock;

	{
		std::lock_guard<std::mutex> lock(m_map_mutex);

		it = m_id_mutexes.find(ID);
		bFoundLock = it != m_id_mutexes.end();
	}

	if (bFoundLock)
	{
		it->second->unlock();
	}
	else
	{
		// TODO add output
//		cVerboseOut(0, "%s fatal: cannot unlock lock for ID %lu\n", ERR(), ID);
	}

	return bFoundLock;
}

//-----------------------------------------------------------------------------
void KParallelForEachPrintProgress(size_t iMax, size_t iDone, size_t iRunning)
//-----------------------------------------------------------------------------
{
	if (!iMax)
	{
		return;
	}

	if ((iDone * 100 % iMax) != 0)
	{
		return;
	}

	size_t iPercent    = iDone    * 100 / iMax;
	size_t iInProgress = iRunning * 100 / iMax;

	// TODO add output
//	cVerboseOut(2, "%s parallel_for_each: completed %lu%%, in progress %lu%% \n",
//				OK(),
//				iPercent,
//				iInProgress);
}

} // end of namespace dekaf2

