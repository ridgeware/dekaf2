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

/// @file kcircuitbreaker.h
/// refuses calls to a service that keeps failing

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/kbackoff.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup time_duration
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Refuses the calls to a service that keeps failing, so that not every caller
/// waits for its own timeout. One object is shared by all threads that call the
/// same service.
///
/// The breaker is closed while the service works. A count of failed calls in a
/// row opens it: calls are then refused, for a time that grows with every
/// further failure. After that time one call is let through as a probe. If it
/// succeeds the breaker closes, if it fails the breaker stays open for the next,
/// longer time.
///
/// The caller decides what a failure is - a connection that could not be made, a
/// timeout, a server error. An answer that refuses the request is none.
///
/// The breaker writes to the debug log when it opens and when it closes, with
/// Options::sName to tell the breakers of a program apart.
///
/// @code
/// auto Call = Breaker.Try();
///
/// if (!Call)
/// {
///     return Error("service not available");
/// }
///
/// if (SendRequest())
/// {
///     Call.Success();
/// }
/// else
/// {
///     Call.Failure();
/// }
/// @endcode
class DEKAF2_PUBLIC KCircuitBreaker
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	enum class State : uint8_t
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Closed,   ///< calls pass
		Open,     ///< calls are refused, until the open time has passed and a call takes the probe
		HalfOpen  ///< one call is under way as the probe, all others are refused
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Options() = default;

		/// @param iFailures the count of failed calls in a row that opens the breaker
		/// @param OpenTime the time the breaker stays open
		/// @param sName the name of the service, for the log
		Options(uint32_t iFailures, KBackoff::Options OpenTime, KString sName = KString{})
		: iFailures(iFailures)
		, OpenTime(std::move(OpenTime))
		, sName(std::move(sName))
		{
		}

		/// the count of failed calls in a row that opens the breaker
		uint32_t          iFailures { 5 };
		/// the time the breaker stays open: the initial time after it opened, a longer
		/// one after every probe that failed. A factor of 1 keeps the time.
		KBackoff::Options OpenTime  { chrono::seconds(5), chrono::minutes(1) };
		/// the name of the service, for the log
		KString           sName;

	}; // Options

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// One call to the service. It tells if the call may be made, and takes its
	/// result. A call that reports nothing changes nothing.
	class DEKAF2_PUBLIC Call
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KCircuitBreaker;

	//------
	public:
	//------

		/// a call that is refused
		Call() = default;
		Call(Call&& other) noexcept;
		Call(const Call&) = delete;
		Call& operator=(Call&&) = delete;
		Call& operator=(const Call&) = delete;
		~Call();

		/// @return true if the call may be made
		explicit operator bool() const
		{
			return m_bAllowed;
		}

		/// The service answered. Only the first report of a call counts.
		void Success();

		/// The service did not answer, or failed. Only the first report of a call counts.
		void Failure(KSteadyTime Now = KSteadyTime::now());

	//------
	private:
	//------

		Call(KCircuitBreaker& Breaker, uint32_t iGeneration)
		: m_pBreaker(&Breaker)
		, m_iGeneration(iGeneration)
		, m_bAllowed(true)
		{
		}

		// null for a refused call, and after the report
		KCircuitBreaker* m_pBreaker    { nullptr };
		uint32_t         m_iGeneration { 0 };
		bool             m_bAllowed    { false };

	}; // Call

	//-----------------------------------------------------------------------------
	KCircuitBreaker() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	explicit KCircuitBreaker(Options Options)
	//-----------------------------------------------------------------------------
	: m_Options(std::move(Options))
	{
	}

	KCircuitBreaker(const KCircuitBreaker&) = delete;
	KCircuitBreaker& operator=(const KCircuitBreaker&) = delete;

	//-----------------------------------------------------------------------------
	/// Ask for a call to the service
	/// @return a Call that is true if the call may be made, and that takes its result
	DEKAF2_NODISCARD
	Call Try(KSteadyTime Now = KSteadyTime::now());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	State GetState() const
	//-----------------------------------------------------------------------------
	{
		return m_State.load(std::memory_order_acquire);
	}

	//-----------------------------------------------------------------------------
	/// @return the time until which an open breaker refuses all calls, the epoch for a
	/// closed one
	DEKAF2_NODISCARD
	KSteadyTime GetOpenUntil() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Close the breaker and forget all failures. Calls that are under way no longer
	/// count.
	void Reset();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return how often the breaker opened since its construction, not counting the
	/// probes that failed
	DEKAF2_NODISCARD
	uint64_t GetOpenings() const
	//-----------------------------------------------------------------------------
	{
		return m_iOpenings.load(std::memory_order_relaxed);
	}

	//-----------------------------------------------------------------------------
	/// @return the count of refused calls since the construction
	DEKAF2_NODISCARD
	uint64_t GetRefused() const
	//-----------------------------------------------------------------------------
	{
		return m_iRefused.load(std::memory_order_relaxed);
	}

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

	enum class Result : uint8_t
	{
		Success,
		Failure,
		None      ///< the call ended without a report
	};

	/// takes the result of a call that was let through
	DEKAF2_PRIVATE
	void Report(uint32_t iGeneration, Result Outcome, KSteadyTime Now);

	/// every change of the state starts a new generation - with m_Mutex held
	DEKAF2_PRIVATE
	void SetState(State NewState);

	/// opens the breaker
	/// @param iOpened the count of times in a row it opened, this one included
	/// @return the time it stays open
	DEKAF2_PRIVATE
	KDuration Open(uint32_t iOpened, KSteadyTime Now);

	Options               m_Options;
	mutable std::mutex    m_Mutex;
	// written with m_Mutex held, read also without it
	std::atomic<State>    m_State       { State::Closed };
	std::atomic<uint32_t> m_iGeneration { 0 };
	// the failed calls in a row while the breaker is closed
	std::atomic<uint32_t> m_iFailures   { 0 };
	std::atomic<uint64_t> m_iOpenings   { 0 };
	std::atomic<uint64_t> m_iRefused    { 0 };
	// only with m_Mutex held: the count of times in a row the breaker opened without
	// a successful probe in between, and the end of the current open time
	uint32_t              m_iOpened     { 0 };
	KSteadyTime           m_OpenUntil;

}; // KCircuitBreaker

/// @}

DEKAF2_NAMESPACE_END
