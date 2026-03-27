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

#include "kratelimiter.h"
#include "klog.h"
#include "kformat.h"

DEKAF2_NAMESPACE_BEGIN

static constexpr KDuration s_CleanupInterval = chrono::seconds(60);
static constexpr KDuration s_StaleTimeout    = chrono::seconds(60);

//-----------------------------------------------------------------------------
KRateLimiter::KRateLimiter(double dRequestsPerSecond, uint16_t iBurstSize)
//-----------------------------------------------------------------------------
: m_tLastCleanup(KSteadyTime::now())
, m_dRate(dRequestsPerSecond)
, m_iBurstSize(iBurstSize)
{
	if (m_dRate > 0 && m_iBurstSize < 1)
	{
		m_iBurstSize = 1;
	}

	if (m_dRate > 0)
	{
		kDebug(2, "rate limiter enabled: {:.1f} req/s, burst {}", m_dRate, m_iBurstSize);
	}

} // ctor

//-----------------------------------------------------------------------------
bool KRateLimiter::CheckImpl(KStringView sKey, KSteadyTime tNow)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	CleanupIfNeeded(tNow);

	auto it = m_Buckets.find(sKey);

	if (it == m_Buckets.end())
	{
		// new client: start with full burst minus one (this request)
		it = m_Buckets.emplace(KString(sKey), Bucket { static_cast<double>(m_iBurstSize) - 1.0, tNow }).first;
		return true;
	}

	auto& bucket = it->second;

	// refill tokens based on elapsed time
	auto tElapsed = tNow - bucket.tLastRefill;
	double dElapsedSeconds = chrono::duration_cast<chrono::microseconds>(tElapsed).count() / 1000000.0;

	if (dElapsedSeconds > 0)
	{
		bucket.dTokens = std::min(static_cast<double>(m_iBurstSize),
		                          bucket.dTokens + dElapsedSeconds * m_dRate);
		bucket.tLastRefill = tNow;
	}

	// try to consume one token
	if (bucket.dTokens >= 1.0)
	{
		bucket.dTokens -= 1.0;
		return true;
	}

	// rate limit exceeded
	kDebug(1, "rate limit exceeded for {}", sKey);

	return SetError(kFormat("rate limit exceeded ({:.0f} req/s)", m_dRate));

} // CheckImpl

//-----------------------------------------------------------------------------
std::size_t KRateLimiter::GetBucketCount() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_Buckets.size();

} // GetBucketCount

//-----------------------------------------------------------------------------
void KRateLimiter::Cleanup(KSteadyTime tNow)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	CleanupIfNeeded(tNow);

} // Cleanup

//-----------------------------------------------------------------------------
void KRateLimiter::CleanupIfNeeded(KSteadyTime tNow)
//-----------------------------------------------------------------------------
{
	// caller must hold m_Mutex

	if (tNow - m_tLastCleanup < s_CleanupInterval)
	{
		return;
	}

	m_tLastCleanup = tNow;

	auto iSizeBefore = m_Buckets.size();

	for (auto it = m_Buckets.begin(); it != m_Buckets.end(); )
	{
		if (tNow - it->second.tLastRefill > s_StaleTimeout)
		{
			it = m_Buckets.erase(it);
		}
		else
		{
			++it;
		}
	}

	auto iRemoved = iSizeBefore - m_Buckets.size();

	if (iRemoved > 0)
	{
		kDebug(3, "cleaned up {} stale rate limit buckets, {} remaining", iRemoved, m_Buckets.size());
	}

} // CleanupIfNeeded

DEKAF2_NAMESPACE_END
