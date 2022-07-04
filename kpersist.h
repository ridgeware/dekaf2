/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2022, Ridgeware, Inc.
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

/// @file kpersist.h
/// persist temporary values

#include <forward_list>

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <class Storage, bool bDeduplicate = true>
/// template to store Values that can always be dereferenced from the same address
class DEKAF2_PUBLIC KPersist
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using StorageContainer = std::forward_list<Storage>;
	using const_iterator   = typename StorageContainer::const_iterator;
	using size_type        = typename StorageContainer::size_type;

	KPersist() = default;

	//-----------------------------------------------------------------------------
	template <class Value>
	const Storage& Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		if (value == s_Default)
		{
			return s_Default;
		}
		else
		{
			if (bDeduplicate)
			{
				auto it = std::find(m_Persisted.begin(), m_Persisted.end(), value);
				if (it != m_Persisted.end())
				{
					return *it;
				}
			}
			m_Persisted.push_front(std::forward<Value>(value));
			return m_Persisted.front();
		}
	}

	//-----------------------------------------------------------------------------
	template <class Value>
	Storage& MutablePersist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		m_Persisted.push_front(std::forward<Value>(value));
		return m_Persisted.front();
	}

	//-----------------------------------------------------------------------------
	template <class Value>
	const_iterator find(Value&& value) const
	//-----------------------------------------------------------------------------
	{
		return std::find(m_Persisted.begin(), m_Persisted.end(), std::forward<Value>(value));
	}

	//-----------------------------------------------------------------------------
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.end();
	}

	//-----------------------------------------------------------------------------
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.end();
	}

	//-----------------------------------------------------------------------------
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.size();
	}

//----------
private:
//----------

	static const Storage s_Default;
	StorageContainer     m_Persisted;

}; // KPersist

template<typename Storage, bool bDeduplicate>
const Storage KPersist<Storage, bDeduplicate>::s_Default{};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// template to store Values that can always be dereferenced from the same address, except for an
/// exception type that will not be stored
template<class Storage, class Except, bool bDeduplicate = true>
class KPersistExcept : public KPersist<Storage, bDeduplicate>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = KPersist<Storage, bDeduplicate>;

//----------
public:
//----------

	KPersistExcept() = default;

	//-----------------------------------------------------------------------------
	template<class Value,
	          typename std::enable_if<
	              std::is_same<
	                  Except,
	                  typename std::decay<Value>::type
	              >::value, int>::type = 0>
	Value Persist(Value value)
	//-----------------------------------------------------------------------------
	{
		return value;
	}

	//-----------------------------------------------------------------------------
	template <class Value,
	          typename std::enable_if<
	              !std::is_same<
	                  Except,
	                  typename std::decay<Value>::type
	              >::value, int>::type = 0>
	const Storage& Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		return base::Persist(std::forward<Value>(value));
	}

	//-----------------------------------------------------------------------------
	template <class Value>
	Storage& MutablePersist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		return base::MutablePersist(std::forward<Value>(value));
	}

};

template <bool bDeduplicate = true>
using KPersistStrings = KPersistExcept<KString, const char*, bDeduplicate>;

} // end of namespace dekaf2
