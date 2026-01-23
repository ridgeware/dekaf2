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

#include "ktimer.h"
#include "dekaf2.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KTimer::KTimer(KDuration MaxIdle)
//---------------------------------------------------------------------------
: m_MaxIdle(MaxIdle)
, m_bShutdown(false)
{
} // ctor

//---------------------------------------------------------------------------
KTimer::~KTimer()
//---------------------------------------------------------------------------
{
	// if m_bShutdown is already true CleanupChildAfterFork() had been called,
	// and this is already a dead instance
	if (!m_bShutdown)
	{
		// signal the thread to shutdown
		m_bShutdown = true;
		// and release if paused
		m_bPause = false;

		// make sure we are not right in initialization of a new thread
		std::lock_guard<std::mutex> Lock(m_ThreadCreationMutex);

		if (m_TimingThread)
		{
			// wait until thread has finished
			m_TimingThread->join();
			kDebug(2, "joined timer thread");
		}
	}

} // dtor

//---------------------------------------------------------------------------
void KTimer::CleanupChildAfterFork()
//---------------------------------------------------------------------------
{
	m_bShutdown = true;
	// clear the unique_ptr with the thread - the thread doesn't exist anymore,
	// and there is no way to cleanly destroy it
	memset(reinterpret_cast<void*>(&m_TimingThread), 0, sizeof(m_TimingThread));

} // CleanupChildAfterFork

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::AddTimer(Timer timer)
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ThreadCreationMutex);

	if (timer.ID == InvalidID)
	{
		timer.ID = GetNextID();
	}

	ID_t ID;

	{
		auto Timers = m_Timers.unique();

		auto ret = Timers->emplace(timer.ID, std::move(timer));

		if (ret.second)
		{
			ID = ret.first->second.ID;
			m_bAddedTimer = true;
		}
		else
		{
			ID = InvalidID;
		}
	}

	if (!m_TimingThread)
	{
		m_TimingThread = std::make_unique<std::thread>(&KTimer::TimingLoop, this, m_MaxIdle);
	}

	return ID;

} // AddTimer

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallEvery(KDuration interval, Callback CB, bool bOwnThread)
//---------------------------------------------------------------------------
{
	if (interval.count() == 0)
	{
		return InvalidID;
	}

	return AddTimer(Timer(interval, std::move(CB), bOwnThread, false));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(KUnixTime timepoint, Callback CB, bool bOwnThread)
//---------------------------------------------------------------------------
{
	return AddTimer(Timer(timepoint, std::move(CB), bOwnThread));

} // CallOnce

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(KDuration interval, Callback CB, bool bOwnThread)
//---------------------------------------------------------------------------
{
	return AddTimer(Timer(interval, std::move(CB), bOwnThread, true));

} // CallOnce

//---------------------------------------------------------------------------
void KTimer::SleepFor(KDuration interval)
//---------------------------------------------------------------------------
{
	std::this_thread::sleep_for(interval);
}

//---------------------------------------------------------------------------
void KTimer::SleepUntil(KUnixTime timepoint)
//---------------------------------------------------------------------------
{
	std::this_thread::sleep_until(timepoint);
}

//---------------------------------------------------------------------------
bool KTimer::Cancel(ID_t ID)
//---------------------------------------------------------------------------
{
	auto Timers = m_Timers.unique();

	auto it = Timers->find(ID);

	if (it == Timers->end())
	{
		// ID not known for this timer
		return false;
	}

	Timers->erase(it);

	return true;

} // Cancel

//---------------------------------------------------------------------------
bool KTimer::Restart(ID_t ID)
//---------------------------------------------------------------------------
{
	auto Timers = m_Timers.unique();

	auto it = Timers->find(ID);

	if (it == Timers->end())
	{
		// ID not known for this timer
		return false;
	}

	if (it->second.Interval == KDuration::zero())
	{
		return false;
	}

	it->second.ExpiresAt = KUnixTime::now() + it->second.Interval;

	m_bAddedTimer = true;

	return true;

} // Restart

//---------------------------------------------------------------------------
bool KTimer::Restart(ID_t ID, KDuration interval)
//---------------------------------------------------------------------------
{
	auto Timers = m_Timers.unique();

	auto it = Timers->find(ID);

	if (it == Timers->end())
	{
		// ID not known for this timer
		return false;
	}

	if (it->second.Interval == KDuration::zero())
	{
		return false;
	}

	it->second.Interval  = interval;
	it->second.ExpiresAt = KUnixTime::now() + it->second.Interval;

	m_bAddedTimer = true;

	return true;

} // Restart

//---------------------------------------------------------------------------
bool KTimer::Restart(ID_t ID, KUnixTime timepoint)
//---------------------------------------------------------------------------
{
	auto Timers = m_Timers.unique();

	auto it = Timers->find(ID);

	if (it == Timers->end())
	{
		// ID not known for this timer
		return false;
	}

	if ((it->second.Flags & Once) != Once)
	{
		return false;
	}

	it->second.ExpiresAt = timepoint;

	m_bAddedTimer = true;

	return true;

} // Restart

namespace {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DueCallback
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//---------------------------------------------------------------------------
	DueCallback(KTimer::Callback CB, bool bOwnThread)
	//---------------------------------------------------------------------------
	: m_CB(std::move(CB))
	, m_bOwnThread(bOwnThread)
	{
	}

	//---------------------------------------------------------------------------
	void Run(KUnixTime Tp)
	//---------------------------------------------------------------------------
	{
		if (m_bOwnThread)
		{
			std::thread thread(m_CB, Tp);
			thread.detach();
		}
		else
		{
			m_CB(Tp);
		}
	}

//----------
private:
//----------

	KTimer::Callback m_CB;
	bool             m_bOwnThread;

}; // DueCallback

} // end of anonymous namespace

//---------------------------------------------------------------------------
void KTimer::Pause()
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ThreadCreationMutex);

	m_bPause = true;

	if (m_TimingThread)
	{
		do
		{
			std::this_thread::sleep_for(m_MaxIdle / 4);
		}
		while (!m_bIsPaused);
	}
	else
	{
		m_bIsPaused = true;
	}

} // Pause

//---------------------------------------------------------------------------
void KTimer::Resume()
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ThreadCreationMutex);

	m_bPause = false;

	if (!m_TimingThread)
	{
		m_bIsPaused = false;
	}

} // Resume

//---------------------------------------------------------------------------
void KTimer::TimingLoop(KDuration MaxIdle)
//---------------------------------------------------------------------------
{
	// make sure we do not catch signals in this thread (this can happen if
	// the signal handler thread had not been started at init of dekaf2)
	kBlockAllSignals();

	// return immediately if this thread was created after the instance
	// was shutdown
	if (m_bShutdown) return;

	kDebug(2, "new timer thread started with max idle {}", MaxIdle);

	auto      tNow       = KUnixTime::now();
	KUnixTime tNext      = tNow + MaxIdle;
	KUnixTime tNextTimer = tNow + Infinite;

	// find closest deadline
	{
		auto Timers = m_Timers.shared();

		for (auto& it : Timers.get())
		{
			if (it.second.ExpiresAt < tNextTimer)
			{
				tNextTimer = it.second.ExpiresAt;
			}
		}
	}

	std::vector<DueCallback> DueCallbacks;
	std::vector<ID_t>        CancelledCallbacks;

	for (;;)
	{
//		kDebug(4, "next deadline in {}", tNext - tNow);
		for (;;)
		{
			if (tNext > tNextTimer)
			{
				tNext = tNextTimer;
			}

			std::this_thread::sleep_until(tNext);

			// exit immediately if class or program are ended
			if (m_bShutdown || Dekaf::IsShutDown()) return;

			// pause until resume?
			if (m_bPause)
			{
				m_bIsPaused = true;

				do
				{
					std::this_thread::sleep_for(chrono::milliseconds(100));

					// exit immediately if class or program are ended
					if (m_bShutdown || Dekaf::IsShutDown()) return;
				}
				while (m_bPause);

				m_bIsPaused = false;
			}

			// do we have an expired timer?
			if (tNext >= tNextTimer) break;

			// or a new timer added?
			if (m_bAddedTimer) break;

			// else prepare for next sleep
			tNext += MaxIdle;
		}

		// enable logging in this thread, the global instance of
		// KTimer is started long before any option parsing
		KLog::SyncLevel();

		tNow       = KUnixTime::now();
		tNext      = tNow + MaxIdle;
		tNextTimer = tNow + Infinite;

		{
			auto Timers = m_Timers.unique();

			// reset flag
			m_bAddedTimer = false;

			// check all timers for their expiration date
			for (auto& it : Timers.get())
			{
				auto& Timer = it.second;

				if (Timer.ExpiresAt <= tNow)
				{
					DueCallbacks.push_back(DueCallback(Timer.CB, Timer.Flags & OwnThread));

					if ((Timer.Flags & Once) == Once)
					{
						// remove this timer
						CancelledCallbacks.push_back(Timer.ID);
					}
					else
					{
						// calculate next expiration
						Timer.ExpiresAt  = tNow + Timer.Interval;
					}
				}

				// check for closest deadline
				if (Timer.ExpiresAt < tNextTimer)
				{
					tNextTimer = Timer.ExpiresAt;
				}
			}

			for (auto ID : CancelledCallbacks)
			{
				kDebug(2, "remove one-time timer {}", ID);
				Timers->erase(ID);
			}
			
			CancelledCallbacks.clear();

		} // end of scope for unique lock

		if (m_bShutdown) return;

		// now call all due callbacks
		for (auto& Due : DueCallbacks)
		{
			Due.Run(tNow);
		}

		// and delete the temporary vector
		DueCallbacks.clear();
	}

} // TimingLoop

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::GetNextID()
//---------------------------------------------------------------------------
{
	static std::atomic<KTimer::ID_t> s_LastID { InvalidID };

	ID_t ID = ++s_LastID;

	while (ID == InvalidID)
	{
		kWarning("timer ID overflow - now reusing IDs");
		ID = ++s_LastID;
	}

	return ID;

} // GetNextID

DEKAF2_NAMESPACE_END
