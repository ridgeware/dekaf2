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
#include <memory>
#include <mutex>


namespace dekaf2 {

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

	/// returns a pointer to an object that can be deleted with delete
	Value* Create() { return new Value(); };

	/// called once an object is taken from the pool, bIsNew is set to
	/// true when the object was newly created
	void Popped(Value* value, bool bIsNew) {};

	/// called once an object is returned to the pool
	void Pushed(Value* value) {};

}; // KPoolControl

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic pool of objects of type Value. If bShared is false,
/// a unique_ptr is returned to the user, else a shared_ptr. Implement a class
/// Control either as child of KPoolControl or independently, and overwrite
/// single or all methods from KPoolControl if you want to create complex objects
/// or get notifications about object usage.
template<class Value, bool bShared = false, class Control = KPoolControl<Value> >
class KPool
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using value_type = std::conditional_t<bShared,
										  std::shared_ptr<Value>,
	                                      std::unique_ptr<Value, std::function<void(Value*)>>>;

	using pool_type = std::vector<std::unique_ptr<Value>>;
	using size_type = typename pool_type::size_type;

	enum { DEFAULT_MAX_POOL_SIZE = 10000 };

	//-----------------------------------------------------------------------------
	/// ctor
	KPool(Control& _Control = s_DefaultControl, size_t iMaxSize = DEFAULT_MAX_POOL_SIZE)
	//-----------------------------------------------------------------------------
    : m_iMaxSize(iMaxSize)
	, m_Control(_Control)
	{
	}

	KPool(const KPool&) = delete;
	KPool& operator=(const KPool&) = delete;
	KPool(KPool&&) = default;
	KPool& operator=(KPool&&) = default;

	//-----------------------------------------------------------------------------
	/// stores a Value* in the pool
	void push_back(Value* value)
	//-----------------------------------------------------------------------------
	{
		m_Control.Pushed(value);
		int_push_back(value);
	}

	//-----------------------------------------------------------------------------
	/// returns either a std::unique_ptr<Value> or std::shared_ptr<Value>
	value_type pop_back()
	//-----------------------------------------------------------------------------
	{
		std::pair<Value*, bool> vb = int_pop_back();
		m_Control.Popped(vb.first, vb.second);
		return value_type(vb.first,
					std::bind(&KPool::push_back, this, std::placeholders::_1));
	}

	//-----------------------------------------------------------------------------
	/// clears the pool
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_Pool.clear();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Pool.empty();
	}

	//-----------------------------------------------------------------------------
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_Pool.size();
	}

	//-----------------------------------------------------------------------------
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		return m_iMaxSize;
	}

	//-----------------------------------------------------------------------------
	void max_size(size_type size)
	//-----------------------------------------------------------------------------
	{
		m_iMaxSize = size;
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// stores a Value* in the pool
	void int_push_back(Value* value)
	//-----------------------------------------------------------------------------
	{
		if (size() < max_size())
		{
			m_Pool.push_back(std::unique_ptr<Value>(value));
		}
		else
		{
			delete value;
		}
	}

	//-----------------------------------------------------------------------------
	/// returns a pair < Value*, bool >
	std::pair<Value*, bool> int_pop_back()
	//-----------------------------------------------------------------------------
	{
		Value* value;

		if (empty())
		{
			value = m_Control.Create();
			return { value, true };
		}
		else
		{
			value = m_Pool.back().release();
			m_Pool.pop_back();
			return { value, false };
		}
	}

	static KPoolControl<Value> s_DefaultControl;

	pool_type m_Pool;
	size_t m_iMaxSize;
	Control& m_Control;

}; // KPool

// defines the template's static const
template<class Value, bool bShared, class Control >
KPoolControl<Value> KPool<Value, bShared, Control >::s_DefaultControl;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements a generic pool for shared access of objects of type Value.
/// If bShared is false, a unique_ptr is returned to the user, else a shared_ptr.
/// Implement a class Control either as child of KPoolControl or independently,
/// and overwrite single or all methods from KPoolControl if you want to create
/// complex objects or get notifications about object usage.
template<class Value, bool bShared = false, class Control = KPoolControl<Value> >
class KSharedPool : public KPool<Value, bShared, Control>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type  = KPool<Value, bShared, Control>;
	using size_type  = typename base_type::size_type;
	using value_type = typename base_type::value_type;

	//-----------------------------------------------------------------------------
	/// ctor
	KSharedPool(Control& _Control = base_type::s_DefaultControl, size_t iMaxSize = base_type::DEFAULT_MAX_POOL_SIZE)
	//-----------------------------------------------------------------------------
    : base_type(_Control, iMaxSize)
	{
	}

	//-----------------------------------------------------------------------------
	/// stores a Value* in the pool
	void push_back(Value* value)
	//-----------------------------------------------------------------------------
	{
		base_type::m_Control.Pushed(value);
		std::lock_guard Lock(m_Mutex);
		base_type::int_push_back(value);
	}

	//-----------------------------------------------------------------------------
	/// returns either a std::unique_ptr<Value> or std::shared_ptr<Value>
	value_type pop_back()
	//-----------------------------------------------------------------------------
	{
		std::pair<Value*, bool> vb;
		{
			std::lock_guard Lock(m_Mutex);
			vb = base_type::int_pop_back();
		}
		base_type::m_Control.Popped(vb.first, vb.second);
		return value_type(vb.first,
					std::bind(&KSharedPool::push_back, this, std::placeholders::_1));
	}

	//-----------------------------------------------------------------------------
	/// clears the pool
	void clear()
	//-----------------------------------------------------------------------------
	{
		std::lock_guard Lock(m_Mutex);
		base_type::clear();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		std::lock_guard Lock(m_Mutex);
		return base_type::empty();
	}

	//-----------------------------------------------------------------------------
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		std::lock_guard Lock(m_Mutex);
		return base_type::size();
	}

	//-----------------------------------------------------------------------------
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		std::lock_guard Lock(m_Mutex);
		return base_type::max_size();
	}

	//-----------------------------------------------------------------------------
	void max_size(size_type size)
	//-----------------------------------------------------------------------------
	{
		std::lock_guard Lock(m_Mutex);
		base_type::max_size(size);
	}

//----------
private:
//----------

	mutable std::mutex m_Mutex;

};

} // of namespace dekaf2

