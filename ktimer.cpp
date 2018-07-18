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
#include "klog.h"

namespace dekaf2 {

//---------------------------------------------------------------------------
KTimer::KTimer()
//---------------------------------------------------------------------------
{
	m_tTiming = std::make_unique<std::thread>(&KTimer::TimingLoop, this);

} // ctor

//---------------------------------------------------------------------------
/// Avoid a memory violation on program shutdown by providing external storage
/// for an internal flag
KTimer::KTimer(bool& bShutdownStorage)
//---------------------------------------------------------------------------
: m_bShutdown(bShutdownStorage)
{
	m_bShutdown = false;
	m_tTiming = std::make_unique<std::thread>(&KTimer::TimingLoop, this);

} // ctor

//---------------------------------------------------------------------------
KTimer::~KTimer()
//---------------------------------------------------------------------------
{
	// signal the thread to shutdown
	m_bShutdown = true;

	if (m_tTiming)
	{
		if (!m_bDestructWithJoin)
		{
			// detach the thread, we do not want to wait until it has joined
			m_tTiming->detach();
		}
		else
		{
			// wait until thread has finished
			m_tTiming->join();
		}
	}

} // dtor

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::AddTimer(Timer&& timer)
//---------------------------------------------------------------------------
{
	if (timer.ID == INVALID)
	{
		timer.ID = GetNextID();
	}

	std::lock_guard<std::mutex> Lock(m_TimerMutex);

	auto ret = m_Timers.emplace(timer.ID, std::move(timer));

	if (ret.second)
	{
		return ret.first->second.ID;
	}
	else
	{
		return INVALID;
	}

} // AddTimer

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallEvery(Interval intv, Callback CB)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt = Clock::now() + intv;
	timer.IVal = intv;
	timer.CB = CB;
	timer.Flags = NONE;

	return AddTimer(std::move(timer));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(Timepoint tp, Callback CB)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt = tp;
	timer.CB = CB;
	timer.Flags = ONCE;

	return AddTimer(std::move(timer));

} // CallOnce

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallEvery(time_t intv, CallbackTimeT CBT)
//---------------------------------------------------------------------------
{
	Timer timer;
	Interval iv = std::chrono::seconds(intv);
	timer.ExpiresAt = Clock::now() + iv;
	timer.IVal = iv;
	timer.CBT = CBT;
	timer.Flags = TIMET;

	return AddTimer(std::move(timer));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(time_t tp, CallbackTimeT CBT)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt = Clock::now();
	time_t now = ToTimeT(timer.ExpiresAt);
	timer.ExpiresAt += std::chrono::seconds(tp - now);
	timer.CBT = CBT;
	timer.Flags = ONCE | TIMET;

	return AddTimer(std::move(timer));

} // CallOnce

//---------------------------------------------------------------------------
void KTimer::SleepFor(Interval intv)
//---------------------------------------------------------------------------
{
	std::this_thread::sleep_for(intv);
}

//---------------------------------------------------------------------------
void KTimer::SleepUntil(Timepoint tp)
//---------------------------------------------------------------------------
{
	std::this_thread::sleep_until(tp);
}

//---------------------------------------------------------------------------
void KTimer::SleepFor(time_t intv)
//---------------------------------------------------------------------------
{
	std::this_thread::sleep_for(std::chrono::seconds(intv));
}

//---------------------------------------------------------------------------
void KTimer::SleepUntil(time_t tp)
//---------------------------------------------------------------------------
{
	auto cnow = Clock::now();
	time_t now = ToTimeT(cnow);
	cnow += std::chrono::seconds(tp - now);

	std::this_thread::sleep_until(cnow);

} // SleepUntil

//---------------------------------------------------------------------------
bool KTimer::Cancel(ID_t ID)
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_TimerMutex);

	auto it = m_Timers.find(ID);

	if (it == m_Timers.end())
	{
		// ID not known for this KTimer
		return false;
	}

	// mark as removed
	it->second.Flags |= REMOVED;
	return true;

} // Cancel

//---------------------------------------------------------------------------
void KTimer::TimingLoop()
//---------------------------------------------------------------------------
{
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (m_bShutdown)
		{
			return;
		}

		auto now = Clock::now();

		std::lock_guard<std::mutex> Lock(m_TimerMutex);

		// check all timers for their expiration date
		for (auto& it : m_Timers)
		{
			auto& Timer = it.second;

			if ((Timer.Flags & REMOVED) == REMOVED)
			{
				// remove this timer
				m_Timers.erase(Timer.ID);
			}
			else if (Timer.ExpiresAt < now)
			{
				if ((Timer.Flags & TIMET) == TIMET)
				{
					// call the thread with a time_t value
					std::thread thread(Timer.CBT, ToTimeT(now));
					thread.detach();
				}
				else
				{
					// call the thread with a time_point value
					std::thread thread(Timer.CB, now);
					thread.detach();
				}
				if ((Timer.Flags & ONCE) == ONCE)
				{
					// remove this timer
					m_Timers.erase(Timer.ID);
				}
				else
				{
					// calculate next expiration
					Timer.ExpiresAt = now + Timer.IVal;
				}
			}
		}
	}

} // TimingLoop

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::GetNextID()
//---------------------------------------------------------------------------
{
	static std::atomic<KTimer::ID_t> s_LastID{INVALID};

	ID_t ID = ++s_LastID;

	while (ID == INVALID)
	{
		kWarning("timer ID overflow - now reusing IDs");
		ID = ++s_LastID;
	}

	return ID;

} // GetNextID

//---------------------------------------------------------------------------
time_t KTimer::ToTimeT(Timepoint tp)
//---------------------------------------------------------------------------
{
	return Clock::to_time_t(tp);
}

//---------------------------------------------------------------------------
KTimer::Timepoint KTimer::FromTimeT(time_t tt)
//---------------------------------------------------------------------------
{
	return Clock::from_time_t(tt);
}

} // of namespace dekaf2
