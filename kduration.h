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

#include "bits/kcppcompat.h"
#include <chrono>
#include <vector>
#include <type_traits>

/// @file kduration.h
/// keep and measure durations

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KDuration
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Clock     = std::chrono::steady_clock;
	using Duration  = Clock::duration;
	using size_type = std::size_t;
	using self      = KDuration;

	static KDuration zero() { return KDuration(Duration::zero()); }
	static KDuration min()  { return KDuration(Duration::min() ); }
	static KDuration max()  { return KDuration(Duration::max() ); }

	KDuration() = default;
	template<typename T,
	         typename std::enable_if<std::is_assignable<Duration, T>::value, int>::type = 0>
	KDuration(T D) : m_duration(D) {}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_duration = Duration::zero();
	}

	//-----------------------------------------------------------------------------
	/// returns duration (converted into any duration type, per default nanoseconds)
	template<typename DurationType = std::chrono::nanoseconds>
	DurationType duration() const
	//-----------------------------------------------------------------------------
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return m_duration;
		}
		else
	#endif
		{
			return std::chrono::duration_cast<DurationType>(m_duration);
		}

	} // duration

	//-----------------------------------------------------------------------------
	operator Duration()
	//-----------------------------------------------------------------------------
	{
		return m_duration;
	}

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
	/// add another duration to this
	KDuration operator+(const KDuration& other) const
	//-----------------------------------------------------------------------------
	{
		KDuration temp(*this);
		return temp += other;
	}

	//-----------------------------------------------------------------------------
	/// add another duration to this
	self& operator+=(const KDuration& other)
	//-----------------------------------------------------------------------------
	{
		m_duration += other.m_duration;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// subtract another duration from this
	KDuration operator-(const KDuration& other) const
	//-----------------------------------------------------------------------------
	{
		KDuration temp(*this);
		return temp -= other;
	}

	//-----------------------------------------------------------------------------
	/// subtract another duration from this
	self& operator-=(const KDuration& other)
	//-----------------------------------------------------------------------------
	{
		m_duration -= other.m_duration;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// multiply interval by an integer
	self& operator*=(size_type iMultiplier)
	//-----------------------------------------------------------------------------
	{
		m_duration *= iMultiplier;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// divide interval by an integer
	KDuration operator/(size_type iDivisor) const
	//-----------------------------------------------------------------------------
	{
		KDuration temp(*this);
		return temp /= iDivisor;
	}

	//-----------------------------------------------------------------------------
	/// divide interval by an integer
	self& operator/=(size_type iDivisor)
	//-----------------------------------------------------------------------------
	{
		m_duration /= iDivisor;
		return *this;
	}

	friend bool operator==(const KDuration& left, const KDuration& right);
	friend bool operator <(const KDuration& left, const KDuration& right);


//----------
private:
//----------

	Duration m_duration { Duration::zero() };

}; // KDuration

inline bool operator==(const KDuration& left, const KDuration& right)
{
	return left.m_duration == right.m_duration;
}

inline bool operator<(const KDuration& left, const KDuration& right)
{
	return left.m_duration < right.m_duration;
}

DEKAF2_COMPARISON_OPERATORS(KDuration)

inline KDuration operator*(KDuration D, KDuration::size_type iMultiplier)
{
	return D *= iMultiplier;
}

inline KDuration operator*(KDuration::size_type iMultiplier, KDuration D)
{
	return D *= iMultiplier;
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measure the time between two or more events, continuously
class DEKAF2_PUBLIC KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Clock    = KDuration::Clock;

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
	/// returns elapsed time as KDuration
	KDuration elapsed(Clock::time_point tNow = Clock::now()) const
	//-----------------------------------------------------------------------------
	{
		return tNow - m_Start;
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed time and resets start time after readout
	KDuration elapsedAndClear()
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

	//-----------------------------------------------------------------------------
	/// returns elapsed nanoseconds
	std::chrono::nanoseconds::rep nanoseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().nanoseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed microseconds
	std::chrono::microseconds::rep microseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().microseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed milliseconds
	std::chrono::milliseconds::rep milliseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().milliseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed seconds
	std::chrono::seconds::rep seconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().seconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed minutes
	std::chrono::minutes::rep minutes() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().minutes();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed hours
	std::chrono::hours::rep hours() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().hours();
	}

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

	using base     = KStopTime;
	using Duration = KDuration;

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
	/// returns elapsed (active) time
	Duration elapsed() const
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

	} // elapsed

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

	//-----------------------------------------------------------------------------
	/// returns elapsed nanoseconds
	std::chrono::nanoseconds::rep nanoseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().nanoseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed microseconds
	std::chrono::microseconds::rep microseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().microseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed milliseconds
	std::chrono::milliseconds::rep milliseconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().milliseconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed seconds
	std::chrono::seconds::rep seconds() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().seconds();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed minutes
	std::chrono::minutes::rep minutes() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().minutes();
	}

	//-----------------------------------------------------------------------------
	/// returns elapsed hours
	std::chrono::hours::rep hours() const
	//-----------------------------------------------------------------------------
	{
		return elapsed().hours();
	}

//----------
private:
//----------

	Duration m_iDurationSoFar { Duration::zero() };
	bool m_bIsHalted { false };

}; // KStopWatch


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps multiple durations
class DEKAF2_PUBLIC KMultiDuration : public KDuration
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self     = KMultiDuration;
	using Duration = KDuration;

	KMultiDuration() = default;
	KMultiDuration(Duration duration)
	: KDuration(duration)
	, m_iRounds(1)
	{}

	static KMultiDuration zero() { return KMultiDuration(); }

	//-----------------------------------------------------------------------------
	/// reset
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// push back another interval
	void push_back(Duration duration);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the average duration, that is, the total duration time divided by the count of durations
	Duration average()
	//-----------------------------------------------------------------------------
	{
		return m_iRounds ? Duration(duration() / m_iRounds) : Duration();
	}

	//-----------------------------------------------------------------------------
	/// returns count of stored durations
	size_type Rounds() const
	//-----------------------------------------------------------------------------
	{
		return m_iRounds;
	}

	//-----------------------------------------------------------------------------
	/// do we have any interval?
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return Rounds() == 0;
	}

	//-----------------------------------------------------------------------------
	/// add another duration to this
	KMultiDuration operator+(const KMultiDuration& other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// add another duration to this
	self& operator+=(const KMultiDuration& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// subtract another duration from this
	KMultiDuration operator-(const KMultiDuration& other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// subtract another duration from this
	self& operator-=(const KMultiDuration& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// multiply interval by an integer
	KMultiDuration operator*(size_type iMultiplier) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// multiply interval by an integer
	self& operator*=(size_type iMultiplier);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// divide interval by an integer
	KMultiDuration operator/(size_type iDivisor) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// divide interval by an integer
	self& operator/=(size_type iDivisor);
	//-----------------------------------------------------------------------------

//------
private:
//------

	// has m_duration from KDuration
	size_type m_iRounds  { 0 };

}; // KMultiDuration

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measures multiple consecutive time intervals
class DEKAF2_PUBLIC KStopDuration : public KMultiDuration
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using base = KMultiDuration;

	//-----------------------------------------------------------------------------
	/// reset
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start a new interval
	void Start()
	//-----------------------------------------------------------------------------
	{
		m_Timer.clear();
	}

	//-----------------------------------------------------------------------------
	///stop and store the interval
	void Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the KStopTime object for the current round (read access only)
	const KStopTime& CurrentRound() const
	//-----------------------------------------------------------------------------
	{
		return m_Timer;
	}

//----------
private:
//----------

	KStopTime m_Timer;

}; // KStopDuration


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps multiple consecutive time intervals
class DEKAF2_PUBLIC KDurations
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self           = KDurations;
	using Duration       = KMultiDuration;
	using DurationBase   = KMultiDuration::Duration;
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
	/// get duration of an interval
	/// @param iInterval 0 based index on intervals, returns zero duration if out of bounds
	DurationBase duration(size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_type TotalRounds() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns total duration of all durations
	DurationBase duration() const;
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
	const Duration& operator[](size_type iInterval) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	Duration& operator[](size_type iInterval);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_type Rounds(size_type i) const
	//-----------------------------------------------------------------------------
	{
		return operator[](i).Rounds();
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

}; // KDurations


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

	//-----------------------------------------------------------------------------
	/// returns the KStopTime object for the current round (read access only)
	const KStopTime& CurrentRound() const
	//-----------------------------------------------------------------------------
	{
		return m_Timer;
	}

//----------
private:
//----------

	KStopTime m_Timer;

}; // KStopDurations

} // of namespace dekaf2
