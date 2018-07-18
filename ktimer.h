/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, RID_tgeware, Inc.
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
// |/|   THE SOFTWARE IS PROVID_tED "AS IS", WITHOUT WARRANTY OF ANY         |/|
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

#pragma once

#include <chrono>
#include <thread>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <mutex>

/// @file ktimer.h
/// general timing facilities

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measure the time between two events
class KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using clock_t = std::chrono::steady_clock;

	//-----------------------------------------------------------------------------
	size_t elapsed()
	//-----------------------------------------------------------------------------
	{
		return (clock_t::now() - m_Start).count();
	}

//----------
protected:
//----------

	clock_t::time_point m_Start { clock_t::now() };

}; // KStopTime



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// really simple implementation of a stop watch
class KStopWatch : public KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	size_t elapsed()
	//-----------------------------------------------------------------------------
	{
		if (!m_bIsHalted)
		{
			return (clock_t::now() - m_Start).count() + m_iNanoSoFar;
		}
		else
		{
			return m_iNanoSoFar;
		}
	}

	//-----------------------------------------------------------------------------
	void halt()
	//-----------------------------------------------------------------------------
	{
		m_iNanoSoFar += (clock_t::now() - m_Start).count();
		m_bIsHalted = true;
	}

	//-----------------------------------------------------------------------------
	void resume()
	//-----------------------------------------------------------------------------
	{
		if (m_bIsHalted)
		{
			m_bIsHalted = false;
			m_Start = clock_t::now();
		}
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_iNanoSoFar = 0;
		m_bIsHalted = true;
	}

//----------
private:
//----------

	bool m_bIsHalted { false };
	size_t m_iNanoSoFar { 0 };

}; // KStopWatch

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KTimer can be used to call functions both repeatedly after a fixed
/// time interval or once after expiration of a time interval, or at
/// a fixed time point. Granularity is low (1 second), it is intended for
/// cleanup purposes or similar repetitive tasks, not high precision
/// timing.
class KTimer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ID_t = size_t;
	enum { INVALID = 0 };
	using Clock         = std::chrono::system_clock;
	using Interval      = Clock::duration;
	using Timepoint     = Clock::time_point;
	using Callback      = std::function<void(Timepoint)>;
	using CallbackTimeT = std::function<void(time_t)>;

	//---------------------------------------------------------------------------
	KTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(KTimer&&) = default;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	~KTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(KTimer&&) = default;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb every interval in a separate thread.
	/// A return other than INVALID is a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallEvery(Interval intv, Callback CB);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb exactly once at a given timepoint
	/// A return other than INVALID is a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallOnce(Timepoint tp, Callback CB);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep for interval
	static void SleepFor(Interval intv);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep until timepoint
	static void SleepUntil(Timepoint tp);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb every interval in a separate thread
	/// A return other than INVALID is a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallEvery(time_t intv, CallbackTimeT CBT);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb exactly once at a given timepoint
	/// A return other than INVALID is a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallOnce(time_t tp, CallbackTimeT CBT);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep for interval
	static void SleepFor(time_t intv);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep until timepoint
	static void SleepUntil(time_t tp);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Cancel pending timer. Returns false if not found.
	bool Cancel(ID_t ID);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Shorthand to convert a KTimer::Timepoint into a time_t value
	static time_t ToTimeT(Timepoint tp);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Shorthand to convert a time_t value into a KTimer::Timepoint
	static Timepoint FromTimeT(time_t tt);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template
	<
	        typename DstTimePoint,
	        typename SrcTimePoint,
	        typename DstClock = typename DstTimePoint::clock,
	        typename SrcClock = typename SrcTimePoint::clock
	        >
	/// Generic conversion function between two Timepoints of different
	/// source clocks. Involves three system calls, so is somewhat
	/// expensive. Offers precision in the one digit microsecond domain.
	static DstTimePoint TimepointCast(const SrcTimePoint tp)
	//---------------------------------------------------------------------------
	{
		if (std::is_same<SrcClock, DstClock>{})
		{
			return tp;
		}
		const auto src_before = SrcClock::now();
		const auto dst_now    = DstClock::now();
		const auto src_after  = SrcClock::now();
		const auto src_diff   = src_after - src_before;
		const auto src_now    = src_before + src_diff / 2;
		return dst_now + (tp - src_now);
	}

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template
	<
	        typename DstTimePoint,
	        typename ScrTimePoint,
	        typename DstDuration = typename DstTimePoint::duration,
	        typename SrcDuration = typename ScrTimePoint::duration,
	        typename DstClock    = typename DstTimePoint::clock,
	        typename SrcClock    = typename ScrTimePoint::clock
	        >
	/// Generic precision conversion function between two Timepoints of
	/// different source clocks. Involves up to fifteen system calls, so is
	/// expensive. Offers precision in the one digit nanosecond domain.
	static DstTimePoint TimepointPrecisionCast(
	        const ScrTimePoint tp,
	        const SrcDuration tolerance = std::chrono::nanoseconds {100},
	        int limit = 5)
	//---------------------------------------------------------------------------
	{
		if (std::is_same<SrcClock, DstClock>{})
		{
			return tp;
		}

		if (limit < 1)
		{
			limit = 1;
		}

		auto itercnt = 0;
		auto src_now = ScrTimePoint {};
		auto dst_now = DstTimePoint {};
		auto epsilon = max_duration<SrcDuration>();
		do
		{
			const auto src_before  = SrcClock::now();
			const auto dst_between = DstClock::now();
			const auto src_after   = SrcClock::now();
			const auto src_diff    = src_after - src_before;
			const auto delta       = abs_duration(src_diff);
			if (delta < epsilon)
			{
				src_now = src_before + src_diff / 2;
				dst_now = dst_between;
				epsilon = delta;
			}
			if (++itercnt >= limit)
				break;
		}
		while (epsilon > tolerance);
		return dst_now + (tp - src_now);
	}

	//---------------------------------------------------------------------------
	void DestructWithJoin()
	//---------------------------------------------------------------------------
	{
		m_bDestructWithJoin = true;
	}

//----------
private:
//----------

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template <typename DurationT, typename ReprT = typename DurationT::rep>
	/// helper function for precision clock conversion
	static constexpr DurationT max_duration() noexcept
	//---------------------------------------------------------------------------
	{
		return DurationT {std::numeric_limits<ReprT>::max()};
	}

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template <typename DurationT>
	/// helper function for precision clock conversion
	static constexpr DurationT abs_duration(const DurationT d) noexcept
	//---------------------------------------------------------------------------
	{
		return DurationT {(d.count() < 0) ? -d.count() : d.count()};
	}

	//---------------------------------------------------------------------------
	void TimingLoop();
	//---------------------------------------------------------------------------

	struct Timer;

	//---------------------------------------------------------------------------
	ID_t AddTimer(Timer&& timer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	static void StartCallback();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	static ID_t GetNextID();
	//---------------------------------------------------------------------------

	enum FLAGS
	{
		NONE    = 0,
		ONCE    = 1 << 0, // this timer shall be run only once
		TIMET   = 1 << 1, // this timer shall be called with a time_t argument
		REMOVED = 1 << 2  // this timer is deleted
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Timer
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		ID_t ID{INVALID};
		Timepoint ExpiresAt;
		Interval IVal;
		Callback CB{nullptr};
		CallbackTimeT CBT{nullptr};
		uint8_t Flags{NONE};
	};

	std::unique_ptr<std::thread> m_tTiming;
	bool m_bShutdown{false};
	bool m_bDestructWithJoin{false};

	using map_t = std::unordered_map<ID_t, Timer>;
	map_t m_Timers;
	std::mutex m_TimerMutex;

}; // KTimer

} // of namespace dekaf2
