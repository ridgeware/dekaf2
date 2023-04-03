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

/// converts any UTC timepoint to a local timepoint (at least exact for the past, not necessarily for the future)
template<class Duration>
DEKAF2_PUBLIC
auto kToLocalZone(chrono::sys_time<Duration> tp, const chrono::time_zone* zone = chrono::current_zone()) -> chrono::zoned_time<Duration>
{
	return chrono::zoned_time<Duration>(zone, tp);
}

/// converts any UTC timepoint to a local timepoint (at least exact for the past, not necessarily for the future)
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
/// this class is a modern version of the legacy time point usage of time_t -
/// in contrast to the legacy duration usage of time_t, which should be done with KDuration
class DEKAF2_PUBLIC KUnixTime : public chrono::system_clock::time_point
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self       = KUnixTime;
	using base       = chrono::system_clock::time_point;
	using clock      = chrono::system_clock;

	DEKAF2_CONSTEXPR_14 KUnixTime(const base& other) noexcept : base(other) {}
	DEKAF2_CONSTEXPR_14 KUnixTime(base&& other)      noexcept : base(std::move(other)) {}

	/// construct from time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14          KUnixTime(time_t time) : KUnixTime(from_time_t(time)) {}
	/// construct from std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 explicit KUnixTime(const std::tm& tm)  : KUnixTime(    from_tm(tm)  ) {}
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
	DEKAF2_CONSTEXPR_14 explicit operator std::time_t()                const noexcept { return to_time_t(*this);                                      }

	DEKAF2_CONSTEXPR_17 self& operator+=(std::time_t seconds)       noexcept { base::operator += (chrono::seconds(seconds)); return *this;   }
	DEKAF2_CONSTEXPR_17 self& operator-=(std::time_t seconds)       noexcept { base::operator -= (chrono::seconds(seconds)); return *this;   }

	using base::operator+=;
	using base::operator-=;

	/// converts to std::time_t timepoint 
	DEKAF2_CONSTEXPR_14 std::time_t to_time_t ()              const noexcept { return to_time_t(*this);                                      }
	/// converts to std::tm timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 std::tm     to_tm     ()              const noexcept { return to_tm(*this);                                          }

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
	DEKAF2_CONSTEXPR_14 static time_point  from_time_t(std::time_t tTime) noexcept { return time_point(chrono::seconds(tTime)); }
	/// converts from std::tm timepoint to system_clock timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static time_point  from_tm    (const std::tm& tm) noexcept;

	/// returns a KUnixTime with the current time
	                    static KUnixTime   now()                             { return clock::now(); }

}; // KUnixTime

//-----------------------------------------------------------------------------
// constexpr implementation of std::tm to std::time_t conversion
DEKAF2_CONSTEXPR_14 KUnixTime::time_point KUnixTime::from_tm(const std::tm& tm) noexcept
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
#if DEKAF2_HAS_CPP_20 && (DEKAF2_NO_GCC || DEKAF2_GCC_VERSION_MAJOR >= 10)
	std::tm tm;
#else
	std::tm tm{};
#endif
	// break up
	auto dp      = chrono::floor<chrono::days>(tp);
	auto ymd     = chrono::year_month_day(dp);
	auto time    = chrono::make_time(tp - dp);
	// assign
	tm.tm_sec    = static_cast<int>(time.seconds().count());
	tm.tm_min    = static_cast<int>(time.minutes().count());
	tm.tm_hour   = static_cast<int>(time.hours().count());
	tm.tm_mday   = static_cast<int>(unsigned(ymd.day()));
	tm.tm_mon    = static_cast<int>(unsigned(ymd.month())) - 1;
	tm.tm_year   = static_cast<int>(ymd.year()) - 1900;
	tm.tm_wday   = chrono::weekday(dp).c_encoding();
	tm.tm_yday   = (dp - chrono::sys_days(chrono::year_month_day(ymd.year()/1/1))).count();
	tm.tm_isdst  = 0;
#ifndef DEKAF2_IS_WINDOWS
	tm.tm_gmtoff = 0;
	tm.tm_zone   = const_cast<char*>("UTC");
#endif
	return tm;

} // KUnixTime::to_tm

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// constexpr breaking down any duration or timepoint into a 24h duration with accessors for hours, minutes, seconds, subseconds
class DEKAF2_PUBLIC KTimeOfDay : public chrono::hh_mm_ss<chrono::system_clock::duration>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = chrono::hh_mm_ss<chrono::system_clock::duration>;

	constexpr KTimeOfDay() noexcept = default;
	template<typename Duration>
	constexpr explicit KTimeOfDay(Duration d) noexcept : base(d % Duration(chrono::hours(24))) {}
	constexpr KTimeOfDay(KUnixTime timepoint) noexcept : KTimeOfDay(timepoint.time_since_epoch()) {}

//  the base class adds accessors:
//	constexpr bool is_negative()        const noexcept
//	constexpr chrono::hours hours()     const noexcept
//	constexpr chrono::minutes minutes() const noexcept
//	constexpr chrono::seconds seconds() const noexcept
//	constexpr precision subseconds()    const noexcept
//	constexpr precision to_duration()   const noexcept

}; // KTimeOfDay

/// Get the English or local abbreviated or full weekday name, input 0..6, 0 == Sunday
DEKAF2_PUBLIC
KStringViewZ kGetDayName(uint16_t iDay, bool bAbbreviated, bool bLocal);

/// Get the English or local abbreviated or full month name, input 0..11, 0 == January
DEKAF2_PUBLIC
KStringViewZ kGetMonthName(uint16_t iMonth, bool bAbbreviated, bool bLocal);

/// Returns day of week for every gregorian date. Sunday == 0.
DEKAF2_PUBLIC
uint16_t kDayOfWeek(uint16_t iDay, uint16_t iMonth, uint16_t iYear);

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

/// Parse a time zone name like EDT / GMT / CEST in uppercase and return offset to GMT in seconds
/// @return KDuration of timezone offset, or -1 second in case of error
DEKAF2_PUBLIC
KDuration kGetTimezoneOffset(KStringView sTimezone);

/// Parse any timestamp that matches a format string built from h m s D M Y, and a S z Z N ?
/// Y(ear) could be 2 or 4 digits,
/// aa = am/pm, case insensitive
/// S = milliseconds (ignored for output, but checked for 0..9)
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


namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in UTC, base class to store data and access interface
class DEKAF2_PUBLIC KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KBrokenDownTime;
	
	                    KBrokenDownTime() = default;

	/// return chrono::days
	DEKAF2_CONSTEXPR_14 chrono::days    days     () const noexcept { return chrono::days(unsigned(m_ymd.day()));     }
	/// return chrono::months
	DEKAF2_CONSTEXPR_14 chrono::months  months   () const noexcept { return chrono::months(unsigned(m_ymd.month())); }
	/// return chrono::years
	DEKAF2_CONSTEXPR_14 chrono::years   years    () const noexcept { return chrono::years(signed(m_ymd.year()));     }
	/// return hour in 24 hour format
	DEKAF2_CONSTEXPR_14 chrono::hours   hours24  () const noexcept { return m_hhmmss.hours();                        }
	/// return hour in 12 hour format
	DEKAF2_CONSTEXPR_14 chrono::hours   hours12  () const noexcept { auto h = hours24().count() % 12; return chrono::hours((h == 0) ? 12 : h); }
	/// return hour in 24 hour format
	DEKAF2_CONSTEXPR_14 chrono::hours   hours    () const noexcept { return hours24();                               }
	/// return minute
	DEKAF2_CONSTEXPR_14 chrono::minutes minutes  () const noexcept { return m_hhmmss.minutes();                      }
	/// return second
	DEKAF2_CONSTEXPR_14 chrono::seconds seconds  () const noexcept { return m_hhmmss.seconds();                      }
	/// return weekday (Sunday == 0)
	DEKAF2_CONSTEXPR_14 chrono::weekday weekday  () const noexcept { return m_weekday;                               }
	/// return English or localized full or abbreviated weekday name
	KStringViewZ                   get_day_name  (bool bAbbreviated, bool bLocalized = false) const;
	/// return English or localized full or abbreviated month name
	KStringViewZ                 get_month_name  (bool bAbbreviated, bool bLocalized = false) const;
	/// returns true if hour > 11
	DEKAF2_CONSTEXPR_14 bool            is_pm    () const noexcept { return chrono::is_pm(m_hhmmss.hours());         }
	/// returns true if hour < 12
	DEKAF2_CONSTEXPR_14 bool            is_am    () const noexcept { return chrono::is_am(m_hhmmss.hours());         }
	/// returns true if year is leap year
	DEKAF2_CONSTEXPR_14 bool            is_leap  () const noexcept { return m_ymd.year().is_leap();                  }
	/// returns true if this is not a valid date
	DEKAF2_CONSTEXPR_14 bool            empty    () const noexcept { return !m_ymd.ok();                             }
	/// returns true if this is a valid date
	DEKAF2_CONSTEXPR_14 explicit operator bool   () const noexcept { return !empty();                                }

	/// set to midnight, that is, clear hours, minutes, and seconds
	DEKAF2_CONSTEXPR_14 self&           to_midnight ()    noexcept
	{
		m_hhmmss = chrono::hh_mm_ss<chrono::system_clock::duration>();
		return *this;
	}

	/// return KHourMinSec (the hours/minutes/seconds of the day)
	DEKAF2_CONSTEXPR_14 chrono::hh_mm_ss<
		chrono::system_clock::duration
	>                                   to_hhmmss   () const noexcept { return m_hhmmss; }

	/// return chrono::year_month_day
	DEKAF2_CONSTEXPR_14 chrono::year_month_day
	                                    to_ymd      () const noexcept { return m_ymd;    }

//--------
protected:
//--------

	chrono::hh_mm_ss<KUnixTime::duration> m_hhmmss {};
	chrono::year_month_day m_ymd { };
	chrono::weekday m_weekday {};

}; // KBrokenDownTime

} // end of namespace detail

class KLocalTime;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in UTC
class DEKAF2_PUBLIC KUTCTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KUTCTime;

	                    KUTCTime() = default;

	/// construct from a KUnixTime
	DEKAF2_CONSTEXPR_14 KUTCTime (KUnixTime time) noexcept { split(time); }

	/// construct from a struct tm time
	DEKAF2_CONSTEXPR_14 KUTCTime (const std::tm& tm_time) noexcept { from_tm(tm_time); }

	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	                    KUTCTime (KStringView sTimestamp) : KUTCTime(kParseTimestamp(sTimestamp)) {}
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                    KUTCTime (KStringView sFormat, KStringView sTimestamp) : KUTCTime(kParseTimestamp(sFormat, sTimestamp)) {}
	/// construct from current time
	                    KUTCTime (std::nullptr_t) { *this = now(); }

	                    KUTCTime (const KLocalTime& local) noexcept;

	/// get the current time as GMT / UTC time
	static KUTCTime now() { return KUTCTime(chrono::system_clock::now()); }

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

	/// return struct tm
	DEKAF2_CONSTEXPR_14 std::tm   to_tm     ()  const noexcept { return KUnixTime::to_tm(join()); }
	/// return our own KUnixTime (a replacement for the non-typesafe std::time_t)
	DEKAF2_CONSTEXPR_14 KUnixTime to_unix   ()  const noexcept { return join();  }
	/// return chrono::system_clock::time_point
	DEKAF2_CONSTEXPR_14 chrono::system_clock::time_point to_sys () const noexcept { return chrono::system_clock::time_point(to_unix()); }

	DEKAF2_CONSTEXPR_14          operator  std::tm   ()  const noexcept { return to_tm      (); }
	DEKAF2_CONSTEXPR_14 explicit operator  KUnixTime ()  const noexcept { return to_unix    (); }
	                             operator  KString   ()  const          { return to_string  (); }
	DEKAF2_CONSTEXPR_14 explicit operator  chrono::system_clock::time_point                 () const { return to_sys(); }
	DEKAF2_CONSTEXPR_14 explicit operator  chrono::hh_mm_ss<chrono::system_clock::duration> () const { return to_hhmmss(); }

	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString            Format    (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const { return to_string(sFormat); }
	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString            to_string (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const;

//--------
private:
//--------

	DEKAF2_CONSTEXPR_14 void split(KUnixTime tp) noexcept
	{
		m_days      = chrono::floor<chrono::days>(tp);
		m_ymd       = chrono::year_month_day(m_days);
		m_hhmmss    = chrono::make_time(tp - m_days);
		m_weekday   = chrono::weekday(m_days);

	} // split

	DEKAF2_CONSTEXPR_14 KUnixTime join() const noexcept
	{
		return !empty() ? m_days
		                  + m_hhmmss.hours()
		                  + m_hhmmss.minutes()
		                  + m_hhmmss.seconds()
		                : KUnixTime(0);
	}

	DEKAF2_CONSTEXPR_14 void from_tm(const std::tm& tm) noexcept { split(KUnixTime::from_tm(tm)); } // needs to consider possible utc offset

	chrono::sys_days m_days;

}; // KUTCTime

DEKAF2_CONSTEXPR_14 bool operator==(const KUTCTime& left, const KUTCTime& right)
{
	return left.to_sys() == right.to_sys();
}

DEKAF2_CONSTEXPR_14 bool operator<(const KUTCTime& left, const KUTCTime& right)
{
	return left.to_sys() < right.to_sys();
}

DEKAF2_COMPARISON_OPERATORS(KUTCTime)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A broken down time representation in local time (any timezone that this system supports)
class DEKAF2_PUBLIC KLocalTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KLocalTime;

	KLocalTime() = default;

	/// construct from a KUnixTime, and translate into a timezone, default timezone is the system's local time
	KLocalTime (KUnixTime time, const chrono::time_zone* timezone = chrono::current_zone())
	: m_zoned_time(kToLocalZone(time, timezone))
	{
		split(m_zoned_time.get_local_time());
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
	/// construct from current time
	KLocalTime (std::nullptr_t) { *this = now(); }
	/// construct from KUTCTime, and translate into a timezone, default timezone is the system's local time
	KLocalTime (const KUTCTime& utc, const chrono::time_zone* timezone = chrono::current_zone())
	: KLocalTime(utc.to_unix(), timezone)
	{
	}

	/// get the current time as local time or any other timezone, default timezone is the system's local time
	static KLocalTime   now(const chrono::time_zone* timezone = chrono::current_zone()) { return KLocalTime(chrono::system_clock::now(), timezone); }

	/// return KUnixTime
	KUnixTime           to_unix   ()  const { return m_zoned_time.get_sys_time(); }
	/// return struct tm (needed for efficient formatting)
	std::tm             to_tm     ()  const;
	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString             Format    (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const { return to_string(sFormat);    }
	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString             to_string (KStringView sFormat = "%Y-%m-%d %H:%M:%S") const;

			   operator KString   () const { return to_string(); }
	           operator chrono::hh_mm_ss<chrono::system_clock::duration> () const { return to_hhmmss(); }
	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::system_clock::time_point () const { return to_sys(); }
	/// zero cost conversion, but loses time zone properties in implicit conversion..
	explicit   operator chrono::local_time<chrono::system_clock::duration> () const { return to_local(); }

	/// returns the chrono::time_zone struct
	const chrono::time_zone*                           get_time_zone     () const { return m_zoned_time.get_time_zone();   }
	/// returns the chrono::sys_info struct
	chrono::sys_info                                   get_info          () const { return m_zoned_time.get_info();        }
	/// get offset from UTC
	chrono::seconds                                    get_utc_offset    () const { return get_info().offset;              }
	/// get offset for DST
	chrono::minutes                                    get_dst_offset    () const { return get_info().save;                }
	/// return chrono::local_time
	chrono::local_time<chrono::system_clock::duration> to_local          () const { return m_zoned_time.get_local_time();  }
	/// return UTC chrono::system_clock::time_point
	chrono::system_clock::time_point                   to_sys            () const { return m_zoned_time.get_sys_time();    }
	/// returns true if DST is active
	bool                                               is_dst            () const { return get_dst_offset() != chrono::minutes::zero(); }
	/// returns the abbreviated timezone name
	KString                                            get_zone_abbrev   () const { return get_info().abbrev;              }
	/// returns the full timezone name
	KString                                            get_zone_name     () const { return get_time_zone()->name();        }

//--------
private:
//--------

	using local_unix = chrono::local_time<chrono::system_clock::duration>;

	DEKAF2_CONSTEXPR_14 void split(local_unix tp) noexcept
	{
		m_days      = chrono::floor<chrono::days>(tp);
		m_ymd       = chrono::year_month_day(m_days);
		m_hhmmss    = chrono::make_time(tp - m_days);
		m_weekday   = chrono::weekday(m_days);

	} // split

	chrono::zoned_time<chrono::system_clock::duration> m_zoned_time;
	chrono::local_days m_days;

#ifndef DEKAF2_IS_WINDOWS
	static KThreadSafe<std::set<KString>> s_TimezoneNameStore;
#endif

}; // KLocalTime

inline KUTCTime::KUTCTime (const KLocalTime& local) noexcept : KUTCTime(local.to_sys()) {}

inline bool operator==(const KLocalTime& left, const KLocalTime& right) { return left.to_sys() == right.to_sys(); }
inline bool operator< (const KLocalTime& left, const KLocalTime& right) { return left.to_sys() <  right.to_sys(); }
DEKAF2_COMPARISON_OPERATORS(KLocalTime)

inline bool operator==(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() == right.to_sys(); }
inline bool operator< (const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() <  right.to_sys(); }
inline bool operator<=(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() <= right.to_sys(); }
inline bool operator> (const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() >  right.to_sys(); }
inline bool operator>=(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() >= right.to_sys(); }
inline bool operator< (const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() <  right.to_sys(); }
inline bool operator<=(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() <= right.to_sys(); }
inline bool operator> (const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() > right.to_sys();  }
inline bool operator>=(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() >= right.to_sys(); }

inline KDuration operator-(const KUTCTime&   left, const KUTCTime&   right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KLocalTime& left, const KLocalTime& right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KUTCTime&   left, const KLocalTime& right) { return left.to_sys() - right.to_sys(); }
inline KDuration operator-(const KLocalTime& left, const KUTCTime&   right) { return left.to_sys() - right.to_sys(); }

inline chrono::system_clock::duration operator-(const KUTCTime& left, const chrono::system_clock::time_point right) { return left.to_sys() - right; }
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KUTCTime& right) { return left - right.to_sys(); }

inline chrono::system_clock::duration operator-(const KLocalTime& left, const chrono::system_clock::time_point right) { return left.to_sys() - right; }
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KLocalTime& right) { return left - right.to_sys(); }

} // end of namespace dekaf2

namespace fmt {

template <>
struct formatter<dekaf2::KUnixTime> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KUnixTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(dekaf2::KUnixTime::to_tm(time), ctx);
	}
};

template <>
struct formatter<dekaf2::KLocalTime> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KLocalTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

template <>
struct formatter<dekaf2::KUTCTime> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KUTCTime& time, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(time.to_tm(), ctx);
	}
};

} // end of namespace fmt
