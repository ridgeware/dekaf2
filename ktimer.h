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

#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <vector>
#include "bits/kcppcompat.h"
#include "kthreadsafe.h"

/// @file ktimer.h
/// general timing facilities

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measure the time between two or more events, continuously
class DEKAF2_PUBLIC KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Clock    = std::chrono::steady_clock;
	using Duration = Clock::duration;

	/// tag to force construction without starting the timer
	static struct ConstructHalted {} Halted;

	//-----------------------------------------------------------------------------
	/// constructs and starts counting
	KStopTime()
	//-----------------------------------------------------------------------------
	: m_Start(Clock::now())
	{
	}

	//-----------------------------------------------------------------------------
	/// constructs without starting - use clear() to start counting
	explicit KStopTime(ConstructHalted)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed time (converted into any duration type, per default
 	/// nanoseconds)
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType elapsed(Clock::time_point tNow = Clock::now()) const
	//-----------------------------------------------------------------------------
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return tNow - m_Start;
		}
		else
	#endif
		{
			return std::chrono::duration_cast<DurationType>(tNow - m_Start);
		}

	} // elapsed

	//-----------------------------------------------------------------------------
	/// returns elapsed nanoseconds
	std::chrono::nanoseconds::rep nanoseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed microseconds
	std::chrono::microseconds::rep microseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed milliseconds
	std::chrono::milliseconds::rep milliseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed seconds
	std::chrono::seconds::rep seconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed minutes
	std::chrono::minutes::rep minutes() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed hours
	std::chrono::hours::rep hours() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed time (converted into any duration type, per default
	/// nanoseconds) and resets start time after readout
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType elapsedAndClear()
	//-----------------------------------------------------------------------------
	{
		auto tNow = Clock::now();

		auto tDuration = elapsed(tNow);

		m_Start = tNow;

		return tDuration;

	} // elapsedAndClear

	//-----------------------------------------------------------------------------
	/// resets start time to now
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_Start = Clock::now();

	} // clear

//----------
protected:
//----------

	Clock::time_point m_Start;

}; // KStopTime


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// implementation of a stop watch (which is a timer that can be stopped
/// and restarted, and only counts the time while it was not stopped)
class DEKAF2_PUBLIC KStopWatch : private KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base = KStopTime;
	using Duration = typename base::Duration;

	/// tag to force construction without starting the timer
	static struct ConstructHalted {} Halted;

	//-----------------------------------------------------------------------------
	/// constructs and starts counting
	KStopWatch() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// constructs without starting
	explicit KStopWatch(ConstructHalted)
	//-----------------------------------------------------------------------------
	: base(base::Halted)
	, m_bIsHalted(true)
	{
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed time (converted into any duration type, default is nanoseconds)
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType elapsed() const
	//-----------------------------------------------------------------------------
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return elapsed_int();
		}
		else
	#endif
		{
			return std::chrono::duration_cast<DurationType>(elapsed_int());
		}

	} // elapsed

	//-----------------------------------------------------------------------------
	/// returns elapsed nanoseconds
	std::chrono::nanoseconds::rep nanoseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed microseconds
	std::chrono::microseconds::rep microseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed milliseconds
	std::chrono::milliseconds::rep milliseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed seconds
	std::chrono::seconds::rep seconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed minutes
	std::chrono::minutes::rep minutes() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed hours
	std::chrono::hours::rep hours() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// halts elapsed time counting
	void halt()
	//-----------------------------------------------------------------------------
	{
		m_iDurationSoFar += base::elapsed();
		m_bIsHalted = true;

	} // halt

	//-----------------------------------------------------------------------------
	/// resumes elapsed time counting
	void resume()
	//-----------------------------------------------------------------------------
	{
		if (m_bIsHalted)
		{
			m_bIsHalted = false;
			base::clear();
		}

	} // resume

	//-----------------------------------------------------------------------------
	/// resets elapsed time, stops counter - call resume() to continue
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_iDurationSoFar = Duration::zero();
		m_bIsHalted = true;

	} // clear

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// returns elapsed (active) time based on clock_t's duration type
	Duration elapsed_int() const
	//-----------------------------------------------------------------------------
	{
		if (!m_bIsHalted)
		{
			return base::elapsed() + m_iDurationSoFar;
		}
		else
		{
			return m_iDurationSoFar;
		}

	} // elapsed_int

	Duration m_iDurationSoFar { Duration::zero() };
	bool m_bIsHalted { false };

}; // KStopWatch


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps multiple consecutive time intervals
class DEKAF2_PUBLIC KDurations
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self           = KDurations;
	using Duration       = typename KStopTime::Duration;
	using Storage        = std::vector<Duration>;
	using size_type      = typename Storage::size_type;
	using const_iterator = typename Storage::const_iterator;

	//-----------------------------------------------------------------------------
	/// reset all intervals
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reserve intervals
	void reserve(size_type iSize)
	//-----------------------------------------------------------------------------
	{
		m_Durations.reserve(iSize);
	}

	//-----------------------------------------------------------------------------
	/// resize for intervals
	void resize(size_type iSize)
	//-----------------------------------------------------------------------------
	{
		m_Durations.resize(iSize);
	}

	//-----------------------------------------------------------------------------
	/// push back another interval
	void push_back(Duration duration)
	//-----------------------------------------------------------------------------
	{
		m_Durations.push_back(duration);
	}

	//-----------------------------------------------------------------------------
	/// get last interval
	const Duration& back() const
	//-----------------------------------------------------------------------------
	{
		return m_Durations.back();
	}

	//-----------------------------------------------------------------------------
	/// get duration of an interval, rounded from internal duration type,
	/// per default nanoseconds
	/// @param iInterval 0 based index on intervals, returns zero duration if out
	/// of bounds
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType GetDuration(size_type iInterval) const
	//-----------------------------------------------------------------------------
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return GetDuration<Duration>(iInterval);
		}
		else
	#endif
		{
			return std::chrono::duration_cast<DurationType>(GetDuration<Duration>(iInterval));
		}
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed nanoseconds of an interval
	std::chrono::nanoseconds::rep nanoseconds(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed microseconds of an interval
	std::chrono::microseconds::rep microseconds(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed milliseconds of an interval
	std::chrono::milliseconds::rep milliseconds(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed seconds of an interval
	std::chrono::seconds::rep seconds(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed minutes of an interval
	std::chrono::minutes::rep minutes(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed hours of an interval
	std::chrono::hours::rep hours(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// subscription access
	template<typename DurationType = std::chrono::nanoseconds>
	Duration operator[](size_type iInterval) const
	//-----------------------------------------------------------------------------
	{
		return GetDuration<DurationType>(iInterval);
	}

	//-----------------------------------------------------------------------------
	/// returns total duration of all durations, rounded from internal duration
	/// type, per default nanoseconds
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType TotalDuration() const
	//-----------------------------------------------------------------------------
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return TotalDuration<Duration>();
		}
		else
	#endif
		{
			return std::chrono::duration_cast<DurationType>(TotalDuration<Duration>());
		}
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed total nanoseconds of all intervals
	std::chrono::nanoseconds::rep nanoseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed total microseconds of all intervals
	std::chrono::microseconds::rep microseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed total milliseconds of all intervals
	std::chrono::milliseconds::rep milliseconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed total seconds of all intervals
	std::chrono::seconds::rep seconds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed total minutes of all intervals
	std::chrono::minutes::rep minutes() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns elapsed total hours of all intervals
	std::chrono::hours::rep hours() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns start iterator
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_Durations.begin();
	}

	//-----------------------------------------------------------------------------
	/// returns end iterator
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_Durations.end();
	}

	//-----------------------------------------------------------------------------
	/// returns count of intervals
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_Durations.size();
	}

	//-----------------------------------------------------------------------------
	/// do we have intervals?
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Durations.empty();
	}

	//-----------------------------------------------------------------------------
	const Duration& operator[](size_type i) const
	//-----------------------------------------------------------------------------
	{
		return m_Durations[i];
	}

	//-----------------------------------------------------------------------------
	Duration& operator[](size_type i)
	//-----------------------------------------------------------------------------
	{
		return m_Durations[i];
	}

	//-----------------------------------------------------------------------------
	size_type Rounds() const
	//-----------------------------------------------------------------------------
	{
		return m_iRounds;
	}

	//-----------------------------------------------------------------------------
	/// add other intervals to this, expands size if necessary
	KDurations operator+(const KDurations& other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// add other intervals to this, expands size if necessary
	self& operator+=(const KDurations& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// substract other intervals from this, expands size if necessary
	KDurations operator-(const KDurations& other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// substract other intervals from this, expands size if necessary
	self& operator-=(const KDurations& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// multiply intervals by an integer
	KDurations operator*(size_type iMultiplier) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// multiply intervals by an integer
	self& operator*=(size_type iMultiplier);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// divide intervals by an integer
	KDurations operator/(size_type iDivisor) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// divide intervals by an integer
	self& operator/=(size_type iDivisor);
	//-----------------------------------------------------------------------------

//------
private:
//------

	Storage   m_Durations;
	size_type m_iRounds { 1 };

}; // KDurations

// gcc does not like these specializations in the class itself..

//-----------------------------------------------------------------------------
/// get duration of an interval in internal duration type
template<>
KDurations::Duration KDurations::GetDuration<KDurations::Duration>(size_type iInterval) const;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// returns total duration in internal duration type
template<>
KDurations::Duration KDurations::TotalDuration<KDurations::Duration>() const;
//-----------------------------------------------------------------------------


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measures multiple consecutive time intervals
class DEKAF2_PUBLIC KStopDurations : public KDurations
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using base = KDurations;

	//-----------------------------------------------------------------------------
	/// reset all intervals, restart clock
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start a new interval, store the current one
	void StartNextInterval();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start a new interval, store the current one
	/// @param iInterval index position to store the current interval at
	void StoreInterval(size_type iInterval);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	KStopTime m_Timer;

}; // KStopDurations

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KTimer can be used to call functions both repeatedly after a fixed
/// time interval or once after expiration of a time interval, or at
/// a fixed time point. Granularity is low (1 second), it is intended for
/// cleanup purposes or similar repetitive tasks, not high precision
/// timing.
class DEKAF2_PUBLIC KTimer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ID_t          = size_t;
	using Clock         = std::chrono::system_clock;
	using Interval      = Clock::duration;
	using Timepoint     = Clock::time_point;
	using Callback      = std::function<void(Timepoint)>;
	using CallbackTimeT = std::function<void(time_t)>;

	static constexpr ID_t INVALID { 0 };

	//---------------------------------------------------------------------------
	KTimer(Interval Granularity = std::chrono::seconds(1));
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(KTimer&&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	~KTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(KTimer&&) = delete;
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
		if DEKAF2_CONSTEXPR_IF(std::is_same<SrcClock, DstClock>::value)
		{
			return tp;
		}
		else
		{
			const auto src_before = SrcClock::now();
			const auto dst_now    = DstClock::now();
			const auto src_after  = SrcClock::now();
			const auto src_diff   = src_after - src_before;
			const auto src_now    = src_before + src_diff / 2;
			return dst_now + (tp - src_now);
		}
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
		if DEKAF2_CONSTEXPR_IF(std::is_same<SrcClock, DstClock>::value)
		{
			return tp;
		}
		else
		{
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
				{
					break;
				}
			}
			while (epsilon > tolerance);
			return dst_now + (tp - src_now);
		}
	}

	//---------------------------------------------------------------------------
	/// If called, the KTimer object will wait for its timer thread to terminate before leaving the constructor
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
		return DurationT { std::numeric_limits<ReprT>::max() };
	}

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template <typename DurationT>
	/// helper function for precision clock conversion
	static constexpr DurationT abs_duration(const DurationT d) noexcept
	//---------------------------------------------------------------------------
	{
		return DurationT { (d.count() < 0) ? -d.count() : d.count() };
	}

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void TimingLoop(Interval Granularity);
	//---------------------------------------------------------------------------

	struct Timer;

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	ID_t AddTimer(Timer timer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static void StartCallback();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static ID_t GetNextID();
	//---------------------------------------------------------------------------

	enum FLAGS
	{
		NONE    = 0,
		ONCE    = 1 << 0, // this timer shall be run only once
		TIMET   = 1 << 1, // this timer shall be called with a time_t argument
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Timer
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		ID_t          ID    { INVALID          };
		Timepoint     ExpiresAt;
		Interval      IVal  { Interval::zero() };
		Callback      CB    { nullptr          };
		CallbackTimeT CBT   { nullptr          };
		uint8_t       Flags { NONE             };
	};

	std::shared_ptr<std::thread>      m_tTiming;
	std::shared_ptr<std::atomic_bool> m_bShutdown;
	bool                              m_bDestructWithJoin { false };

	using map_t = std::unordered_map<ID_t, Timer>;
	KThreadSafe<map_t>                m_Timers;

}; // KTimer

} // of namespace dekaf2
