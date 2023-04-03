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
/// make sure we have calendar extensions for std::chrono

#include "bits/kcppcompat.h"
#include <kconfiguration.h>

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
constexpr chrono::day  operator ""d (unsigned long long d) noexcept { return chrono::day (static_cast<unsigned>(d)); }
constexpr chrono::year operator ""y (unsigned long long y) noexcept { return chrono::year(static_cast<int>(y));      }
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

} // end of namespace dekaf2
