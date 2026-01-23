/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kdefinitions.h"
#include "kthreadsafe.h"
#include "ktime.h"
#include <chrono>
#include <memory>
#include <thread>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <mutex>

/// @file ktimer.h
/// general timing facilities

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KTimer can be used to call functions both repeatedly after a fixed
/// time interval or once after expiration of a time interval, or at
/// a fixed time point. It can take up to 50 milliseconds (or, actually,
/// the MaxIdle time given at construction) until a new timer is started,
/// but from then on repeat intervals can be as short as needed, down
/// to micro- (MacOS) or nanoseconds (Linux).
/// KTimer does not start a timing thread until a timer is started.
class DEKAF2_PUBLIC KTimer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ID_t          = size_t;
	using Callback      = std::function<void(KUnixTime)>;
	// the following are for backward compatibility only, do not use them in new code
	using Timepoint     = KUnixTime;
	using Clock         = KUnixTime::clock;
	// backward compatibility until here

	static constexpr ID_t InvalidID { 0 };
	static constexpr KDuration Infinite { chrono::years(100) };

	//---------------------------------------------------------------------------
	/// create a new KTimer instance
	/// @param MaxIdle the maximum wait time until a new timer loop is started - new timers are only
	/// integrated when a new loop iteration starts - defaults to 50 milliseconds
	KTimer(KDuration MaxIdle = std::chrono::milliseconds(50));
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer(KTimer&&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	~KTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(const KTimer&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KTimer& operator=(KTimer&&) = delete;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// forces the main timing thread to pause before next round of execution, waits for Resume().
	/// Pause() will not return before the timing thread has paused.
	void Pause();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// if the main timing thread had been paused it will now resume
	void Resume();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb every interval in a separate thread.
	/// @param interval duration to wait between two calls of the callback
	/// @param CB the callback to call
	/// @param bOwnThread if true (default), the callback is called in a thread of its own, else it
	/// is called in the main timer loop thread and must return fastly and without blocking
	/// @return if unequal InvalidID a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallEvery(KDuration interval, Callback CB, bool bOwnThread = true);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb exactly once at a given timepoint
	/// @param timepoint a timepoint at which the callback is called. The callback will also be called
	/// if the timepoint lies in the past
	/// @param CB the callback to call
	/// @param bOwnThread if true (default), the callback is called in a thread of its own, else it
	/// is called in the main timer loop thread and must return fastly and without blocking
	/// @return if unequal InvalidID a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallOnce(KUnixTime timepoint, Callback CB, bool bOwnThread = true);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// nonblocking: calls cb exactly once after a certain duration from now has elapse
	/// @param interval duration to wait from now on at which the callback is called. The callback will also be called
	/// if the duration is negative.
	/// @param CB the callback to call
	/// @param bOwnThread if true (default), the callback is called in a thread of its own, else it
	/// is called in the main timer loop thread and must return fastly and without blocking
	/// @return if unequal InvalidID a handle that can be used
	/// to remove the callback (cancel the timer)
	ID_t CallOnce(KDuration interval, Callback CB, bool bOwnThread = true);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// cancel pending timer. Returns false if not found.
	bool Cancel(ID_t ID);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// restart pending timer (increase deadline by repeat interval from now). Returns false if not found or not
	/// a timer constructed with an interval (including timers triggered only once).
	bool Restart(ID_t ID);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// restart pending timer (increase deadline by repeat interval from now). Returns false if not found or not
	/// a timer constructed with an interval (including timers triggered only once). Uses new interval.
	bool Restart(ID_t ID, KDuration interval);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// restart pending timer by setting new deadline timepoint. Returns false if not found or if
	/// a timer constructed with an interval (including timers triggered only once).
	bool Restart(ID_t ID, KUnixTime timepoint);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep for interval (simple alias for std::this_thread::sleep_for)
	static void SleepFor(KDuration interval);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// blocking: sleep until timepoint (simple alias for std::this_thread::sleep_until)
	static void SleepUntil(KUnixTime timepoint);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template
	<
	        typename DstTimePoint,
	        typename SrcTimePoint,
	        typename DstClock = typename DstTimePoint::clock,
	        typename SrcClock = typename SrcTimePoint::clock
	        >
	/// Generic conversion function between two Timepoints of different
	/// source clocks. Involves three system calls, so is somewhat
	/// expensive. Offers precision in the one digit microsecond domain.
	static DstTimePoint TimepointCast(const SrcTimePoint tp)
	//---------------------------------------------------------------------------
	{
		if DEKAF2_CONSTEXPR_IF(std::is_same<SrcClock, DstClock>::value)
		{
			return tp;
		}
		else
		{
			const auto src_before = SrcClock::now();
			const auto dst_now    = DstClock::now();
			const auto src_after  = SrcClock::now();
			const auto src_diff   = src_after - src_before;
			const auto src_now    = src_before + src_diff / 2;
			return dst_now + (tp - src_now);
		}
	}

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template
	<
	        typename DstTimePoint,
	        typename ScrTimePoint,
	        typename DstDuration = typename DstTimePoint::duration,
	        typename SrcDuration = typename ScrTimePoint::duration,
	        typename DstClock    = typename DstTimePoint::clock,
	        typename SrcClock    = typename ScrTimePoint::clock
	        >
	/// Generic precision conversion function between two Timepoints of
	/// different source clocks. Involves up to fifteen system calls, so is
	/// expensive. Offers precision in the one digit nanosecond domain.
	static DstTimePoint TimepointPrecisionCast(
	        const ScrTimePoint tp,
	        const SrcDuration tolerance = std::chrono::nanoseconds {100},
	        int limit = 5)
	//---------------------------------------------------------------------------
	{
		if DEKAF2_CONSTEXPR_IF(std::is_same<SrcClock, DstClock>::value)
		{
			return tp;
		}
		else
		{
			if (limit < 1)
			{
				limit = 1;
			}

			auto itercnt = 0;
			auto src_now = ScrTimePoint {};
			auto dst_now = DstTimePoint {};
			auto epsilon = max_duration<SrcDuration>();
			do
			{
				const auto src_before  = SrcClock::now();
				const auto dst_between = DstClock::now();
				const auto src_after   = SrcClock::now();
				const auto src_diff    = src_after - src_before;
				const auto delta       = abs_duration(src_diff);
				if (delta < epsilon)
				{
					src_now = src_before + src_diff / 2;
					dst_now = dst_between;
					epsilon = delta;
				}
				if (++itercnt >= limit)
				{
					break;
				}
			}
			while (epsilon > tolerance);
			return dst_now + (tp - src_now);
		}
	}

	//---------------------------------------------------------------------------
	/// only call this directly after a fork() in the child instance, to make sure the rest of the class
	/// understands there is no thread running anymore..
	void CleanupChildAfterFork();
	//---------------------------------------------------------------------------

//----------
private:
//----------

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template <typename DurationT, typename ReprT = typename DurationT::rep>
	/// helper function for precision clock conversion
	static constexpr DurationT max_duration() noexcept
	//---------------------------------------------------------------------------
	{
		return DurationT { std::numeric_limits<ReprT>::max() };
	}

	//---------------------------------------------------------------------------
	// https://stackoverflow.com/a/35293183
	template <typename DurationT>
	/// helper function for precision clock conversion
	static constexpr DurationT abs_duration(const DurationT d) noexcept
	//---------------------------------------------------------------------------
	{
		return DurationT { (d.count() < 0) ? -d.count() : d.count() };
	}

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void TimingLoop(KDuration MaxIdle);
	//---------------------------------------------------------------------------

	struct Timer;

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	ID_t AddTimer(Timer timer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static void StartCallback();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static ID_t GetNextID();
	//---------------------------------------------------------------------------

	enum Flags : uint8_t
	{
		None      = 0,
		Once      = 1 << 0, // this timer shall be run only once
		OwnThread = 1 << 1, // the callback shall be called in its own thread
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Timer
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Timer() = default;
		Timer(KUnixTime timepoint, Callback CB, bool bOwnThread)
		: ExpiresAt(timepoint)
		, CB(std::move(CB))
		, Flags(bOwnThread ? static_cast<enum Flags>(OwnThread | Once) : Once)
		{
		}
		Timer(KDuration interval,  Callback CB, bool bOwnThread, bool bOnce)
		: ExpiresAt(KUnixTime::now() + interval)
		, Interval(interval)
		, CB(std::move(CB))
		, Flags(static_cast<enum Flags>((bOwnThread ? OwnThread : None) | (bOnce ? Once : None)))
		{
		}

		ID_t          ID       { InvalidID         };
		KUnixTime     ExpiresAt;
		KDuration     Interval { KDuration::zero() };
		Callback      CB       { nullptr           };
		enum Flags    Flags    { None              };
	};

	KDuration                          m_MaxIdle;
	std::mutex                         m_ThreadCreationMutex;
	std::unique_ptr<std::thread>       m_TimingThread;
	bool                               m_bShutdown         { false };
	bool                               m_bPause            { false };
	bool                               m_bIsPaused         { false };
	bool                               m_bAddedTimer       { false };

	KThreadSafe<std::unordered_map<ID_t, Timer>> m_Timers;

}; // KTimer

DEKAF2_NAMESPACE_END
