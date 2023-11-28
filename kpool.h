/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

/// @file kpool.h
/// generic pools to reuse expensive to construct objects

#include "kdefinitions.h"
#include "klog.h"
#include "ktimeseries.h"
#include <memory>
#include <mutex>
#include <condition_variable>

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_HAS_CPP_14
// this code needs C++14, it uses (and needs) auto deduced return types

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements functions to create a new pool object, and to get notifications
/// once an object is popped from the pool or pushed back into
template<class Value>
class KPoolControl
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KPoolControl() = default;

	virtual ~KPoolControl() = default;

	/// returns a pointer to an object that can be deleted with delete
	virtual Value* Create() { return new Value(); };

	/// called once an object is taken from the pool, bIsNew is set to
	/// true when the object was newly created
	virtual void Popped(Value* value, bool bIsNew) {};

	/// called once an object is returned to the pool
	virtual void Pushed(Value* value) {};

}; // KPoolControl

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a no-op mutex, condition_variable and lock
template<bool bShared>
struct KPoolMutex
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	struct MyMutex {};
	struct MyLock
	{
		MyLock(MyMutex) {}
		void lock() {}
		void unlock() {}
	};
	struct MyCond
	{
		template<class Predicate>
		void wait(MyLock lock, Predicate pred)
		{
			// simply execute predicate, do not wait on failure..
			pred();
		}
		void notify_one() {}
	};

	mutable MyMutex m_Mutex;
	mutable MyCond  m_CondVar;

}; // KPoolMutex

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a real mutex and lock
template<>
struct KPoolMutex<true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using MyMutex = std::mutex;
	using MyLock  = std::unique_lock<std::mutex>;
	using MyCond  = std::condition_variable;

	mutable MyMutex m_Mutex;
	mutable MyCond  m_CondVar;

}; // KPoolMutex<true>

/// To create multiples of microseconds, seconds, minutes, etc. declare your own duration type with an odd duration, like
/// ```
/// using FiveMinDuration = std::chrono::duration<long, std::ratio<5 * 60>>;
/// ```
/// (basis of std::duration is one second)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a real KTimeSeries
template<uint16_t iIntervals = 0, typename Resolution = std::chrono::seconds>
class KPoolTimeSeries
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using TimeSeries = KTimeSeries<std::size_t, Resolution>;
	using Clock      = typename TimeSeries::Clock;
	using Timepoint  = typename Clock::time_point;

	Timepoint GetLastAverage()                    const { return m_LastAverage;                }
	void SetLastAverage(Timepoint tp)                   { m_LastAverage = tp;                  }
	void AddToIntervals(Timepoint tp, std::size_t i)    { m_Intervals.Add(tp, i);              }
	typename TimeSeries::Stored GetIntervalsSum() const { return m_Intervals.Sum();            }
	std::size_t GetAbsoluteMaxSize()              const { return m_iAbsoluteMaxSize;           }
	void SetAbsoluteMaxSize(std::size_t i)              { m_iAbsoluteMaxSize = i;              }
	static Timepoint GetCurrentTime()                   { return TimeSeries::GetCurrentTime(); }

//----------
private:
//----------

	TimeSeries  m_Intervals        { iIntervals         };
	Timepoint   m_LastAverage      { Resolution::zero() };
	std::size_t m_iAbsoluteMaxSize;

}; // functional KPoolTimeSeries

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a no-op KTimeSeries clone
template<typename Resolution>
class KPoolTimeSeries<0, Resolution>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using TimeSeries = KTimeSeries<std::size_t, Resolution>;
	using Clock      = typename TimeSeries::Clock;
	using Timepoint  = typename Clock::time_point;

	// dummy operations to satisfy the compiler, they are never called..
	Timepoint GetLastAverage()                    const { return Timepoint(); }
	void SetLastAverage(Timepoint tp)                   {}
	void AddToIntervals(Timepoint tp, std::size_t i)    {}
	typename TimeSeries::Stored GetIntervalsSum() const { return typename TimeSeries::Stored(); }
	std::size_t GetAbsoluteMaxSize()              const { return 0; }
	void SetAbsoluteMaxSize(std::size_t i)              {}
	static Timepoint GetCurrentTime()                   { return TimeSeries::GetCurrentTime(); }

}; // KPoolTimeSeries<0>

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Value, uint16_t iIntervals = 0, typename Resolution = std::chrono::seconds, bool bShared = false>
class KPoolBase : private KPoolMutex<bShared>, private KPoolTimeSeries<iIntervals, Resolution>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type   = KPoolMutex<bShared>;
	using time_series = KPoolTimeSeries<iIntervals, Resolution>;
	using pool_type   = std::vector<std::unique_ptr<Value>>;
	using size_type   = typename pool_type::size_type;

	enum { DEFAULT_MAX_POOL_SIZE = 10000 };

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// holds statistics for the pool
	struct Stats
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		size_type iUsed;         ///< count of currently used pool objects
		size_type iAvailable;    ///< count of still available pool objects
		size_type iMaxPool;      ///< current dynamic maximum size for the pool
		size_type iAbsoluteMax;  ///< absolute maximum size

	}; // Stats

	//-----------------------------------------------------------------------------
	/// ctor, pass derived class of KPoolControl for better control
	KPoolBase(KPoolControl<Value>& _Control = s_DefaultControl, size_type iMaxSize = DEFAULT_MAX_POOL_SIZE)
	//-----------------------------------------------------------------------------
    : m_iMaxSize(iMaxSize)
	, m_Control(_Control)
	{
		time_series::SetAbsoluteMaxSize(iMaxSize);
	}

	KPoolBase(const KPoolBase&) = delete;
	KPoolBase& operator=(const KPoolBase&) = delete;
	KPoolBase(KPoolBase&&) = default;
	KPoolBase& operator=(KPoolBase&&) = default;

	//-----------------------------------------------------------------------------
	/// returns true if pool is currently empty
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);
		return m_Pool.empty();
	}

	//-----------------------------------------------------------------------------
	/// returns current size of pool, including used objects
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);
		return m_Pool.size() + m_iPopped;
	}

	//-----------------------------------------------------------------------------
	/// returns used object count
	size_type used() const
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);
		return m_iPopped;
	}

	//-----------------------------------------------------------------------------
	/// disable pooling, clear existing pool, and call Control.Create() for every get() request
	/// @returns previous disabled state
	bool disable(bool bDisablePooling)
	//-----------------------------------------------------------------------------
	{
		if (m_bDisabled == bDisablePooling)
		{
			// short circuit without lock
			return bDisablePooling;
		}
		else
		{
			typename base_type::MyLock Lock(base_type::m_Mutex);
			bool bWasDisabled = m_bDisabled;

			if (bWasDisabled != bDisablePooling)
			{
				m_bDisabled = bDisablePooling;

				if (bDisablePooling)
				{
					m_Pool.clear();
				}
			}

			return bWasDisabled;
		}
	}

	//-----------------------------------------------------------------------------
	/// returns Stats object
	Stats GetStats()
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);

		Stats stats;
		stats.iUsed        = m_iPopped;
		stats.iAvailable   = m_Pool.size();
		stats.iMaxPool     = m_iMaxSize;
		stats.iAbsoluteMax = (iIntervals) ? time_series::GetAbsoluteMaxSize() : m_iMaxSize;

		return stats;
	}

	//-----------------------------------------------------------------------------
	/// returns max size for the pool
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);

		if (iIntervals)
		{
			return time_series::GetAbsoluteMaxSize();
		}
		else
		{
			return m_iMaxSize;
		}
	}

	//-----------------------------------------------------------------------------
	/// sets max size for the pool
	void max_size(size_type iMaxSize)
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);

		if (iIntervals)
		{
			time_series::SetAbsoluteMaxSize(iMaxSize);
		}

		m_iMaxSize = iMaxSize;
	}

	//-----------------------------------------------------------------------------
	/// stores a Value* in the pool, rare public uses may exist
	void put(Value* value)
	//-----------------------------------------------------------------------------
	{
		deleter(value, false);
	}

	//-----------------------------------------------------------------------------
	/// returns a std::unique_ptr<Value> pointing to a valid object - convert it at
	/// any time to a std::shared_ptr<Value> if more appropriate
	auto get()
	//-----------------------------------------------------------------------------
	{
		Value* value { nullptr };
		bool bNew { false };

		if (!m_bDisabled)
		{
			typename base_type::MyLock Lock(base_type::m_Mutex);

			// in a shared environment wait until there is a new object
			// available from the pool if used count reached maximum,
			// in a non-shared environment the lambda returns immediately
			// after execution
			base_type::m_CondVar.wait(Lock, [this, &value]() -> bool
			{
				if (!m_Pool.empty())
				{
					value = m_Pool.back().release();
					m_Pool.pop_back();
					++m_iPopped;
					AdjustPoolSize();
					return true;
				}
				else
				{
					return m_iPopped <= m_iMaxSize;
				}
			});
		}

		if (!value)
		{
			value = m_Control.Create();
			bNew  = true;
			typename base_type::MyLock Lock(base_type::m_Mutex);
			++m_iPopped;
			AdjustPoolSize();
		}

		auto PoolDeleter = [this](Value* value) { this->deleter(value, true); };

		auto ptr = std::unique_ptr<Value, decltype(PoolDeleter)>(value, PoolDeleter);

		// call the control method _after_ wrapping the pointer so that it
		// is exception safe
		m_Control.Popped(value, bNew);

		return ptr;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// stores a Value* in the pool, normally called by the customized deleter of
	/// an object generated with get() at the end of its lifetime
	void deleter(Value* value, bool bFromUniquePtr)
	//-----------------------------------------------------------------------------
	{
		// wrap immediately into unique_ptr
		auto ptr = std::unique_ptr<Value>(value);

		if (value)
		{
			m_Control.Pushed(value);
		}

		bool bUnderflow { false };

		{
			typename base_type::MyLock Lock(base_type::m_Mutex);

			if (bFromUniquePtr)
			{
				if (DEKAF2_UNLIKELY(!m_iPopped))
				{
					bUnderflow = true;
				}
				else
				{
					--m_iPopped;
				}
			}

			if (m_bDisabled == false)
			{
				if (value && m_Pool.size() + m_iPopped < m_iMaxSize)
				{
					m_Pool.push_back(std::move(ptr));
					base_type::m_CondVar.notify_one();
				}
			}
		}

		if (bUnderflow)
		{
			// warn outside the lock
			kWarning("underflow in m_iPopped");
		}

		// default deleter will delete the ptr if still valid
	}

	//-----------------------------------------------------------------------------
	// call only when lock is set
	void AdjustPoolSize()
	//-----------------------------------------------------------------------------
	{
		if (iIntervals && m_bDisabled == false)
		{
			// get the current time in the time_series resolution
			auto tNow = time_series::GetCurrentTime();
			// add a new data point to the current interval
			time_series::AddToIntervals(tNow, m_iPopped);

			// check if we shall adjust the max pool size, doing that after
			// one duration of the resolution has passed
			if (time_series::GetLastAverage() < tNow)
			{
				// one interval has passed since the last averages calculation
				time_series::SetLastAverage(tNow);
				auto Values = time_series::GetIntervalsSum();
				kDebug(2, "last {} intervals pool usage: min: {} mean: {} max: {}, oldmax: {}",
					   iIntervals, Values.Min(), Values.Mean(), Values.Max(), m_iMaxSize);
				m_iMaxSize = Values.Max();

				// now compute new vector size
				auto iPoolSize = m_Pool.size();

				if (m_iMaxSize < iPoolSize + m_iPopped)
				{
					size_type iRemove = (m_iMaxSize < m_iPopped)
					                     ? iPoolSize
					                     : iPoolSize - (m_iMaxSize - m_iPopped);

					if (iRemove)
					{
						kDebug(2, "shrinking pool by {} elements, new size {}", iRemove, iPoolSize - iRemove);
						// and remove from the begin() (== oldest objects)
						m_Pool.erase(m_Pool.begin(), m_Pool.begin() + iRemove);
					}
				}
			}
			else if (m_iPopped > m_iMaxSize)
			{
				if (m_iPopped <= time_series::GetAbsoluteMaxSize())
				{
					// make sure we can always grow the pool in automatic resize mode
					// as long as we did not hit the absolute ceiling..
					m_iMaxSize = m_iPopped;
				}
			}
		}
	}

	static KPoolControl<Value> s_DefaultControl;

	pool_type m_Pool;
	size_type m_iMaxSize { 0 };
	size_type m_iPopped  { 0 };
	KPoolControl<Value>& m_Control;
	bool      m_bDisabled { false };

}; // KPoolBase

// defines the template's static const
template<class Value, uint16_t iIntervals, typename Resolution, bool bShared>
KPoolControl<Value> KPoolBase<Value, iIntervals, Resolution, bShared>::s_DefaultControl;

} // end of namespace detail


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic pool of objects of type Value. A unique_ptr is returned
/// to the user, easily convertible to a shared_ptr. Implement a class
/// Control as child of KPoolControl, and override single or all methods from
/// KPoolControl if you want to create complex objects or get notifications
/// about object usage.
template<class Value, uint16_t iIntervals = 0, typename Resolution = std::chrono::seconds>
class KPool : public detail::KPoolBase<Value, iIntervals, Resolution, false>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type  = detail::KPoolBase<Value, iIntervals, Resolution, false>;
	using size_type  = typename base_type::size_type;
	using unique_ptr = decltype(base_type().get());
	using shared_ptr = std::shared_ptr<Value>;

	using base_type::base_type;

}; // KPool


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic pool for shared access of objects of type Value.
/// A unique_ptr is returned to the user, easily convertible to a shared_ptr.
/// Implement a class Control as child of KPoolControl, and override single
/// or all methods from KPoolControl if you want to create complex objects
/// or get notifications about object usage.
template<class Value, uint16_t iIntervals = 0, typename Resolution = std::chrono::seconds>
class KSharedPool : public detail::KPoolBase<Value, iIntervals, Resolution, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type  = detail::KPoolBase<Value, iIntervals, Resolution, true>;
	using size_type  = typename base_type::size_type;
	using unique_ptr = decltype(base_type().get());
	using shared_ptr = std::shared_ptr<Value>;

	using base_type::base_type;

}; // KSharedPool

#endif

DEKAF2_NAMESPACE_END
