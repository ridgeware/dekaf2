/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file kstack.h
/// provides a container with accessors from std::deque plus std::stack,
/// and range protection

#include <deque>
#include "klog.h"

namespace  dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A container with accessors from std::deque plus std::stack, and range protection
template<class Value>
class KStack
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using value_type = Value;
	using Storage_Type = std::deque<value_type>;
	using iterator = typename Storage_Type::iterator;
	using const_iterator = typename Storage_Type::const_iterator;
	using reverse_iterator = typename Storage_Type::reverse_iterator;
	using const_reverse_iterator =  typename Storage_Type::const_reverse_iterator;

	// ===== Construction =====

	KStack() = default;
	KStack (std::initializer_list<value_type> il) : m_Storage(il) {}
	template<typename... Args>
	KStack(Args&&... args) : m_Storage(std::forward<Args>(args)...) {}

	// ===== Comparison =====

	bool operator==(const KStack& other) const
	{
		return m_Storage == other.m_Storage;
	}

	bool operator!=(const KStack& other) const
	{
		return m_Storage != other.m_Storage;
	}

	// ===== STANDARD STACK INTERACTIONS =====

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (const) (item still on stack)
	const value_type& back () const
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return s_cEmptyValue;
		}
		// If not empty, there is a no throw guarantee
		return m_Storage.back();
	}

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (const) (item still on stack)
	value_type& back ()
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return m_EmptyValue;
		}
		// If not empty, there is a no throw guarantee
		return m_Storage.back();
	}

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (const) (item still on stack)
	const value_type& top () const
	//-----------------------------------------------------------------------------
	{
		return back ();
	}

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (const) (item still on stack)
	value_type& top ()
	//-----------------------------------------------------------------------------
	{
		return back ();
	}

	//-----------------------------------------------------------------------------
	/// Gets the item at the bottom of the stack (const) (item still on bottom of stack)
	const value_type& front () const
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return s_cEmptyValue;
		}
		// If not empty, there is a no throw guarantee
		return m_Storage.front();
	}

	//-----------------------------------------------------------------------------
	/// Gets the item at the bottom of the stack (const) (item still on bottom of stack)
	value_type& front ()
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return m_EmptyValue;
		}
		// If not empty, there is a no throw guarantee
		return m_Storage.front();
	}

	//-----------------------------------------------------------------------------
	/// Put a new item on the bottom of the stack
	template<typename Input>
	bool push_front (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		// May throw bad_alloc if can't allocate space for new stack item
		DEKAF2_TRY_EXCEPTION

		m_Storage.push_front(std::forward<Input>(newItem));
		return true;

		DEKAF2_LOG_EXCEPTION

		return false;
	}

	//-----------------------------------------------------------------------------
	/// Take the item from the bottom of the stack (removes item)
	value_type pop_front ()
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return s_cEmptyValue;
		}

		// If not empty, there is a no throw guarantee
		value_type retrievedItem = std::move(m_Storage.front());
		m_Storage.pop_front();
		return retrievedItem;
	}

	//-----------------------------------------------------------------------------
	/// Put a new item onto the top of the stack
	template<typename Input>
	bool push (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		// May throw bad_alloc if can't allocate space for new stack item
		DEKAF2_TRY_EXCEPTION

		m_Storage.push_back(std::forward<Input>(newItem));
		return true;

		DEKAF2_LOG_EXCEPTION

		return false;
	}

	//-----------------------------------------------------------------------------
	/// insert iterator interface
	template<typename Input>
	bool push_back (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		return push(std::forward<Input>(newItem));
	}

	//-----------------------------------------------------------------------------
	/// Take the item from the top of the stack (removes item)
	value_type pop ()
	//-----------------------------------------------------------------------------
	{
		// Make sure not empty
		if (m_Storage.empty())
		{
			kDebug(2, "stack is empty");
			return s_cEmptyValue;
		}

		// If not empty, there is a no throw guarantee
		value_type retrievedItem = std::move(m_Storage.back());
		m_Storage.pop_back();
		return retrievedItem;
	}

	// ===== RANDOM ACCESS TO STACK =====

	//-----------------------------------------------------------------------------
	/// Gets an item (const) via Zero based index from the bottom of the stack
	const value_type& at (size_t index) const
	//-----------------------------------------------------------------------------
	{
		// Make sure element exists
		if (index >= m_Storage.size())
		{
			return s_cEmptyValue;
		}
		return m_Storage[index];
	}

	//-----------------------------------------------------------------------------
	/// Gets an item via Zero based index from the bottom of the stack
	value_type& at (size_t index)
	//-----------------------------------------------------------------------------
	{
		// Make sure element exists
		if (index >= m_Storage.size())
		{
			return m_EmptyValue;
		}
		return m_Storage[index];
	}

	//-----------------------------------------------------------------------------
	/// Gets Item at position, returns empty value if out of bounds.
	const value_type& operator[] (size_t n) const
	//-----------------------------------------------------------------------------
	{
		return at (n);
	}

	//-----------------------------------------------------------------------------
	/// Gets Item at position, returns empty value if out of bounds.
	value_type& operator[] (size_t n)
	//-----------------------------------------------------------------------------
	{
		return at (n);
	}

	// ===== STACK STATS =====

	//-----------------------------------------------------------------------------
	/// Returns the number of elements on the stack
	inline size_t size () const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.size();
	}

	//-----------------------------------------------------------------------------
	/// Checks if the stack is empty
	inline bool empty () const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.empty();
	}

	//-----------------------------------------------------------------------------
	/// test if the container is non-empty
	explicit operator bool() const { return !empty(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clears all elements off of the stack
	inline void clear ()
	//-----------------------------------------------------------------------------
	{
		return m_Storage.clear();
	}

	// Iterators

	// Forward Iterators
	//-----------------------------------------------------------------------------
	iterator                       begin ()       { return m_Storage.begin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                cbegin () const { return m_Storage.cbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                 begin () const { return cbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	iterator                         end ()       { return m_Storage.end(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                  cend () const { return m_Storage.cend(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                   end () const { return cend(); }
	//-----------------------------------------------------------------------------

	// Reverse Iterators
	//-----------------------------------------------------------------------------
	reverse_iterator              rbegin ()       { return m_Storage.rbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator       crbegin () const { return m_Storage.crbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator        rbegin () const { return crbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	reverse_iterator                rend ()       { return m_Storage.rend(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator         crend () const { return m_Storage.crend(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator          rend () const { return crend(); }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	Storage_Type m_Storage;
	value_type m_EmptyValue{}; // for operator[] and front() and back() if empty
	static constexpr value_type s_cEmptyValue {};

}; // KStack

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
// defines the template's static const for c++ < 17
template<typename Stack_Type>
const Stack_Type KStack<Stack_Type>::s_cEmptyValue;
#endif

} // END NAMESPACE DEKAF2
