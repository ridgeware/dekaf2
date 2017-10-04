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

#pragma once

/// @file kmru.h
/// provides a least / most recently used container

#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <algorithm>
#include "../klog.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMRU base type for non-maps
template <typename Key, typename Value = Key, bool IsMap = false>
class KMRUBaseType
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using MRU_t = std::unordered_set<Key>;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMRU base type for maps
template <typename Key, typename Value>
class KMRUBaseType<Key, Value, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using MRU_t = std::unordered_map<Key, Value>;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a generalized Most/Least Recently Used container. Once it has reached
/// its max capacity it will start deleting the least recently used elements.
template <typename Key, typename Value = Key, bool IsMap = false>
class KMRUBase : KMRUBaseType<Key, Value, IsMap>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type    = KMRUBaseType<Key, Value, IsMap>;
	using MRU_t        = typename base_type::MRU_t;
	using index_t      = std::list<typename MRU_t::iterator>;

//----------
public:
//----------

	using element_type = typename MRU_t::value_type;
	using iterator     = typename MRU_t::iterator;
	using pair_ib      = std::pair<iterator, bool>;

	//-----------------------------------------------------------------------------
	/// Constructor.
	/// @param iMaxElements Maximum size of the container.
	KMRUBase(size_t iMaxElements)
	//-----------------------------------------------------------------------------
	    : m_iMaxElements(iMaxElements)
	{
	}

	//-----------------------------------------------------------------------------
	/// Sets a new maximum size for the container
	/// @param iMaxElements The new value for the maximum size.
	void SetMaxSize(size_t iMaxElements)
	//-----------------------------------------------------------------------------
	{
		m_iMaxElements = iMaxElements;

		while (m_Store.size() > m_iMaxElements)
		{
			auto it = m_Index.back();
			m_Store.erase(it);
			m_Index.pop_back();
		}
	}

	//-----------------------------------------------------------------------------
	/// Gets the maximum size of the container.
	size_t GetMaxSize() const
	//-----------------------------------------------------------------------------
	{
		return m_iMaxElements;
	}

	//-----------------------------------------------------------------------------
	/// Insert a const elememt into the container, new or known. The element will be
	/// placed at the top of the most recently used list.
	iterator insert(const element_type& element)
	//-----------------------------------------------------------------------------
	{
		post_insert(m_Store.insert(element));
		return begin();
	}

	//-----------------------------------------------------------------------------
	/// Insert a consumable elememt into the container, new or known. The element will be
	/// placed at the top of the most recently used list. The element's value will be
	/// transfered by a move operation if possible.
	iterator insert(element_type&& element)
	//-----------------------------------------------------------------------------
	{
		auto pair = m_Store.insert(std::move(element));
		post_insert(pair);
		return begin();
	}

	//-----------------------------------------------------------------------------
	/// Erase the element with the given key from the container.
	bool erase(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto it = m_Store.find(key);
		if (it != m_Store.end())
		{
			auto lit = std::find(m_Index.begin(), m_Index.end(), it);
			if (lit != m_Index.end())
			{
				m_Index.erase(lit);
			}
			else
			{
				kWarning("existing element not found in MRU index");
			}
			m_Store.erase(it);
			return true;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	/// Return an iterator on the first element in the MRU sequence.
	iterator begin()
	//-----------------------------------------------------------------------------
	{
		return m_Store.begin();
	}

	//-----------------------------------------------------------------------------
	/// Return an end iterator.
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return m_Store.end();
	}

	//-----------------------------------------------------------------------------
	/// Finds the element with the given key. Returns end() if not found. Moves
	/// the element to the top of the MRU list if found.
	iterator find(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto it = m_Store.find(key);
		if (it != end())
		{
			// a find is equivalent to a "touch". Bring the element to the front of the list..
			touch(it);
		}
		return it;
	}

	//-----------------------------------------------------------------------------
	/// Returns the size of the container.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Store.size();
	}

	//-----------------------------------------------------------------------------
	/// Checks if the container is empty.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Store.empty();
	}

	//-----------------------------------------------------------------------------
	/// Removes all content from the container.
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_Store.clear();
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// Brings the element pointed at with @p it to the front.
	void touch(iterator it)
	//-----------------------------------------------------------------------------
	{
		if (it != m_Index.front())
		{
			auto lit = std::find(m_Index.begin(), m_Index.end(), it);
			if (lit != m_Index.end())
			{
				// Bring element to front.
				m_Index.erase(lit);
			}
			else
			{
				kWarning("known element not found in MRU index");
			}
			m_Index.push_front(it);
		}
	}

	//-----------------------------------------------------------------------------
	/// Depending on whether the inserted element was new or known this function
	/// brings it to the front or checks if old elements have to be deleted.
	void post_insert(pair_ib& pair)
	//-----------------------------------------------------------------------------
	{
		if (!pair.second)
		{
			// element was already known. Bring to front.
			touch(pair.first);
		}
		else
		{
			if (m_Store.size() > m_iMaxElements)
			{
				// new Element - remove oldest
				auto it = m_Index.back();
				m_Store.erase(it);
				m_Index.pop_back();
			}
			m_Index.push_front(pair.first);
		}
	}

	size_t m_iMaxElements;
	MRU_t m_Store;
	index_t m_Index;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a Most/Least Recently Used container for elements which themselves
/// are the key, like lists.
template <typename Element>
class KMRUList : public KMRUBase<Element>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KMRUBase<Element>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KMRUList(size_t iMaxElements)
	//-----------------------------------------------------------------------------
	    : base_type(iMaxElements)
	{}
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a Most/Least Recently Used container for map-like elements formed out of a
/// Key and a Value, where only the Key is indexed
template <typename Key, typename Value>
class KMRUMap : public KMRUBase<Key, Value, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KMRUBase<Key, Value, true>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KMRUMap(size_t iMaxElements)
	//-----------------------------------------------------------------------------
	    : base_type(iMaxElements)
	{}
};

} // end of namespace dekaf2

