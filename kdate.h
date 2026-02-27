/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2023, Ridgeware, Inc.
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

/// @file kdate.h
/// make sure we have calendar and timezone extensions for std::chrono
/// and provides the KDate class to calculate with dates

#include "kdefinitions.h"
#if DEKAF2_HAS_INCLUDE(<kconfiguration.h>)
	#include <kconfiguration.h>
#endif
#include "kstring.h"
#include "kstringview.h"
#include <chrono>
#include <locale>
#include <ostream>

// We do not generally want to use std::time_put() for time formatting because it uses a different
// formatting engine than std::format() or fmt::format() - the rules for the conversion specifiers
// differ slightly, see e.g. %Ez %Oz and %Z (and this is actually visible in real code).
// Unfortunately, the call interface of std::time_put(), although hidden behind an opaque stream
// interface, is easier to call for existing code, because it does not require the format string
// being framed in "{: }". We alleviate that somehow by providing defaults for the formatting
// functions that use the std::format syntax already, and check if we need to wrap user supplied
// format strings to the new syntax.

#ifndef DEKAF2_USE_TIME_PUT
	#define DEKAF2_USE_TIME_PUT 0
#endif

// we need to allow users of this lib to define a standard less than C++20
// even when our lib is compiled with C++20 and without Howard Hinnant's date lib -
// the latter is compatible to the known stdlibs and then offers the interface that the
// users would expect from our library
#ifndef DEKAF2_USE_HINNANT_DATE
	#if defined(DEKAF2_STD_CHRONO_HAS_CALENDAR) && DEKAF2_HAS_CPP_20
		#define DEKAF2_USE_HINNANT_DATE 0
	#elif DEKAF2_HAS_INCLUDE(<date/date.h>)
		#define DEKAF2_USE_HINNANT_DATE 1
	#elif DEKAF2_HAS_INCLUDE("hhinnant-date.h")
		#define DEKAF2_USE_HINNANT_DATE 2
	#else
		#define DEKAF2_USE_HINNANT_DATE 0
	#endif
#endif

// again, when we built our lib with C++20, but someone else uses it
// with a previous standard, we have to undefine DEKAF2_STD_CHRONO_HAS_LOCAL_T
// because the chrono header will not make it visible then
#if DEKAF2_STD_CHRONO_HAS_LOCAL_T && !DEKAF2_HAS_CPP_20
	#undef DEKAF2_STD_CHRONO_HAS_LOCAL_T
#endif

#if DEKAF2_USE_HINNANT_DATE || !DEKAF2_HAS_CHRONO_ROUND || !DEKAF2_STD_CHRONO_HAS_CLOCK_CAST
	#if DEKAF2_HAS_INCLUDE(<date/date.h>)
		// date:: only creates the is_clock check when we have void_t - and we supply
		// a void_t also for C++ < 17
		#define HAS_VOID_T 1
		#include <date/date.h>
	#elif DEKAF2_HAS_INCLUDE("hhinnant-date.h")
		#define HAS_VOID_T 1
		#include "hhinnant-date.h"
	#endif
#endif

#ifndef DEKAF2_USE_HINNANT_TIMEZONE
	#if defined(DEKAF2_STD_CHRONO_HAS_TIMEZONE) && DEKAF2_HAS_CPP_20
		#define DEKAF2_USE_HINNANT_TIMEZONE 0
		#define DEKAF2_HAS_TIMEZONES 1
	#elif DEKAF2_HAS_INCLUDE(<date/tz.h>)
		#define DEKAF2_USE_HINNANT_TIMEZONE 1
		#define DEKAF2_HAS_TIMEZONES 1
		#include <date/tz.h>
	#else
		#define DEKAF2_USE_HINNANT_TIMEZONE 0
	#endif
#else
	#define DEKAF2_HAS_TIMEZONES 1
	#include <date/tz.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace chrono {

#if !DEKAF2_HAS_TIMEZONES
typedef void time_zone;
#endif

// importing std::chrono into dekaf2::chrono
using namespace std::chrono;

#if DEKAF2_USE_HINNANT_DATE

#if (__cpp_lib_chrono < 201907L && !(DEKAF2_IS_OSX && DEKAF2_HAS_CPP_20))
// take care with these - they exist in post 2017 libs independently of the calendar functions
using date::days;
using date::weeks;
using date::years;
using date::months;
#endif

// import all other types from date::
using date::last_spec;

template <class Duration>
using sys_time = date::sys_time<Duration>;
using date::sys_days;
using date::sys_seconds;

template <class Duration>
using local_time = date::local_time<Duration>;
using date::local_seconds;
using date::local_days;

using date::day;
using date::month;
using date::year;

using date::weekday;
using date::weekday_indexed;
using date::weekday_last;

using date::month_day;
using date::month_day_last;
using date::month_weekday;
using date::month_weekday_last;

using date::year_month;

using date::year_month_day;
using date::year_month_day_last;
using date::year_month_weekday;
using date::year_month_weekday_last;

template <class Duration>
using hh_mm_ss = date::hh_mm_ss<Duration>;

template <class T>
using is_clock = date::is_clock<T>;
using date::is_am;
using date::is_pm;
using date::make12;
using date::make24;
using date::make_time;

using date::operator/;
using date::operator==;
using date::operator!=;
using date::operator<;
using date::operator<=;
using date::operator>;
using date::operator>=;

using date::last_spec;
using date::Sunday;
using date::Monday;
using date::Tuesday;
using date::Wednesday;
using date::Thursday;
using date::Friday;
using date::Saturday;

using date::January;
using date::February;
using date::March;
using date::April;
using date::May;
using date::June;
using date::July;
using date::August;
using date::September;
using date::October;
using date::November;
using date::December;

#else // of DEKAF2_USE_HINNANT_DATE

// helper from date that did not make it into the standard
template <class Rep, class Period>
inline constexpr hh_mm_ss<duration<Rep, Period>> make_time(const duration<Rep, Period>& d) { return hh_mm_ss<duration<Rep, Period>>(d); }

#endif // of DEKAF2_USE_HINNANT_DATE

#if !DEKAF2_HAS_CHRONO_ROUND
using date::floor;
using date::ceil;
using date::round;
using date::abs;
#endif

#if !DEKAF2_STD_CHRONO_HAS_CLOCK_CAST
using date::clock_cast;
#endif

#if DEKAF2_IS_GCC && defined(DEKAF2_HAS_WARN_LITERAL_SUFFIX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#endif

inline namespace literals { // dekaf2::chrono::literals

#if (__cpp_lib_chrono_udls < 201304L) && (__cplusplus < 201402L)
// the namespace literals are missing in this lib.. add them here
#if !DEKAF2_IS_CLANG
// clang effectively makes it impossible to define system literals
// but, irritatingly, provides the below literals also for C++11 on newer libs
// (which means that we do not have to create them on our own, which we couldn't anyway)
constexpr chrono::hours                                operator""h   (unsigned long long h)  { return chrono::hours(static_cast<chrono::hours::rep>(h));                }
constexpr chrono::duration<long double, ratio<3600,1>> operator""h   (long double h)         { return chrono::duration<long double, ratio<3600,1>>(h);                  }
constexpr chrono::minutes                              operator""min (unsigned long long m)  { return chrono::minutes(static_cast<chrono::minutes::rep>(m));            }
constexpr chrono::duration<long double, ratio<60,1>>   operator""min (long double m)         { return chrono::duration<long double, ratio<60,1>>(m);                    }
constexpr chrono::seconds                              operator""s   (unsigned long long s)  { return chrono::seconds(static_cast<chrono::seconds::rep>(s));            }
constexpr chrono::duration<long double>                operator""s   (long double s)         { return chrono::duration<long double>(s);                                 }
constexpr chrono::milliseconds                         operator""ms  (unsigned long long ms) { return chrono::milliseconds(static_cast<chrono::milliseconds::rep>(ms)); }
constexpr chrono::duration<long double, milli>         operator""ms  (long double ms)        { return chrono::duration<long double, milli>(ms);                         }
constexpr chrono::microseconds                         operator""us  (unsigned long long us) { return chrono::microseconds(static_cast<chrono::microseconds::rep>(us)); }
constexpr chrono::duration<long double, micro>         operator""us  (long double us)        { return chrono::duration<long double, micro>(us);                         }
constexpr chrono::nanoseconds                          operator""ns  (unsigned long long ns) { return chrono::nanoseconds(static_cast<chrono::nanoseconds::rep>(ns));   }
constexpr chrono::duration<long double, nano>          operator""ns  (long double ns)        { return chrono::duration<long double, nano>(ns);                          }
#endif // !DEKAF2_IS_CLANG

#else // not missing chrono literals

// importing std::literals::chrono_literals into dekaf2::chrono::literals
using namespace std::literals::chrono_literals;

#endif // of missing chrono literals

#if !defined(DEKAF2_HAS_CHRONO_CALENDAR)
// d and y are not included until after C++17, but are also missing in older libs with C++20. Unfortunately we cannot
// inject them into clang, but for gcc the below works
#if !DEKAF2_IS_CLANG
// clang effectively makes it impossible to define system literals,
// so with clang we will always need C++20 to use the below literals (from the base lib)
#if 0
// as this does not work for all build environments we disable the injection of these literals
constexpr chrono::day  operator ""d (unsigned long long d) noexcept { return chrono::day (static_cast<unsigned>(d)); }
constexpr chrono::year operator ""y (unsigned long long y) noexcept { return chrono::year(static_cast<int>(y));      }
#endif
#endif
#endif

} // end of namespace dekaf2::chrono::literals

#if DEKAF2_IS_GCC && defined(DEKAF2_HAS_WARN_LITERAL_SUFFIX)
#pragma GCC diagnostic pop
#endif

#if DEKAF2_USE_HINNANT_TIMEZONE

#if !DEKAF2_USE_HINNANT_DATE && (!defined(DEKAF2_STD_CHRONO_HAS_LOCAL_T) || !DEKAF2_HAS_CPP20)
template <class Duration>
using local_time = date::local_time<Duration>;
using date::local_seconds;
using date::local_days;
#endif

using date::time_zone;
using date::current_zone;
using date::sys_info;
using date::tzdb;
using date::tzdb_list;
using date::get_tzdb;
using date::get_tzdb_list;
using date::get_leap_second_info;
using date::locate_zone;

// gcc aliases this, but clang does not and tells that only class templates can be type deduced (this is an alias on a class template)
// so always declare the template type for the first argument (normally chrono::system_clock::duration) or the build will fail on clang
template <class Duration, class TimeZonePtr = const date::time_zone*>
using zoned_time = date::zoned_time<Duration, TimeZonePtr>;

#endif

} // end of namespace dekaf2::chrono





namespace detail {

#if DEKAF2_USE_TIME_PUT
constexpr KStringView fDefaultDate { "%Y-%m-%d" };
#else
constexpr KStringView fDefaultDate { "{:%Y-%m-%d}" };
#endif

// gcc > 10 and its libs can currently not convert a ymd into local_days, as it is not yet
// prepared for local time conversions.
// Therefore we add a conversion into the neutral chrono::days, that can then be assigned
// to either local_days or sys_days
// (from https://howardhinnant.github.io/date_algorithms.html#days_from_civil )
// "Consider these donated to the public domain."
//-----------------------------------------------------------------------------
DEKAF2_NODISCARD
constexpr chrono::days days_from_civil(const chrono::year_month_day& ymd) noexcept
//-----------------------------------------------------------------------------
{
	      auto y = static_cast<int     >(ymd.year ());
	const auto m = static_cast<unsigned>(ymd.month());
	const auto d = static_cast<unsigned>(ymd.day  ());

	y -= m <= 2;

	const int      era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d-1;
	const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
	return chrono::days{ era * 146097 + static_cast<int>(doe) - 719468 };
}
// Howard Hinnant until here

// compute the weekday from ymd without going over days_from_civil
//-----------------------------------------------------------------------------
DEKAF2_NODISCARD
constexpr chrono::weekday weekday_from_civil(const chrono::year_month_day& ymd) noexcept
//-----------------------------------------------------------------------------
{
	      auto y = static_cast<int     >(ymd.year ());
	const auto m = static_cast<unsigned>(ymd.month());
	      auto d = static_cast<unsigned>(ymd.day  ());

	d += m <= 2 ? y-- : y - 2;
	
	return chrono::weekday{ (23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7 };
}

//-----------------------------------------------------------------------------
// Compile-time constant expression to compute a lookup table for the month length
// difference of all months following February from the expected naive month length
// of 31 stuffed into one 32 bit unsigned.
// The offset of a month start to the naive month start (month * 31) varies from
// 3 to 7, and needs thus 3 bits to represent, times 10 months from March to
// December = 30 bits.
// We could of course also simply hardcode the result for the lookup value,
// which is 462609335U, but constexpr explains better the algorithm behind..
constexpr uint32_t compute_lookup_offsets()
//-----------------------------------------------------------------------------
{
	uint32_t lookup_offsets = 0;
	uint16_t real_start     = 0;

	// we use a built-in array type, not a std::array, as that is constexpr with C++11..
	uint16_t month_start_offset[12] { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30 };

	for (int month = 1; month <= 12; ++month)
	{
		// 10 months from march til december
		real_start += month_start_offset[month - 1];

		if (month > 2)
		{
			uint16_t naive  = (month - 1) * 31;
			auto     offset = naive - real_start;
			// make room for the next offset
			lookup_offsets <<= 3;
			lookup_offsets  |= offset;
		}
	}

	return lookup_offsets;
}

//-----------------------------------------------------------------------------
// we do not want to use lookup tables in the calculation because that would require
// to do bounds checking, which we do not want to expense - so we use a lookup table
// for the month start offsets from a naive first calculation (m * 31) that is condensed
// into an unsigned integer and accessed by bit shifts - all in a constant expression.
DEKAF2_NODISCARD
constexpr chrono::days yearday_from_civil(const chrono::year_month_day& ymd) noexcept
//-----------------------------------------------------------------------------
{
	const auto y = static_cast<int     >(ymd.year ());
	const auto m = static_cast<unsigned>(ymd.month()) - 1;
	const auto d = static_cast<unsigned>(ymd.day  ());

	constexpr auto lookup_offsets = compute_lookup_offsets();

	return chrono::days{ m * 31 + d - ((m < 2)
	                                   ? 0
	                                   : ((lookup_offsets >> ((11 - m) * 3) & 7)
	                                      - (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)))) };
}

} // end of namespace detail

class KUnixTime;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a class representing a const date that can only be constructed, not changed, useful in local time contexts
class DEKAF2_PUBLIC KConstDate : public chrono::year_month_day
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KConstDate;
	using base = chrono::year_month_day;

	/// default construct to an invalid, but predictable date: 0/0/0
	constexpr KConstDate()                  noexcept : KConstDate(chrono::year(0)/0/0) {}
	constexpr KConstDate(const base& other) noexcept : base(other) {}
	constexpr KConstDate(base&& other)      noexcept : base(std::move(other)) {}

	// allow all base class constructors
	using base::base;

	/// return chrono::days
	DEKAF2_NODISCARD
	constexpr chrono::days    days           () const noexcept { return chrono::days  (unsigned(day()));   }
	/// return chrono::months
	DEKAF2_NODISCARD
	constexpr chrono::months  months         () const noexcept { return chrono::months(unsigned(month())); }
	/// return chrono::years
	DEKAF2_NODISCARD
	constexpr chrono::years   years          () const noexcept { return chrono::years (signed(year()));    }
	/// return weekday
	DEKAF2_NODISCARD
	constexpr chrono::weekday weekday        () const noexcept { return chrono::weekday(detail::weekday_from_civil(*this)); }
	/// return day of year (Jan 01 == 1)
	DEKAF2_NODISCARD
	constexpr chrono::days    yearday        () const noexcept { return detail::yearday_from_civil(*this); }
	/// returns true if year is leap year
	DEKAF2_NODISCARD
	constexpr bool            is_leap        () const noexcept { return year().is_leap();                  }
	/// returns the last day of the month
	DEKAF2_NODISCARD
	constexpr chrono::day     last_day       () const noexcept { return chrono::year_month_day_last(year(), chrono::month_day_last(month())).day(); }
	/// returns true if this is the last day of its month
	DEKAF2_NODISCARD
	constexpr bool            is_last_day    () const noexcept { return last_day() == day();               }
	/// returns true if day or month are zero (which is the state after default construction)
	DEKAF2_NODISCARD
	constexpr bool            empty          () const noexcept { return day() == chrono::day(0) || month() == chrono::month(0); }
	/// returns true if this is a valid date
	constexpr explicit        operator bool  () const noexcept { return ok();                              }

	/// return count of days in epoch
	DEKAF2_NODISCARD
	constexpr chrono::sys_days to_sys_days   () const noexcept { return chrono::sys_days(detail::days_from_civil(*this)); }
	/// return count of days in epoch
	DEKAF2_NODISCARD
	constexpr chrono::local_days to_local_days () const noexcept { return chrono::local_days(detail::days_from_civil(*this));  }
	/// return KUnixTime
	DEKAF2_NODISCARD
	constexpr KUnixTime       to_unix        ()  const noexcept; // implemented in ktime.h ..
	/// return struct tm (needed for efficient formatting)
	DEKAF2_NODISCARD
	constexpr std::tm         to_tm          ()  const noexcept;
	/// return a string following std::format patterns - default = {:%Y-%m-%d}
	DEKAF2_NODISCARD
	KString                   to_string      (KFormatString<const KConstDate&> sFormat) const noexcept; // implemented in ktime.h ..
	/// return a string following std::format patterns - default = {:%Y-%m-%d}
	DEKAF2_NODISCARD
	KString                   to_string      () const noexcept; // implemented in ktime.h ..
	/// return a string following std::format patterns, use given locale for formatting - default = {:%Y-%m-%d}
	DEKAF2_NODISCARD
	KString                   to_string      (const std::locale& locale, KFormatString<const KConstDate&> sFormat) const noexcept; // this one is implemented in ktime.h ..
	/// return a string following std::format patterns, use given locale for formatting - default = {:%Y-%m-%d}
	DEKAF2_NODISCARD
	KString                   to_string      (const std::locale& locale) const noexcept; // implemented in ktime.h ..

	// has also day()/month()/year() from its base

//--------
private:
//--------

	friend class KDate;

	constexpr self& operator+=(chrono::months m) noexcept { base::operator+=(m); return *this; }
	constexpr self& operator+=(chrono::years  y) noexcept { base::operator+=(y); return *this; }
	constexpr self& operator-=(chrono::months m) noexcept { base::operator-=(m); return *this; }
	constexpr self& operator-=(chrono::years  y) noexcept { base::operator-=(y); return *this; }

}; // KConstDate


//-----------------------------------------------------------------------------
DEKAF2_NODISCARD
constexpr std::tm KConstDate::to_tm () const noexcept
//-----------------------------------------------------------------------------
{
	// we do not know all fields of std::tm, therefore let's default initialize it
	std::tm tm{};

	tm.tm_mday   = days   ().count();
	tm.tm_mon    = months ().count() - 1;
	tm.tm_year   = years  ().count() - 1900;
	tm.tm_wday   = weekday().c_encoding();
	tm.tm_yday   = yearday().count() - 1;
	// tm_isdst is irrelevant in this context. Leave at 0 instead of setting to -1 to avoid
	// issues with code that only checks for is_dst == true.
#ifndef DEKAF2_IS_WINDOWS
	// tm_gmtoff stays at 0
	tm.tm_zone   = const_cast<char*>(""); // cannot be nullptr or format() crashes on %Z
#endif
	return tm;

} // KConstDate::to_tm


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A class representing a date that can be calculated and changed. This class follows more palpable
/// rules when adding/subtracting months, years and days than a chrono::duration calculation would:
/// Normally, adding a month or a year never changes the day. As that could lead to illegal dates (days
/// 29/30/31 do not exist in all months..) there are two member functions to fix the result: to_ceil() and to_floor():
/// - to_ceil() changes the date to the first day of the next month (if the day is invalid for the current month)
/// - to_floor() to the last day of the current month (if the day is invalid for the current month).
/// The addition and subtraction methods now automatically call to_floor(), as that is the result expected
/// by most humans.
/// However, when changing days, months, and years manually with the setter methods, it is your responsibility
/// to either call to_floor() or to_ceil() in the end to ensure a valid date.
class DEKAF2_PUBLIC KDate : public KConstDate
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KDate;
	using base = KConstDate;

	/// default construct to an invalid, but predictable date: 0/0/0
	KDate() = default;
	constexpr KDate(const base& other) noexcept : base(other) {}
	constexpr KDate(base&& other)      noexcept : base(std::move(other)) {}

	// allow all base class constructors
	using base::base;

	/// sets the day of the month - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& day  (chrono::day   d) noexcept { return *this = self(year(), month(), d); }
	/// sets the month of the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& month(chrono::month m) noexcept { return *this = self(year(), m, day());   }
	/// sets the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& year (chrono::year  y) noexcept { return *this = self(y, month(), day());  }

	/// sets the day of the month - does not check for last_day()- call floor() or ceil() to adjust
	constexpr self& day  (uint8_t       d) noexcept { return day  (chrono::day(d)  );          }
	/// sets the month of the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& month(uint8_t       m) noexcept { return month(chrono::month(m));          }
	/// sets the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& year (int16_t       y) noexcept { return year (chrono::year(y) );          }

	/// sets the day of the month from an indexed weeday (1..5) - does not check for last_day()- call floor() or ceil() to adjust
	constexpr self& weekday(chrono::weekday_indexed wi) noexcept { return *this = self(year()/month()/wi); }
	/// sets the day of the month from a last weeday
	constexpr self& weekday(chrono::weekday_last wl)    noexcept { return *this = self(year()/month()/wl); }

	using base::day;
	using base::month;
	using base::year;
	using base::weekday;

	constexpr self& operator+=(chrono::days   d) noexcept { return *this = chrono::sys_days(*this) + d;    }
	constexpr self& operator+=(chrono::months m) noexcept { base::operator+=(m); to_floor(); return *this; }
	constexpr self& operator+=(chrono::years  y) noexcept { base::operator+=(y); to_floor(); return *this; }
	constexpr self& operator-=(chrono::days   d) noexcept { return operator+=(-d);                         }
	constexpr self& operator-=(chrono::months m) noexcept { base::operator-=(m); to_floor(); return *this; }
	constexpr self& operator-=(chrono::years  y) noexcept { base::operator-=(y); to_floor(); return *this; }

	/// makes a day that is > last_day() the last_day() of the month - this is our general perception about how adding months works
	constexpr self& to_floor() noexcept;
	/// makes a day that is > last_day() the first day of the next month, except if the month is december, in which case the day will be set to 31
	constexpr self& to_ceil () noexcept;
	/// makes sure month is between 1..12 and day between 1..31 - does not adjust the day for last_day()..
	constexpr self& to_trunc() noexcept;
	/// sets the day, month and year to the next day with the given weekday. If times > 1 will add the respective week count
	constexpr self& to_next(chrono::weekday weekday, uint16_t times = 1);
	/// sets the day, month and year to the previous day with the given weekday. If times > 1 will subtract the respective week count
	constexpr self& to_previous(chrono::weekday weekday, uint16_t times = 1);

}; // KDate

// +- years
inline constexpr
KDate operator+(const KDate& left, const chrono::years& right) noexcept
{ KDate d = operator+(KDate::base(left), right); d.to_floor(); return d; }

inline constexpr
KDate operator+(const chrono::years& left, const KDate& right) noexcept
{ return right + left; }

inline constexpr
KDate operator-(const KDate& left, const chrono::years& right) noexcept
{ KDate d = operator-(KDate::base(left), right); d.to_floor(); return d; }

// +- months
inline constexpr
KDate operator+(const KDate& left, const chrono::months& right) noexcept
{ KDate d = operator+(KDate::base(left), right); d.to_floor(); return d; }

inline constexpr
KDate operator+(const chrono::months& left, const KDate& right) noexcept
{ return right + left; }

inline constexpr
KDate operator-(const KDate& left, const chrono::months& right) noexcept
{ KDate d = operator-(KDate::base(left), right); d.to_floor(); return d; }

// +- days
inline constexpr
KDate operator+(const KDate& left, const chrono::days& right) noexcept
{ return chrono::sys_days(left) + right; }

inline constexpr
KDate operator+(const chrono::days& left, const KDate& right) noexcept
{ return right + left; }

inline constexpr
KDate operator-(const KDate& left, const chrono::days& right) noexcept
{ return chrono::sys_days(left) - right; }

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to give easy access on years or months in a day count
class DEKAF2_PUBLIC KDays : public chrono::days
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KDays;
	using base = chrono::days;

	KDays() = default;
	constexpr KDays(const base& other) noexcept : base(other) {}
	constexpr KDays(base&& other)      noexcept : base(std::move(other)) {}

	// allow all base class constructors
	using base::base;

	/// return chrono::days
	DEKAF2_NODISCARD
	constexpr chrono::days    to_days        () const noexcept { return *this;                                      }
	/// return floored chrono::weeks
	DEKAF2_NODISCARD
	constexpr chrono::weeks   to_weeks       () const noexcept { return chrono::floor<chrono::weeks>(to_days());    }
	/// return floored chrono::months
	DEKAF2_NODISCARD
	constexpr chrono::months  to_months      () const noexcept { return chrono::floor<chrono::months>(to_days());   }
	/// return floored chrono::years
	DEKAF2_NODISCARD
	constexpr chrono::years   to_years       () const noexcept { return chrono::floor<chrono::years>(to_days());    }
	/// returns a string representation like 3d
	DEKAF2_NODISCARD
	KString                   to_string      () const;

}; // KDays

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to give easy access on broken down years, months, and days in a date diff
class DEKAF2_PUBLIC KDateDiff : public KDays
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self = KDateDiff;
	using base = KDays;

	KDateDiff() = default;

	/// construct from two KConstDate/KDate objects
	constexpr KDateDiff(const KConstDate& left, const KConstDate& right) noexcept;

	/// return broken down days in date diff
	DEKAF2_NODISCARD
	constexpr chrono::days    days        () const noexcept { return chrono::days(m_days);     }
	/// return broken down months in date diff
	DEKAF2_NODISCARD
	constexpr chrono::months  months      () const noexcept { return chrono::months(m_months); }
	/// return broken down years in date diff
	DEKAF2_NODISCARD
	constexpr chrono::years   years       () const noexcept { return chrono::years(m_years);   }
	/// returns true if date difference is negative
	DEKAF2_NODISCARD
	constexpr bool            is_negative () const noexcept { return m_is_negative;            }
	/// returns a string representation of the date difference, like 1y 2m 3d
	DEKAF2_NODISCARD
	KString                   to_string   () const;

//--------
private:
//--------

	uint16_t m_years       {};
	uint8_t  m_days        {};
	uint8_t  m_months      {};
	bool     m_is_negative {};

}; // KDateDiff

constexpr KDateDiff::KDateDiff(const KConstDate& left, const KConstDate& right) noexcept
: base(left.to_sys_days() - right.to_sys_days())
{
	m_is_negative = base(*this) < chrono::days(0);

	const auto& newer = (m_is_negative) ? right : left;
	const auto& older = (m_is_negative) ? left  : right;

	m_years = chrono::years(newer.year() - older.year()).count();

	if (newer.month() >= older.month())
	{
		m_months = chrono::months(newer.month() - older.month()).count();
	}
	else
	{
		m_months = unsigned(newer.month() + (chrono::December - older.month()));
		// and finally subtract one year, as we had created the count without
		// looking at the months
		--m_years;
	}

	if (newer.day() >= older.day())
	{
		m_days = chrono::days(newer.day() - older.day()).count();
	}
	else
	{
		// the days from the newer month
		m_days = unsigned(newer.day());
		// get the previous month
		auto month_before = (newer.month() <= chrono::January) ? chrono::December : --(newer.month());
		// get the year of the previous month
		auto year_before  = (newer.month() <= chrono::January) ? --newer.year() : newer.year();
		// caculate last day of the month of that year
		auto last_day = chrono::year_month_day_last(year_before, chrono::month_day_last(month_before)).day();
		// check how many days from the older day of month projected into the
		// previous month of the new date we still have to add
		if (last_day > older.day())
		{
			// else this was moved into the days past the last day of that month,
			// which we silently correct to the last day of that month and which
			// would result in 0 more days
			m_days += (last_day - older.day()).count();
		}
		// and finally subtract one month, as we had created the count without
		// looking at the days
		--m_months;
	}
}

inline constexpr
KDateDiff operator-(const KDate&      left, const KDate&      right)
{ return KDateDiff(left, right); }

inline constexpr
KDateDiff operator-(const KConstDate& left, const KConstDate& right)
{ return KDateDiff(left, right); }

inline constexpr
KDateDiff operator-(const KConstDate& left, const KDate&      right)
{ return KDateDiff(left, right); }

inline constexpr
KDateDiff operator-(const KDate&      left, const KConstDate& right)
{ return KDateDiff(left, right); }

inline constexpr
KDate& KDate::to_trunc() noexcept
{
	if      (month() < chrono::January ) month(chrono::January );
	else if (month() > chrono::December) month(chrono::December);
	if      (day  () < chrono::day(1)  ) day  (chrono::day(1)  );
	else if (day  () > chrono::day(31) ) day  (chrono::day(31) );
	return *this;
}

inline constexpr
KDate& KDate::to_floor() noexcept
{
	if (day() > chrono::day(28))
	{
		auto last = last_day();
		if (day() > last) day(last);
	}
	return *this;
}

inline constexpr
KDate& KDate::to_ceil() noexcept
{
	if (day() > chrono::day(28))
	{
		auto last = last_day();
		if (day() > last)
		{
			// prevent from overflow - this was an invalid date
			if (month() == chrono::December)
			{
				day(last);
			}
			else
			{
				day(chrono::day(1));
				*this += chrono::months(1);
			}
		}
	}
	return *this;
}

inline constexpr
KDate& KDate::to_next(chrono::weekday wd, uint16_t times)
{
	if (!times) return *this;
	--times;
	auto cur = weekday();
	return operator+=(chrono::days(((cur.c_encoding() < wd.c_encoding()) ? 0 : 7) + wd.c_encoding() - cur.c_encoding() + times * 7));
}

inline constexpr
KDate& KDate::to_previous(chrono::weekday wd, uint16_t times)
{
	if (!times) return *this;
	--times;
	auto cur = weekday();
	return operator-=(chrono::days(((wd.c_encoding() < cur.c_encoding()) ? 0 : 7) + cur.c_encoding() - wd.c_encoding() + times * 7));
}

DEKAF2_NAMESPACE_END

#if DEKAF2_HAS_INCLUDE("kformat.h")

// kFormat formatters

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE {

#if DEKAF2_HAS_FMT_FORMAT
template<> struct formatter<DEKAF2_PREFIX KConstDate> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KConstDate& date, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(date.to_tm(), ctx);
	}
};
#else
template<> struct formatter<DEKAF2_PREFIX KConstDate> : formatter<std::chrono::year_month_day>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KConstDate& date, FormatContext& ctx) const
	{
		return formatter<std::chrono::year_month_day>::format(std::chrono::year_month_day(date), ctx);
	}
};
#endif

#if DEKAF2_HAS_FMT_FORMAT
template<> struct formatter<DEKAF2_PREFIX KDate> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDate& date, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(date.to_tm(), ctx);
	}
};
#else
template<> struct formatter<DEKAF2_PREFIX KDate> : formatter<std::chrono::year_month_day>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDate& date, FormatContext& ctx) const
	{
		return formatter<std::chrono::year_month_day>::format(std::chrono::year_month_day(date), ctx);
	}
};
#endif

#if DEKAF2_HAS_FMT_FORMAT
template<> struct formatter<DEKAF2_PREFIX KDays> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDays& days, FormatContext& ctx) const
	{
		return formatter<string_view>::format(std::to_string(days.to_days().count()) + "d", ctx);
	}
};
#else
template<> struct formatter<DEKAF2_PREFIX KDays> : formatter<std::chrono::days>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDays& days, FormatContext& ctx) const
	{
		return formatter<std::chrono::days>::format(days.to_days(), ctx);
	}
};
#endif

template<> struct formatter<DEKAF2_PREFIX KDateDiff> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KDateDiff& datediff, FormatContext& ctx) const
	{
		return formatter<string_view>::format(datediff.to_string(), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // has #include "kformat.h"

#if DEKAF2_HAS_INCLUDE("bits/khash.h")

#include "bits/khash.h"

namespace std {

template<> struct hash<DEKAF2_PREFIX KConstDate>
{
	std::size_t operator()(DEKAF2_PREFIX KConstDate date) const noexcept
	{
		return DEKAF2_PREFIX kHash(&date, sizeof(date));
	}
};

template<> struct hash<DEKAF2_PREFIX KDate>
{
	std::size_t operator()(DEKAF2_PREFIX KDate date) const noexcept
	{
		return DEKAF2_PREFIX kHash(&date, sizeof(date));
	}
};

} // end of namespace std

DEKAF2_NAMESPACE_BEGIN

inline DEKAF2_PUBLIC
KString KDays::to_string() const
{
	return kFormat("{}", *this);
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KConstDate date)
{
	auto s = date.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KDate date)
{
	auto s = date.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KDays days)
{
	auto s = days.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KDateDiff datediff)
{
	auto s = datediff.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

DEKAF2_NAMESPACE_END

#endif // of has #include "bits/khash.h"
