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

#include "bits/kcppcompat.h"
#include <kconfiguration.h>
#include "kstring.h"
#include "kstringview.h"
#include "bits/khash.h"

// we need to allow users of this lib to define a standard less than C++20
// even when our lib is compiled with C++20 and without Howard Hinnant's date lib -
// the latter is compatible to the known stdlibs and then offers the interface that the
// users would expect from our library
#ifndef DEKAF2_USE_HINNANT_DATE
	#if defined(DEKAF2_STD_CHRONO_HAS_CALENDAR) && DEKAF2_HAS_CPP_20
		#define DEKAF2_USE_HINNANT_DATE 0
	#else
		#define DEKAF2_USE_HINNANT_DATE 1
	#endif
#endif

#ifndef DEKAF2_USE_HINNANT_TIMEZONE
	#if defined(DEKAF2_STD_CHRONO_HAS_TIMEZONE) && DEKAF2_HAS_CPP_20
		#define DEKAF2_USE_HINNANT_TIMEZONE 0
	#else
		#define DEKAF2_USE_HINNANT_TIMEZONE 1
#endif
#endif

// again, when we built our lib with C++20, but someone else uses it
// with a previous standard, we have to undefine DEKAF2_STD_CHRONO_HAS_LOCAL_T
// because the chrono header will not make it visible then
#if DEKAF2_STD_CHRONO_HAS_LOCAL_T && !DEKAF2_HAS_CPP_20
	#undef DEKAF2_STD_CHRONO_HAS_LOCAL_T
#endif

#include <chrono>
#if DEKAF2_USE_HINNANT_DATE || !DEKAF2_HAS_CHRONO_ROUND
	// date:: only creates the is_clock check when we have void_t - and we supply
	// a void_t also for C++ < 17
	#define HAS_VOID_T 1
	#include <date/date.h>
#endif

#ifdef DEKAF2_USE_HINNANT_TIMEZONE
	#include <date/tz.h>
#endif

namespace dekaf2 {

namespace chrono {

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

// gcc > 10 and its libs can currently not convert a ymd into local_days, as it is not yet
// prepared for local time conversions.
// Therefore we add a conversion into the neutral chrono::days, that can then be assigned
// to either local_days or sys_days
// (from https://howardhinnant.github.io/date_algorithms.html#days_from_civil )
// "Consider these donated to the public domain."
//-----------------------------------------------------------------------------
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
constexpr chrono::weekday weekday_from_civil(const chrono::year_month_day& ymd) noexcept
//-----------------------------------------------------------------------------
{
	      auto y = static_cast<int     >(ymd.year ());
	const auto m = static_cast<unsigned>(ymd.month());
	      auto d = static_cast<unsigned>(ymd.day  ());

	d += m <= 2 ? y-- : y - 2;
	
	return chrono::weekday((23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7);
}

//-----------------------------------------------------------------------------
// Compile-time constant expression to compute a lookup table for all months
// following February stuffed into one 32 bit unsigned.
// The offset of a month start to the naive month start (month * 31) varies from
// 3 to 7, and needs thus 3 bits to represent, times 10 months from March to
// December = 30 bits.
// We could of course also simply hardcode the result for the lookup value,
// which is 462609335U, but constexpr explains better the algorithm behind..
constexpr inline uint32_t compute_lookup_offsets()
//-----------------------------------------------------------------------------
{
	uint32_t lookup_offsets = 0;
	uint8_t  real_start     = 0;

	std::array<uint16_t, 12> month_start_offset {{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30 }};

	for (int month = 1; month <= 12; ++month)
	{
		// 10 months from march til december
		real_start += month_start_offset[month - 1];

		if (month > 2)
		{
			uint8_t naive  = (month - 1) * 31;
			auto    offset = naive - real_start;
			// make room for the next offset
			lookup_offsets <<= 3;
			lookup_offsets |= offset;
		}
	}

	return lookup_offsets;
}

//-----------------------------------------------------------------------------
// we do not want to use lookup tables in the calculation because that would require
// to do bounds checking, which we do not want to expense - so we use a lookup table
// for the month start offsets from a naive first calculation (m * 31) that is condensed
// into an unsigned integer and accessed by bit shifts - all in a constant expression.
constexpr chrono::days yearday_from_civil(const chrono::year_month_day& ymd) noexcept
//-----------------------------------------------------------------------------
{
	const auto y = static_cast<int     >(ymd.year ());
	const auto m = static_cast<unsigned>(ymd.month()) - 1;
	const auto d = static_cast<unsigned>(ymd.day  ());

	constexpr auto lookup_offset = compute_lookup_offsets();

	return chrono::days{ m * 31 + d - ((m < 2)
									   ? 0
									   : ((lookup_offset >> ((11 - m) * 3) & 7)
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
	constexpr chrono::days    days           () const noexcept { return chrono::days  (unsigned(day()));   }
	/// return chrono::months
	constexpr chrono::months  months         () const noexcept { return chrono::months(unsigned(month())); }
	/// return chrono::years
	constexpr chrono::years   years          () const noexcept { return chrono::years (signed(year()));    }
	/// return weekday (Sunday == 0)
	constexpr chrono::weekday weekday        () const noexcept { return chrono::weekday(detail::weekday_from_civil(*this)); }
	/// return day of year (Jan 01 == 1)
	constexpr chrono::days    yearday        () const noexcept { return detail::yearday_from_civil(*this); }
	/// returns true if year is leap year
	constexpr bool            is_leap        () const noexcept { return year().is_leap();                  }
	/// returns the last day of the month
	constexpr chrono::day     last_day       () const noexcept { return chrono::year_month_day_last(year(), chrono::month_day_last(month())).day(); }
	/// returns true if this is the last day of its month
	constexpr bool            is_last_day    () const noexcept { return last_day() == day();               }
	/// returns true if day or month are zero (which is the state after default construction)
	constexpr bool            empty          () const noexcept { return day() == chrono::day(0) || month() == chrono::month(0); }
	/// returns true if this is a valid date
	constexpr /*explicit*/    operator bool  () const noexcept { return ok();                              }

	/// return count of days in epoch
	constexpr chrono::sys_days to_sys_days   () const noexcept { return chrono::sys_days(detail::days_from_civil(*this)); }
	/// return count of days in epoch
	constexpr chrono::local_days to_local_days () const noexcept { return chrono::local_days(detail::days_from_civil(*this));  }
	/// return KUnixTime
	constexpr KUnixTime       to_unix        ()  const noexcept; // this one is implemented in ktime.h ..
	/// return struct tm (needed for efficient formatting)
	constexpr std::tm         to_tm          ()  const noexcept;
	/// return a string following std::format patterns - default = %Y-%m-%d
	KString                   Format         (KStringView sFormat = "%Y-%m-%d") const { return to_string(sFormat);    }
	/// return a string following std::format patterns - default = %Y-%m-%d
	KString                   to_string      (KStringView sFormat = "%Y-%m-%d") const;

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
constexpr std::tm KConstDate::to_tm () const noexcept
//-----------------------------------------------------------------------------
{
	// we do not know all fields of std::tm, therefore let's default initialize it
	std::tm tm{};

	tm.tm_sec    = 0;
	tm.tm_min    = 0;
	tm.tm_hour   = 0;
	tm.tm_mday   = days   ().count();
	tm.tm_mon    = months ().count() - 1;
	tm.tm_year   = years  ().count() - 1900;
	tm.tm_wday   = weekday().c_encoding();
	tm.tm_yday   = (chrono::sys_days(*this) - chrono::sys_days(chrono::year_month_day(year()/1/1))).count();
	tm.tm_isdst  = 0; // we do not know this ..
#ifndef DEKAF2_IS_WINDOWS
	tm.tm_gmtoff = 0;
	tm.tm_zone   = const_cast<char*>("");
#endif
	return tm;

} // KConstDate::to_tm


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A class representing a date that can be calculated and changed. This class follows more palpable
/// rules when adding/subtracting months, years and days than a chrono::duration calculation would:
/// Adding a month or a year never changes the day. As that could lead to illegal dates (days 29/30/31
/// do not exist in all months..) there are two member functions to fix the result: ceil() and floor():
/// ceil() changes the date to the first day of the next month (if the day is invalid for the current month)
/// floor() to the last day of the current month (if the day is invalid for the current month).
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
	constexpr self& day  (uint8_t       d) noexcept { return *this = self(year(), month(), chrono::day(d)); }
	/// sets the month of the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& month(uint8_t       m) noexcept { return *this = self(year(), chrono::month(m), day()); }
	/// sets the year - does not check for last_day() - call floor() or ceil() to adjust
	constexpr self& year (int16_t       y) noexcept { return *this = self(chrono::year(y), month(), day()); }
	// consider weekday(weekday[index]/last)

	using base::day;
	using base::month;
	using base::year;

	constexpr self& operator+=(chrono::days   d) noexcept { return *this = chrono::sys_days(*this) + d; }
	constexpr self& operator+=(chrono::months m) noexcept { base::operator+=(m); return *this; }
	constexpr self& operator+=(chrono::years  y) noexcept { base::operator+=(y); return *this; }
	constexpr self& operator-=(chrono::days   d) noexcept { return operator+=(-d);             }
	constexpr self& operator-=(chrono::months m) noexcept { base::operator-=(m); return *this; }
	constexpr self& operator-=(chrono::years  y) noexcept { base::operator-=(y); return *this; }

	/// makes a day that is > last_day() the last_day() of the month
	constexpr self& floor() noexcept;
	/// makes a day that is > last_day() the first day of the next month
	constexpr self& ceil () noexcept;
	/// makes sure month is between 1..12 and day between 1..31 - does not adjust the day for last_day()..
	constexpr self& trunc() noexcept;

}; // KDate

// +- years
inline constexpr
KDate operator+(const KDate& left, const chrono::years& right) noexcept
{ return operator+(KDate::base(left), right); }

inline constexpr
KDate operator+(const chrono::years& left, const KDate& right) noexcept
{ return right + left; }

inline constexpr
KDate operator-(const KDate& left, const chrono::years& right) noexcept
{ return operator-(KDate::base(left), right); }

// +- months
inline constexpr
KDate operator+(const KDate& left, const chrono::months& right) noexcept
{ return operator+(KDate::base(left), right); }

inline constexpr
KDate operator+(const chrono::months& left, const KDate& right) noexcept
{ return right + left; }

inline constexpr
KDate operator-(const KDate& left, const chrono::months& right) noexcept
{ return operator-(KDate::base(left), right); }

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

inline constexpr
chrono::days operator-(const KDate&      left, const KDate&      right)
{ return left.to_sys_days() - right.to_sys_days(); }

inline constexpr
chrono::days operator-(const KConstDate& left, const KConstDate& right)
{ return left.to_sys_days() - right.to_sys_days(); }

inline constexpr
chrono::days operator-(const KConstDate& left, const KDate&      right)
{ return left.to_sys_days() - right.to_sys_days(); }

inline constexpr
chrono::days operator-(const KDate&      left, const KConstDate& right)
{ return left.to_sys_days() - right.to_sys_days(); }

inline constexpr
KDate& KDate::trunc() noexcept
{
	if      (month() < chrono::January ) month(chrono::January );
	else if (month() > chrono::December) month(chrono::December);
	if      (day  () < chrono::day(1)  ) day  (chrono::day(1)  );
	else if (day  () > chrono::day(31) ) day  (chrono::day(31) );
	return *this;
}

inline constexpr
KDate& KDate::floor() noexcept
{
	if (day() > chrono::day(28))
	{
		auto last = last_day();
		if (day() > last) day(last);
	}
	return *this;
}

inline constexpr
KDate& KDate::ceil() noexcept
{
	if (day() > chrono::day(28))
	{
		auto last = last_day();
		if (day() > last)
		{
			day(chrono::day(1));
			*this += chrono::months(1);
		}
	}
	return *this;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KConstDate time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

inline DEKAF2_PUBLIC
std::ostream& operator<<(std::ostream& stream, KDate time)
{
	auto s = time.to_string();
	stream.write(s.data(), s.size());
	return stream;
}

} // end of namespace dekaf2

namespace fmt {

template<> struct formatter<dekaf2::KConstDate> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KConstDate& date, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(date.to_tm(), ctx);
	}
};

template<> struct formatter<dekaf2::KDate> : formatter<std::tm>
{
	template <typename FormatContext>
	auto format(const dekaf2::KDate& date, FormatContext& ctx) const
	{
		return formatter<std::tm>::format(date.to_tm(), ctx);
	}
};

} // end of namespace fmt

namespace std {

template<> struct hash<dekaf2::KConstDate>
{
	std::size_t operator()(dekaf2::KConstDate date) const noexcept
	{
		return dekaf2::kHash(&date, sizeof(date));
	}
};

template<> struct hash<dekaf2::KDate>
{
	std::size_t operator()(dekaf2::KDate date) const noexcept
	{
		return dekaf2::kHash(&date, sizeof(date));
	}
};

} // end of namespace std
