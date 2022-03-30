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

#include "kduration.h"
#include "klog.h"

namespace dekaf2 {

#ifdef _MSC_VER
KStopTime::ConstructHalted KStopTime::Halted;
KStopWatch::ConstructHalted KStopWatch::Halted;
#endif

//-----------------------------------------------------------------------------
std::chrono::nanoseconds::rep KDuration::nanoseconds() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::nanoseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::microseconds::rep KDuration::microseconds() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::microseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::milliseconds::rep KDuration::milliseconds() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::milliseconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::seconds::rep KDuration::seconds() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::seconds>().count();
}

//-----------------------------------------------------------------------------
std::chrono::minutes::rep KDuration::minutes() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::minutes>().count();
}

//-----------------------------------------------------------------------------
std::chrono::hours::rep KDuration::hours() const
//-----------------------------------------------------------------------------
{
	return duration<std::chrono::hours>().count();
}


//-----------------------------------------------------------------------------
void KMultiDuration::clear()
//-----------------------------------------------------------------------------
{
	KDuration::clear();
	m_iRounds = 0;

} // clear

//-----------------------------------------------------------------------------
void KMultiDuration::push_back(Duration duration)
//-----------------------------------------------------------------------------
{
	KDuration::operator+=(duration);
	++m_iRounds;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator+(const KMultiDuration& other) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp += other;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator+=(const KMultiDuration& other)
//-----------------------------------------------------------------------------
{
	KDuration::operator+=(other);
	m_iRounds  += other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator-(const KMultiDuration& other) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp -= other;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator-=(const KMultiDuration& other)
//-----------------------------------------------------------------------------
{
	KDuration::operator-=(other);
	m_iRounds  -= other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator*(size_type iMultiplier) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp *= iMultiplier;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator*=(size_type iMultiplier)
//-----------------------------------------------------------------------------
{
	KDuration::operator*=(iMultiplier);
	m_iRounds  *= iMultiplier;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator/(size_type iDivisor) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp /= iDivisor;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator/=(size_type iDivisor)
//-----------------------------------------------------------------------------
{
	if (iDivisor)
	{
		KDuration::operator/=(iDivisor);
		m_iRounds  /= iDivisor;
	}
	else
	{
		kWarning("cannot divide by 0");
	}

	return *this;
}

//-----------------------------------------------------------------------------
void KStopDuration::clear()
//-----------------------------------------------------------------------------
{
	base::clear();
	m_Timer.clear();

} // clear

//---------------------------------------------------------------------------
KStopDuration::base::Duration KStopDuration::Stop()
//---------------------------------------------------------------------------
{
	auto D = m_Timer.elapsedAndClear();

	push_back(D);

	return D;

} // StoreInterval


//-----------------------------------------------------------------------------
void KDurations::clear()
//-----------------------------------------------------------------------------
{
	m_Durations.clear();

} // clear

//-----------------------------------------------------------------------------
const KDurations::Duration& KDurations::operator[](size_type iInterval) const
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval];
	}
	else
	{
		static Duration s_EmptyDuration;
		kDebug(1, "subscript access out of range: {} in {}..{}", iInterval, 0, size());
		return s_EmptyDuration;
	}
}

//-----------------------------------------------------------------------------
KDurations::Duration& KDurations::operator[](size_type iInterval)
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval];
	}
	else
	{
		static Duration s_EmptyDuration;
		kDebug(1, "subscript access out of range: {} in {}..{}", iInterval, 0, size());
		return s_EmptyDuration;
	}
}

//-----------------------------------------------------------------------------
KDurations::DurationBase KDurations::duration(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval].duration();
	}
	else
	{
		return DurationBase::zero();
	}
}

//---------------------------------------------------------------------------
KDurations::size_type KDurations::TotalRounds() const
//---------------------------------------------------------------------------
{
	size_type Total { 0 };

	for (const auto& Duration : m_Durations)
	{
		Total += Duration.Rounds();
	}

	return Total;

} // TotalRounds

//---------------------------------------------------------------------------
KDurations::DurationBase KDurations::duration() const
//---------------------------------------------------------------------------
{
	Duration Total = Duration::zero();

	for (const auto& Duration : m_Durations)
	{
		Total += Duration;
	}

	return Total.duration();

} // TotalDuration

//-----------------------------------------------------------------------------
KDurations KDurations::operator+(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp += other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator+=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = size();

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] += other.m_Durations[i];
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator-(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp -= other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator-=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = size();

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] -= other.m_Durations[i];
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator*(size_type iMultiplier) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp *= iMultiplier;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator*=(size_type iMultiplier)
//-----------------------------------------------------------------------------
{
	for (auto& it : m_Durations)
	{
		it *= iMultiplier;
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator/(size_type iDivisor) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp /= iDivisor;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator/=(size_type iDivisor)
//-----------------------------------------------------------------------------
{
	if (iDivisor)
	{
		for (auto& it : m_Durations)
		{
			it /= iDivisor;
		}
	}
	else
	{
		kWarning("cannot divide by 0");
	}

	return *this;
}

//-----------------------------------------------------------------------------
void KStopDurations::clear()
//-----------------------------------------------------------------------------
{
	base::clear();
	m_Timer.clear();

} // clear

//---------------------------------------------------------------------------
void KStopDurations::StartNextInterval()
//---------------------------------------------------------------------------
{
	push_back(m_Timer.elapsedAndClear());

} // StartNextInterval

//---------------------------------------------------------------------------
KStopDurations::base::DurationBase KStopDurations::StoreInterval(size_type iInterval)
//---------------------------------------------------------------------------
{
	if (size() < iInterval + 1)
	{
		resize(iInterval + 1);
	}

	auto D = m_Timer.elapsedAndClear();

	operator[](iInterval) += D;

	return D;

} // StoreInterval

} // of namespace dekaf2
