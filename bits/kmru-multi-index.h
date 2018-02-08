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

#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <iterator>
#include "kmutable_pair.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMRU base type for non-maps
template <typename Element, typename Key = Element, bool IsMap = false>
class KMRUBaseType
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	struct SeqIdx {};
	struct KeyIdx {};

	using MRU_t = boost::multi_index::multi_index_container<
		Element,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<boost::multi_index::tag<SeqIdx> >,
			boost::multi_index::hashed_unique<boost::multi_index::tag<KeyIdx>,
			boost::multi_index::identity<Element> >
		>
	>;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMRU base type for maps
template <typename Element, typename Key>
class KMRUBaseType<Element, Key, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	struct SeqIdx {};
	struct KeyIdx {};

	using MRU_t = boost::multi_index::multi_index_container<
		Element,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<boost::multi_index::tag<SeqIdx> >,
			boost::multi_index::hashed_unique<boost::multi_index::tag<KeyIdx>,
			boost::multi_index::member<Element, Key, &Element::first> >
		>
	>;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a generalized Most/Least Recently Used container. Once it has reached
/// its max capacity it will start deleting the least recently used elements.
template <typename Element, typename Key = Element, bool IsMap = false>
class KMRUBase : KMRUBaseType<Element, Key, IsMap>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type    = KMRUBaseType<Element, Key, IsMap>;
	using MRU_t        = typename base_type::MRU_t;
	using SeqIdx       = typename base_type::SeqIdx;
	using KeyIdx       = typename base_type::KeyIdx;

//----------
public:
//----------

	using element_type = Element;
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

		while (m_Elements.size() > m_iMaxElements)
		{
			m_Elements.pop_back();
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
	/// Insert a const element into the container, new or known. The element will be
	/// placed at the top of the most recently used list.
	iterator insert(const element_type& element)
	//-----------------------------------------------------------------------------
	{
		post_insert(m_Elements.push_front(element));
		return begin();
	}

	//-----------------------------------------------------------------------------
	/// Insert a consumable element into the container, new or known. The element will be
	/// placed at the top of the most recently used list. The element's value will be
	/// transfered by a move operation if possible.
	iterator insert(element_type&& element)
	//-----------------------------------------------------------------------------
	{
		auto pair = m_Elements.push_front(std::move(element));
		post_insert(pair);
		return begin();
	}

	//-----------------------------------------------------------------------------
	/// Erase the element with the given key from the container.
	bool erase(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto& KeyView = m_Elements.template get<KeyIdx>();
		auto it = KeyView.find(key);
		if (it != KeyView.end())
		{
			KeyView.erase(it);
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
		return m_Elements.begin();
	}

	//-----------------------------------------------------------------------------
	/// Return an end iterator.
	iterator end()
	//-----------------------------------------------------------------------------
	{
		return m_Elements.end();
	}

	//-----------------------------------------------------------------------------
	/// Finds the element with the given key. Returns end() if not found. Moves
	/// the element to the top of the MRU list if found.
	iterator find(const Key& key)
	//-----------------------------------------------------------------------------
	{
		auto& KeyView = m_Elements.template get<KeyIdx>();
		auto kit = KeyView.find(key);
		// convert into base iterator
		iterator it = m_Elements.template project<SeqIdx>(kit);
		if (it != end())
		{
			// a find is equivalent to a "touch". Bring the element to the front of the list..
			touch(it);
			return begin();
		}
		else
		{
			return end();
		}
	}

	//-----------------------------------------------------------------------------
	/// Returns the size of the container.
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_Elements.size();
	}

	//-----------------------------------------------------------------------------
	/// Checks if the container is empty.
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Elements.empty();
	}

	//-----------------------------------------------------------------------------
	/// Removes all content from the container.
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_Elements.clear();
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// Brings the element pointed at with @p it to the front.
	void touch(iterator it)
	//-----------------------------------------------------------------------------
	{
		if (it != begin())
		{
			// Bring element to front.
			m_Elements.relocate(m_Elements.begin(), it);
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
		else if (m_Elements.size() > m_iMaxElements)
		{
			// new Element - remove oldest
			m_Elements.pop_back();
		}
	}

	size_t m_iMaxElements;
	MRU_t m_Elements;
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
class KMRUMap : public KMRUBase<detail::KMutablePair<Key, Value>, Key, true>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KMRUBase<detail::KMutablePair<Key, Value>, Key, true>;

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

