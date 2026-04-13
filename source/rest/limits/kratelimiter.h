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

/// @file kratelimiter.h
/// Token bucket rate limiter for request throttling

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup rest_limits
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Token bucket rate limiter. Thread-safe. When rate is 0 (the default),
/// the limiter is completely disabled and Check() is a no-op with zero cost.
/// By default, Check() returns false and sets an error message when a client
/// exceeds its rate limit. With SetThrowOnError(true), it throws instead.
class DEKAF2_PUBLIC KRateLimiter : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default constructor - rate limiter is disabled (zero cost)
	KRateLimiter() = default;

	/// construct an active rate limiter
	/// @param dRequestsPerSecond maximum sustained request rate per key
	/// @param iBurstSize maximum burst capacity (tokens). Must be >= 1 when rate > 0.
	KRateLimiter(double dRequestsPerSecond, uint16_t iBurstSize = 10);

	/// check if this rate limiter is enabled
	DEKAF2_NODISCARD
	bool IsEnabled() const { return m_dRate > 0; }

	/// check rate limit for the given key (typically a client IP).
	/// @return true if the request is allowed, false if rate limited.
	/// When rate limited, the error message is available via GetLastError().
	/// With SetThrowOnError(true), throws KException instead of returning false.
	/// no-op (returns true) if the limiter is disabled.
	/// @param sKey the rate limit key (e.g. client IP address)
	DEKAF2_NODISCARD
	bool Check(KStringView sKey)
	{
		if (!IsEnabled()) return true;
		return CheckImpl(sKey, KSteadyTime::now());
	}

	/// check rate limit for the given key at a specific time (for testing).
	/// @return true if the request is allowed, false if rate limited.
	/// @param sKey the rate limit key
	/// @param tNow the current time
	DEKAF2_NODISCARD
	bool Check(KStringView sKey, KSteadyTime tNow)
	{
		if (!IsEnabled()) return true;
		return CheckImpl(sKey, tNow);
	}

	/// return the number of tracked keys (buckets)
	DEKAF2_NODISCARD
	std::size_t GetBucketCount() const;

	/// manually trigger cleanup of stale buckets
	/// @param tNow the current time
	void Cleanup(KSteadyTime tNow);

//------
private:
//------

	struct Bucket
	{
		double      dTokens;
		KSteadyTime tLastRefill;
	};

	bool CheckImpl(KStringView sKey, KSteadyTime tNow);
	void CleanupIfNeeded(KSteadyTime tNow);

	mutable std::mutex m_Mutex;
	KUnorderedMap<KString, Bucket>  m_Buckets;
	KSteadyTime m_tLastCleanup;
	double      m_dRate      { 0 };  // tokens per second
	uint16_t    m_iBurstSize { 0 };  // max tokens (burst capacity)

}; // KRateLimiter


/// @}

DEKAF2_NAMESPACE_END
