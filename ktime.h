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

#include "kstring.h"
#include "kstringview.h"
#include "kduration.h"
#include "kthreadsafe.h"
#include "bits/ktemplate.h"
#include "bits/khash.h"
#include "kdate.h"
#include <cinttypes>
#include <ctime>
#include <chrono>
#include <set>

namespace dekaf2
{

/// returns the timezone of the local timezone for this process
inline const chrono::time_zone* kGetLocalTimezone()
{
	return chrono::current_zone();
}

/// returns the timezone name of the local timezone for this process
inline KString kGetLocalTimezoneName()
{
	return kGetLocalTimezone()->name();
}

/// returns a sorted vector with all time zone names
DEKAF2_PUBLIC
std::vector<KString> kGetListOfTimezoneNames();

/// returns a time zone that matches the sZoneName, or throws std::runtime_error
DEKAF2_PUBLIC
const chrono::time_zone* kFindTimezone(KStringView sZoneName);

// We deliberately do not offer a method to set the local timezone
// as it would only have effect on C time utilities, not on the C++
// variants. Use timezones passed in to the C++ functions instead
// of switching the process globally.

/// Converts any UTC timepoint to a local zoned time (at least exact for the past, not necessarily for the future)
/// Never use local times to do arithmetics: DST changes, leap seconds, and history will all bite you!
/// A zoned time is a local timepoint with all information about the local timezone it is contained in, at its very point in time, but not at any other point in time
template<class Duration>
DEKAF2_PUBLIC
auto kToLocalZone(chrono::sys_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::zoned_time<Duration>
{
	return chrono::zoned_time<Duration>(zone, tp);
}

/// Converts any UTC timepoint to a local timepoint (at least exact for the past, not necessarily for the future)
/// Never use local timepoints to do arithmetics: DST changes, leap seconds, and history will all bite you!
/// A local timepoint is only valid at its exact point in time.
template<class Duration>
DEKAF2_PUBLIC
auto kToLocalTime(chrono::sys_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::local_time<typename std::common_type<Duration, chrono::seconds>::type>
{
	return zone->to_local(tp);
}

/// converts any local timepoint to a UTC timepoint (at least exact for the past, not necessarily for the future)
template<class Duration>
DEKAF2_PUBLIC
auto kFromLocalTime(chrono::zoned_time<Duration> ztp) -> chrono::sys_time<typename std::common_type<Duration, chrono::seconds>::type>
{
	return ztp.get_sys_time();
}

/// converts any local timepoint to a UTC timepoint (at least exact for the past, not necessarily for the future)
template<class Duration>
DEKAF2_PUBLIC
auto kFromLocalTime(chrono::local_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::sys_time<typename std::common_type<Duration, chrono::seconds>::type>
{
	return zone->to_sys(tp);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class is a modern version of the legacy time point usage of time_t -
/// in contrast to the legacy duration usage of time_t, which should be done with KDuration.
/// It uses high resolution (clang libc++: microseconds, gnu libstdc++: nanoseconds), but
/// is as small as a std::time_t (long long).
/// It publicly inherits from chrono::system_clock::time_point, and adds constexpr conversions
/// to and from std::time_t and std::tm.
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
	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	                    explicit KUnixTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                             KUnixTime (KStringView sFormat, KStringView sTimestamp);

	using base::base;

	/// returns true if not zero
	DEKAF2_CONSTEXPR_14 explicit operator bool()              const noexcept { return time_since_epoch() != duration::zero();                }
	/// converts implicitly to time_t timepoint, although it loses precision..
	DEKAF2_CONSTEXPR_14 explicit operator std::time_t()       const noexcept { return to_time_t(*this);                                      }

	DEKAF2_CONSTEXPR_17 self& operator+=(std::time_t seconds)       noexcept { base::operator += (chrono::seconds(seconds)); return *this;   }
	DEKAF2_CONSTEXPR_17 self& operator-=(std::time_t seconds)       noexcept { base::operator -= (chrono::seconds(seconds)); return *this;   }

	using base::operator+=;
	using base::operator-=;

	/// converts to std::time_t timepoint
	DEKAF2_CONSTEXPR_14 std::time_t to_time_t ()              const noexcept { return to_time_t(*this);                                      }
	/// converts to std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 std::tm     to_tm     ()              const noexcept { return to_tm(*this);                                          }
	/// converts to string
	                    KString     to_string ()              const noexcept;

	/// converts from KUnixTime to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(KUnixTime tp)  noexcept { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from system_clock timepoint to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(time_point tp) noexcept { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from time_t timepoint to system_clock timeppoint (constexpr)
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

}; // KUnixTime

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


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// constexpr breaking down any duration or timepoint into a 24h duration with accessors for hours, minutes, seconds, subseconds
class DEKAF2_PUBLIC KTimeOfDay : public chrono::hh_mm_ss<chrono::system_clock::duration>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self     = KTimeOfDay;
	using base     = chrono::hh_mm_ss<chrono::system_clock::duration>;
	using duration = chrono::system_clock::duration;

	KTimeOfDay() = default;
	template<typename Duration, typename std::enable_if<detail::is_duration<Duration>::value, int>::type = 0>
	constexpr explicit KTimeOfDay(Duration d) noexcept : base(d % Duration(chrono::hours(24))) {}
	constexpr KTimeOfDay(KUnixTime timepoint) noexcept : KTimeOfDay(timepoint.time_since_epoch()) {}
	constexpr KTimeOfDay(const base& other)   noexcept : base(other) {}
	constexpr KTimeOfDay(base&& other)        noexcept : base(std::move(other)) {}

//  the base class adds accessors:
//	constexpr bool is_negative()        const noexcept
//	constexpr chrono::hours hours()     const noexcept
//	constexpr chrono::minutes minutes() const noexcept
//	constexpr chrono::seconds seconds() const noexcept
//	constexpr precision subseconds()    const noexcept
//	constexpr precision to_duration()   const noexcept

	/// return hour in 24 hour format
	constexpr chrono::hours   hours24  () const noexcept { return hours();                 }
	/// return hour in 12 hour format
	constexpr chrono::hours   hours12  () const noexcept { return chrono::make12(hours()); }
	/// returns true if time is pm
	constexpr bool            is_pm    () const noexcept { return chrono::is_pm(hours());  }
	/// returns true if time is am
	constexpr bool            is_am    () const noexcept { return chrono::is_am(hours());  }

}; // KTimeOfDay

/// returns the current system time as KUnixTime in high resolution (clang libc++: microseconds, gnu libstdc++: nanoseconds)
DEKAF2_PUBLIC inline
KUnixTime kNow() { return KUnixTime::now(); }

/// Get the English or local abbreviated or full weekday name, input 0..6, 0 == Sunday
DEKAF2_PUBLIC
KStringViewZ kGetDayName(uint16_t iDay, bool bAbbreviated, bool bLocal);

/// Get the English or local abbreviated or full month name, input 0..11, 0 == January
DEKAF2_PUBLIC
KStringViewZ kGetMonthName(uint16_t iMonth, bool bAbbreviated, bool bLocal);

/// Returns day of week for every gregorian date. Sunday == 0.
DEKAF2_PUBLIC constexpr
uint16_t kDayOfWeek(chrono::year year, chrono::month month, chrono::day day)
{ return detail::weekday_from_civil(chrono::year_month_day(year/month/day)).c_encoding(); }

/// Create a time stamp following std::format patterns, defaults to "%Y-%m-%d %H:%M:%S"
/// @param time time struct
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const std::tm& time, KStringView sFormat = "%Y-%m-%d %H:%M:%S");

/// Create a time stamp following std::format patterns, defaults to "%Y-%m-%d %H:%M:%S"
/// @param locale a system locale to localize day and month names
/// @param time time struct
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const std::locale& locale, const std::tm& time, KStringView sFormat = "%Y-%m-%d %H:%M:%S");

/// Create a UTC time stamp following std::format patterns, defaults to "%Y-%m-%d %H:%M:%S"
/// If tTime is constructed with 0, current time is used.
/// @param tTime KUnixTime. If 0, query current time from the system
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (KUnixTime tTime = KUnixTime::now(), KStringView sFormat = "%Y-%m-%d %H:%M:%S");

/// Create a local time stamp following std::format patterns, defaults to "%Y-%m-%d %H:%M:%S"
/// If tTime is constructed with 0, current time is used.
/// @param timezone the time zone into which the unix time shall be translated
/// @param tTime KUnixTime. If 0, query current time from the system
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const chrono::time_zone* timezone, KUnixTime tTime = KUnixTime::now(), KStringView sFormat = "%Y-%m-%d %H:%M:%S");

/// Create a time stamp following std::format patterns, defaults to "%Y-%m-%d %H:%M:%S"
/// If tTime is constructed with 0, current time is used.
/// @param locale a system locale to localize day and month names
/// @param timezone the time zone into which the unix time shall be translated
/// @param tTime KUnixTime. If 0, query current time from the system
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const std::locale& locale, const chrono::time_zone* timezone, KUnixTime tTime = KUnixTime::now(), KStringView sFormat = "%Y-%m-%d %H:%M:%S");

/// Create a HTTP time stamp
/// @param tTime Seconds since epoch. If constructed with 0 or defaulted, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormHTTPTimestamp (KUnixTime tTime = KUnixTime::now());

/// Create a SMTP time stamp
/// @param tTime Seconds since epoch. If constructed with 0 or defaulted, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormSMTPTimestamp (KUnixTime tTime = KUnixTime::now());

/// Create a common log format  time stamp
/// @param tTime Seconds since epoch. If constructed with 0 or defaulted, query current time from the system
/// @param bAsLocalTime display as local time instead of UTC, defaults to false
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormCommonLogTimestamp(KUnixTime tTime = KUnixTime::now(), bool bAsLocalTime = false);

/// Parse a HTTP time stamp - only accepts GMT timezone
/// @param sTime time stamp to parse
/// @return KUnixTime of the time stamp or 0 for error
DEKAF2_PUBLIC
KUnixTime kParseHTTPTimestamp (KStringView sTime);

/// Parse a SMTP time stamp - accepts variable timezone in -0500 format
/// @param sTime time stamp to parse
/// @return KUnixTime of the time stamp or 0 for error
DEKAF2_PUBLIC
KUnixTime kParseSMTPTimestamp (KStringView sTime);

/// Parse a time zone name like EDT / GMT / CEST in uppercase and return offset to GMT in minutes
/// @return chrono::minutes of timezone offset, or -1 minute in case of error
DEKAF2_PUBLIC
chrono::minutes kGetTimezoneOffset(KStringViewZ sTimezone);

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
/// @return KUnixTime of the time stamp or 0 for error
DEKAF2_PUBLIC
KUnixTime kParseTimestamp(KStringView sFormat, KStringView sTimestamp);

/// parse a timestamp from predefined formats
/// - the format is automatically detected from about 100 common patterns
DEKAF2_PUBLIC
KUnixTime kParseTimestamp(KStringView sTimestamp);

/// Form a string that expresses a duration
DEKAF2_PUBLIC
KString kTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);

/// Form a string that expresses a duration
/// @param Duration a KDuration value
/// @param bLongForm set to true for verbose output
/// @param sMinInterval unit to display if duration is 0 ("less than a ...")
DEKAF2_PUBLIC
KString kTranslateDuration(const KDuration& Duration, bool bLongForm = false, KStringView sMinInterval = "nanosecond");

/// converts to string
inline KString KUnixTime::to_string () const noexcept { return kFormTimestamp(*this); }



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

	/// construct from a KUnixTime
	DEKAF2_CONSTEXPR_14 KUTCTime (KUnixTime time) noexcept
	: KDate      (chrono::floor<chrono::days>(time))
	, KTimeOfDay (chrono::make_time(time - chrono::floor<chrono::days>(time)))
	{
	}

	/// construct from a struct tm time
	DEKAF2_CONSTEXPR_14 KUTCTime (const std::tm& tm) noexcept
	: KDate      (chrono::sys_days(chrono::day(tm.tm_mday) / chrono::month(tm.tm_mon + 1) / chrono::year(tm.tm_year + 1900)))
	, KTimeOfDay (chrono::make_time
	                 (chrono::system_clock::duration
	                   (chrono::hours  (tm.tm_hour)
	                  + chrono::minutes(tm.tm_min)
#ifndef DEKAF2_IS_WINDOWS
				      - chrono::seconds(tm.tm_gmtoff)
#endif
				      + chrono::seconds(tm.tm_sec))
	                 )
	             )
	{
	}

	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	                    KUTCTime (KStringView sTimestamp) : KUTCTime(kParseTimestamp(sTimestamp)) {}
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                    KUTCTime (KStringView sFormat, KStringView sTimestamp) : KUTCTime(kParseTimestamp(sFormat, sTimestamp)) {}
	/// construct from KLocalTime
	                    KUTCTime (const KLocalTime& local) noexcept;

	// allowing all base class constructors -> KDate
	using KDate::KDate;
	// allowing all base class constructors -> KTimeOfDay
	using KTimeOfDay::KTimeOfDay;

	/// get the current time as GMT / UTC time
	static KUTCTime now() { return KUTCTime(KUnixTime::now()); }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator+=(T Duration)      { *this = KUnixTime(to_unix() + chrono::duration_cast<KUnixTime::duration>(Duration)); return *this; }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator-=(T Duration)      { return operator+=(-Duration); }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator+=(T Duration)      { *this += chrono::seconds(Duration); return *this; }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_17 self& operator-=(T Duration)      { *this -= chrono::seconds(Duration); return *this; }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14 self& operator=(T Duration)       { *this = Duration; return *this; }

	// prefer the chrono::days/months/years += from KDate
	using KDate::operator+=;
	// prefer the chrono::days/months/years -= from KDate
	using KDate::operator-=;

	/// return struct tm
	DEKAF2_CONSTEXPR_14 std::tm   to_tm     ()  const noexcept { return KUnixTime::to_tm(join()); }
	/// return our own KUnixTime (a replacement for the non-typesafe std::time_t)
	DEKAF2_CONSTEXPR_14 KUnixTime to_unix   ()  const noexcept { return join();  }
	/// return chrono::system_clock::time_point
	DEKAF2_CONSTEXPR_14 chrono::system_clock::time_point to_sys () const noexcept { return chrono::system_clock::time_point(to_unix()); }

	DEKAF2_CONSTEXPR_14          operator  std::tm   ()  const noexcept { return to_tm      (); }
	DEKAF2_CONSTEXPR_14 explicit operator  KUnixTime ()  const noexcept { return to_unix    (); }
	DEKAF2_CONSTEXPR_14 explicit operator  chrono::system_clock::time_point                 () const { return to_sys(); }

	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString            Format    (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const { return to_string(sFormat); }
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString            to_string (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const;

//--------
private:
//--------

	DEKAF2_CONSTEXPR_14 KUnixTime join() const noexcept
	{
		return !empty() ? chrono::sys_days(KDate(*this))
		                  + hours()
		                  + minutes()
		                  + seconds()
		                : KUnixTime(0);
	}

}; // KUTCTime

DEKAF2_CONSTEXPR_14 bool operator==(const KUTCTime& left, const KUTCTime& right) { return left.to_sys() == right.to_sys(); }
DEKAF2_CONSTEXPR_14 bool operator< (const KUTCTime& left, const KUTCTime& right) { return left.to_sys() <  right.to_sys(); }

DEKAF2_COMPARISON_OPERATORS(KUTCTime) // TODO these should be constexpr

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in local time (any timezone that this system supports) - you
/// cannot do arithmetics on it, because that is in general invalid on local times (you may step
/// transition points in local time, like DST changes and leap seconds)
class DEKAF2_PUBLIC KLocalTime : private chrono::zoned_time<chrono::system_clock::duration>, public KConstDate, public KTimeOfDay
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using zoned_time = chrono::zoned_time<chrono::system_clock::duration>;
	using local_time = chrono::local_time<chrono::system_clock::duration>;

//--------
public:
//--------

	using self = KLocalTime;

	KLocalTime() = default;

	/// construct from a KUnixTime, and translate into a timezone, default timezone is the system's local time
	KLocalTime (KUnixTime time, const chrono::time_zone* timezone = chrono::current_zone())
	: zoned_time(kToLocalZone(time, timezone))
	, KConstDate(chrono::floor<chrono::days>(get_local_time()))
	, KTimeOfDay(chrono::make_time(local_time(get_local_time()) - chrono::floor<chrono::days>(get_local_time())))
	{
	}

	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	KLocalTime (KStringView sTimestamp)
	: KLocalTime(kParseTimestamp(sTimestamp))
	{
	}
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	KLocalTime (KStringView sFormat, KStringView sTimestamp)
	: KLocalTime(kParseTimestamp(sFormat, sTimestamp))
	{
	}
	/// construct from KUTCTime, and translate into a timezone, default timezone is the system's local time
	KLocalTime (const KUTCTime& utc, const chrono::time_zone* timezone = chrono::current_zone())
	: KLocalTime(utc.to_unix(), timezone)
	{
	}

	// do not allow base class constructors - they do not make sense for the static KLocalTime

	/// get the current time as local time or any other timezone, default timezone is the system's local time
	static KLocalTime   now(const chrono::time_zone* timezone = chrono::current_zone()) { return KLocalTime(chrono::system_clock::now(), timezone); }

	/// return KUnixTime
	KUnixTime           to_unix   ()  const { return to_sys(); }
	/// return struct tm (needed for efficient formatting)
	std::tm             to_tm     ()  const;
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString             Format    (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const { return to_string(sFormat);    }
	/// return a string following std::format patterns - default = %Y-%m-%d %H:%M:%S
	KString             to_string (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const;

	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::system_clock::time_point () const { return to_sys(); }
	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::local_time<chrono::system_clock::duration> () const { return to_local(); }

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

inline bool operator==(const KLocalTime& left, const KLocalTime& right) { return left.to_sys() == right.to_sys(); }
inline bool operator< (const KLocalTime& left, const KLocalTime& right) { return left.to_sys() <  right.to_sys(); }
DEKAF2_COMPARISON_OPERATORS(KLocalTime)

inline bool operator==(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() == right.to_sys(); }
inline bool operator!=(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() != right.to_sys(); }
inline bool operator< (const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() <  right.to_sys(); }
inline bool operator<=(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() <= right.to_sys(); }
inline bool operator> (const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() >  right.to_sys(); }
inline bool operator>=(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() >= right.to_sys(); }
inline bool operator==(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() == right.to_sys(); }
inline bool operator!=(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() != right.to_sys(); }
inline bool operator< (const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() <  right.to_sys(); }
inline bool operator<=(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() <= right.to_sys(); }
inline bool operator> (const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() >  right.to_sys(); }
inline bool operator>=(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() >= right.to_sys(); }

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

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KUnixTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KUTCTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KLocalTime time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

} // end of namespace dekaf2

namespace fmt {

template<> struct formatter<dekaf2::KUnixTime> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const dekaf2::KUnixTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(dekaf2::KUnixTime::to_tm(time), ctx);
	}
};

template<> struct formatter<dekaf2::KLocalTime> : formatter<std::tm>
{
	template <typename FormatContext>
	DEKAF2_CONSTEXPR_14
	auto format(const dekaf2::KLocalTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

template<> struct formatter<dekaf2::KUTCTime> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KUTCTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

} // end of namespace fmt

namespace std {

template<> struct hash<dekaf2::KUnixTime>
{
	std::size_t operator()(dekaf2::KUnixTime time) const noexcept
	{
		return dekaf2::kHash(&time, sizeof(time));
	}
};

template<> struct hash<dekaf2::KTimeOfDay>
{
	std::size_t operator()(dekaf2::KTimeOfDay time) const noexcept
	{
		return dekaf2::kHash(&time, sizeof(time));
	}
};

template<> struct hash<dekaf2::KUTCTime>
{
	std::size_t operator()(dekaf2::KUTCTime time) const noexcept
	{
		return dekaf2::kHash(&time, sizeof(time));
	}
};

template<> struct hash<dekaf2::KLocalTime>
{
	std::size_t operator()(dekaf2::KLocalTime time) const noexcept
	{
		return dekaf2::kHash(&time, sizeof(time));
	}
};

} // end of namespace std
