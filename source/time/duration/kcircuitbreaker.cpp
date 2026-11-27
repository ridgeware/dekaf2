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


#include <dekaf2/time/duration/kcircuitbreaker.h>
#include <dekaf2/core/logging/klog.h>
#include <limits>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KCircuitBreaker::Call::Call(Call&& other) noexcept
//-----------------------------------------------------------------------------
: m_pBreaker(other.m_pBreaker)
, m_iGeneration(other.m_iGeneration)
, m_bAllowed(other.m_bAllowed)
{
	// the result is now reported by this object
	other.m_pBreaker = nullptr;

} // Call::ctor

//-----------------------------------------------------------------------------
KCircuitBreaker::Call::~Call()
//-----------------------------------------------------------------------------
{
	if (m_pBreaker)
	{
		m_pBreaker->Report(m_iGeneration, Result::None, KSteadyTime());
	}

} // Call::dtor

//-----------------------------------------------------------------------------
void KCircuitBreaker::Call::Success()
//-----------------------------------------------------------------------------
{
	if (m_pBreaker)
	{
		m_pBreaker->Report(m_iGeneration, Result::Success, KSteadyTime());
		m_pBreaker = nullptr;
	}

} // Call::Success

//-----------------------------------------------------------------------------
void KCircuitBreaker::Call::Failure(KSteadyTime Now)
//-----------------------------------------------------------------------------
{
	if (m_pBreaker)
	{
		m_pBreaker->Report(m_iGeneration, Result::Failure, Now);
		m_pBreaker = nullptr;
	}

} // Call::Failure

//-----------------------------------------------------------------------------
KCircuitBreaker::Call KCircuitBreaker::Try(KSteadyTime Now)
//-----------------------------------------------------------------------------
{
	// The generation is read before the state: if the state changes in between,
	// the call has the generation of an earlier state and its result does not count.
	auto iGeneration = m_iGeneration.load(std::memory_order_acquire);

	if (m_State.load(std::memory_order_acquire) == State::Closed)
	{
		return Call(*this, iGeneration);
	}

	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		switch (m_State.load())
		{
			case State::Closed:
				return Call(*this, m_iGeneration.load());

			case State::Open:
				if (Now < m_OpenUntil)
				{
					break;
				}

				// the open time has passed, this call is the probe
				SetState(State::HalfOpen);

				return Call(*this, m_iGeneration.load());

			case State::HalfOpen:
				break;
		}
	}

	// the call is refused, the log is written without the lock
	m_iRefused.fetch_add(1, std::memory_order_relaxed);

	kDebug(3, "'{}' is open, call refused", m_Options.sName);

	return Call();

} // Try

//-----------------------------------------------------------------------------
void KCircuitBreaker::Report(uint32_t iGeneration, Result Outcome, KSteadyTime Now)
//-----------------------------------------------------------------------------
{
	// a call from before the last change of the state does not count
	if (iGeneration != m_iGeneration.load(std::memory_order_acquire))
	{
		return;
	}

	// the common case, without the lock: a call that did not fail has nothing to
	// tell a closed breaker that counts no failures
	if (Outcome != Result::Failure
		&& m_State.load(std::memory_order_acquire) == State::Closed
		&& m_iFailures.load(std::memory_order_acquire) == 0)
	{
		return;
	}

	// what this report changed, for the log that is written without the lock
	enum class Change : uint8_t
	{
		None,
		Opened,
		StaysOpen,
		Closed
	};

	Change    Changed { Change::None };
	uint32_t  iFailed { 0 };
	KDuration OpenTime;

	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		if (iGeneration != m_iGeneration.load())
		{
			return;
		}

		switch (m_State.load())
		{
			case State::Closed:
				switch (Outcome)
				{
					case Result::Success:
						m_iFailures.store(0);
						break;

					case Result::Failure:
						iFailed = m_iFailures.fetch_add(1) + 1;

						// a limit of zero opens with the first failure as well
						if (iFailed >= m_Options.iFailures)
						{
							m_iOpenings.fetch_add(1, std::memory_order_relaxed);
							OpenTime = Open(1, Now);
							Changed  = Change::Opened;
						}
						break;

					case Result::None:
						break;
				}
				break;

			case State::Open:
				// no call has the generation of an open breaker
				break;

			case State::HalfOpen:
				// only the probe has the generation of a half open breaker
				switch (Outcome)
				{
					case Result::Success:
						m_iOpened   = 0;
						m_OpenUntil = KSteadyTime();
						SetState(State::Closed);
						Changed     = Change::Closed;
						break;

					case Result::Failure:
						OpenTime = Open(m_iOpened < std::numeric_limits<uint32_t>::max() ? m_iOpened + 1 : m_iOpened, Now);
						Changed  = Change::StaysOpen;
						break;

					case Result::None:
						// the open time has passed already: the next call is the probe
						SetState(State::Open);
						break;
				}
				break;
		}
	}

	switch (Changed)
	{
		case Change::None:
			break;

		case Change::Opened:
			kDebug(1, "'{}' opens for {}, failed calls in a row: {}", m_Options.sName, OpenTime, iFailed);
			break;

		case Change::StaysOpen:
			kDebug(1, "'{}' stays open for {}, the probe failed", m_Options.sName, OpenTime);
			break;

		case Change::Closed:
			kDebug(1, "'{}' closes, the probe succeeded", m_Options.sName);
			break;
	}

} // Report

//-----------------------------------------------------------------------------
void KCircuitBreaker::SetState(State NewState)
//-----------------------------------------------------------------------------
{
	// The generation is written before the state, see Try(): who reads the new
	// state has either read this generation, or an older one that no longer counts.
	m_iGeneration.fetch_add(1);
	m_State.store(NewState);

} // SetState

//-----------------------------------------------------------------------------
KDuration KCircuitBreaker::Open(uint32_t iOpened, KSteadyTime Now)
//-----------------------------------------------------------------------------
{
	auto OpenTime = KBackoff(m_Options.OpenTime).GetWait(iOpened);

	m_iOpened    = iOpened;
	m_OpenUntil  = Now;
	m_OpenUntil += OpenTime;
	m_iFailures.store(0);

	SetState(State::Open);

	return OpenTime;

} // Open

//-----------------------------------------------------------------------------
KSteadyTime KCircuitBreaker::GetOpenUntil() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	return m_OpenUntil;

} // GetOpenUntil

//-----------------------------------------------------------------------------
void KCircuitBreaker::Reset()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		m_iFailures.store(0);
		m_iOpened   = 0;
		m_OpenUntil = KSteadyTime();

		SetState(State::Closed);
	}

	kDebug(2, "'{}' is reset", m_Options.sName);

} // Reset

DEKAF2_NAMESPACE_END
