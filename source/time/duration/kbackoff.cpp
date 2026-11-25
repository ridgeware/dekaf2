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

#include <dekaf2/time/duration/kbackoff.h>
#include <dekaf2/crypto/random/krandom.h>
#include <cmath>
#include <limits>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KDuration KBackoff::GetMaxWait(uint32_t iFailures) const
//-----------------------------------------------------------------------------
{
	if (m_Options.bImmediateFirst && iFailures)
	{
		--iFailures;
	}

	if (!iFailures)
	{
		return KDuration::zero();
	}

	auto dMax  = static_cast<double>(m_Options.Max.nanoseconds().count());
	auto dWait = static_cast<double>(m_Options.Initial.nanoseconds().count()) * std::pow(m_Options.dFactor, iFailures - 1);

	// written this way a value that is no number takes the limit as well
	if (!(dWait < dMax))
	{
		return m_Options.Max;
	}

	return KDuration(chrono::nanoseconds(static_cast<chrono::nanoseconds::rep>(dWait)));

} // GetMaxWait

//-----------------------------------------------------------------------------
KDuration KBackoff::GetWait(uint32_t iFailures) const
//-----------------------------------------------------------------------------
{
	auto Wait = GetMaxWait(iFailures);

	if (m_Options.dJitter > 0.0 && Wait > KDuration::zero())
	{
		// a random value in [0, 1)
		auto dRandom = static_cast<double>(kRandom64() >> 11) / static_cast<double>(uint64_t(1) << 53);
		auto dWait   = static_cast<double>(Wait.nanoseconds().count());
		auto dLess   = dWait * m_Options.dJitter * dRandom;

		// the random share takes away from the time, but never more than all of it
		if (dLess > 0.0)
		{
			Wait = (dLess < dWait) ? KDuration(chrono::nanoseconds(static_cast<chrono::nanoseconds::rep>(dWait - dLess)))
			                       : KDuration::zero();
		}
	}

	return Wait;

} // GetWait

//-----------------------------------------------------------------------------
KDuration KBackoff::Next()
//-----------------------------------------------------------------------------
{
	if (m_iFailures < std::numeric_limits<uint32_t>::max())
	{
		++m_iFailures;
	}

	return GetWait(m_iFailures);

} // Next

DEKAF2_NAMESPACE_END
