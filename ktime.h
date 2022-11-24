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
#include <cinttypes>
#include <ctime>

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

//-----------------------------------------------------------------------------
/// Get the English or local abbreviated or full weekday name, input 0..6, 0 == Sunday
DEKAF2_PUBLIC
KStringViewZ kGetDayName(uint16_t iDay, bool bAbbreviated, bool bLocal);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get the English or local abbreviated or full month name, input 0..11, 0 == January
DEKAF2_PUBLIC
KStringViewZ kGetMonthName(uint16_t iMonth, bool bAbbreviated, bool bLocal);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns day of week for every gregorian date. Sunday == 0.
DEKAF2_PUBLIC
uint16_t kDayOfWeek(uint16_t iDay, uint16_t iMonth, uint16_t iYear);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a time stamp following strftime patterns.
/// @param time time struct
/// @param pszFormat format string
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (const std::tm& time, const char* pszFormat = "%Y-%m-%d %H:%M:%S");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a time stamp following strftime patterns. If tTime is 0, current time is
/// used.
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @param pszFormat format string
/// @param bAsLocalTime use tTime as local time instead of utc
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S", bool bAsLocalTime = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a HTTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormHTTPTimestamp (time_t tTime = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a SMTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormSMTPTimestamp (time_t tTime = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a common log format  time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @param bAsLocalTime interpret tTime as local time, defaults to false
/// @return the timestamp string
DEKAF2_PUBLIC
KString kFormCommonLogTimestamp(time_t tTime = 0, bool bAsLocalTime = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Parse a HTTP time stamp - only accepts GMT timezone
/// @param sTime time stamp to parse
/// @return time_t of the time stamp or 0 for error
DEKAF2_PUBLIC
time_t kParseHTTPTimestamp (KStringView sTime);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Parse a SMTP time stamp - accepts variable timezone in -0500 format
/// @param sTime time stamp to parse
/// @return time_t of the time stamp or 0 for error
DEKAF2_PUBLIC
time_t kParseSMTPTimestamp (KStringView sTime);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Parse a time zone name like EDT / GMT / CEST in uppercase and return offset to GMT in seconds
/// @return time_t of timezone offset (seconds), or -1 in case of error
DEKAF2_PUBLIC
time_t kGetTimezoneOffset(KStringView sTimezone);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Parse any timestamp that matches a format string built from h m s D M Y, and a S z Z N ?
/// Y(ear) could be 2 or 4 digits,
/// aa = am/pm, case insensitive
/// S = milliseconds (ignored for output, but checked for 0..9)
/// zzz = time zone like "EST"
/// ZZZZZ = time zone like "-0630",
/// NNN = abbreviated month name like "Jan", both in English and the user's locale
/// ? = any character matches
/// example: "???, DD NNN YYYY hh:mm:ss zzz" for a web time stamp
/// @return time_t of the time stamp or 0 for error
DEKAF2_PUBLIC
time_t kParseTimestamp(KStringView sFormat, KStringView sTimestamp);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// parse a timestamp from predefined formats
DEKAF2_PUBLIC
time_t kParseTimestamp(KStringView sTimestamp);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Form a string that expresses a duration
DEKAF2_PUBLIC
KString kTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

class KDuration;

//-----------------------------------------------------------------------------
/// Form a string that expresses a duration
/// @param Duration a KDuration value
/// @param bLongForm set to true for verbose output
/// @param sMinInterval unit to display if duration is 0 ("less than a ...")
DEKAF2_PUBLIC
KString kTranslateDuration(const KDuration& Duration, bool bLongForm = false, KStringView sMinInterval = "nanosecond");
//-----------------------------------------------------------------------------

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

	/// return day of month (1-based)
	uint16_t GetDay    () const { return Get(m_time.tm_mday);        }
	/// return month (1-based)
	uint16_t GetMonth  () const { return Get(m_time.tm_mon)  + 1;    }
	/// return year (0-based)
	uint16_t GetYear   () const { return Get(m_time.tm_year) + 1900; }
	/// return hour in 24 hour format
	uint16_t GetHour24 () const { return Get(m_time.tm_hour);        }
	/// return hour in 12 hour format
	uint16_t GetHour12 () const { return GetHour24() % 12;           }
	/// return hour in 24 hour format
	uint16_t GetHour   () const { return GetHour24();                }
	/// return minute
	uint16_t GetMinute () const { return Get(m_time.tm_min);         }
	/// return second
	uint16_t GetSecond () const { return Get(m_time.tm_sec);         }
	/// return weekday (Sunday == 0)
	uint16_t GetWeekday() const { return Get(m_time.tm_wday);        }
	/// return English or localized full or abbreviated weekday name
	KStringViewZ GetDayName   (bool bAbbreviated, bool bLocalized = false) const;
	/// return English or localized full or abbreviated month name
	KStringViewZ GetMonthName (bool bAbbreviated, bool bLocalized = false) const;

	/// set day of month (1-based)
	self& SetDay    (uint16_t iVal) { Set(m_time.tm_mday, iVal); return *this; }
	/// set month (1-based)
	self& SetMonth  (uint16_t iVal) { Set(m_time.tm_mon,  iVal - 1);    return *this; }
	/// set year (0-based)
	self& SetYear   (uint16_t iVal) { Set(m_time.tm_year, iVal - 1900); return *this; }
	/// set hour in 24 hour format
	self& SetHour   (uint16_t iVal) { Set(m_time.tm_hour, iVal); return *this; }
	/// set minute
	self& SetMinute (uint16_t iVal) { Set(m_time.tm_min,  iVal); return *this; }
	/// set second
	self& SetSecond (uint16_t iVal) { Set(m_time.tm_sec,  iVal); return *this; }

	/// add days, value may be negative to substract
	self& AddDays   (int32_t iVal)  { Add(m_time.tm_mday, iVal); return *this; }
	/// add months, value may be negative to substract
	self& AddMonths (int32_t iVal)  { Add(m_time.tm_mon,  iVal); return *this; }
	/// add years, value may be negative to substract
	self& AddYears  (int32_t iVal)  { Add(m_time.tm_year, iVal); return *this; }
	/// add hours, value may be negative to substract
	self& AddHours  (int32_t iVal)  { Add(m_time.tm_hour, iVal); return *this; }
	/// add minutes, value may be negative to substract
	self& AddMinutes(int32_t iVal)  { Add(m_time.tm_min,  iVal); return *this; }
	/// add seconds, value may be negative to substract
	self& AddSeconds(int32_t iVal)  { Add(m_time.tm_sec,  iVal); return *this; }

	/// return struct tm
	const std::tm& ToTM ()     const;
	/// return time_t
	time_t ToTimeT () const;

	/// return a string following strftime patterns - default = %Y-%m-%d %H:%M:%S
	KString Format (const char* szFormat = "%Y-%m-%d %H:%M:%S") const;
	/// return a string with the strftime pattern %Y-%m-%d %H:%M:%S
	KString ToString ()        const { return Format();         }

	operator const std::tm& () const { return ToTM ();          }
	operator time_t ()         const { return ToTimeT ();       }
	operator KString ()        const { return Format ();        }

	/// returns true if hour > 11
	bool IsPM () const { return Get(m_time.tm_hour) > 11; }
	/// returns true if hour < 12
	bool IsAM () const { return !IsPM();             }
	/// return true if daylight saving time is in effect
	bool IsDST() const { return Get(m_time.tm_isdst) > 0; }
	/// returns true if this is not a valid date
	bool empty() const { return Get(m_time.tm_mday) == 0; }

	operator bool() const { return !empty();              }

//--------
protected:
//--------

	/// construct from a time_t epoch time, either as local or as GMT / UTC time
	KBrokenDownTime (time_t tGMTime, bool bAsLocalTime);
	/// construct from a struct tm time
	KBrokenDownTime (const std::tm& tm_time);

	/// return the value of a struct tm field, enforce normalization if needed
	uint16_t Get (const int& field) const
	{
		CheckNormalization();
		return field;
	}

	/// set the value of a struct tm field, request normalization
	void Set (int& field, uint16_t iValue)
	{
		field = iValue;
		ForceNormalization();
	}

	/// add the value of a struct tm field, request normalization
	void Add (int& field, int32_t iValue)
	{
		field += iValue;
		ForceNormalization();
	}

	/// virtual method to normalize the date struct
	virtual void Normalize() const = 0;

	/// set the day of week to an invalid value to indicate normalization required
	void ForceNormalization() { m_time.tm_wday = -1; }

	/// check if normalization is required, and do it
	void CheckNormalization() const
	{
		if (m_time.tm_wday == -1)
		{
			// the tm_wday will be set by the normalization to a non-negative value ..
			Normalize();
		}
	}

	mutable std::time_t m_time_t { 0 };
	mutable std::tm     m_time   {   };

}; // KBrokenDownTime

} // end of namespace detail

class KUTCTime;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A wrapper around datetime functions that works cross platform - time is local time
class DEKAF2_PUBLIC KLocalTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = detail::KBrokenDownTime;

	KLocalTime() = default;
	/// construct from a time_t epoch time, as local time
	KLocalTime (time_t tGMTime) : base(tGMTime, true) {}
	/// construct from a struct tm time
	KLocalTime (const std::tm& tm_time) : base(tm_time) {}
	/// construct from a KUTCTime
	KLocalTime (const KUTCTime& gmtime);
	/// construct from a string representation, which is interpreted as local time (if there is no time zone indicator telling other)
	KLocalTime (KStringView sTimestamp) : KLocalTime(kParseTimestamp(sTimestamp)) {}
	/// construct from a string representation with format description
	KLocalTime (KStringView sFormat, KStringView sTimestamp) : KLocalTime(kParseTimestamp(sFormat, sTimestamp)) {}

	/// return the offset in seconds between this time and UTC
	int32_t GetUTCOffset () const;

//--------
protected:
//--------

	virtual void Normalize() const override final
	{
		m_time_t = mktime(const_cast<std::tm*>(&m_time));
	}

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
/// A wrapper around datetime functions that works cross platform - time is UTC / GMT
class DEKAF2_PUBLIC KUTCTime : public detail::KBrokenDownTime
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using base = detail::KBrokenDownTime;

	KUTCTime() = default;
	/// construct from a time_t epoch time,  as GMT / UTC time
	KUTCTime (time_t tGMTime) : base(tGMTime, false) {}
	/// construct from a struct tm time
	KUTCTime (const std::tm& tm_time) : base(tm_time) {}
	/// construct from a KLocalTime
	KUTCTime (const KLocalTime& localtime);
	/// construct from a string representation, which is interpreted as UTC time (if there is no time zone indicator telling other)
	KUTCTime (KStringView sTimestamp) : KUTCTime(kParseTimestamp(sTimestamp)) {}
	/// construct from a string representation with format description
	KUTCTime (KStringView sFormat, KStringView sTimestamp) : KUTCTime(kParseTimestamp(sFormat, sTimestamp)) {}

	/// return the offset in seconds between this time and UTC (always 0)
	int32_t GetUTCOffset () const { return 0; };

//--------
protected:
//--------

	virtual void Normalize() const override final
	{
		m_time_t = timegm(const_cast<std::tm*>(&m_time));
	}

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

} // end of namespace dekaf2
