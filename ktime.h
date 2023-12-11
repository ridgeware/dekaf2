/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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
 */

#pragma once

/// @file ktime.h
/// cross platform date and time utilities

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kduration.h"
#include "ktemplate.h"
#include "kdate.h"
#include <cinttypes>
#include <ctime>
#include <chrono>
#include <set>
#include <array>

DEKAF2_NAMESPACE_BEGIN

/// returns the timezone of the local timezone for this process
DEKAF2_PUBLIC inline const chrono::time_zone* kGetLocalTimezone()                  { return chrono::current_zone();         }

/// returns the timezone name of the local timezone for this process
DEKAF2_PUBLIC inline KString kGetLocalTimezoneName()                               { return kGetLocalTimezone()->name();    }

/// returns a sorted vector with all time zone names
DEKAF2_PUBLIC std::vector<KString> kGetListOfTimezoneNames();

/// returns a time zone that matches the sZoneName, or throws std::runtime_error
DEKAF2_PUBLIC inline const chrono::time_zone* kFindTimezone(KStringView sZoneName) { return chrono::locate_zone(sZoneName); }

// We deliberately do not offer a method to set the local timezone
// as it would only have effect on C time utilities, not on the C++
// variants. Use timezones passed in to the C++ functions instead
// of switching the process globally.

/// Converts any UTC timepoint to a local zoned time (at least exact for the past, not necessarily for the future)
/// Never use local times to do arithmetics: DST changes, leap seconds, and history will all bite you!
/// A zoned time is a local timepoint with all information about the local timezone it is contained in, at its very point in time, but not at any other point in time
template<class Duration>
DEKAF2_PUBLIC auto kToLocalZone(chrono::sys_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::zoned_time<Duration> { return chrono::zoned_time<Duration>(zone, tp); }

/// Converts any UTC timepoint to a local zoned time (at least exact for the past, not necessarily for the future)
/// Never use local times to do arithmetics: DST changes, leap seconds, and history will all bite you!
/// A zoned time is a local timepoint with all information about the local timezone it is contained in, at its very point in time, but not at any other point in time
template<class Duration>
DEKAF2_PUBLIC auto kToLocalZone(chrono::sys_time<Duration> tp, KStringView sZone) -> chrono::zoned_time<Duration> { return chrono::zoned_time<Duration>(sZone, tp); }

/// Converts any UTC timepoint to a local timepoint (at least exact for the past, not necessarily for the future)
/// Never use local timepoints to do arithmetics: DST changes, leap seconds, and history will all bite you!
/// A local timepoint is only valid at its exact point in time.
template<class Duration>
DEKAF2_PUBLIC auto kToLocalTime(chrono::sys_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::local_time<typename std::common_type<Duration, chrono::seconds>::type> { return zone->to_local(tp); }

/// converts any local timepoint to a UTC timepoint (at least exact for the past, not necessarily for the future)
template<class Duration>
DEKAF2_PUBLIC auto kFromLocalTime(chrono::zoned_time<Duration> ztp) -> chrono::sys_time<typename std::common_type<Duration, chrono::seconds>::type> { return ztp.get_sys_time(); }

/// converts any local timepoint to a UTC timepoint (at least exact for the past, not necessarily for the future)
template<class Duration>
DEKAF2_PUBLIC auto kFromLocalTime(chrono::local_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::sys_time<typename std::common_type<Duration, chrono::seconds>::type> { return zone->to_sys(tp); }

class KLocalTime;
class KUTCTime;

namespace detail {

// we need a constructor in KUnixTime for this because otherwise gcc 8 complains about conversion ambiguities
class KParsedTimestampBase;

#if DEKAF2_USE_TIME_PUT
constexpr KStringView fDefaultDateTime { "%Y-%m-%d %H:%M:%S" };
#else
constexpr KStringView fDefaultDateTime { "{:%Y-%m-%d %H:%M:%S}" };
#endif

} // of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class is a modern version of the legacy time point usage of time_t -
/// in contrast to the legacy duration usage of time_t, which should be done with KDuration.
/// It uses high resolution (clang libc++: microseconds, gnu libstdc++: nanoseconds), but
/// is as small as a std::time_t (long long).
/// It publicly inherits from chrono::system_clock::time_point, and adds constexpr conversions
/// to and from std::time_t and std::tm.
/// Please note that due to the higher resolution, the covered date range is much lower than the
/// one of std::time_t:
/// std::time_t:      -292,471,206,707 years to 292,471,210,647 years
/// clang::KUnixTime:  -28164-12-21 04:00:54 to 32103-01-10 04:00:54
/// gcc::KUnixTime:      1677-09-21 23:47:16 to 2262-04-11 23:47:16
class DEKAF2_PUBLIC KUnixTime : public chrono::system_clock::time_point
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self       = KUnixTime;
	using base       = chrono::system_clock::time_point;
	using time_point = chrono::system_clock::time_point;
	using clock      = chrono::system_clock;
	
	                             KUnixTime() = default;
	DEKAF2_CONSTEXPR_14          KUnixTime(const base& other) noexcept : base(other) {}
	DEKAF2_CONSTEXPR_14          KUnixTime(base&& other)      noexcept : base(std::move(other)) {}

	/// construct from time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14          KUnixTime(time_t time)       noexcept : KUnixTime(from_time_t(time)) {}
	/// construct from std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 explicit KUnixTime(const std::tm& tm) noexcept : KUnixTime(    from_tm(tm)  ) {}
	// we need this constructor because otherwise gcc 8 complains about conversion ambiguities
	                    explicit KUnixTime (const detail::KParsedTimestampBase& parsed) noexcept;
	/// construct from KUTCTime timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 explicit KUnixTime(const KUTCTime& local) noexcept;
	/// construct from KLocalTime timepoint (constexpr)
	                    explicit KUnixTime(const KLocalTime& local) noexcept;
	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 130 common patterns
	                    explicit KUnixTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                             KUnixTime (KStringView sFormat, KStringView sTimestamp);

	using base::base;

	/// converts implicitly to time_t timepoint, although it loses precision..
	DEKAF2_CONSTEXPR_14 explicit operator std::time_t()       const noexcept { return to_time_t(*this);                                      }

	DEKAF2_FULL_CONSTEXPR_17 self& operator+=(std::time_t seconds)  noexcept { base::operator += (chrono::seconds(seconds)); return *this;   }
	DEKAF2_FULL_CONSTEXPR_17 self& operator-=(std::time_t seconds)  noexcept { base::operator -= (chrono::seconds(seconds)); return *this;   }

	using base::operator+=;
	using base::operator-=;

	/// converts to std::time_t timepoint
	DEKAF2_CONSTEXPR_14 std::time_t to_time_t ()              const noexcept { return to_time_t(*this);                                      }
	/// converts to std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 std::tm     to_tm     ()              const noexcept { return to_tm(*this);                                          }
	/// converts to string
	                    KString     to_string (KStringView sFormat = detail::fDefaultDateTime) const noexcept;

	DEKAF2_CONSTEXPR_14 bool        ok        ()              const noexcept { return time_since_epoch() != duration::zero();                }

	/// converts from KUnixTime to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(KUnixTime tp)  noexcept { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from system_clock timepoint to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(time_point tp) noexcept { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from KUnixTime to std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::tm     to_tm(KUnixTime tp)      noexcept;
	/// converts from system_clock timepoint to std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::tm     to_tm(time_point tp)     noexcept { return to_tm(KUnixTime(tp)); }

	/// converts from time_t timepoint to system_clock timeppoint (constexpr)
	DEKAF2_CONSTEXPR_14 static KUnixTime   from_time_t(std::time_t tTime) noexcept { return KUnixTime(chrono::seconds(tTime)); }
	/// converts from std::tm timepoint to system_clock timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static KUnixTime   from_tm    (const std::tm& tm) noexcept;

	/// returns a KUnixTime with the current time
	                    static KUnixTime   now()                             { return clock::now(); }

//--------
private:
//--------

	// we do not permit addition or subtraction of years and months
	// - they will always lead to unexpected results
	constexpr void operator+=(chrono::months) noexcept {}
	constexpr void operator+=(chrono::years ) noexcept {}
	constexpr void operator-=(chrono::months) noexcept {}
	constexpr void operator-=(chrono::years ) noexcept {}

}; // KUnixTime

DEKAF2_CONSTEXPR_14 KDuration operator-(const KUnixTime& left, const KUnixTime& right)
{ return KUnixTime::base(left) - KUnixTime::base(right); }

//-----------------------------------------------------------------------------
// constexpr implementation of std::tm to std::time_t conversion
DEKAF2_CONSTEXPR_14 KUnixTime KUnixTime::from_tm(const std::tm& tm) noexcept
//-----------------------------------------------------------------------------
{
	return chrono::sys_days(chrono::day(tm.tm_mday) / chrono::month(tm.tm_mon + 1) / chrono::year(tm.tm_year + 1900))
	     + chrono::hours(tm.tm_hour)
	     + chrono::minutes(tm.tm_min)
#ifndef DEKAF2_IS_WINDOWS
	     - chrono::seconds(tm.tm_gmtoff)
#endif
	     + chrono::seconds(tm.tm_sec);
}

//-----------------------------------------------------------------------------
// constexpr implementation of std::time_t to std::tm conversion for UTC timepoints
DEKAF2_CONSTEXPR_14 std::tm KUnixTime::to_tm(KUnixTime tp) noexcept
//-----------------------------------------------------------------------------
{
	// we do not know all fields of std::tm, therefore let's default initialize it
	std::tm tm{};

	// break up
	auto dp      = chrono::floor<chrono::days>(tp);
	auto ymd     = chrono::year_month_day(dp);
	auto time    = chrono::make_time(tp.time_since_epoch() % chrono::hours(24)); // the modulo operation prevents an overflow around the min value of the time point
	// assign
	tm.tm_sec    = static_cast<int>(time.seconds().count());
	tm.tm_min    = static_cast<int>(time.minutes().count());
	tm.tm_hour   = static_cast<int>(time.hours().count());
	tm.tm_mday   = static_cast<int>(unsigned(ymd.day()));
	tm.tm_mon    = static_cast<int>(unsigned(ymd.month())) - 1;
	tm.tm_year   = static_cast<int>(ymd.year()) - 1900;
	tm.tm_wday   = chrono::weekday(dp).c_encoding();
	tm.tm_yday   = (dp - chrono::sys_days(chrono::year_month_day(ymd.year()/1/1))).count();
	// tm_isdst is defaulted to 0
#ifndef DEKAF2_IS_WINDOWS
	// tm_gmtoff is defaulted to 0
	tm.tm_zone   = const_cast<char*>("UTC");
#endif
	return tm;

} // KUnixTime::to_tm

//-----------------------------------------------------------------------------
constexpr KUnixTime KConstDate::to_unix () const noexcept
//-----------------------------------------------------------------------------
{
	return to_sys_days();

} // KConstDate::to_unix

namespace detail {

// helper for KConstTimeOfDay
constexpr uint64_t calc_power_of_10(unsigned exp) noexcept
{
	uint64_t ret = 1;
	for (unsigned i = 0; i < exp; ++i) ret *= 10U;
	return ret;
}

// helper for KConstTimeOfDay -- cannot be declared in-class, see here:
// https://stackoverflow.com/questions/29551223/static-constexpr-function-called-in-a-constant-expression-is-an-error
// IMHO this is a bug in the C++ standard
constexpr unsigned calc_fractional_width(uint64_t n, uint64_t d = 10, unsigned w = 0) noexcept
{
	if (n >= 2 && d != 0 && w < 19) return 1 + calc_fractional_width(n, d % n * 10, w + 1);
	return 0;
}

// helper for KConstTimeOfDay -- cannot be declared in-class, see here:
// https://stackoverflow.com/questions/29551223/static-constexpr-function-called-in-a-constant-expression-is-an-error
// IMHO this is a bug in the C++ standard
// std::abs() is only constexpr with C++23..
constexpr uint8_t tiny_abs(int8_t i) noexcept
{
	// we will never see an underflow because we modulo the input value far below its maximum/minimum count
	return (i >= 0) ? +i : -i;
}

#if DEKAF2_USE_TIME_PUT
constexpr KStringView fDefaultTime { "%H:%M:%S" };
#else
constexpr KStringView fDefaultTime { "{:%H:%M:%S}" };
#endif

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// constexpr breaking down any duration or timepoint into a const 24h duration with accessors for hours, minutes, seconds, subseconds
class DEKAF2_PUBLIC KConstTimeOfDay
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
private:
//--------

	using Duration = chrono::system_clock::duration;
	using CommonType = typename std::common_type<Duration, chrono::seconds>::type;

//--------
public:
//--------

	static unsigned constexpr fractional_width = detail::calc_fractional_width(CommonType::period::den) < 19 ? detail::calc_fractional_width(CommonType::period::den) : 6u;

	using self       = KConstTimeOfDay;
	using precision  = chrono::duration<typename std::common_type<typename Duration::rep, chrono::seconds::rep>::type, std::ratio<1, detail::calc_power_of_10(fractional_width)>>;

	/// construct default time of day (00:00:00)
	KConstTimeOfDay() = default;
	/// construct time of day from chrono::hours, minutes, seconds, subseconds
	constexpr KConstTimeOfDay(chrono::hours h, chrono::minutes m, chrono::seconds s, precision subseconds = precision(0)) noexcept
	: m_is_neg(false)
	, m_hour  (detail::tiny_abs(h.count() % 24))
	, m_minute(detail::tiny_abs(m.count() % 60))
	, m_second(detail::tiny_abs(s.count() % 60))
	, m_subseconds(subseconds)
	{
	}
	/// construct time of day from unsigned hours, minutes, seconds
	constexpr KConstTimeOfDay(uint8_t h, uint8_t m, uint8_t s) noexcept
	: m_is_neg(false)
	, m_hour  (h % 24)
	, m_minute(m % 60)
	, m_second(s % 60)
	{
	}
	/// construct time of day from a chrono::system_clock::duration (modulo 24 hours to not overflow the hour counter)
	constexpr explicit KConstTimeOfDay(Duration d) noexcept : KConstTimeOfDay(chrono::abs(d % Duration(chrono::hours(24))), d < Duration(0)) {}
	/// construct time of day from a KUnixTime
	constexpr KConstTimeOfDay(KUnixTime timepoint) noexcept : KConstTimeOfDay(timepoint.time_since_epoch()) {}

	/// is the duration negative?
	constexpr bool            is_negative () const noexcept { return m_is_neg;                  }
	/// return hour in 24 hour format
	constexpr chrono::hours   hours       () const noexcept { return chrono::hours(m_hour);     }
	/// return minutes
	constexpr chrono::minutes minutes     () const noexcept { return chrono::minutes(m_minute); }
	/// return seconds
	constexpr chrono::seconds seconds     () const noexcept { return chrono::seconds(m_second); }
	/// return subseconds (either nano- or microseconds, depending on the std::chrono library)
	constexpr precision       subseconds  () const noexcept { return m_subseconds;              }

	/// return the duration into the day
	constexpr precision       to_duration () const noexcept
	{
		auto dur = hours() + minutes() + seconds() + subseconds();
		return is_negative() ? -dur : dur;
	}

	/// return hour in 24 hour format
	constexpr chrono::hours   hours24     () const noexcept { return hours();                   }
	/// return hour in 12 hour format
	constexpr chrono::hours   hours12     () const noexcept { return chrono::make12(hours());   }
	/// returns true if time is pm
	constexpr bool            is_pm       () const noexcept { return chrono::is_pm(hours());    }
	/// returns true if time is am
	constexpr bool            is_am       () const noexcept { return chrono::is_am(hours());    }

	/// return a string following std::format patterns - default = %H:%M:%S
	KString                   Format      (KStringView sFormat = detail::fDefaultTime) const { return to_string(sFormat); }
	/// return a string following std::format patterns - default = %H:%M:%S
	KString                   to_string   (KStringView sFormat = detail::fDefaultTime) const;
	/// convert into a std::tm (date part is obviously zeroed)
	constexpr std::tm         to_tm       () const noexcept;

	// a KConstTimeOfDay is always in the valid range, it does not need a check for bool ok() ..

//--------
private:
//--------

	friend class KTimeOfDay;

	constexpr KConstTimeOfDay(Duration d, bool bIsNeg) noexcept
	: m_is_neg     (bIsNeg)
	, m_hour       (chrono::duration_cast<chrono::hours  >(d).count())
	, m_minute     (chrono::duration_cast<chrono::minutes>(d - hours()).count())
	, m_second     (chrono::duration_cast<chrono::seconds>(d - hours() - minutes()).count())
	, m_subseconds (d - hours() - minutes() - seconds())
	{
	}

	bool      m_is_neg     {false};
	uint8_t   m_hour       {0};
	uint8_t   m_minute     {0};
	uint8_t   m_second     {0};
	precision m_subseconds {0};

}; // KConstTimeOfDay

//-----------------------------------------------------------------------------
constexpr bool operator==(const KConstTimeOfDay& left, const KConstTimeOfDay& right)
//-----------------------------------------------------------------------------
{
	return left.is_negative() == right.is_negative()
	     && left.hours()      == right.hours()
	     && left.minutes()    == right.minutes()
	     && left.seconds()    == right.seconds()
	     && left.subseconds() == right.subseconds();
}

//-----------------------------------------------------------------------------
constexpr bool operator< (const KConstTimeOfDay& left, const KConstTimeOfDay& right)
//-----------------------------------------------------------------------------
{
	return left.to_duration() < right.to_duration();
}

DEKAF2_COMPARISON_OPERATORS(KConstTimeOfDay)

constexpr KDuration operator-(const KConstTimeOfDay& left, const KConstTimeOfDay& right)
{ return left.to_duration() - right.to_duration(); }

//-----------------------------------------------------------------------------
constexpr std::tm KConstTimeOfDay::to_tm () const noexcept
//-----------------------------------------------------------------------------
{
	// we do not know all fields of std::tm, therefore let's default initialize it
	std::tm tm{};

	tm.tm_hour   = static_cast<int>(hours   ().count());
	tm.tm_min    = static_cast<int>(minutes ().count());
	tm.tm_sec    = static_cast<int>(seconds ().count());
#ifndef DEKAF2_IS_WINDOWS
	tm.tm_zone   = const_cast<char*>(""); // cannot be nullptr or format() crashes on %Z
#endif
	return tm;

} // KConstTimeOfDay::to_tm

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// constexpr breaking down any duration or timepoint into a 24h duration with accessors for hours, minutes, seconds, subseconds.
/// The time of day can be modified with accessors hour(), minute(), second()
class DEKAF2_PUBLIC KTimeOfDay : public KConstTimeOfDay
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = KConstTimeOfDay;
	using self = KTimeOfDay;

	                KTimeOfDay() = default;

	constexpr       KTimeOfDay(const base& other) noexcept : base(other) {}
	constexpr       KTimeOfDay(base&& other)      noexcept : base(std::move(other)) {}
	
	using base::base;

	/// set the hour of the day from chrono::hours
	constexpr self& hour       (chrono::hours   h) noexcept { m_hour   = detail::tiny_abs(h.count() % 24); return *this; }
	/// set the minute of the hour from chrono::minutes
	constexpr self& minute     (chrono::minutes m) noexcept { m_minute = detail::tiny_abs(m.count() % 60); return *this; }
	/// set the second of the minute from chrono::seconds
	constexpr self& second     (chrono::seconds s) noexcept { m_second = detail::tiny_abs(s.count() % 60); return *this; }
	/// set the subseconds of the second from KTimeOfDay::precision
	constexpr self& subseconds (precision subsecs) noexcept { m_subseconds = chrono::abs(subsecs);         return *this; }
	// make the base method visible again
	using base::subseconds;
	/// set the hour of the day from unsigned
	constexpr self& hour       (uint8_t h)         noexcept { m_hour   = h % 24; return *this; }
	/// set the minute of the hour from unsigned
	constexpr self& minute     (uint8_t m)         noexcept { m_minute = m % 60; return *this; }
	/// set the second of the minute from unsigned
	constexpr self& second     (uint8_t s)         noexcept { m_second = s % 60; return *this; }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	constexpr self& operator+= (T Duration)        noexcept { return *this = KTimeOfDay(to_duration() + chrono::duration_cast<KTimeOfDay::precision>(Duration)); }
	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	constexpr self& operator-= (T Duration)        noexcept { return operator+=(-Duration);    }

}; // KTimeOfDay

class KLocalTime;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in UTC - you can do arithmetics with it, but it is more expensive
/// than doing arithmetics on a time point (or a KUnixTime)
class DEKAF2_PUBLIC KUTCTime : public KDate, public KTimeOfDay
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KUTCTime;

	                    KUTCTime() = default;

	/// construct from a KDate and a KTimeofDay
	DEKAF2_CONSTEXPR_14 KUTCTime (KDate date, KTimeOfDay time) noexcept
	: KDate      (std::move(date))
	, KTimeOfDay (std::move(time))
	{
	}

	/// construct from a KUnixTime
	DEKAF2_CONSTEXPR_14 KUTCTime (KUnixTime time) noexcept
	: KDate      (chrono::floor<chrono::days>(time))
	, KTimeOfDay (time.time_since_epoch())
	{
	}

	/// construct from a system_clock::time_point
	DEKAF2_CONSTEXPR_14 KUTCTime (chrono::system_clock::time_point time) noexcept
	: KUTCTime(KUnixTime(time))
	{
	}

	/// construct from a struct tm time
	DEKAF2_CONSTEXPR_14 KUTCTime (const std::tm& tm) noexcept
	: KUTCTime(KUnixTime(tm))
	{
	}

	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 130 common patterns
	                    KUTCTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                    KUTCTime (KStringView sFormat, KStringView sTimestamp);
	/// construct from KLocalTime
	           explicit KUTCTime (const KLocalTime& local) noexcept;

	// allowing all base class constructors -> KDate
	using KDate::KDate;
	// allowing all base class constructors -> KTimeOfDay
	using KTimeOfDay::KTimeOfDay;

	/// get the current time as GMT / UTC time
	static KUTCTime now() { return KUTCTime(KUnixTime::now()); }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator+=(T Duration)      { return *this = KUnixTime(to_unix() + chrono::duration_cast<KUnixTime::duration>(Duration)); }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator-=(T Duration)      { return operator+=(-Duration);              }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator+=(T Duration)      { return *this += chrono::seconds(Duration); }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator-=(T Duration)      { return *this -= chrono::seconds(Duration); }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14 self& operator=(T Duration)       { return *this = Duration;                   }

	// prefer the chrono::days/months/years += from KDate
	using KDate::operator+=;
	// prefer the chrono::days/months/years -= from KDate
	using KDate::operator-=;

	/// return struct tm
	DEKAF2_CONSTEXPR_14 std::tm   to_tm     ()  const noexcept { return int_to_tm(); }
	/// return our own KUnixTime (a replacement for the non-typesafe std::time_t)
	DEKAF2_CONSTEXPR_14 KUnixTime to_unix   ()  const noexcept { return join();                   }
	/// return chrono::system_clock::time_point
	DEKAF2_CONSTEXPR_14 chrono::system_clock::time_point to_sys () const noexcept { return chrono::system_clock::time_point(to_unix()); }

	DEKAF2_CONSTEXPR_14          operator  std::tm () const noexcept { return to_tm(); }
	DEKAF2_CONSTEXPR_14 explicit operator  chrono::system_clock::time_point () const { return to_sys(); }

	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString            Format    (KStringView sFormat = detail::fDefaultDateTime) const { return to_string(sFormat); }
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString            to_string (KStringView sFormat = detail::fDefaultDateTime) const;

//--------
private:
//--------

	DEKAF2_CONSTEXPR_14 KUnixTime join() const noexcept
	{
		return !empty() ? to_sys_days()
		                  + hours()
		                  + minutes()
		                  + seconds()
		                  + subseconds()
		                : KUnixTime(0);
	}

	DEKAF2_CONSTEXPR_14 std::tm int_to_tm() const noexcept
	{
		std::tm tm{};
		tm.tm_sec    = static_cast<int>(seconds().count());
		tm.tm_min    = static_cast<int>(minutes().count());
		tm.tm_hour   = static_cast<int>(hours  ().count());
		tm.tm_mday   = days   ().count();
		tm.tm_mon    = months ().count() - 1;
		tm.tm_year   = years  ().count() - 1900;
		tm.tm_wday   = weekday().c_encoding();
		tm.tm_yday   = yearday().count() - 1;
		// tm_isdst is defaulted to 0
#ifndef DEKAF2_IS_WINDOWS
		// tm_gmtoff is defaulted to 0
		tm.tm_zone   = const_cast<char*>("UTC");
#endif
		return tm;
	}

}; // KUTCTime

DEKAF2_WRAPPED_COMPARISON_OPERATORS_ONE_TYPE(KUTCTime, left.to_sys(), right.to_sys())

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in local time (any timezone that this system supports) - you
/// cannot do arithmetics on it, because that is in general invalid on local times (you may step
/// transition points in local time, like DST changes and leap seconds)
class DEKAF2_PUBLIC KLocalTime : private chrono::zoned_time<chrono::system_clock::duration>, public KConstDate, public KConstTimeOfDay
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using zoned_time = chrono::zoned_time<chrono::system_clock::duration>;
	using local_time = chrono::local_time<chrono::system_clock::duration>;

//--------
public:
//--------

	using self = KLocalTime;

	KLocalTime() = default;

	/// construct from a KUnixTime, and translate into a timezone, default timezone is the system's local zone
	KLocalTime (KUnixTime time, const chrono::time_zone* timezone)
	: zoned_time(kToLocalZone(time, timezone))
	, KConstDate(chrono::floor<chrono::days>(get_local_time()))
	, KConstTimeOfDay(local_time(get_local_time()) - chrono::floor<chrono::days>(get_local_time()))
	{
	}

	/// construct from a KUnixTime, and a timezone string
	KLocalTime (KUnixTime time, KStringView sTimezone)
	: zoned_time(kToLocalZone(time, sTimezone))
	, KConstDate(chrono::floor<chrono::days>(get_local_time()))
	, KConstTimeOfDay(local_time(get_local_time()) - chrono::floor<chrono::days>(get_local_time()))
	{
	}

	explicit KLocalTime (KUnixTime time)
	: KLocalTime(time, chrono::current_zone())
	{
	}

	explicit KLocalTime (chrono::system_clock::time_point time)
	: KLocalTime(KUnixTime(time))
	{
	}

	/// construct from a chrono::zoned_time
	KLocalTime (const zoned_time& time) noexcept
	: zoned_time(time)
	, KConstDate(chrono::floor<chrono::days>(get_local_time()))
	, KConstTimeOfDay(local_time(get_local_time()) - chrono::floor<chrono::days>(get_local_time()))
	{
	}

	/// construct from a KConstDate, a KConstTimeofDay, and a timezone, default timezone is the system's local zone
	KLocalTime (KConstDate date, KConstTimeOfDay time, const chrono::time_zone* timezone = chrono::current_zone()) noexcept
	: KLocalTime(zoned_time(timezone, date.to_local_days() + time.to_duration()))
	{
	}

	/// construct from a KConstDate, a KConstTimeofDay, and a timezone string
	KLocalTime (KConstDate date, KConstTimeOfDay time, KStringView sTimezone) noexcept
	: KLocalTime(zoned_time(sTimezone, date.to_local_days() + time.to_duration()))
	{
	}

	/// construct from a string representation, which is interpreted as time in the timezone time (if there is no time zone string indicator telling other)
	/// - the string format is automatically detected from about 130 common patterns
	KLocalTime (KStringView sTimestamp, const chrono::time_zone* timezone = chrono::current_zone());

	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	KLocalTime (KStringView sFormat, KStringView sTimestamp, const chrono::time_zone* timezone = chrono::current_zone());

	/// construct from KUTCTime, and translate into a timezone
	KLocalTime (const KUTCTime& utc, const chrono::time_zone* timezone)
	: KLocalTime(utc.to_unix(), timezone)
	{
	}

	/// construct from KUTCTime, and translate into a timezone
	KLocalTime (const KUTCTime& utc, KStringView sTimezone)
	: KLocalTime(utc.to_unix(), sTimezone)
	{
	}

	/// construct from KLocalTime, and translate into a timezone
	KLocalTime (const KLocalTime& local, const chrono::time_zone* timezone)
	: KLocalTime(local.to_unix(), timezone)
	{
	}

	/// construct from KLocalTime, and translate into a timezone
	KLocalTime (const KLocalTime& local, KStringView sTimezone)
	: KLocalTime(local.to_unix(), sTimezone)
	{
	}

	/// construct from KUTCTime, and translate into the system's local time
	explicit KLocalTime(const KUTCTime& utc)
	: KLocalTime(utc, chrono::current_zone())
	{
	}

	// do not allow base class constructors - they do not make sense for the const KLocalTime

	/// get the current time as local time or any other timezone, default timezone is the system's local zone
	static KLocalTime   now(const chrono::time_zone* timezone = chrono::current_zone()) { return KLocalTime(KUnixTime::now(), timezone); }

	/// return KUnixTime
	KUnixTime           to_unix   ()  const { return to_sys(); }
	/// return struct tm (needed for efficient formatting)
	std::tm             to_tm     ()  const;
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString             Format    (KStringView sFormat = detail::fDefaultDateTime) const { return to_string(sFormat);    }
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString             Format    (const std::locale& locale, KStringView sFormat = detail::fDefaultDateTime) const { return to_string(locale, sFormat); }
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString             to_string (KStringView sFormat = detail::fDefaultDateTime) const;
	/// return a string following std::format patterns, using the given locale - default = %Y-%m-%d %H:%M:%S
	KString             to_string (const std::locale& locale, KStringView sFormat = detail::fDefaultDateTime) const;

	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::system_clock::time_point () const { return to_sys(); }
	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::local_time<chrono::system_clock::duration> () const { return to_local();                 }

	/// returns the chrono::time_zone struct
	const chrono::time_zone*                           get_time_zone     () const { return zoned_time::get_time_zone();  }
	/// returns the chrono::sys_info struct
	chrono::sys_info                                   get_info          () const { return zoned_time::get_info();       }
	/// get offset from UTC
	chrono::seconds                                    get_utc_offset    () const { return get_info().offset;            }
	/// get offset for DST
	chrono::minutes                                    get_dst_offset    () const { return get_info().save;              }
	/// return chrono::local_time
	chrono::local_time<chrono::system_clock::duration> to_local          () const { return zoned_time::get_local_time(); }
	/// return UTC chrono::system_clock::time_point
	chrono::system_clock::time_point                   to_sys            () const { return zoned_time::get_sys_time();   }
	/// returns true if DST is active
	bool                                               is_dst            () const { return get_dst_offset() != chrono::minutes::zero(); }
	/// returns the abbreviated timezone name
	KString                                            get_zone_abbrev   () const { return get_info().abbrev;            }
	/// returns the full timezone name
	KString                                            get_zone_name     () const { return get_time_zone()->name();      }

//--------
private:
//--------

#ifndef DEKAF2_IS_WINDOWS
	static KThreadSafe<std::set<KString>> s_TimezoneNameStore;
#endif

}; // KLocalTime

inline KUTCTime::KUTCTime (const KLocalTime& local) noexcept : KUTCTime(KUnixTime(local.to_sys())) {}
inline KUnixTime::KUnixTime(const KLocalTime& local) noexcept : KUnixTime(local.to_unix()) {}
DEKAF2_CONSTEXPR_14 KUnixTime::KUnixTime(const KUTCTime& utc) noexcept : KUnixTime(utc.to_unix()) {}

DEKAF2_WRAPPED_COMPARISON_OPERATORS_ONE_TYPE (KLocalTime, left.to_sys(), right.to_sys())
DEKAF2_WRAPPED_COMPARISON_OPERATORS_TWO_TYPES(KUTCTime, KLocalTime, first.to_sys(), second.to_sys())

constexpr
inline KDuration operator-(const KUTCTime&   left, const KUTCTime&   right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KLocalTime& left, const KLocalTime& right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() - right.to_sys(); }

constexpr
inline chrono::system_clock::duration operator-(const KUTCTime& left, const chrono::system_clock::time_point right) { return left.to_sys() - right; }
constexpr
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KUTCTime& right) { return left - right.to_sys(); }

inline chrono::system_clock::duration operator-(const KLocalTime& left, const chrono::system_clock::time_point right) { return left.to_sys() - right; }
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KLocalTime& right) { return left - right.to_sys(); }

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KUnixTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KUTCTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, KLocalTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// helper class to delay time format conversion after string parsing
class KParsedTimestampBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
protected:
//--------

	struct raw_time
	{
		int16_t hour, minute, second, millisecond, microsecond, nanosecond, day, month, year, utc_offset_minutes;
		const char* tzname;
		bool has_tz, is_valid;
	};

//--------
public:
//--------

	using self = KParsedTimestampBase;

	KParsedTimestampBase() = default;
	KParsedTimestampBase(raw_time time, const chrono::time_zone* timezone) : m_tm(std::move(time)), m_from_tz(timezone) {}

	operator KUnixTime  () const { return to_unix  (); }
	operator KUTCTime   () const { return to_utc   (); }
	operator KLocalTime () const { return to_local (); }

	bool ok() const { return m_tm.is_valid; }
	explicit operator bool() const { return ok(); }

//--------
private:
//--------

	// gcc 8 needs the below explicit conversions
	friend class dekaf2::KUTCTime;
	friend class dekaf2::KUnixTime;

	KUnixTime  to_unix  () const;
	KUTCTime   to_utc   () const;
	KLocalTime to_local () const;

	// these must be declared as friend and with access on .to_unix() as otherwise gcc 8 would complain about ambiguous conversions
	friend KDuration operator-(const KParsedTimestampBase& left, const KParsedTimestampBase&  right) { return left.to_unix() - right.to_unix(); }
	friend KDuration operator-(const KUnixTime& left, const KParsedTimestampBase&  right) { return left - right.to_unix(); }
	friend KDuration operator-(const KParsedTimestampBase& left, const KUnixTime&  right) { return left.to_unix() - right; }
	friend KDuration operator-(const KUTCTime& left, const KParsedTimestampBase&   right) { return left - right.to_unix(); }
	friend KDuration operator-(const KParsedTimestampBase& left, const KUTCTime&   right) { return left.to_unix() - right; }
	friend KDuration operator-(const KLocalTime& left, const KParsedTimestampBase& right) { return left - right.to_unix(); }
	friend KDuration operator-(const KParsedTimestampBase& left, const KLocalTime& right) { return left.to_unix() - right; }

	DEKAF2_WRAPPED_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(friend, KUnixTime, KParsedTimestampBase, first, second.to_unix())

	raw_time                 m_tm      { };
	const chrono::time_zone* m_from_tz { nullptr };

}; // KParsedTimestampBase

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// helper class to delay time format conversion after string parsing
class KParsedTimestamp : public KParsedTimestampBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KParsedTimestamp;
	using base = KParsedTimestampBase;

	KParsedTimestamp() = default;
	KParsedTimestamp(KStringView sTimestamp, const chrono::time_zone* timezone = nullptr)                      : base(Parse(sTimestamp), timezone)          {}
	KParsedTimestamp(KStringView sFormat, KStringView sTimestamp, const chrono::time_zone* timezone = nullptr) : base(Parse(sFormat, sTimestamp), timezone) {}

	using base::base;

//--------
private:
//--------

	static raw_time Parse(KStringView sTimestamp);
	static raw_time Parse(KStringView sFormat, KStringView sTimestamp);

}; // KParsedTimestamp

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// helper class to delay time format conversion after string parsing
class KParsedWebTimestamp : public KParsedTimestampBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KParsedWebTimestamp;
	using base = KParsedTimestampBase;

	KParsedWebTimestamp() = default;
	KParsedWebTimestamp(KStringView sTimestamp, bool bOnlyGMT) : base(Parse(sTimestamp, bOnlyGMT), nullptr) {}

	using base::base;

//--------
private:
//--------

	static raw_time Parse(KStringView sTimestamp, bool bOnlyGMT);

}; // KParsedWebTimestamp

constexpr std::array<KStringViewZ,  7> AbbreviatedWeekdays {{ "Sun"   , "Mon"   , "Tue"    , "Wed"      , "Thu"     , "Fri"   , "Sat"      }};
constexpr std::array<KStringViewZ,  7> Weekdays            {{ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }};
constexpr std::array<KStringViewZ, 12> AbbreviatedMonths   {{ "Jan"    , "Feb"     , "Mar"  , "Apr"  , "May", "Jun" , "Jul" , "Aug"   , "Sep"      , "Oct"    , "Nov"     , "Dec"      }};
constexpr std::array<KStringViewZ, 12> Months              {{ "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" }};

DEKAF2_PUBLIC const std::array<KString, 12>& GetDefaultLocalMonthNames(bool bAbbreviated);
DEKAF2_PUBLIC const std::array<KString,  7>& GetDefaultLocalDayNames  (bool bAbbreviated);

// "Tue, 03 Aug 2021 10:23:42 +0000"
DEKAF2_PUBLIC                   KString FormWebTimestamp (const std::tm& tTime, KStringView sTimezoneDesignator);
DEKAF2_PUBLIC                   KString FormTimestamp    (const std::tm& time, KStringView sFormat);
DEKAF2_PUBLIC                   KString FormTimestamp    (const std::locale& locale, const std::tm& time, KStringView sFormat);

} // end of namespace detail

inline KUnixTime::KUnixTime (const detail::KParsedTimestampBase& parsed) noexcept : KUnixTime(parsed.to_unix()) {}

/// returns the current system time as KUnixTime in high resolution (clang libc++: microseconds, gnu libstdc++: nanoseconds)
DEKAF2_PUBLIC inline          KUnixTime kNow                    ()                                           { return KUnixTime::now(); }

/// Get the English abbreviated or full weekday name
DEKAF2_PUBLIC constexpr    KStringViewZ kGetDayName             (chrono::weekday weekday, bool bAbbreviated) { return !weekday.ok() ? KStringViewZ{} : bAbbreviated ? detail::AbbreviatedWeekdays[weekday.c_encoding()] : detail::Weekdays[weekday.c_encoding()]; }
/// Get the local abbreviated or full weekday name of the default locale
DEKAF2_PUBLIC inline       KStringViewZ kGetLocalDayName        (chrono::weekday weekday, bool bAbbreviated) { return !weekday.ok() ? KStringViewZ{} : KStringViewZ(detail::GetDefaultLocalDayNames(bAbbreviated)[weekday.c_encoding()]); }
/// Get an array of the local abbreviated or full weekday names of the given locale
DEKAF2_PUBLIC    std::array<KString, 7> kGetLocalDayNames       (const std::locale& locale, bool bAbbreviated);

/// Get the English abbreviated or full month name
DEKAF2_PUBLIC constexpr    KStringViewZ kGetMonthName           (chrono::month month, bool bAbbreviated)     { return !month.ok() ? KStringViewZ{} : bAbbreviated ? detail::AbbreviatedMonths[unsigned(month) - 1] : detail::Months[unsigned(month) - 1]; }
/// Get the local abbreviated or full month name of the default locale
DEKAF2_PUBLIC inline       KStringViewZ kGetLocalMonthName      (chrono::month month, bool bAbbreviated)     { return !month.ok() ? KStringViewZ{} : KStringViewZ(detail::GetDefaultLocalMonthNames(bAbbreviated)[unsigned(month) - 1]); }
/// Get an array of the local abbreviated or full month names of the given locale
DEKAF2_PUBLIC   std::array<KString, 12> kGetLocalMonthNames     (const std::locale& locale, bool bAbbreviated);

/// Returns day of week for every gregorian date
DEKAF2_PUBLIC constexpr chrono::weekday kDayOfWeek              (chrono::year year, chrono::month month, chrono::day day) { return detail::weekday_from_civil(chrono::year_month_day(year/month/day)); }

/// Create a UTC time stamp following std::format patterns
/// @param tUTC KUTCTime. If !ok(), queries current time from the system, defaults to current time
/// @param sFormat format string, defaults to "%Y-%m-%d %H:%M:%S"
/// @return the timestamp string
DEKAF2_PUBLIC                   KString kFormTimestamp          (const KUTCTime& tUTC = KUTCTime::now(), KStringView sFormat = detail::fDefaultDateTime);

/// Create a local time stamp following std::format patterns
/// @param tLocal KLocalTime
/// @param sFormat format string, defaults to "%Y-%m-%d %H:%M:%S"
/// @return the timestamp string
DEKAF2_PUBLIC                   KString kFormTimestamp          (const KLocalTime& tLocal, KStringView sFormat = detail::fDefaultDateTime);

/// Create a time stamp following std::format patterns
/// @param locale a system locale to localize day and month names
/// @param tUTC KUTCTime. If !ok(), queries current time from the system
/// @param sFormat format string, defaults to "%Y-%m-%d %H:%M:%S"
/// @return the timestamp string
DEKAF2_PUBLIC                   KString kFormTimestamp          (const std::locale& locale, const KUTCTime& tUTC, KStringView sFormat = detail::fDefaultDateTime);

/// Create a time stamp following std::format patterns
/// @param locale a system locale to localize day and month names
/// @param tLocal KLocalTime
/// @param sFormat format string, defaults to "%Y-%m-%d %H:%M:%S"
/// @return the timestamp string
DEKAF2_PUBLIC                   KString kFormTimestamp          (const std::locale& locale, const KLocalTime& tLocal, KStringView sFormat = detail::fDefaultDateTime);

/// Create a HTTP time stamp
/// @param tUTC KUTCTime, defaults to current time
/// @return the timestamp string
DEKAF2_PUBLIC inline            KString kFormHTTPTimestamp      (const KUTCTime& tUTC = KUTCTime::now())     { return detail::FormWebTimestamp(tUTC, "GMT"); }

/// Create a SMTP time stamp
/// @param tUTC KUTCTime, defaults to current time
/// @return the timestamp string
DEKAF2_PUBLIC inline            KString kFormSMTPTimestamp      (const KUTCTime& tUTC = KUTCTime::now())     { return detail::FormWebTimestamp(tUTC, "-0000"); }

/// Create a common log format time stamp
/// @param tUTC KUTCTime, defaults to current time
/// @return the timestamp string
// [18/Sep/2011:19:18:28 +0000]
DEKAF2_PUBLIC                   KString kFormCommonLogTimestamp (const KUTCTime& tUTC = KUTCTime::now());

/// Parse any timestamp that matches a format string built from h m s D M Y, and a S U z Z N ?
/// Y(ear) could be 2 or 4 digits,
/// aa = am/pm, case insensitive
/// SSS = milliseconds
/// UUU = microseconds
/// zzz = time zone like "EST"
/// ZZZZZ = time zone like "-0630",
/// NNN = abbreviated month name like "Jan", both in English and the user's locale
/// ? = any character matches
/// example: "???, DD NNN YYYY hh:mm:ss zzz" for a web time stamp
/// @param sFormat the parse format string
/// @param sTimestamp the string to parse
/// @param timezone the timezone to assume if there is no timezone indication in the string - defaults to UTC
/// @return a detail::KParsedTimestamp, that implicitly converts into either KUnixTime, KUTCTime, or KLocalTime. Check with .ok() for valid result.
DEKAF2_PUBLIC inline    detail::KParsedTimestamp kParseTimestamp      (KStringView sFormat, KStringView sTimestamp, const chrono::time_zone* timezone = nullptr) { return detail::KParsedTimestamp(sFormat, sTimestamp, timezone); }

/// Parse any timestamp that matches a format string built from h m s D M Y, and a S U z Z N ?
/// Y(ear) could be 2 or 4 digits,
/// aa = am/pm, case insensitive
/// SSS = milliseconds
/// UUU = microseconds
/// zzz = time zone like "EST"
/// ZZZZZ = time zone like "-0630",
/// NNN = abbreviated month name like "Jan", both in English and the user's locale
/// ? = any character matches
/// example: "???, DD NNN YYYY hh:mm:ss zzz" for a web time stamp
/// @param sFormat the parse format string
/// @param sTimestamp the string to parse
/// @param timezone the timezone to assume if there is no timezone indication in the string - defaults to current zone. Is also used as the timezone for the result type. If the string contained a time zone indication, it is translated into the timezone of the result type.
/// @return KLocalTime of the time stamp. Check with .ok() for valid result.
DEKAF2_PUBLIC inline                  KLocalTime kParseLocalTimestamp (KStringView sFormat, KStringView sTimestamp, const chrono::time_zone* timezone = chrono::current_zone()) { return detail::KParsedTimestamp(sFormat, sTimestamp, timezone); }

/// parse a timestamp from predefined formats - the format is automatically (and fast) detected from about 130 common patterns
/// @param sTimestamp the string to parse - if there is no timezone indication in the string it is assumed as UTC
/// @return a detail::KParsedTimestamp, that implicitly converts into either KUnixTime, KUTCTime, or KLocalTime. Check with .ok() for valid result.
DEKAF2_PUBLIC inline    detail::KParsedTimestamp kParseTimestamp      (KStringView sTimestamp) { return detail::KParsedTimestamp(sTimestamp); }

/// parse a timestamp from predefined formats - the format is automatically (and fast) detected from about 130 common patterns
/// @param sTimestamp the string to parse
/// @param timezone the timezone to assume if there is no timezone indication in the string - defaults to current zone. Is also used as the timezone for the result type. If the string contained a time zone indication, it is translated into the timezone of the result type.
/// @return KLocalTime of the time stamp. Check with .ok() for valid result.
DEKAF2_PUBLIC inline                  KLocalTime kParseLocalTimestamp (KStringView sTimestamp, const chrono::time_zone* timezone = chrono::current_zone()) { return detail::KParsedTimestamp(sTimestamp, timezone); }

/// Parse a HTTP time stamp - only accepts GMT timezone
/// @param sTime time stamp to parse
/// @return a detail::KParsedWebTimestamp, that implicitly converts into either KUnixTime, KUTCTime, or KLocalTime. Check with .ok() for valid result.
DEKAF2_PUBLIC inline detail::KParsedWebTimestamp kParseHTTPTimestamp  (KStringView sTime) { return detail::KParsedWebTimestamp(sTime, true ); }

/// Parse a SMTP time stamp - accepts variable timezone in -0500 format or timezone acronyms
/// @param sTime time stamp to parse
/// @return a detail::KParsedWebTimestamp, that implicitly converts into either KUnixTime, KUTCTime, or KLocalTime. Check with .ok() for valid result.
DEKAF2_PUBLIC inline detail::KParsedWebTimestamp kParseSMTPTimestamp  (KStringView sTime) { return detail::KParsedWebTimestamp(sTime, false); }

/// Parse a time zone abbreviation like EDT / GMT / CEST in uppercase and return offset to GMT
/// @param sTimezone the abbreviation of a timezone to search for
/// @return chrono::minutes of timezone offset, or -1 minute in case of error
DEKAF2_PUBLIC                    chrono::minutes kGetTimezoneOffset   (KStringViewZ sTimezone);

/// Form a string that expresses a duration
/// @param iNumSeconds count of seconds
/// @param bLongForm set to true for verbose output: verbose = "3 days, 23 hrs, 47 mins, 16 secs", condensed = "2.2 wks"
/// @return the output string
DEKAF2_PUBLIC                            KString kTranslateSeconds    (int64_t iNumSeconds, bool bLongForm = false);

/// Form a string that expresses a duration
/// @param Duration a KDuration value
/// @param bLongForm set to true for verbose output: verbose = "3 days, 23 hrs, 47 mins, 16 secs", condensed = "2.2 wks"
/// @return the output string
DEKAF2_PUBLIC inline                     KString kTranslateDuration   (const KDuration& Duration, bool bLongForm = false, KDuration::BaseInterval Interval = KDuration::BaseInterval::NanoSeconds)
{
	return Duration.ToString(bLongForm ? KDuration::Format::Long : KDuration::Format::Smart, Interval);
}


/// converts to string
inline KString KUnixTime::to_string (KStringView sFormat) const noexcept { return detail::FormTimestamp(to_tm(), sFormat); }

DEKAF2_NAMESPACE_END

namespace fmt {

template<> struct formatter<DEKAF2_PREFIX KUnixTime> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const DEKAF2_PREFIX KUnixTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(DEKAF2_PREFIX KUnixTime::to_tm(time), ctx);
	}
};

template<> struct formatter<DEKAF2_PREFIX KConstTimeOfDay> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const DEKAF2_PREFIX KConstTimeOfDay& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

template<> struct formatter<DEKAF2_PREFIX KTimeOfDay> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const DEKAF2_PREFIX KTimeOfDay& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

template<> struct formatter<DEKAF2_PREFIX KLocalTime> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const DEKAF2_PREFIX KLocalTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

template<> struct formatter<DEKAF2_PREFIX KUTCTime> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KUTCTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

} // end of namespace fmt

namespace std {

template<> struct hash<DEKAF2_PREFIX KUnixTime>
{
	std::size_t operator()(DEKAF2_PREFIX KUnixTime time) const noexcept
	{
		return DEKAF2_PREFIX kHash(&time, sizeof(time));
	}
};

template<> struct hash<DEKAF2_PREFIX KConstTimeOfDay>
{
	std::size_t operator()(DEKAF2_PREFIX KConstTimeOfDay time) const noexcept
	{
		return DEKAF2_PREFIX kHash(&time, sizeof(time));
	}
};

template<> struct hash<DEKAF2_PREFIX KTimeOfDay>
{
	std::size_t operator()(DEKAF2_PREFIX KTimeOfDay time) const noexcept
	{
		return DEKAF2_PREFIX kHash(&time, sizeof(time));
	}
};

template<> struct hash<DEKAF2_PREFIX KUTCTime>
{
	std::size_t operator()(DEKAF2_PREFIX KUTCTime time) const noexcept
	{
		return DEKAF2_PREFIX kHash(&time, sizeof(time));
	}
};

template<> struct hash<DEKAF2_PREFIX KLocalTime>
{
	std::size_t operator()(DEKAF2_PREFIX KLocalTime time) const noexcept
	{
		return DEKAF2_PREFIX kHash(&time, sizeof(time));
	}
};

} // end of namespace std
