/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

/// @file kjuliandate.h
/// julian date clock

#include "kdefinitions.h"
#include "ktime.h"
#include "kdate.h"

DEKAF2_NAMESPACE_BEGIN

//=============================================================================
// Julian date from https://stackoverflow.com/a/33964462/576911 (hinnant)

namespace chrono {

struct jdate_clock;

template <class Duration>
using jdate_time = std::chrono::time_point<jdate_clock, Duration>;

struct jdate_clock
{
	using rep        = double;
	using period     = std::chrono::days::period;
	using duration   = std::chrono::duration<rep, period>;
	using time_point = std::chrono::time_point<jdate_clock>;

	static constexpr bool is_steady = false;

	static time_point now() noexcept;

	template <class Duration> DEKAF2_CONSTEXPR_14
	static auto from_sys(std::chrono::sys_time<Duration> const& tp) noexcept;

	template <class Duration> DEKAF2_CONSTEXPR_14
	static auto to_sys(jdate_time<Duration> const& tp) noexcept;
};

template <class Duration> DEKAF2_CONSTEXPR_14
auto jdate_clock::from_sys(std::chrono::sys_time<Duration> const& tp) noexcept
{
	using namespace std;
	using namespace chrono;
	auto constexpr epoch = sys_days{November/24/-4713} + 12h;
	using ddays = std::chrono::duration<long double, days::period>;

	if DEKAF2_CONSTEXPR_IF (sys_time<ddays>{sys_time<Duration>::min()} < sys_time<ddays>{epoch})
	{
		return jdate_time{tp - epoch};
	}
	else
	{
		// Duration overflows at the epoch.  Sub in new Duration that won't overflow.
		using D = std::chrono::duration<int64_t, ratio<1, 10'000'000>>;
		return jdate_time{round<D>(tp) - epoch};
	}
}

template <class Duration> DEKAF2_CONSTEXPR_14
auto jdate_clock::to_sys(jdate_time<Duration> const& tp) noexcept
{
	using namespace std::chrono;
	return sys_time{tp - chrono::clock_cast<jdate_clock>(sys_days{})};
}

inline
jdate_clock::time_point jdate_clock::now() noexcept
{
	using namespace std::chrono;
	return chrono::clock_cast<jdate_clock>(system_clock::now());
}

} // end of namespace dekaf2::chrono

// end of Julian clock / time
//=============================================================================

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Julian Date
class DEKAF2_PUBLIC KJulianDate : public chrono::jdate_clock::time_point
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	using self       = KJulianDate;
	using clock      = chrono::jdate_clock;
	using base       = clock::time_point;
	using time_point = clock::time_point;
	using duration   = clock::duration;

	/// default ctor, initializes with epoch (0)
	KJulianDate() = default;
	DEKAF2_CONSTEXPR_14          KJulianDate(const base& other) noexcept : base(other) {}
	DEKAF2_CONSTEXPR_14          KJulianDate(base&& other)      noexcept : base(std::move(other)) {}

	/// construct from time_t timepoint (constexpr)
	DEKAF2_CONSTEXPR_14 explicit KJulianDate(KUnixTime time)    noexcept : base(clock::from_sys(time)) {}
	DEKAF2_CONSTEXPR_14 explicit KJulianDate(double time)       noexcept : base(duration(time)) {}
	DEKAF2_CONSTEXPR_14 explicit KJulianDate(chrono::days time) noexcept : base(duration(time)) {}
	DEKAF2_CONSTEXPR_14 explicit KJulianDate(long time)         noexcept : KJulianDate(chrono::days(time)) {}

	using base::base;
	using base::operator+=;
	using base::operator-=;
	using base::operator=;

	/// converts to KUnixTime timepoint
	DEKAF2_NODISCARD
	DEKAF2_CONSTEXPR_14 KUnixTime   to_unix ()            const noexcept { return to_sys_time(); }
	/// converts to std::chrono::sys_time
	DEKAF2_NODISCARD
	DEKAF2_CONSTEXPR_14 chrono::sys_time<chrono::system_clock::duration>
	                            to_sys_time ()            const noexcept { return chrono::round<chrono::system_clock::duration>(chrono::clock_cast<chrono::system_clock>(*this)); }
	DEKAF2_NODISCARD
	DEKAF2_CONSTEXPR_14 double    to_double ()            const noexcept { return time_since_epoch().count(); }
	DEKAF2_NODISCARD    KString   to_string ()            const noexcept;
	DEKAF2_NODISCARD
	DEKAF2_CONSTEXPR_14 bool             ok ()            const noexcept { return time_since_epoch() != duration::zero(); }

	DEKAF2_CONSTEXPR_14 explicit operator KUnixTime()     const noexcept { return to_unix();    }

	/// returns a KUnixTime with the current time
	DEKAF2_NODISCARD static KJulianDate now ()                  noexcept { return clock::now(); }
	DEKAF2_NODISCARD
	constexpr        static KJulianDate min ()                  noexcept { return base::min();  }
	DEKAF2_NODISCARD
	constexpr        static KJulianDate max ()                  noexcept { return base::max();  }

//--------
private:
//--------

	// we do not permit addition or subtraction of years and months
	// - they will always lead to unexpected results
	constexpr void operator+=(chrono::months) noexcept {}
	constexpr void operator+=(chrono::years ) noexcept {}
	constexpr void operator-=(chrono::months) noexcept {}
	constexpr void operator-=(chrono::years ) noexcept {}

}; // KJulianDate

DEKAF2_NAMESPACE_END
