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
	#if defined(DEKAF2_HAS_CHRONO_CALENDAR) && DEKAF2_HAS_CPP_20
		#define DEKAF2_USE_HINNANT_DATE 0
	#else
		#define DEKAF2_USE_HINNANT_DATE 1
	#endif
#endif

#include <chrono>
#if DEKAF2_USE_HINNANT_DATE
	// the date lib does not compile the code for stream formatting with gcc 8
	// as we do not need it at all we switch it off (there are three if/endif
	// sections added to the original code)
	#ifndef DEKAF2_DATE_WITH_STREAMS_AND_FORMAT
		#define DEKAF2_DATE_WITH_STREAMS_AND_FORMAT 0
	#endif
	#include <date/date.h>
#endif

namespace dekaf2 {

namespace chrono {

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

using date::is_am;
using date::is_pm;
using date::make12;
using date::make24;
using date::make_time;

#endif

} // end of namespace (dekaf2::)chrono

} // end of namespace dekaf2
