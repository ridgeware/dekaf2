/*
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

namespace dekaf2 {

//-----------------------------------------------------------------------------
/// sets number of threads to #cpu if numThreads == 0, but
/// not higher than maxThreads if maxThreads > 0.
std::size_t KRunThreads::SetSize(std::size_t iNumThreads, std::size_t iMaxThreads) noexcept
//-----------------------------------------------------------------------------
{
	m_numThreads = iNumThreads;
	if (!m_numThreads) {
		// calculate number of threads if auto value (0) given:
		m_numThreads = std::thread::hardware_concurrency();
	}
	if (iMaxThreads && m_numThreads > iMaxThreads)
	{
		m_numThreads = iMaxThreads;
	}
	return m_numThreads;

} // SetSize

//-----------------------------------------------------------------------------
/// store (or detach) the new thread object
std::thread::id KRunThreads::Store(std::thread thread)
//-----------------------------------------------------------------------------
{
	if (m_start_detached)
	{
		// detach
		thread.detach();
		// return empty thread id
		return std::thread::id{};
	}
	else
	{
		// add it to the KThreads object and return thread id
		return Add(std::move(thread));
	}

} // Store

//-----------------------------------------------------------------------------
void KBlockOnID::Data::Lock(std::size_t ID)
//-----------------------------------------------------------------------------
{
	lockmap_t::iterator it;

	{
		auto id_mutexes = m_id_mutexes.unique();

		it = id_mutexes->find(ID);
		
		if (it == id_mutexes->end())
		{
			it = id_mutexes->emplace(ID, std::make_unique<std::mutex>()).first;
		}
	}

	it->second->lock();

} // Data::Lock

//-----------------------------------------------------------------------------
bool KBlockOnID::Data::Unlock(std::size_t ID)
//-----------------------------------------------------------------------------
{
	lockmap_t::const_iterator it;
	bool bFoundLock;

	{
		auto id_mutexes = m_id_mutexes.shared();
		it = id_mutexes->find(ID);
		bFoundLock = it != id_mutexes->end();
	}

	if (bFoundLock)
	{
		it->second->unlock();
	}
	else
	{
		kWarning("cannot unlock lock for ID {}", ID);
	}

	return bFoundLock;

} // Data::Unlock

} // end of namespace dekaf2

