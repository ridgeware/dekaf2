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

#include <deque>

namespace  dekaf2
{

//-----------------------------------------------------------------------------
template<class Stack_Type>
class KStack
//-----------------------------------------------------------------------------
{

//----------
public:
//----------
	/// Default Constructor
	KStack();
	/// Default Constructor
	~KStack(){}

	// For methods that return Stack_Type& they can either be (or compiler bawks)
	// 1. const qualified, return const and use std::move on return values
	// 2. not be const and not use std::move on return values
	// 2.1 returning Stack_Type&& allows std::move() of return val when not const
	// http://www.cplusplus.com/reference/deque/deque/front/

	/// Put a new item onto the top of the stack
	bool        push        (Stack_Type& newItem); // TODO add const ref version?
	/// Take the item from the top of the stack (removes item)
	bool        pop         (Stack_Type& retrievedItem);
	/// Take the item from the top of the stack (removes item)
	Stack_Type&& pop        ();
	/// Gets the item at the top of the stack (item still on stack)
	bool        peek        (Stack_Type& topItem)    const;
	/// Gets the item at the top of the stack (const) (item still on stack)
	const Stack_Type& peek  ()                       const;
	/// Gets the item at the top of the stack (item still on stack)
	Stack_Type&& peek       ()                       ;

	/// Put a new item on the bottom of the stack
	bool        push_bottom (Stack_Type& newItem); // TODO add const ref version?
	/// Take the item from the bottom of the stack (removes item)
	bool        pop_bottom  (Stack_Type& retrievedItem);
	/// Take the item from the bottom of the stack (removes item)
	Stack_Type&& pop_bottom ();
	/// Gets the item at the bottom of the stack (item still on bottom of stack)
	bool        peek_bottom (Stack_Type& bottomItem) const;
	/// Gets the item at the bottom of the stack (item still on bottom of stack)
	Stack_Type&& peek_bottom()                       ;
	/// Gets the item at the bottom of the stack (const) (item still on bottom of stack)
	const Stack_Type& peek_bottom ()                 const;

	/// Gets and item via Zero based index from the top of the stack
	bool        getItem     (unsigned int index, Stack_Type& item)       const;
	/// Gets and item via Zero based index from the top of the stack
	Stack_Type&& getItem    (unsigned int index)                         ;
	/// Gets and item (const) via Zero based index from the top of the stack
	bool        getItem     (unsigned int index, const Stack_Type& item) const;
	/// Gets and item (const) via Zero based index from the top of the stack
	const Stack_Type& getItem(unsigned int index)                        const;
	/// Sets the item via Zero based index from the top of the stack
	bool        setItem     (unsigned int index, Stack_Type& item);
	// TODO insert anywhere?

	/// Gets the empty value returned when the requested value doesn't exist
	const Stack_Type& getEmptyValue() { return m_EmptyValue; }

	/// Returns the number of elements on the stack
	int size         () const { return m_Storage.size(); }
	/// Checks if the stack is empty
	bool empty       () const { return m_Storage.empty(); }
	/// Clears all elements off of the stack
	void clear       ()       { return m_Storage.clear(); }

	// Operators overloads
	/// Assigns one KStack to Another (old data is destroyed)
	KStack&     operator=  (KStack&& other)      { m_Storage = std::move(other.m_Storage); return *this; }
	KStack&     operator=  (const KStack& other) { m_Storage = other.m_Storage; return *this; }
	/// Gets Item at position, returns nullptr if out of bounds.
	Stack_Type& operator[] (int n);

	// Iterators
	typedef typename std::deque<Stack_Type>::iterator iterator;
	typedef typename std::deque<Stack_Type>::const_iterator const_iterator;

	// Forward Iterators
	iterator         begin() { return m_Storage.begin(); }
	const_iterator  cbegin() { return m_Storage.cbegin(); }
	iterator           end() { return m_Storage.end(); }
	const_iterator    cend() { return m_Storage.cend(); }
	// Reverse Iterators
	iterator        rbegin() { return m_Storage.rbegin(); }
	const_iterator crbegin() { return m_Storage.crbegin(); }
	iterator          rend() { return m_Storage.rend(); }
	const_iterator   crend() { return m_Storage.crend(); }

//----------
private:
//----------

	std::deque<Stack_Type>  m_Storage;
	// Making static causes undefined reference error
	Stack_Type m_EmptyValue{}; // TODO GETTER for empty value

};

//-----------------------------------------------------------------------------
template<class Stack_Type>
KStack<Stack_Type>::KStack()
//-----------------------------------------------------------------------------
{
	//m_EmptyValue = Stack_Type();
}

// ===== STANDARD STACK INTERACTIONS =====

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::push(Stack_Type& newItem)
//-----------------------------------------------------------------------------
{
	// TODO May throw when allocating, bad_alloc usually
	m_Storage.push_front(std::move(newItem));
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::pop(Stack_Type& retrievedItem)
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
Stack_Type&& KStack<Stack_Type>::pop()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return std::move(m_EmptyValue);
	}

	// If not empty, there is a no throw guarantee
	Stack_Type& retrievedItem = m_Storage.front();
	m_Storage.pop_front();
	return std::move(retrievedItem);
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::peek(Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}
	// If not empty, there is a no throw guarantee
	topItem = std::move(m_Storage.front());
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::peek() const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return m_EmptyValue;
	}
	// If not empty, there is a no throw guarantee
	return std::move(m_Storage.front());
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type&& KStack<Stack_Type>::peek()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return std::move(m_EmptyValue);
	}
	// If not empty, there is a no throw guarantee
	return std::move(m_Storage.front());
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::push_bottom(Stack_Type& newItem)
//-----------------------------------------------------------------------------
{
	// TODO May throw when allocating, bad_alloc usually
	m_Storage.push_back(std::move(newItem));
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::pop_bottom(Stack_Type& retrievedItem)
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
Stack_Type&& KStack<Stack_Type>::pop_bottom()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return std::move(m_EmptyValue);
	}

	// If not empty, there is a no throw guarantee
	Stack_Type& retrievedItem = m_Storage.back();
	m_Storage.pop_back();
	return std::move(retrievedItem);
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::peek_bottom(Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}
	// If not empty, there is a no throw guarantee
	topItem = std::move(m_Storage.back());
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type&& KStack<Stack_Type>::peek_bottom()
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return std::move(m_EmptyValue);
	}
	// If not empty, there is a no throw guarantee
	return std::move(m_Storage.back());

}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::peek_bottom() const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return std::move(m_EmptyValue);
	}
	// If not empty, there is a no throw guarantee
	return std::move(m_Storage.back());

}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::getItem (unsigned int index, Stack_Type& item) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return false;
	}

	item = std::move(m_Storage.at(index));
	return true;
}

// ===== RANDOM ACCESS TO STACK =====

//-----------------------------------------------------------------------------
template<class Stack_Type>
Stack_Type&& KStack<Stack_Type>::getItem (unsigned int index)
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return std::move(m_EmptyValue);
	}
	// Compiler complains about std::move() if used on next line
	return std::move(m_Storage.at(index));
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
const Stack_Type& KStack<Stack_Type>::getItem (unsigned int index) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return m_EmptyValue;
	}
	// Compiler complains about std::move() if used on next line
	return std::move(m_Storage.at(index));
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::getItem (unsigned int index, const Stack_Type& item) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (index >= m_Storage.size())
	{
		return false;
	}

	item = std::move(m_Storage.at(index));
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::setItem (unsigned int index, Stack_Type& item)
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
Stack_Type& KStack<Stack_Type>::operator[] (int n)
//-----------------------------------------------------------------------------
{
	if (n >= m_Storage.size())
	{
		return m_EmptyValue;
	}
	return m_Storage.at(n) ;
}


} // END NAMESPACE DEKAF2
