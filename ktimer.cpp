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
: m_bShutdown(std::make_shared<std::atomic<bool>>(false))
, m_MaxIdle(MaxIdle)
{
} // ctor

//---------------------------------------------------------------------------
KTimer::~KTimer()
//---------------------------------------------------------------------------
{
	if (m_TimingThread)
	{
		// signal the thread to shutdown
		*m_bShutdown = true;

		if (!m_bDestructWithJoin)
		{
			// detach the thread, we do not want to wait until it has joined
			m_TimingThread->detach();
			kDebug(2, "detached timer thread");
		}
		else
		{
			// wait until thread has finished
			m_TimingThread->join();
			kDebug(2, "joined timer thread");
		}
	}

} // dtor

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::AddTimer(Timer timer)
//---------------------------------------------------------------------------
{
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
		}
		else
		{
			ID = InvalidID;
		}
	}

	std::lock_guard<std::mutex> Lock(m_ThreadCreationMutex);

	if (!m_TimingThread)
	{
		m_TimingThread = std::make_shared<std::thread>(&KTimer::TimingLoop, this, m_MaxIdle);
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
void KTimer::TimingLoop(KDuration MaxIdle)
//---------------------------------------------------------------------------
{
	// make sure we do not catch signals in this thread (this can happen if
	// the signal handler thread had not been started at init of dekaf2)
	kBlockAllSignals();

	// create a copy of the class variable, as this is a shared_ptr,
	// both instances will point to the same bool
	auto bShutdown(m_bShutdown);

	// and copy this thread's shared ptr as well, to keep us alive
	// even after KTimer goes away
	auto MySelf(m_TimingThread);

	kDebug(2, "new timer thread started with max idle {}", MaxIdle);

	auto tNow = KUnixTime::now();

	KUnixTime tNext = tNow + MaxIdle;

	// find closest deadline
	{
		auto Timers = m_Timers.shared();

		for (auto& it : Timers.get())
		{
			if (it.second.ExpiresAt < tNext)
			{
				tNext = it.second.ExpiresAt;
			}
		}
	}

	std::vector<DueCallback> DueCallbacks;
	std::vector<ID_t>        CancelledCallbacks;

	for (;;)
	{
		kDebug(3, "next deadline in {}", tNext - tNow);
		std::this_thread::sleep_until(tNext);

		// enable logging in this thread, the global instance of
		// KTimer is started long before any option parsing
		KLog::SyncLevel();

		if (*bShutdown || Dekaf::IsShutDown())
		{
			// exit this thread.. parent class is gone
			return;
		}

		tNow = KUnixTime::now();

		tNext  = tNow;
		tNext += MaxIdle;

		{
			auto Timers = m_Timers.unique();

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
				if (Timer.ExpiresAt < tNext)
				{
					tNext = Timer.ExpiresAt;
				}

			}

			for (auto ID : CancelledCallbacks)
			{
				kDebug(2, "remove one-time timer {}", ID);
				Timers->erase(ID);
			}
			
			CancelledCallbacks.clear();

		} // end of scope for unique lock

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
