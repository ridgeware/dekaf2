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

#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

// points to the control block of the callback the current thread is executing,
// so that Cancel() from inside a callback does not wait for itself
thread_local const void* tls_pRunningControl = nullptr;

} // end of anonymous namespace

//---------------------------------------------------------------------------
KTimer::InFlight::InFlight(std::shared_ptr<Flight> pFlight)
//---------------------------------------------------------------------------
: m_pFlight(std::move(pFlight))
{
	std::lock_guard<std::mutex> Lock(m_pFlight->Mutex);
	++m_pFlight->iInFlight;

} // ctor InFlight

//---------------------------------------------------------------------------
KTimer::InFlight::~InFlight()
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_pFlight->Mutex);
	--m_pFlight->iInFlight;
	// notify before unlock - the woken destructor may destroy the KTimer,
	// this object only shares ownership of the flight control
	m_pFlight->CVDone.notify_all();

} // dtor InFlight

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

		// wait for callbacks that still run in their own (detached) threads -
		// no new ones can start, the timing thread is gone
		std::unique_lock<std::mutex> FlightLock(m_Flight->Mutex);
		m_Flight->CVDone.wait(FlightLock, [this]{ return m_Flight->iInFlight == 0; });
	}

} // dtor

//---------------------------------------------------------------------------
void KTimer::CleanupChildAfterFork()
//---------------------------------------------------------------------------
{
	m_bShutdown = true;
	// clear the unique_ptr with the thread - the thread doesn't exist anymore,
	// and there is no way to cleanly destroy it.
	// Note: memset on unique_ptr is technically UB, but zeroing its internal
	// pointer is equivalent to a null state, which prevents the destructor
	// from trying to join or detach a dead thread after fork(). This is a
	// deliberate post-fork cleanup hack - there is no conforming way to
	// abandon a std::thread that no longer exists in the child process.
	memset(reinterpret_cast<void*>(&m_TimingThread), 0, sizeof(m_TimingThread));
	// callback threads did not survive the fork either - start with a clean
	// flight count so a later destruction does not wait forever
	m_Flight = std::make_shared<Flight>();

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

	timer.Control = std::make_shared<struct Control>();

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
		m_TimingThread = std::make_unique<std::thread>(kMakeThread(&KTimer::TimingLoop, this, m_MaxIdle));
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
	std::shared_ptr<struct Control> pControl;

	{
		auto Timers = m_Timers.unique();

		auto it = Timers->find(ID);

		if (it == Timers->end())
		{
			// ID not known for this timer
			return false;
		}

		pControl = std::move(it->second.Control);

		Timers->erase(it);
	}

	if (pControl)
	{
		std::unique_lock<std::mutex> Lock(pControl->Mutex);

		// prevents any callback of this timer that has already been extracted
		// from the timer map from starting
		pControl->bCancelled = true;

		if (tls_pRunningControl == pControl.get())
		{
			// called from inside the own callback - do not wait for ourselves
			return true;
		}

		// drain: wait until no callback of this timer is running anymore
		pControl->CVDone.wait(Lock, [&pControl]{ return pControl->iRunning == 0; });
	}

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

//---------------------------------------------------------------------------
void KTimer::RunGuarded(const Callback& CB, KUnixTime Tp, Control& Ctrl)
//---------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(Ctrl.Mutex);

		if (Ctrl.bCancelled)
		{
			// cancelled after extraction from the timer map
			return;
		}

		++Ctrl.iRunning;
	}

	// marks this thread as the one executing the callback, and brings the
	// running count down again even when the callback throws
	class Running
	{
	public:
		Running(Control& Ctrl)
		: m_Ctrl(Ctrl)
		, m_pPrevious(tls_pRunningControl)
		{
			tls_pRunningControl = &Ctrl;
		}
		~Running()
		{
			tls_pRunningControl = m_pPrevious;
			std::lock_guard<std::mutex> Lock(m_Ctrl.Mutex);
			--m_Ctrl.iRunning;
			// notify before unlock - the woken Cancel() may drop the last
			// other reference to this control right after
			m_Ctrl.CVDone.notify_all();
		}
	private:
		Control&    m_Ctrl;
		const void* m_pPrevious;
	};

	Running RunningGuard(Ctrl);

	CB(Tp);

} // RunGuarded

//---------------------------------------------------------------------------
void KTimer::RunDue(const Timer& timer, KUnixTime Tp)
//---------------------------------------------------------------------------
{
	if ((timer.Flags & OwnThread) == OwnThread)
	{
		// raise the flight count before the thread exists, so that the
		// destructor cannot miss it - the thread drops it at exit
		auto pInFlight = std::make_shared<InFlight>(m_Flight);

		kMakeThread([CB = timer.CB, Tp, pControl = timer.Control, pInFlight]()
		{
			RunGuarded(CB, Tp, *pControl);
		}).detach();
	}
	else
	{
		RunGuarded(timer.CB, Tp, *timer.Control);
	}

} // RunDue

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
		while (!m_bIsPaused && !m_bShutdown);
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
	// name this thread for debugging tools like ps, top, or gdb
	kSetThreadName("ktimer");

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

	std::vector<Timer> DueTimers;
	std::vector<ID_t>  CancelledCallbacks;
	uint16_t           iLooped { 0 };

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

			if (++iLooped > 10)
			{
				// sync to real time after max 10 unsynced rounds
				iLooped = 0;
				tNow    = KUnixTime::now();
				tNext   = tNow;
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

		if (iLooped > 1)
		{
			// only query real time if not just done above,
			// or in last loop
			tNow   = KUnixTime::now();
			tNext  = tNow;
		}
		tNext     += MaxIdle;
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
					DueTimers.push_back(Timer);

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
		for (auto& Due : DueTimers)
		{
			RunDue(Due, tNow);
		}

		// and delete the temporary vector
		DueTimers.clear();
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

#if DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KDuration     KTimer::Infinite;
constexpr KTimer::ID_t  KTimer::InvalidID;
#endif


DEKAF2_NAMESPACE_END
