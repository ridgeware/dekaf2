/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file kbackoff.h
/// the time to wait between repeated attempts

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/time/duration/kduration.h>
#include <cstdint>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup time_duration
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Computes the time to wait before an attempt is repeated: it grows with every
/// failed attempt up to a limit, and a random share of it spreads the attempts of
/// clients that failed at the same time.
///
/// @code
/// KBackoff Backoff;
///
/// while (!bStop)
/// {
///     if (Connect())
///     {
///         Backoff.Reset();
///         Run();
///     }
///
///     WaitUnlessStopped(Backoff.Next());
/// }
/// @endcode
///
/// Next() and Reset() write to the debug log, GetWait() and GetMaxWait() do not.
class DEKAF2_PUBLIC KBackoff
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Options() = default;

		/// @param Initial the time to wait after the first failed attempt
		/// @param Max the upper limit of the time to wait
		Options(KDuration Initial, KDuration Max)
		: Initial(Initial)
		, Max(Max)
		{
		}

		/// the time to wait after the first failed attempt
		KDuration Initial         { chrono::seconds(1) };
		/// the upper limit of the time to wait
		KDuration Max             { chrono::minutes(1) };
		/// the factor by which the time grows with every further failed attempt
		double    dFactor         { 2.0 };
		/// the share of the time that is random, from 0 to 1: with 0 the time is exact, with
		/// 0.5 between its half and the whole, with 1 anywhere between zero and the whole
		double    dJitter         { 0.5 };
		/// do not wait after the first failed attempt - for an attempt that may have failed
		/// only because a kept connection was closed by the other side. The time then grows
		/// from the second failed attempt on.
		bool      bImmediateFirst { false };

	}; // Options

	//-----------------------------------------------------------------------------
	KBackoff() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	explicit KBackoff(Options Options)
	//-----------------------------------------------------------------------------
	: m_Options(std::move(Options))
	{
	}

	//-----------------------------------------------------------------------------
	/// @param Initial the time to wait after the first failed attempt
	/// @param Max the upper limit of the time to wait
	KBackoff(KDuration Initial, KDuration Max)
	//-----------------------------------------------------------------------------
	: m_Options(Initial, Max)
	{
	}

	//-----------------------------------------------------------------------------
	/// Count a failed attempt
	/// @return the time to wait before the next attempt
	KDuration Next();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Forget the failed attempts, after an attempt that succeeded
	void Reset();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the count of failed attempts since the construction or the last Reset()
	DEKAF2_NODISCARD
	uint32_t GetFailures() const
	//-----------------------------------------------------------------------------
	{
		return m_iFailures;
	}

	//-----------------------------------------------------------------------------
	/// @param iFailures a count of failed attempts in a row
	/// @return the time to wait after that many failed attempts, zero for none - for a
	/// caller that keeps the count itself
	DEKAF2_NODISCARD
	KDuration GetWait(uint32_t iFailures) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param iFailures a count of failed attempts in a row
	/// @return the longest time that GetWait() returns for that count, the time without
	/// its random share
	DEKAF2_NODISCARD
	KDuration GetMaxWait(uint32_t iFailures) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	const Options& GetOptions() const
	//-----------------------------------------------------------------------------
	{
		return m_Options;
	}

//------
private:
//------

	Options  m_Options;
	uint32_t m_iFailures { 0 };

}; // KBackoff

/// @}

DEKAF2_NAMESPACE_END
