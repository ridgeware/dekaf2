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

#include "kdefinitions.h"
#include "kdate.h"
#include "kstring.h"
#include <vector>
#include <type_traits>

/// @file kduration.h
/// keep and measure durations

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a duration type that constructs always initialized, converts properly from and to time_t,
/// and offers quick accessors to various duration casts
class DEKAF2_PUBLIC KDuration : public chrono::nanoseconds
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Duration  = chrono::nanoseconds;

	// chrono::duration is not default initialized - make sure it is 0
	constexpr KDuration()                      noexcept : Duration(Duration::zero()) {}
	constexpr KDuration(const Duration& other) noexcept : Duration(other) {}
	constexpr KDuration(Duration&& other)      noexcept : Duration(std::move(other)) {}

	using Duration::Duration;

	static constexpr KDuration zero() { return KDuration(Duration::zero()); }
	static constexpr KDuration min()  { return KDuration(Duration::min() ); }
	static constexpr KDuration max()  { return KDuration(Duration::max() ); }

	/// construct from a time_t, considering the duration counted as seconds
	template<typename T, typename std::enable_if<std::is_same<time_t, T>::value, int>::type = 0>
	constexpr KDuration(T tSeconds) : Duration(chrono::seconds(tSeconds)) {}

	/// reset the duration to zero
	constexpr void                           clear()                { *this = Duration::zero(); }

	/// returns duration (converted into any duration type, per default nanoseconds)
	template<typename DurationType = chrono::nanoseconds>
	constexpr DurationType                   duration() const
	{
	#ifdef DEKAF2_HAS_CONSTEXPR_IF
		if DEKAF2_CONSTEXPR_IF(std::is_same<DurationType, Duration>::value)
		{
			return *this;
		}
		else
	#endif
		{
			return chrono::duration_cast<DurationType>(*this);
		}
	}

	/// we convert automatically into time_t durations, which are counted in seconds..
	template<typename TimeT,
			 typename std::enable_if<std::is_same<std::time_t, TimeT>::value, int>::type = 0>
	constexpr operator                  TimeT()         const { return seconds().count(); }

	/// returns elapsed nanoseconds as duration type
	DEKAF2_NODISCARD
	constexpr chrono::nanoseconds       nanoseconds()  const { return duration<chrono::nanoseconds>();  }
	/// returns elapsed microseconds as duration type
	DEKAF2_NODISCARD
	constexpr chrono::microseconds      microseconds() const { return duration<chrono::microseconds>(); }
	/// returns elapsed milliseconds as duration type
	DEKAF2_NODISCARD
	constexpr chrono::milliseconds      milliseconds() const { return duration<chrono::milliseconds>(); }
	/// returns elapsed seconds as duration type
	DEKAF2_NODISCARD
	constexpr chrono::seconds           seconds()      const { return duration<chrono::seconds>();      }
	/// returns elapsed minutes as duration type
	DEKAF2_NODISCARD
	constexpr chrono::minutes           minutes()      const { return duration<chrono::minutes>();      }
	/// returns elapsed hours as duration type
	DEKAF2_NODISCARD
	constexpr chrono::hours             hours()        const { return duration<chrono::hours>();        }
	/// returns elapsed days as duration type
	DEKAF2_NODISCARD
	constexpr chrono::days              days()         const { return duration<chrono::days>();              }
	/// returns elapsed weeks as duration type
	DEKAF2_NODISCARD
	constexpr chrono::weeks             weeks()        const { return duration<chrono::weeks>();             }
	/// returns elapsed months as duration type
	DEKAF2_NODISCARD
	constexpr chrono::months            months()       const { return duration<chrono::months>();            }
	/// returns elapsed years as duration type
	DEKAF2_NODISCARD
	constexpr chrono::years             years()        const { return duration<chrono::years>();             }

	/// output format for ToString()
	enum Format
	{
		Smart = 0, ///< human readable, auto adapting to value
		Long,      ///< verbose ("1 yr, 2 wks, 3 days, 6 hrs, 23 min, 10 sec")
		Brief      ///< brief, auto adapting to value, same size for all ("23.2 ms", "421.3 us")
	};

	/// minimum interval to use for ToString()
	enum BaseInterval
	{
		NanoSeconds = 0,
		MilliSeconds,
		MicroSeconds,
		Seconds,
		Minutes,
		Hours,
		Days,
		Weeks,
		Years
	};

#if DEKAF2_HAS_NANOSECONDS_SYS_CLOCK
	static constexpr BaseInterval DefaultBaseInterval = BaseInterval::NanoSeconds;
#else
	static constexpr BaseInterval DefaultBaseInterval = BaseInterval::MicroSeconds;
#endif

	/// returns formatted string with duration
	/// @param format one of Smart, Long, Brief, default Smart
	/// @param Interval minimum interval (resolution)
	/// @param iPrecision floating point precision to use for Brief format
	DEKAF2_NODISCARD
	KString ToString(Format       Format     = Format::Smart,
					 BaseInterval Interval   = BaseInterval::NanoSeconds,
					 uint8_t      iPrecision = 1) const;

}; // KDuration

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Measure the time between two or more events, continuously
class DEKAF2_PUBLIC KStopTime
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Clock    = chrono::steady_clock;

	/// tag to force construction without starting the timer
	static struct ConstructHalted {} Halted;

	/// constructs and starts counting
	KStopTime() : m_Start(Clock::now()) {}
	/// constructs without starting - use clear() to start counting
	explicit KStopTime(ConstructHalted) {}

	/// resets start time to now
	void clear() { m_Start = Clock::now(); }
	/// returns elapsed time as KDuration
	DEKAF2_NODISCARD
	KDuration elapsed(Clock::time_point tNow = Clock::now()) const { return tNow - m_Start; }
	/// returns elapsed time and resets start time after readout
	DEKAF2_NODISCARD
	KDuration elapsedAndClear()
	{
		auto tNow = Clock::now();
		auto tDuration = elapsed(tNow);
		m_Start = tNow;
		return tDuration;
	}

	/// returns elapsed nanoseconds as duration type
	DEKAF2_NODISCARD
	chrono::nanoseconds  nanoseconds()  const { return elapsed().nanoseconds();  }
	/// returns elapsed microseconds as duration type
	DEKAF2_NODISCARD
	chrono::microseconds microseconds() const { return elapsed().microseconds(); }
	/// returns elapsed milliseconds as duration type
	DEKAF2_NODISCARD
	chrono::milliseconds milliseconds() const { return elapsed().milliseconds(); }
	/// returns elapsed seconds as duration type
	DEKAF2_NODISCARD
	chrono::seconds      seconds()      const { return elapsed().seconds();      }
	/// returns elapsed minutes as duration type
	DEKAF2_NODISCARD
	chrono::minutes      minutes()      const { return elapsed().minutes();      }
	/// returns elapsed hours as duration type
	DEKAF2_NODISCARD
	chrono::hours        hours()        const { return elapsed().hours();        }
	/// returns elapsed days as duration type
	DEKAF2_NODISCARD
	chrono::days         days()         const { return elapsed().days();         }
	/// returns elapsed weeks as duration type
	DEKAF2_NODISCARD
	chrono::weeks        weeks()        const { return elapsed().weeks();        }
	/// returns elapsed months as duration type
	DEKAF2_NODISCARD
	chrono::months       months()       const { return elapsed().months();       }
	/// returns elapsed weeks as duration type
	DEKAF2_NODISCARD
	chrono::years        years()        const { return elapsed().years();        }

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

	/// constructs and starts counting
	KStopWatch() = default;
	/// constructs without starting
	explicit KStopWatch(ConstructHalted) : base(base::Halted), m_bIsHalted(true) {}

	/// returns elapsed (active) time
	DEKAF2_NODISCARD
	Duration elapsed() const { if (!m_bIsHalted) { return base::elapsed() + m_iDurationSoFar; } else { return m_iDurationSoFar; } }
	/// halts elapsed time counting
	void     halt()          { m_iDurationSoFar += base::elapsed(); m_bIsHalted = true;    }
	/// resumes elapsed time counting
	void     resume()        { if (m_bIsHalted) { m_bIsHalted = false; base::clear(); }    }
	/// resets elapsed time, stops counter - call resume() to continue
	void     clear()         { m_iDurationSoFar = Duration::zero(); m_bIsHalted = true;    }

	/// returns elapsed nanoseconds as duration type
	DEKAF2_NODISCARD
	chrono::nanoseconds  nanoseconds()  const { return elapsed().nanoseconds();  }
	/// returns elapsed microseconds as duration type
	DEKAF2_NODISCARD
	chrono::microseconds microseconds() const { return elapsed().microseconds(); }
	/// returns elapsed milliseconds as duration type
	DEKAF2_NODISCARD
	chrono::milliseconds milliseconds() const { return elapsed().milliseconds(); }
	/// returns elapsed seconds as duration type
	DEKAF2_NODISCARD
	chrono::seconds      seconds()      const { return elapsed().seconds();      }
	/// returns elapsed minutes as duration type
	DEKAF2_NODISCARD
	chrono::minutes      minutes()      const { return elapsed().minutes();      }
	/// returns elapsed hours as duration type
	DEKAF2_NODISCARD
	chrono::hours        hours()        const { return elapsed().hours();        }
	/// returns elapsed days as duration type
	DEKAF2_NODISCARD
	chrono::days         days()         const { return elapsed().days();         }
	/// returns elapsed weeks as duration type
	DEKAF2_NODISCARD
	chrono::weeks        weeks()        const { return elapsed().weeks();        }
	/// returns elapsed months as duration type
	DEKAF2_NODISCARD
	chrono::months       months()       const { return elapsed().months();       }
	/// returns elapsed weeks as duration type
	DEKAF2_NODISCARD
	chrono::years        years()        const { return elapsed().years();        }

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

	using self      = KMultiDuration;
	using Duration  = KDuration;
	using size_type = std::size_t;

	KMultiDuration() = default;
	KMultiDuration(Duration duration) : KDuration(duration), m_iRounds(1) {}

	static KMultiDuration zero() { return KMultiDuration(); }

	/// reset
	void           clear();
	/// push back another interval
	void           push_back(Duration duration);
	/// returns the average duration, that is, the total duration time divided by the count of durations
	DEKAF2_NODISCARD
	Duration       average()  const { return m_iRounds ? Duration(duration() / m_iRounds) : Duration(); }
	/// returns count of stored durations
	DEKAF2_NODISCARD
	size_type      Rounds()   const { return m_iRounds;     }
	/// do we have any interval?
	DEKAF2_NODISCARD
	bool           empty()    const { return Rounds() == 0; }
	/// add another duration to this
	KMultiDuration operator+ (const KMultiDuration& other) const;
	/// add another duration to this
	self&          operator+=(const KMultiDuration& other);
	/// subtract another duration from this
	KMultiDuration operator- (const KMultiDuration& other) const;
	/// subtract another duration from this
	self&          operator-=(const KMultiDuration& other);
	/// multiply interval by an integer
	KMultiDuration operator* (size_type iMultiplier)       const;
	/// multiply interval by an integer
	self&          operator*=(size_type iMultiplier);
	/// divide interval by an integer
	KMultiDuration operator/ (size_type iDivisor)          const;
	/// divide interval by an integer
	self&          operator/=(size_type iDivisor);

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

	/// reset
	void             clear();
	/// start a new interval
	void             Start()              { m_Timer.clear(); }
	///stop and store the interval, and return last interval
	base::Duration   Stop();
	/// returns the KStopTime object for the current round (read access only)
	DEKAF2_NODISCARD
	const KStopTime& CurrentRound() const { return m_Timer;  }

//------
private:
//------

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

	/// reset all intervals
	void            clear();
	/// reserve intervals
	void            reserve(size_type iSize)     { m_Durations.reserve(iSize);      }
	/// resize for intervals
	void            resize(size_type iSize)      { m_Durations.resize(iSize);       }
	/// push back another interval
	void            push_back(Duration duration) { m_Durations.push_back(duration); }
	/// get last interval
	DEKAF2_NODISCARD
	const Duration& back()                 const { return m_Durations.back();       }
	/// get duration of an interval
	/// @param iInterval 0 based index on intervals, returns zero duration if out of bounds
	DEKAF2_NODISCARD
	DurationBase    duration(size_type iInterval) const;
	/// returns total duration of all durations
	DEKAF2_NODISCARD
	DurationBase    duration()    const;
	/// returns start iterator
	DEKAF2_NODISCARD
	const_iterator  begin()       const { return m_Durations.begin(); }
	/// returns end iterator
	DEKAF2_NODISCARD
	const_iterator  end()         const { return m_Durations.end();   }
	/// returns count of intervals
	DEKAF2_NODISCARD
	size_type       size()        const { return m_Durations.size();  }
	/// do we have intervals?
	DEKAF2_NODISCARD
	bool            empty()       const { return m_Durations.empty(); }

	DEKAF2_NODISCARD
	size_type       TotalRounds() const;
	DEKAF2_NODISCARD
	size_type       Rounds(size_type i) const { return operator[](i).Rounds(); }

	DEKAF2_NODISCARD
	const Duration& operator[](size_type iInterval) const;
	DEKAF2_NODISCARD
	Duration&       operator[](size_type iInterval);

	/// add other intervals to this, expands size if necessary
	KDurations      operator+(const KDurations& other) const;
	/// add other intervals to this, expands size if necessary
	self&           operator+=(const KDurations& other);
	/// substract other intervals from this, expands size if necessary
	KDurations      operator-(const KDurations& other) const;
	/// substract other intervals from this, expands size if necessary
	self&           operator-=(const KDurations& other);
	/// multiply intervals by an integer
	KDurations      operator*(size_type iMultiplier) const;
	/// multiply intervals by an integer
	self&           operator*=(size_type iMultiplier);
	/// divide intervals by an integer
	KDurations      operator/(size_type iDivisor) const;
	/// divide intervals by an integer
	self&           operator/=(size_type iDivisor);

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

	/// reset all intervals, restart clock
	void               clear();
	/// start a new interval, store the current one
	void               StartNextInterval();
	/// start a new interval, store the current one
	/// @param iInterval index position to store the current interval at
	base::DurationBase StoreInterval(size_type iInterval);
	/// returns the KStopTime object for the current round (read access only)
	DEKAF2_NODISCARD
	const KStopTime&   CurrentRound() const { return m_Timer; }

//----------
private:
//----------

	KStopTime m_Timer;

}; // KStopDurations

DEKAF2_NAMESPACE_END

#if DEKAF2_HAS_INCLUDE("kformat.h")

// kFormat formatters

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KDuration> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDuration& Duration, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Duration.ToString(DEKAF2_PREFIX KDuration::Format::Brief, DEKAF2_PREFIX KDuration::DefaultBaseInterval), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // of has #include "kformat.h"
