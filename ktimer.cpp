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

namespace dekaf2 {

//---------------------------------------------------------------------------
KTimer::KTimer(Interval Granularity)
//---------------------------------------------------------------------------
{
	m_bShutdown = std::make_shared<std::atomic<bool>>(false);
	m_tTiming   = std::make_shared<std::thread>(&KTimer::TimingLoop, this, Granularity);

} // ctor

//---------------------------------------------------------------------------
KTimer::~KTimer()
//---------------------------------------------------------------------------
{
	// signal the thread to shutdown
	*m_bShutdown = true;

	if (m_tTiming)
	{
		if (!m_bDestructWithJoin)
		{
			// detach the thread, we do not want to wait until it has joined
			m_tTiming->detach();
			kDebug(2, "detached timer thread");
		}
		else
		{
			// wait until thread has finished
			m_tTiming->join();
			kDebug(2, "joined timer thread");
		}
	}

} // dtor

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::AddTimer(Timer timer)
//---------------------------------------------------------------------------
{
	if (timer.ID == INVALID)
	{
		timer.ID = GetNextID();
	}

	auto Timers = m_Timers.unique();

	auto ret = Timers->emplace(timer.ID, std::move(timer));

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
	if (intv.count() == 0)
	{
		return INVALID;
	}

	Timer timer;
	timer.ExpiresAt = Clock::now() + intv;
	timer.IVal      = intv;
	timer.CB        = std::move(CB);
	timer.Flags     = NONE;

	return AddTimer(std::move(timer));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(Timepoint tp, Callback CB)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt = tp;
	timer.CB        = std::move(CB);
	timer.Flags     = ONCE;

	return AddTimer(std::move(timer));

} // CallOnce

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallEvery(time_t intv, CallbackTimeT CBT)
//---------------------------------------------------------------------------
{
	if (intv == 0)
	{
		return INVALID;
	}

	Timer timer;
	Interval iv     = std::chrono::seconds(intv);
	timer.ExpiresAt = Clock::now() + iv;
	timer.IVal      = iv;
	timer.CBT       = std::move(CBT);
	timer.Flags     = TIMET;

	return AddTimer(std::move(timer));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(time_t tp, CallbackTimeT CBT)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt  = Clock::now();
	time_t now       = ToTimeT(timer.ExpiresAt);
	timer.ExpiresAt += std::chrono::seconds(tp - now);
	timer.CBT        = std::move(CBT);
	timer.Flags      = ONCE | TIMET;

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
	auto cnow   = Clock::now();
	time_t now  = ToTimeT(cnow);
	cnow       += std::chrono::seconds(tp - now);

	std::this_thread::sleep_until(cnow);

} // SleepUntil

//---------------------------------------------------------------------------
bool KTimer::Cancel(ID_t ID)
//---------------------------------------------------------------------------
{
	auto Timers = m_Timers.unique();

	auto it = Timers->find(ID);

	if (it == Timers->end())
	{
		// ID not known for this KTimer
		return false;
	}

	Timers->erase(it);

	return true;

} // Cancel

//---------------------------------------------------------------------------
void KTimer::TimingLoop(Interval Granularity)
//---------------------------------------------------------------------------
{
	// make sure we do not catch signals in this thread (this can happen if
	// the signal handler thread had not been started at init of dekaf2)
	kBlockAllSignals();

	// create a copy of the class variable, as this is a shared_ptr,
	// both instances will point to the same bool
	auto bShutdown(m_bShutdown);

	kDebug(2, "new timer thread started");

	for (;;)
	{
		std::this_thread::sleep_for(Granularity);

		if (*bShutdown || Dekaf::IsShutDown())
		{
			// exit this thread.. parent class is gone
			return;
		}

		auto now = Clock::now();

		auto Timers = m_Timers.unique();

		// check all timers for their expiration date
		for (auto& it : Timers.get())
		{
			auto& Timer = it.second;

			if (Timer.ExpiresAt < now)
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
					Timers->erase(Timer.ID);
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
	static std::atomic<KTimer::ID_t> s_LastID { INVALID };

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
