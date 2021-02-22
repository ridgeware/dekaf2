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

#ifdef _MSC_VER
KStopTime::ConstructHalted KStopTime::Halted;
KStopWatch::ConstructHalted KStopWatch::Halted;
#endif

//-----------------------------------------------------------------------------
/// returns elapsed nanoseconds
std::chrono::nanoseconds::rep KStopTime::nanoseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::nanoseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed microseconds
std::chrono::microseconds::rep KStopTime::microseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::microseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed milliseconds
std::chrono::milliseconds::rep KStopTime::milliseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::milliseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed seconds
std::chrono::seconds::rep KStopTime::seconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::seconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed minutes
std::chrono::minutes::rep KStopTime::minutes() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::minutes>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed hours
std::chrono::hours::rep KStopTime::hours() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::hours>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed nanoseconds
std::chrono::nanoseconds::rep KStopWatch::nanoseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::nanoseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed microseconds
std::chrono::microseconds::rep KStopWatch::microseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::microseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed milliseconds
std::chrono::milliseconds::rep KStopWatch::milliseconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::milliseconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed seconds
std::chrono::seconds::rep KStopWatch::seconds() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::seconds>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed minutes
std::chrono::minutes::rep KStopWatch::minutes() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::minutes>().count();
}

//-----------------------------------------------------------------------------
/// returns elapsed hours
std::chrono::hours::rep KStopWatch::hours() const
//-----------------------------------------------------------------------------
{
	return elapsed<std::chrono::hours>().count();
}

//-----------------------------------------------------------------------------
void KDurations::clear()
//-----------------------------------------------------------------------------
{
	m_Durations.clear();

} // clear

//---------------------------------------------------------------------------
template<>
KDurations::Duration KDurations::GetDuration<KDurations::Duration>(size_type iInterval) const
//---------------------------------------------------------------------------
{
	if (iInterval < m_Durations.size())
	{
		return m_Durations[iInterval];
	}
	else
	{
		return Duration::zero();
	}

} // GetDuration

//-----------------------------------------------------------------------------
std::chrono::nanoseconds::rep KDurations::nanoseconds(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::nanoseconds>(iInterval).count();
}

//-----------------------------------------------------------------------------
std::chrono::microseconds::rep KDurations::microseconds(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::microseconds>(iInterval).count();
}

//-----------------------------------------------------------------------------
std::chrono::milliseconds::rep KDurations::milliseconds(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::milliseconds>(iInterval).count();
}

//-----------------------------------------------------------------------------
std::chrono::seconds::rep KDurations::seconds(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::seconds>(iInterval).count();
}

//-----------------------------------------------------------------------------
std::chrono::minutes::rep KDurations::minutes(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::minutes>(iInterval).count();
}

//-----------------------------------------------------------------------------
std::chrono::hours::rep KDurations::hours(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	return GetDuration<std::chrono::hours>(iInterval).count();
}

//---------------------------------------------------------------------------
template<>
KDurations::Duration KDurations::TotalDuration<KDurations::Duration>() const
//---------------------------------------------------------------------------
{
	Duration Total = Duration::zero();

	for (const auto& Duration : m_Durations)
	{
		Total += Duration;
	}

	return Total;

} // TotalDuration

//-----------------------------------------------------------------------------
std::chrono::nanoseconds::rep KDurations::nanoseconds() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::nanoseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::microseconds::rep KDurations::microseconds() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::microseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::milliseconds::rep KDurations::milliseconds() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::milliseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::seconds::rep KDurations::seconds() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::seconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::minutes::rep KDurations::minutes() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::minutes>().count();
}

//-----------------------------------------------------------------------------
std::chrono::hours::rep KDurations::hours() const
//-----------------------------------------------------------------------------
{
	return TotalDuration<std::chrono::hours>().count();
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator+(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp += other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator+=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = size();

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] += other.m_Durations[i];
	}

	m_iRounds += other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator-(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp -= other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator-=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = size();

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] -= other.m_Durations[i];
	}

	m_iRounds -= other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator*(size_type iMultiplier) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp *= iMultiplier;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator*=(size_type iMultiplier)
//-----------------------------------------------------------------------------
{
	for (auto& it : m_Durations)
	{
		it *= iMultiplier;
	}

	m_iRounds *= iMultiplier;

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator/(size_type iDivisor) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp /= iDivisor;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator/=(size_type iDivisor)
//-----------------------------------------------------------------------------
{
	if (iDivisor)
	{
		for (auto& it : m_Durations)
		{
			it /= iDivisor;
		}

		m_iRounds /= iDivisor;
	}
	else
	{
		kWarning("cannot divide by 0");
	}

	return *this;
}

//-----------------------------------------------------------------------------
void KStopDurations::clear()
//-----------------------------------------------------------------------------
{
	base::clear();
	m_Timer.clear();

} // clear

//---------------------------------------------------------------------------
void KStopDurations::StartNextInterval()
//---------------------------------------------------------------------------
{
	push_back(m_Timer.elapsedAndClear<Duration>());

} // StartNextInterval

//---------------------------------------------------------------------------
void KStopDurations::StoreInterval(size_type iInterval)
//---------------------------------------------------------------------------
{
	if (size() < iInterval + 1)
	{
		resize(iInterval + 1);
	}

	operator[](iInterval) += m_Timer.elapsedAndClear<Duration>();

} // StoreInterval

//---------------------------------------------------------------------------
KTimer::KTimer()
//---------------------------------------------------------------------------
{
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
	timer.CB = std::move(CB);
	timer.Flags = NONE;

	return AddTimer(std::move(timer));

} // CallEvery

//---------------------------------------------------------------------------
KTimer::ID_t KTimer::CallOnce(Timepoint tp, Callback CB)
//---------------------------------------------------------------------------
{
	Timer timer;
	timer.ExpiresAt = tp;
	timer.CB = std::move(CB);
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
	timer.CBT = std::move(CBT);
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
	timer.CBT = std::move(CBT);
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
	// make sure we do not catch signals in this thread (this can happen if
	// the signal handler thread had not been started at init of dekaf2)
	kBlockAllSignals();

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (Dekaf::IsShutDown() || m_bShutdown)
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
