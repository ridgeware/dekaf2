/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#pragma once

#include "dekaf2.h"
#include <chrono>
#include <map>

/// @file ktimeseries.h
/// collecting and analyzing data points over time

namespace dekaf2 {

//-----------------------------------------------------------------------------

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Datum, typename Resolution = std::chrono::seconds>
/// Creating a time series of data type Datum, with a minimum resolution of Resolution
///
/// To create multiples of microseconds, seconds, minutes, etc. declare your own duration type with an odd duration, like
/// ```
/// using FiveMinDuration = std::chrono::duration<long, std::ratio<5 * 60>>;
/// ```
/// (basis of std::duration is one second)
/// Now you can code:
/// ```
/// KTimeSeries<uint32_t, FiveMinDuration> TS;
/// TS += <any uint32_t value>;
/// TS += 4674532;
/// TS += 30499;
/// ```
/// and values will be averaged in 5 min intervals, to access with
/// ```
/// auto iAverage = TS.front().second.Avg();
/// auto iMax     = TS.front().second.Max();
/// auto Time     = TS.front().first;
/// ```
/// There is automatic conversion of timepoint values from any scale, so you can query the
/// interval expressed by system_clock timepoints:
/// ```
/// auto Interval = TS.Get(std::chrono::system_clock::now());
/// ```
class KTimeSeries
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//--------
public:
//--------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a helper class that stores min, avg, and max values for the Datum type per resolution interval
	class Stored
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//--------
	public:
	//--------

		using self = Stored;

		Stored() = default;
		Stored(Datum datum)
		: m_min(datum)
		, m_avg(datum)
		, m_max(datum)
		, m_iCount(1)
		{
		}

		std::size_t size()  const { return m_iCount;      }
		bool        empty() const { return m_iCount == 0; }

		//-------------------------------------------------------------------------
		/// add a new value of type Stored - creates min, max, and average values
		self& Add(const Stored& stored)
		//-------------------------------------------------------------------------
		{
			if (stored.m_iCount > 0)
			{
				if (!m_iCount || stored.m_min < m_min)
				{
					m_min = stored.m_min;
				}
				if (stored.m_max > m_max)
				{
					m_max = stored.m_max;
				}

				m_avg = static_cast<Datum>((m_avg * m_iCount + stored.m_avg * stored.m_iCount) / (m_iCount + stored.m_iCount));

				m_iCount += stored.m_iCount;
			}

			return *this;

		} // Add

		//-------------------------------------------------------------------------
		/// add a new value of type Datum - creates min, max, and average values
		self& Add(Datum datum)
		//-------------------------------------------------------------------------
		{
			if (!m_iCount)
			{
				m_min = m_avg = m_max = datum;
			}
			else
			{
				if (datum < m_min)
				{
					m_min = datum;
				}
				else if (datum > m_max)
				{
					m_max = datum;
				}

				m_avg = (m_avg * m_iCount + datum) / (m_iCount + 1);
			}

			++m_iCount;

			return *this;

		} // Add

		self& operator+=(const Stored& stored) { return Add(stored); }
		self& operator+=(Datum datum)          { return Add(datum);  }

		/// returns minimum value in interval
		const Datum& Min()   const { return m_min;    }
		/// returns average value in interval
		const Datum& Avg()   const { return m_avg;    }
		/// returns maximum value in interval
		const Datum& Max()   const { return m_max;    }
		/// returns count of probes in interval
		std::size_t  Count() const { return m_iCount; }

	//--------
	private:
	//--------

		Datum       m_min    {};
		Datum       m_avg    {};
		Datum       m_max    {};
		std::size_t m_iCount { 0 };

	}; // Stored

	using self           = KTimeSeries<Datum, Resolution>;
	using self_type      = self;
	using Duration       = Resolution;
	using Clock          = std::chrono::system_clock;
	using Timepoint      = std::chrono::time_point<Clock, Duration>;
	using Storage        = std::map<Timepoint, Stored>;
	using key_type       = Timepoint;
	using value_type     = typename Storage::value_type;
	using size_type      = typename Storage::size_type;
	using iterator       = typename Storage::iterator;
	using const_iterator = typename Storage::const_iterator;

	KTimeSeries() = default;

	//-------------------------------------------------------------------------
	/// construct and set maximum interval count (keep newest)
	KTimeSeries(size_type iMaxIntervals)
	//-------------------------------------------------------------------------
	: m_iMaxIntervals(iMaxIntervals)
	{
	}

	//-------------------------------------------------------------------------
	/// construct from an iterator range
	KTimeSeries(const_iterator it, const_iterator ie)
	//-------------------------------------------------------------------------
	{
		for (;it != ie; ++it)
		{
			m_Storage.insert(*it);
		}
	}

	//-------------------------------------------------------------------------
	/// set maximum interval count (keep newest)
	size_type MaxSize(size_type iMaxIntervals)
	//-------------------------------------------------------------------------
	{
		m_iMaxIntervals = iMaxIntervals;
		ResizeToLast(m_iMaxIntervals);
		return size();
	}

	//-------------------------------------------------------------------------
	/// add a pair<Timepoint, Stored>
	iterator Add(value_type value)
	//-------------------------------------------------------------------------
	{
		auto p = m_Storage.insert(value);

		if (!p.second)
		{
			p.first->second += value.second;
		}
		else
		{
			// check for size limit
			if (size() > m_iMaxIntervals)
			{
				m_Storage.erase(begin(), Inc(begin(), (size() - m_iMaxIntervals)));
			}
		}

		return p.first;
	}

	//-------------------------------------------------------------------------
	/// add a Datum with a given Timepoint
	iterator Add(Timepoint tp, Datum datum)
	//-------------------------------------------------------------------------
	{
		return Add(value_type(std::move(tp), Stored(std::move(datum))));
	}

	//-------------------------------------------------------------------------
	/// add a Datum with a given Timepoint
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	iterator Add(TP tp, Datum datum)
	//-------------------------------------------------------------------------
	{
		return Add(std::chrono::time_point_cast<Duration>(tp), std::move(datum));
	}

	//-------------------------------------------------------------------------
	/// add a Datum at current Timepoint
	iterator Add(Datum datum)
	//-------------------------------------------------------------------------
	{
		return Add(GetCurrentTime(), std::move(datum));
	}

	//-------------------------------------------------------------------------
	/// merge two KTimeSeries
	self& Add(const KTimeSeries& other)
	//-------------------------------------------------------------------------
	{
		for (const auto& value : other)
		{
			Add(value.first, value.second);
		}

		return *this;
	}

	//-------------------------------------------------------------------------
	/// add a Datum at current Timepoint
	self& operator+=(Datum datum)
	//-------------------------------------------------------------------------
	{
		Add(datum);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// add a pair<Timepoint, Stored>
	self& operator+=(value_type value)
	//-------------------------------------------------------------------------
	{
		Add(value);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// merge two KTimeSeries
	self& operator+=(const KTimeSeries& other)
	//-------------------------------------------------------------------------
	{
		return Add(other);
	}

	//-------------------------------------------------------------------------
	/// find the interval at a given Timepoint (returns end() if no Datum was stored in the respective interval)
	const_iterator find(Timepoint tp) const
	//-------------------------------------------------------------------------
	{
		return m_Storage.find(tp);
	}

	//-------------------------------------------------------------------------
	/// find the interval at a given Timepoint (returns end() if no Datum was stored in the respective interval)
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	const_iterator find(TP tp) const
	//-------------------------------------------------------------------------
	{
		return find(std::chrono::time_point_cast<Duration>(tp));
	}

	//-------------------------------------------------------------------------
	/// get the interval at a given Timepoint (may be empty if no Datum was stored in the respective interval)
	const Stored& Get(Timepoint tp) const
	//-------------------------------------------------------------------------
	{
		auto it = m_Storage.find(tp);

		if (it != m_Storage.end())
		{
			return it->second;
		}

		return s_EmptyStored;
	}

	//-------------------------------------------------------------------------
	/// get the interval at a given Timepoint (may be empty if no Datum was stored in the respective interval)
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	const Stored& Get(TP tp) const
	//-------------------------------------------------------------------------
	{
		return Get(std::chrono::time_point_cast<Duration>(tp));
	}

	//-------------------------------------------------------------------------
	/// create the sum of all intervals
	Stored Sum() const
	//-------------------------------------------------------------------------
	{
		Stored Sum;

		for (auto& Val : m_Storage)
		{
			Sum += Val.second;
		}

		return Sum;
	}

	//-------------------------------------------------------------------------
	/// create the sum of all intervals between Timepoints [tStart, tEnd)
	Stored Sum(Timepoint tStart, Timepoint tEnd) const
	//-------------------------------------------------------------------------
	{
		// lazy but correct implementation; better search for start iterator and end iterator,
		// then simply increment
		Stored Sum;

		for (; tStart < tEnd; tStart += Duration(1))
		{
			Sum += Get(tStart);
		}

		return Sum;
	}

	//-------------------------------------------------------------------------
	/// create the sum of all intervals between Timepoints [tStart, tEnd)
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	Stored Sum(TP tStart, TP tEnd) const
	//-------------------------------------------------------------------------
	{
		return Sum(std::chrono::time_point_cast<Duration>(tStart), std::chrono::time_point_cast<Duration>(tEnd));
	}

	//-------------------------------------------------------------------------
	/// erase all data before tBefore
	self& EraseBefore(Timepoint tBefore)
	//-------------------------------------------------------------------------
	{
		auto it = m_Storage.begin();

		for (; it != m_Storage.end() && it->first < tBefore; ++it) {}

		m_Storage.erase(m_Storage.begin(), it);

		return *this;
	}

	//-------------------------------------------------------------------------
	/// erase all data after tAfter (this actually erases the interval that begins with tAfter as well)
	self& EraseAfter(Timepoint tAfter)
	//-------------------------------------------------------------------------
	{
		auto it = m_Storage.end();

		for (; it != m_Storage.begin();)
		{
			--it;

			if (it->first < tAfter)
			{
				++it;
				break;
			}
		}

		m_Storage.erase(it, m_Storage.end());

		return *this;
	}

	//-------------------------------------------------------------------------
	/// erase all data before tBefore
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	self& EraseBefore(TP tBefore)
	//-------------------------------------------------------------------------
	{
		return EraseBefore(std::chrono::time_point_cast<Duration>(tBefore));
	}

	//-------------------------------------------------------------------------
	/// erase all data after tAfter (this actually erases the interval that begins with tAfter as well)
	template<typename TP, typename std::enable_if<std::is_constructible<Timepoint, TP>::value == false, int>::type = 0>
	self& EraseAfter(TP tAfter)
	//-------------------------------------------------------------------------
	{
		return EraseAfter(std::chrono::time_point_cast<Duration>(tAfter));
	}

	//-------------------------------------------------------------------------
	/// returns a KTimeSeries with the last iLast elements of this KTimeSeries
	self_type Last(std::size_t iLast) const
	//-------------------------------------------------------------------------
	{
		auto it = (size() > iLast) ? Dec(end(), iLast) : begin();
		return self(it, end());
	}

	//-------------------------------------------------------------------------
	/// returns a KTimeSeries with the first iFirst elements of this KTimeSeries
	self_type First(std::size_t iFirst) const
	//-------------------------------------------------------------------------
	{
		auto ie = (size() > iFirst) ? Inc(begin(), iFirst) : end();
		return self(begin(), ie);
	}

	//-------------------------------------------------------------------------
	/// limits series to the last iLast elements of this KTimeSeries
	self& ResizeToLast(std::size_t iLast)
	//-------------------------------------------------------------------------
	{
		if (size() > iLast)
		{
			m_Storage.erase(begin(), Dec(end(), iLast));
		}
		return *this;
	}

	//-------------------------------------------------------------------------
	/// limits series to the first iFirst elements of this KTimeSeries
	self& ResizeToFirst(std::size_t iFirst)
	//-------------------------------------------------------------------------
	{
		if (size() > iFirst)
		{
			m_Storage.erase(Inc(begin(), iFirst), end());
		}
		return *this;
	}

	//-------------------------------------------------------------------------
	/// remove all intervals
	void clear()
	//-------------------------------------------------------------------------
	{
		m_Storage.clear();
	}

	/// get the count of stored intervals
	size_type      size()  const { return m_Storage.size();  }
	/// returns true if no intervals are stored
	bool           empty() const { return m_Storage.empty(); }

	/// returns begin iterator
	iterator       begin()       { return m_Storage.begin(); }
	/// returns end iterator
	iterator       end()         { return m_Storage.end();   }
	/// returns  const begin iterator
	const_iterator begin() const { return m_Storage.begin(); }
	/// returns const end iterator
	const_iterator end()   const { return m_Storage.end();   }

	/// returns first stored element, empty if no intervals
	const Stored&  front() const { return empty() ? s_EmptyStored : m_Storage.front(); }
	/// returns last stored element, empty if no intervals
	const Stored&  back()  const { return empty() ? s_EmptyStored : m_Storage.back();  }

	//-------------------------------------------------------------------------
	/// returns interval resolution in nanoseconds
	static constexpr std::chrono::nanoseconds Interval()
	//-------------------------------------------------------------------------
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(Duration(1));
	}

	//-------------------------------------------------------------------------
	/// converts a Timepoint of given resolution into a system_clock timepoint
	static constexpr std::chrono::system_clock::time_point ToSystemClock(const Timepoint& tp)
	//-------------------------------------------------------------------------
	{
		return std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
	}

//--------
protected:
//--------

	//-------------------------------------------------------------------------
	/// returns current time in interval resolution
	Timepoint GetCurrentTime() const
	//-------------------------------------------------------------------------
	{
		if (Duration(1) >= std::chrono::seconds(1))
		{
			return std::chrono::time_point_cast<Duration>(Dekaf::getInstance().GetCurrentTimepoint());
		}
		else
		{
			return std::chrono::time_point_cast<Duration>(Clock::now());
		}
	}

	//-------------------------------------------------------------------------
	// increment the iterator by iCount values
	static const_iterator Inc(const_iterator it, size_type iCount = 1)
	//-------------------------------------------------------------------------
	{
		while (iCount--)
		{
			++it;
		}
		return it;
	}

	//-------------------------------------------------------------------------
	// decrement the iterator by iCount values
	static const_iterator Dec(const_iterator it, size_type iCount = 1)
	//-------------------------------------------------------------------------
	{
		while (iCount--)
		{
			--it;
		}
		return it;
	}

	static const Stored s_EmptyStored;

	Storage   m_Storage;
	size_type m_iMaxIntervals { std::numeric_limits<size_type>::max() };

}; // KTimeSeries

template<typename Datum, typename Resolution>
const typename KTimeSeries<Datum, Resolution>::Stored KTimeSeries<Datum, Resolution>::s_EmptyStored;

} // end of namespace dekaf2
