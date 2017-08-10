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
	KStack(){}
	/// Default Constructor
	~KStack(){}

	/// Put a new item onto the top of the stack
	bool push        (Stack_Type& newItem);
	/// Take the item from the top of the stack (removes item)
	bool pop         (Stack_Type& retrievedItem);
	/// Gets the item at the top of the stack (item still on stack)
	bool peek        (Stack_Type& topItem) const;
	/// Gets the item (const) at the top of the stack (item still on stack)
	bool peek        (const Stack_Type& topItem) const;

	/// Put a new item on the bottom of the stack
	bool push_bottom (Stack_Type& newItem);
	/// Take the item from the bottom of the stack (removes item)
	bool pop_bottom  (Stack_Type& retrievedItem);
	/// Gets the item at the bottom of the stack (item still on bottom of stack)
	bool peek_bottom (Stack_Type& bottomItem) const;
	/// Gets the item at the bottom of the stack (item still on bottom of stack)
	bool peek_bottom (const Stack_Type& bottomItem) const;

	/// Gets and item via Zero based index from the top of the stack
	bool getItem     (unsigned int index, Stack_Type& item) const;
	/// Gets and item (const) via Zero based index from the top of the stack
	bool getItem     (unsigned int index, const Stack_Type& item) const;

	/// Sets the item via Zero based index from the top of the stack
	bool setItem     (unsigned int index, Stack_Type& item);

	/// Returns the number of elements on the stack
	int size         () const
	{
		return m_Storage.size();
	}

	/// Checks if the stack is empty
	bool empty       () const
	{
		return m_Storage.empty();
	}

	/// Clears all elements off of the stack
	void clear        ()
	{
		return m_Storage.clear();
	}

	// Operators overloads
	/// Assigns one KStack to Another (old data is destroyed)
	KStack& operator=(KStack&& other) { m_Storage = std::move(other.m_Storage); return *this; }
	KStack& operator=(const KStack& other) { m_Storage = other.m_Storage; return *this; }
	/// Gets Item at position, returns nullptr if out of bounds.
	Stack_Type& operator[] (int n);

	// Iterators
	typedef typename std::deque<Stack_Type>::iterator iterator;
	typedef typename std::deque<Stack_Type>::const_iterator const_iterator;

	// Forward Iterators
	iterator        begin() { return m_Storage.begin(); }
	const_iterator cbegin() { return m_Storage.cbegin(); }
	iterator          end() { return m_Storage.end(); }
	const_iterator   cend() { return m_Storage.cend(); }
	// Reverse Iterators
	iterator        rbegin() { return m_Storage.rbegin(); }
	const_iterator crbegin() { return m_Storage.crbegin(); }
	iterator          rend() { return m_Storage.rend(); }
	const_iterator   crend() { return m_Storage.crend(); }

//----------
private:
//----------

	std::deque<Stack_Type> m_Storage;
	static Stack_Type emptyValue{nullptr};
};





} // END NAMESPACE DEKAF2
