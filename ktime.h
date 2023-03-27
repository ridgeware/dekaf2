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
#include "bits/ktemplate.h"
#include "kdate.h"
#include <cinttypes>
#include <ctime>
#include <chrono>

namespace dekaf2
{

// There is a general problem with switching locale data between threads:
// Currently (C++17/C++20), C++ does not offer a library function that
// converts a time_t value into a local time std::tm representation
// _without_ changing the process-global locale (which you should really
// not do in multi-threaded environments).
//
// This library therefore assumes that a global locale is _once_ set at
// process initialization (e.g. through Dekaf::SetUnicodeLocale() or
// KInit().SetLocale()), and that all threads then will share the same
// global locale.
//
// If you switch the global locale after you have first used kGetDayName()
// or kGetMonthName() with blocal==true, the day and month names will not
// change to the new locale but stay in the locale you first used them with.
//
// This affects
//
//   kGetDayName()
//   kGetMonthName()
//   kFormCommonLogTimeStamp()
//   kParseTimestamp()
//   KUTCTime/KLocalTime::GetDayName()
//   KUTCTime/KLocalTime::GetMonthName()
//
// when being used in a local context

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

	using base::base;


	/// construct from time_t timepoint
	DEKAF2_CONSTEXPR_14 KUnixTime(time_t other) : KUnixTime(from_time_t(other)) {}
	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	                    KUnixTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	                    KUnixTime (KStringView sFormat, KStringView sTimestamp);
	/// returns true if not zero
	DEKAF2_CONSTEXPR_14 explicit operator bool() const                   { return time_since_epoch() != duration::zero();    }
	/// converts to time_t timepoint
	DEKAF2_CONSTEXPR_14 operator std::time_t() const                     { return to_time_t(*this);                          }

	DEKAF2_CONSTEXPR_17 self& operator+=(std::time_t seconds)            { base::operator += (chrono::seconds(seconds)); return *this;   }
	DEKAF2_CONSTEXPR_17 self& operator-=(std::time_t seconds)            { base::operator -= (chrono::seconds(seconds)); return *this;   }

	using base::operator+=;
	using base::operator-=;

	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString ToString (const char* szFormat = "%Y-%m-%d %H:%M:%S") const;

	/// converts from KUnixTime to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(KUnixTime tp)       { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from system_clock timepoint to time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 static std::time_t to_time_t(time_point tp)      { return time_t(chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count()); }
	/// converts from time_t timepoint to system_clock timeppoint (constexpr)
	DEKAF2_CONSTEXPR_14 static time_point from_time_t(std::time_t tTime) { return time_point(chrono::seconds(tTime));                    }

//--------
private:
//--------

}; // KUnixTime

/// Get the English or local abbreviated or full weekday name, input 0..6, 0 == Sunday
DEKAF2_PUBLIC
KStringViewZ kGetDayName(uint16_t iDay, bool bAbbreviated, bool bLocal);

/// Get the English or local abbreviated or full month name, input 0..11, 0 == January
DEKAF2_PUBLIC
KStringViewZ kGetMonthName(uint16_t iMonth, bool bAbbreviated, bool bLocal);

/// Returns day of week for every gregorian date. Sunday == 0.
DEKAF2_PUBLIC
uint16_t kDayOfWeek(uint16_t iDay, uint16_t iMonth, uint16_t iYear);

/// Create a time stamp following strftime patterns.
/// @param time time struct
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const std::tm& time, const char* pszFormat = "%Y-%m-%d %H:%M:%S");

/// Create a time stamp following strftime patterns. If tTime is 0, current time is
/// used.
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @param pszFormat format string
/// @param bAsLocalTime display as local time instead of UTC
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (KUnixTime tTime = KUnixTime{}, const char* pszFormat = "%Y-%m-%d %H:%M:%S", bool bAsLocalTime = false);

/// Create a HTTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormHTTPTimestamp (KUnixTime tTime = KUnixTime{});

/// Create a SMTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormSMTPTimestamp (KUnixTime tTime = KUnixTime{});

/// Create a common log format  time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @param bAsLocalTime display as local time instead of UTC, defaults to false
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormCommonLogTimestamp(KUnixTime tTime = KUnixTime{}, bool bAsLocalTime = false);

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

/// Instead of using this function prefer using the built-in methods of KUnixTime, KUTCTime or KLocalTime
/// - those require less overhead (two system calls less on average)
///
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

/// When not returning to KUnixTime, instead of using this function prefer using the built-in methods of KUTCTime or KLocalTime
/// - those require less overhead (two system calls less on average)
///
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
/// A wrapper around datetime functions that works cross platform
class DEKAF2_PUBLIC KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KBrokenDownTime;

	KBrokenDownTime() = default;

	// it does not make sense to declare the getters constexpr, they never will
	// be in reality, as there will always be one call to the C time functions in between

	/// return day of month (1-based)
	          uint16_t GetDay    () const { return Get(m_time.tm_mday);                          }
	/// return chrono::days
		chrono::days   days      () const { return chrono::days(GetDay());                       }
	/// return month (1-based)
	          uint16_t GetMonth  () const { return Get(m_time.tm_mon)  + 1;                      }
	/// return chrono::months
		chrono::months months    () const { return chrono::months(GetMonth());                   }
	/// return year (0-based)
	          uint16_t GetYear   () const { return Get(m_time.tm_year) + 1900;                   }
	/// return chrono::years
		chrono::years  years     () const { return chrono::years(GetYear());                     }
	/// return hour in 24 hour format
	          uint16_t GetHour24 () const { return Get(m_time.tm_hour);                          }
	/// return chrono::hours in 24 hour format
		chrono::hours  hours24   () const { return chrono::hours(GetHour());                     }
	/// return hour in 12 hour format
	          uint16_t GetHour12 () const { auto h = GetHour24() % 12; return (h == 0) ? 12 : h; }
	/// return chrono::hours in 12 hour format
		chrono::hours  hours12   () const { return chrono::hours(GetHour12());                   }
	/// return hour in 24 hour format
	          uint16_t GetHour   () const { return GetHour24();                                  }
	/// return chrono::hours in 24 hour format
		chrono::hours  hours     () const { return hours24();                                    }
	/// return minute
	          uint16_t GetMinute () const { return Get(m_time.tm_min);                           }
	/// return chrono::minutes
		chrono::minutes minutes  () const { return chrono::minutes(GetMinute());                 }
	/// return second
	          uint16_t GetSecond () const { return Get(m_time.tm_sec);                           }
	/// return chrono::seconds
		chrono::seconds seconds  () const { return chrono::seconds(GetSecond());                 }
	/// return KHourMinSec (the hours/minutes/seconds of the day)
	chrono::hh_mm_ss<chrono::seconds> GetHourMinSec () const { return chrono::hh_mm_ss<chrono::seconds>(hours() + minutes() + seconds()); }
	/// return weekday (Sunday == 0)
	          uint16_t GetWeekday() const { return Get(m_time.tm_wday);                          }
	/// return English or localized full or abbreviated weekday name
	      KStringViewZ GetDayName   (bool bAbbreviated, bool bLocalized = false) const;
	/// return English or localized full or abbreviated month name
	      KStringViewZ GetMonthName (bool bAbbreviated, bool bLocalized = false) const;
	/// returns true if hour > 11
	          bool     IsPM      ()  const { return Get(m_time.tm_hour) > 11;                   }
	/// returns true if hour < 12
	          bool     IsAM      ()  const { return !IsPM();                                    }
	/// return true if daylight saving time is in effect
	          bool     IsDST     ()  const { return Get(m_time.tm_isdst) > 0;                   }
	/// returns true if this is not a valid date
	          bool     empty     ()  const { return Get(m_time.tm_mday) == 0;                   }
	/// returns true if this is a valid date
	          operator bool      ()  const { return !empty();                                   }

	// the setters however can be constexpr when being initialized from a std::tm

	/// set day of month (1-based)
	constexpr self&    SetDay    (uint16_t iVal) { Set(m_time.tm_mday, iVal); return *this; }
	/// set month (1-based)
	constexpr self&    SetMonth  (uint16_t iVal) { Set(m_time.tm_mon,  iVal - 1);    return *this; }
	/// set year (0-based)
	constexpr self&    SetYear   (uint16_t iVal) { Set(m_time.tm_year, iVal - 1900); return *this; }
	/// set hour in 24 hour format
	constexpr self&    SetHour   (uint16_t iVal) { Set(m_time.tm_hour, iVal); return *this; }
	/// set minute
	constexpr self&    SetMinute (uint16_t iVal) { Set(m_time.tm_min,  iVal); return *this; }
	/// set second
	constexpr self&    SetSecond (uint16_t iVal) { Set(m_time.tm_sec,  iVal); return *this; }

	/// add days, value may be negative to substract
	constexpr self&    AddDays   (int32_t iVal)  { Add(m_time.tm_mday, iVal); return *this; }
	/// add months, value may be negative to substract
	constexpr self&    AddMonths (int32_t iVal)  { Add(m_time.tm_mon,  iVal); return *this; }
	/// add years, value may be negative to substract
	constexpr self&    AddYears  (int32_t iVal)  { Add(m_time.tm_year, iVal); return *this; }
	/// add hours, value may be negative to substract
	constexpr self&    AddHours  (int32_t iVal)  { Add(m_time.tm_hour, iVal); return *this; }
	/// add minutes, value may be negative to substract
	constexpr self&    AddMinutes(int32_t iVal)  { Add(m_time.tm_min,  iVal); return *this; }
	/// add seconds, value may be negative to substract
	template<typename I, typename std::enable_if<sizeof(I) <= sizeof(int32_t), int>::type = 0>
	constexpr self&    AddSeconds(I iVal)        { Add(m_time.tm_sec,  iVal); return *this; }
	/// add seconds, value may be negative to substract
	template<typename I, typename std::enable_if<sizeof(int32_t) < sizeof(I), int>::type = 0>
	constexpr self&    AddSeconds(I iVal)        { AddSeconds64(iVal); return *this;        }

	/// add any chrono::duration type, value may be negative to substract
	constexpr self&    Add(KDuration Duration)   { return AddSeconds(Duration.seconds().count()); }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	constexpr self& operator+=(T Duration)       { return Add(Duration);                    }

	template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	constexpr self& operator-=(T Duration)       { return Add(Duration * -1);               }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	constexpr self& operator+=(T Duration)       { return AddSeconds(Duration);             }

	template<typename T, typename std::enable_if<std::is_same<T, time_t>::value, int>::type = 0>
	constexpr self& operator-=(T Duration)       { return AddSeconds(Duration * -1);        }

	/// set to midnight, that is, clear hours, minutes, and seconds
	constexpr self&    ToMidnight()
	{
		m_time.tm_sec  = 0;
		m_time.tm_min  = 0;
		m_time.tm_hour = 0;
		// do not force normalization.. this operation does not change the
		// integrity of other fields
		return *this;
	}

	/// return struct tm
	std::tm            ToTM      ()  const { CheckNormalization(); return m_time;     }
	/// return our own KUnixTime (a replacement for the non-typesafe std::time_t)
	KUnixTime          ToUnixTime()  const { CheckNormalization(); return m_time_t;   }
	/// return chrono::system_clock::time_point
	chrono::system_clock::time_point ToTimePoint () const { return chrono::system_clock::time_point(ToUnixTime()); }
	/// DEPRECATED, use ToUnixTime() in new code, which converts automatically to time_t
	time_t             ToTimeT   ()  const { return ToUnixTime();                     }

	/// DEPRECATED, use ToString() in new code
	KString            Format    (const char* szFormat = "%Y-%m-%d %H:%M:%S") const { return ToString(szFormat); }
	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString            ToString  (const char* szFormat = "%Y-%m-%d %H:%M:%S") const;

	operator           std::tm   ()  const { return ToTM ();                  }
	operator           time_t    ()  const { return ToTimeT ();               }
	operator           chrono::system_clock::time_point() const { return ToTimePoint(); }
	operator           KUnixTime ()  const { return ToUnixTime();             }
	operator           KString   ()  const { return ToString ();              }
	operator           chrono::hh_mm_ss<chrono::seconds> () const { return GetHourMinSec();    }

//--------
protected:
//--------

	/// construct from a time_t epoch time, either as local or as GMT / UTC time
	                   KBrokenDownTime (std::time_t tGMTime, bool bAsLocalTime);
	/// construct from a chrono::system_clock::time_point time, either as local or as GMT / UTC time
	                   KBrokenDownTime (chrono::system_clock::time_point tTime, bool bAsLocalTime) : KBrokenDownTime(KUnixTime::to_time_t(tTime), bAsLocalTime) {}
	/// construct from a KUnixTime, either as local or as GMT / UTC time
	                   KBrokenDownTime (KUnixTime time, bool bAsLocalTime) : KBrokenDownTime(KUnixTime::to_time_t(time), bAsLocalTime) {}
	/// construct from a struct tm time
	constexpr          KBrokenDownTime (const std::tm& tm_time) : m_time(tm_time) {
		// we do not know if the struct tm was normalized
		ForceNormalization();
	}

	/// return the value of a struct tm field, enforce normalization if needed
	constexpr uint16_t Get          (const int& field) const { CheckNormalization(); return field; }
	/// set the value of a struct tm field, request normalization
	constexpr void     Set          (int& field, uint16_t iValue) { field = iValue; ForceNormalization(); }
	/// add the value of a struct tm field, request normalization
	constexpr void     Add          (int& field, int32_t iValue) { field += iValue; ForceNormalization(); }
	/// add (and properly cast) a long long seconds duration
	constexpr void     AddSeconds64 (int64_t iSeconds)
	{
		if (DEKAF2_UNLIKELY(iSeconds > std::numeric_limits<int>::max() - m_time.tm_sec ||
							iSeconds < std::numeric_limits<int>::min() + m_time.tm_sec))
		{
			// expensive
			CheckNormalization();
			auto seconds = chrono::seconds(iSeconds);
			m_time_t += seconds;
			m_time = BreakDown(m_time_t);
		}
		else
		{
			// cheap
			AddSeconds(static_cast<int32_t>(iSeconds));
		}
	}
	/// virtual method to normalize the date struct
	virtual std::time_t Normalize   (const std::tm& time) const = 0;
	/// virtual method to convert from time_t into tm
	virtual std::tm     BreakDown   (const std::time_t time) const = 0;
	/// set the day of week to an invalid value to indicate normalization required
	DEKAF2_ALWAYS_INLINE
	constexpr void      ForceNormalization()       { m_time.tm_wday = -1; }
	/// check if normalization is required, and do it
	DEKAF2_ALWAYS_INLINE
	constexpr void      CheckNormalization() const { if (m_time.tm_wday < 0) { m_time_t = Normalize(m_time); } }

#ifdef DEKAF2_IS_UNIX
	chrono::seconds GetTzOffset      () const { return chrono::seconds(m_time.tm_gmtoff);            }
#endif

//--------
private:
//--------

	mutable KUnixTime   m_time_t { 0 };
	mutable std::tm     m_time   {   };

}; // KBrokenDownTime

} // end of namespace detail

class KUTCTime;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A wrapper around datetime functions that work cross platform - time is local time
class DEKAF2_PUBLIC KLocalTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = detail::KBrokenDownTime;

	KLocalTime() = default;
	/// construct from a KUnixTime as local time
	KLocalTime (KUnixTime UnixTime) : base(UnixTime, true) {}
	/// construct from a time_t epoch time, as local time
	KLocalTime (std::time_t tGMTime) : base(tGMTime, true) {}
	/// construct from a chrono::system_clock::time_point time
	KLocalTime (chrono::system_clock::time_point tTime) : base(tTime, true) {}
	/// construct from a struct tm time
	KLocalTime (const std::tm& tm_time) : base(tm_time) {}
	/// construct from a KUTCTime
	KLocalTime (const KUTCTime& gmtime);
	/// construct from a string representation, which is interpreted as local time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	KLocalTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	KLocalTime (KStringView sFormat, KStringView sTimestamp);
	/// construct from current time
	KLocalTime (std::nullptr_t) { *this = Now(); }

	/// return the offset in seconds between this time and UTC
#ifdef DEKAF2_IS_UNIX
	chrono::seconds GetUTCOffset () const { return GetTzOffset(); }
#else
	chrono::seconds GetUTCOffset () const;
#endif

	/// get the current time as local time
	static KLocalTime Now() { return KLocalTime(chrono::system_clock::now()); }

//--------
protected:
//--------

	virtual std::time_t Normalize(const std::tm& time   ) const override final { return mktime(const_cast<std::tm*>(&time)); }
	virtual std::tm     BreakDown(const std::time_t time) const override final;

}; // KLocalTime

inline
bool operator==(const KLocalTime& left, const KLocalTime& right)
{
	return left.ToTimeT() == right.ToTimeT();
}

inline
bool operator<(const KLocalTime& left, const KLocalTime& right)
{
	return left.ToTimeT() < right.ToTimeT();
}

DEKAF2_COMPARISON_OPERATORS(KLocalTime)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A wrapper around datetime functions that work cross platform - time is UTC / GMT
class DEKAF2_PUBLIC KUTCTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = detail::KBrokenDownTime;

	KUTCTime() = default;
	/// construct from a KUnixTime as GMT / UTC time
	KUTCTime (KUnixTime UnixTime) : base(UnixTime, false) {}
	/// construct from a time_t epoch time,  as GMT / UTC time
	KUTCTime (std::time_t tGMTime) : base(tGMTime, false) {}
	/// construct from a chrono::system_clock::time_point time
	KUTCTime (chrono::system_clock::time_point tTime) : base(tTime, false) {}
	/// construct from a struct tm time
	KUTCTime (const std::tm& tm_time) : base(tm_time) {}
	/// construct from a KLocalTime
	KUTCTime (const KLocalTime& localtime);
	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	/// - the string format is automatically detected from about 100 common patterns
	KUTCTime (KStringView sTimestamp);
	/// construct from a string representation with format description
	/// @see kParseTimestamp for a format string description
	KUTCTime (KStringView sFormat, KStringView sTimestamp);
	/// construct from current time
	KUTCTime (std::nullptr_t) { *this = Now(); }
	/// return the offset in seconds between this time and UTC (always 0)
	chrono::seconds GetUTCOffset () const { return chrono::seconds{0}; };

	/// get the current time as GMT / UTC time
	static KUTCTime Now() { return KUTCTime(chrono::system_clock::now()); }

//--------
protected:
//--------

	virtual std::time_t Normalize(const std::tm&    time) const override final { return timegm(const_cast<std::tm*>(&time)); }
	virtual std::tm     BreakDown(const std::time_t time) const override final;

}; // KUTCTime

inline
bool operator==(const KUTCTime& left, const KUTCTime& right)
{
	return left.ToTimeT() == right.ToTimeT();
}

inline
bool operator<(const KUTCTime& left, const KUTCTime& right)
{
	return left.ToTimeT() < right.ToTimeT();
}

DEKAF2_COMPARISON_OPERATORS(KUTCTime)

inline KDuration operator-(const KLocalTime& left, const KLocalTime& right) { return left.ToTimeT() - right.ToTimeT(); }
inline KDuration operator-(const KLocalTime& left, const time_t right)      { return left.ToTimeT() - right;           }
inline KDuration operator-(const time_t left, const KLocalTime& right)      { return left - right.ToTimeT();           }
inline chrono::system_clock::duration operator-(const KLocalTime& left, const chrono::system_clock::time_point right) { return left.ToTimePoint() - right; }
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KLocalTime& right) { return left - right.ToTimePoint(); }

template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
inline KLocalTime operator+(const KLocalTime& left, const T Duration)       { KLocalTime Ret(left); Ret.Add(Duration); return Ret;      }
template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
inline KLocalTime operator-(const KLocalTime& left, const T Duration)       { KLocalTime Ret(left); Ret.Add(Duration * -1); return Ret; }

inline KDuration operator-(const KUTCTime& left, const KUTCTime& right)     { return left.ToTimeT() - right.ToTimeT(); }
inline KDuration operator-(const KUTCTime& left, const time_t right)        { return left.ToTimeT() - right;           }
inline KDuration operator-(const time_t left, const KUTCTime& right)        { return left - right.ToTimeT();           }
inline KDuration operator-(const KUTCTime& left, const KLocalTime right)    { return left.ToTimeT() - right.ToTimeT(); }
inline KDuration operator-(const KLocalTime left, const KUTCTime& right)    { return left.ToTimeT() - right.ToTimeT(); }
inline chrono::system_clock::duration operator-(const KUTCTime& left, const chrono::system_clock::time_point right) { return left.ToTimePoint() - right; }
inline chrono::system_clock::duration operator-(const chrono::system_clock::time_point left, const KUTCTime& right) { return left - right.ToTimePoint(); }

template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
inline KUTCTime operator+(const KUTCTime& left, const T Duration)           { KUTCTime Ret(left); Ret.Add(Duration); return Ret;        }
template<typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
inline KUTCTime operator-(const KUTCTime& left, const T Duration)           { KUTCTime Ret(left); Ret.Add(Duration * -1); return Ret;   }

} // end of namespace dekaf2

namespace fmt {

template <>
struct formatter<dekaf2::KLocalTime> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KLocalTime& time, FormatContext& ctx) const
	{
		return formatter<string_view>::format(time.ToString(), ctx);
	}
};

template <>
struct formatter<dekaf2::KUTCTime> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KUTCTime& time, FormatContext& ctx) const
	{
		return formatter<string_view>::format(time.ToString(), ctx);
	}
};

} // end of namespace fmt
