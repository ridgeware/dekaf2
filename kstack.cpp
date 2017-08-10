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

#include "kstack.h"


/*
namespace dekaf2
{

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

	// If not empty guaranteed no except
	retrievedItem = std::move(m_Storage.front());
	m_Storage.pop_front();
	return true;
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

	topItem = std::move(m_Storage.front());
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::peek(const Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}

	topItem = std::move(m_Storage.front());
	return true;
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

	// If not empty guaranteed no except
	retrievedItem = std::move(m_Storage.back());
	m_Storage.pop_back();
	return true;
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

	topItem = std::move(m_Storage.back());
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::peek_bottom(const Stack_Type& topItem) const
//-----------------------------------------------------------------------------
{
	// Make sure not empty
	if (m_Storage.empty())
	{
		return false;
	}

	topItem = std::move(m_Storage.back());
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::getItem (unsigned int index, Stack_Type& item) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (m_Storage.size() <= index)
	{
		return false;
	}

	item = std::move(m_Storage.at(index));
	return true;
}

//-----------------------------------------------------------------------------
template<class Stack_Type>
bool KStack<Stack_Type>::getItem (unsigned int index, const Stack_Type& item) const
//-----------------------------------------------------------------------------
{
	// Make sure element exists
	if (m_Storage.size() <= index)
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
	if (m_Storage.size() <=index)
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
	if (n < m_Storage.size())
	{
		return m_Storage.at(n) ;
	}
	return m_EmptyValue;
}

} // END NAMESPACE DEKAF2

*/

