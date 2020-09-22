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

#include "bits/kcppcompat.h"
#include "dekaf2.h"
#include "klog.h"
#include <memory>
#include <mutex>


namespace dekaf2 {

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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Value, bool bShared = false>
class KPoolBase : private KPoolMutex<bShared>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = KPoolMutex<bShared>;
	using pool_type = std::vector<std::unique_ptr<Value>>;
	using size_type = typename pool_type::size_type;

	enum { DEFAULT_MAX_POOL_SIZE = 10000 };

	//-----------------------------------------------------------------------------
	/// ctor, pass derived class of KPoolControl for better control
	KPoolBase(KPoolControl<Value>& _Control = s_DefaultControl, size_t iMaxSize = DEFAULT_MAX_POOL_SIZE)
	//-----------------------------------------------------------------------------
    : m_iMaxSize(iMaxSize)
	, m_Control(_Control)
	{
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
	/// returns max size for the pool
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		typename base_type::MyLock Lock(base_type::m_Mutex);
		return m_iMaxSize;
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
					return true;
				}
				else
				{
					return m_iPopped < m_iMaxSize;
				}
			});
		}

		if (!value)
		{
			value = m_Control.Create();
			bNew  = true;
			typename base_type::MyLock Lock(base_type::m_Mutex);
			++m_iPopped;
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

			if (value && m_Pool.size() + m_iPopped < m_iMaxSize)
			{
				m_Pool.push_back(std::move(ptr));
				base_type::m_CondVar.notify_one();
			}
		}

		if (bUnderflow)
		{
			// warn outside the lock
			kWarning("underflow in m_iPopped");
		}

		// default deleter will delete the ptr if still valid
	}

	static KPoolControl<Value> s_DefaultControl;

	pool_type m_Pool;
	size_t m_iMaxSize { 0 };
	size_t m_iPopped  { 0 };
	KPoolControl<Value>& m_Control;

}; // KPoolBase

// defines the template's static const
template<class Value, bool bShared>
KPoolControl<Value> KPoolBase<Value, bShared>::s_DefaultControl;

} // end of namespace detail


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic pool of objects of type Value. A unique_ptr is returned
/// to the user, easily convertible to a shared_ptr. Implement a class
/// Control as child of KPoolControl, and override single or all methods from
/// KPoolControl if you want to create complex objects or get notifications
/// about object usage.
template<class Value>
class KPool : public detail::KPoolBase<Value, false>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type  = detail::KPoolBase<Value, false>;
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
template<class Value>
class KSharedPool : public detail::KPoolBase<Value, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type  = detail::KPoolBase<Value, true>;
	using size_type  = typename base_type::size_type;
	using unique_ptr = decltype(base_type().get());
	using shared_ptr = std::shared_ptr<Value>;

	using base_type::base_type;

}; // KSharedPool

#endif

} // of namespace dekaf2

