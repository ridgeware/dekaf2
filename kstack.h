/*
//-----------------------------------------------------------------------------//
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
/// provides a stack-like container with range protection and random index access

#include <deque>
#include "klog.h"

namespace  dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A stack-like container with range protection and random index access.
template<class Stack_Type>
class KStack
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using self_type = KStack<Stack_Type>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Default Constructor
	KStack()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// Default Constructor
	~KStack()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// Copy Constructor
	KStack(const self_type& other)
	//-----------------------------------------------------------------------------
	    :m_Storage(other.m_Storage)
	{
	}

	//-----------------------------------------------------------------------------
	/// Move Constructor
	KStack(self_type&& other)
	//-----------------------------------------------------------------------------
	    : m_Storage(std::move(other.m_Storage))
	{
	}

	// ===== STANDARD STACK INTERACTIONS =====

	//-----------------------------------------------------------------------------
	/// Put a new item onto the top of the stack
	template<typename Input>
	bool Push (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		// May throw bad_alloc if can't allocate space for new stack item
		DEKAF2_TRY_EXCEPTION

		m_Storage.emplace_back(std::forward<Input>(newItem));
		return true;

		DEKAF2_LOG_EXCEPTION

		return false;
	} // Push

	//-----------------------------------------------------------------------------
	/// insert iterator interface
	template<typename Input>
	bool push_back (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		return Push(std::forward<Input>(newItem));
	}

	//-----------------------------------------------------------------------------
	/// Take the item from the top of the stack (removes item)
	bool Pop (Stack_Type& retrievedItem);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Take the item from the top of the stack (removes item)
	Stack_Type Pop ();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (item still on stack)
	bool Peek (Stack_Type& topItem) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the item at the top of the stack (const) (item still on stack)
	const Stack_Type& Peek () const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Put a new item on the bottom of the stack
	template<typename Input>
	bool PushBottom (Input&& newItem)
	//-----------------------------------------------------------------------------
	{
		// May throw bad_alloc if can't allocate space for new stack item
		DEKAF2_TRY_EXCEPTION

		m_Storage.emplace_front(std::forward<Input>(newItem));
		return true;

		DEKAF2_LOG_EXCEPTION

		return false;
	}

	//-----------------------------------------------------------------------------
	/// Take the item from the bottom of the stack (removes item)
	bool PopBottom  (Stack_Type& retrievedItem);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Take the item from the bottom of the stack (removes item)
	Stack_Type PopBottom ();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the item at the bottom of the stack (item still on bottom of stack)
	bool PeekBottom (Stack_Type& bottomItem) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the item at the bottom of the stack (const) (item still on bottom of stack)
	const Stack_Type& PeekBottom () const;
	//-----------------------------------------------------------------------------

	// ===== RANDOM ACCESS TO STACK =====

	//-----------------------------------------------------------------------------
	/// Gets an item via Zero based index from the top of the stack
	bool GetItem (size_t index, Stack_Type& item) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets and item (const) via Zero based index from the top of the stack
	const Stack_Type& GetItem(size_t index) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sets the item via Zero based index from the top of the stack
	bool SetItem (size_t index, Stack_Type& item);
	//-----------------------------------------------------------------------------

	// ===== STACK STATS =====

	//-----------------------------------------------------------------------------
	/// Returns the number of elements on the stack
	inline size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.size();
	}

	//-----------------------------------------------------------------------------
	/// Checks if the stack is empty
	inline bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Storage.empty();
	}

	//-----------------------------------------------------------------------------
	/// Clears all elements off of the stack
	inline void clear()
	//-----------------------------------------------------------------------------
	{
		return m_Storage.clear();
	}

	// Operator overloads
	/// Assigns one KStack to Another (old data is destroyed)
	//-----------------------------------------------------------------------------
	self_type& operator= (self_type&& other)
	//-----------------------------------------------------------------------------
	{
		m_Storage = std::move(other.m_Storage);
		return *this;
	}

	//-----------------------------------------------------------------------------
	self_type& operator= (const self_type& other)
	//-----------------------------------------------------------------------------
	{
		m_Storage = other.m_Storage;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Gets Item at position, returns empty value if out of bounds.
	Stack_Type& operator[] (size_t n);
	//-----------------------------------------------------------------------------

	// Iterators
	typedef std::deque<Stack_Type> Storage_Type;
	typedef typename Storage_Type::iterator iterator;
	typedef typename Storage_Type::const_iterator const_iterator;
	typedef typename Storage_Type::reverse_iterator reverse_iterator;
	typedef typename Storage_Type::const_reverse_iterator const_reverse_iterator;

	// Forward Iterators
	//-----------------------------------------------------------------------------
	iterator                       begin()       { return m_Storage.begin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                cbegin() const { return m_Storage.cbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	iterator                         end()       { return m_Storage.end(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_iterator                  cend() const { return m_Storage.cend(); }
	//-----------------------------------------------------------------------------

	// Reverse Iterators
	//-----------------------------------------------------------------------------
	reverse_iterator              rbegin()       { return m_Storage.rbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator       crbegin() const { return m_Storage.crbegin(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	reverse_iterator                rend()       { return m_Storage.rend(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const_reverse_iterator         crend() const { return m_Storage.crend(); }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	Storage_Type m_Storage;
	Stack_Type m_EmptyValue{}; // for operator[] only
	static const Stack_Type s_cEmptyValue;

};

/// Defines the Templates static const
template<typename Stack_Type>
const Stack_Type KStack<Stack_Type>::s_cEmptyValue = Stack_Type{};

// ===== STANDARD STACK INTERACTIONS =====

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::Pop(Stack_Type& retrievedItem)
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}

	// If not empty, there is a no throw guarantee
	retrievedItem = std::move(m_Storage.back());
	m_Storage.pop_back();
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type KStack<Stack_Type>::Pop()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return s_cEmptyValue;
	}

	// If not empty, there is a no throw guarantee
	Stack_Type retrievedItem = std::move(m_Storage.back());
	m_Storage.pop_back();
	return retrievedItem;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::Peek(Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}
	// If not empty, there is a no throw guarantee
	topItem = m_Storage.back();
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::Peek() const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return s_cEmptyValue;
	}
	// If not empty, there is a no throw guarantee
	return m_Storage.back();
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::PopBottom(Stack_Type& retrievedItem)
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}

	// If not empty, there is a no throw guarantee
	retrievedItem = std::move(m_Storage.front());
	m_Storage.pop_front();
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type KStack<Stack_Type>::PopBottom()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return s_cEmptyValue;
	}

	// If not empty, there is a no throw guarantee
	Stack_Type retrievedItem = std::move(m_Storage.front());
	m_Storage.pop_front();
	return retrievedItem;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::PeekBottom(Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}
	// If not empty, there is a no throw guarantee
	topItem = m_Storage.front();
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::PeekBottom() const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return s_cEmptyValue;
	}
	// If not empty, there is a no throw guarantee
	return m_Storage.front();

}

// ===== RANDOM ACCESS TO STACK =====

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::GetItem (size_t index, Stack_Type& item) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return false;
	}

	item = m_Storage.at(index);
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::GetItem (size_t index) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return s_cEmptyValue;
	}
	// Compiler complains about std::move() if used on next line
	return m_Storage.at(index);
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::SetItem (size_t index, Stack_Type& item)
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return false;
	}

	m_Storage.at(index) = std::move(item);
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type& KStack<Stack_Type>::operator[] (size_t n)
//-----------------------------------------------------------------------------
{
	if (n >= m_Storage.size())
	{
		return m_EmptyValue;
	}
	return m_Storage.at(n) ;
}


} // END NAMESPACE DEKAF2
