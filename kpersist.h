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

#include "ksystem.h"
#include <forward_list>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// template to store Values that can always be dereferenced from the same address
template <class Storage, bool bDeduplicate = true>
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
	DEKAF2_NODISCARD
	const Storage& Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		if (value == s_Default)
		{
			return s_Default;
		}

		if (bDeduplicate)
		{
			auto it = std::find(m_Persisted.begin(), m_Persisted.end(), value);

			if (it != m_Persisted.end())
			{
				return *it;
			}
		}

		m_Persisted.push_front(Storage(std::forward<Value>(value)));

		return m_Persisted.front();
	}

	//-----------------------------------------------------------------------------
	template <class Value>
	DEKAF2_NODISCARD
	Storage& MutablePersist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		m_Persisted.push_front(Storage(std::forward<Value>(value)));

		return m_Persisted.front();
	}

	//-----------------------------------------------------------------------------
	template <class Value>
	DEKAF2_NODISCARD
	const_iterator find(Value&& value) const
	//-----------------------------------------------------------------------------
	{
		return std::find(m_Persisted.begin(), m_Persisted.end(), std::forward<Value>(value));
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.begin();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.end();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_Persisted.empty();
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
/// template to store string values that can always be dereferenced from the same address, making sure
/// that the source either comes from a literal string or string_view in the data section (without persisting it)
/// or from any other string (which will be persisted to make it constantly available)
template<class String = KString, bool bDeduplicate = true,
         typename std::enable_if<detail::is_cpp_str<String>::value, int>::type = 0>
class KPersistStrings : public KPersist<String, bDeduplicate>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = KPersist<String, bDeduplicate>;

//----------
public:
//----------

	KPersistStrings() = default;

	//-----------------------------------------------------------------------------
	template<class Value,
	          typename std::enable_if<
	              std::is_same<
	                  typename String::const_pointer,
	                  typename std::decay<Value>::type
	              >::value, int>::type = 0>
	DEKAF2_NODISCARD
	typename String::const_pointer Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		if (kIsInsideDataSegment(&value))
		{
			return std::forward<Value>(value);
		}
		else
		{
			return base::Persist(std::forward<Value>(value)).c_str();
		}
	}

	//-----------------------------------------------------------------------------
	template<class Value,
	          typename std::enable_if<
                  detail::is_string_view<Value>::value &&
	              std::is_same< // check for same char type ..
	                  typename String::value_type,
	                  typename std::decay<Value>::type::value_type
	              >::value, int>::type = 0>
	DEKAF2_NODISCARD
	typename std::decay<Value>::type Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		if (kIsInsideDataSegment(value.data()))
		{
			return std::forward<Value>(value);
		}
		else
		{
			return base::Persist(std::forward<Value>(value));
		}
	}

	//-----------------------------------------------------------------------------
	template<class Value,
	          typename std::enable_if<
                  !detail::is_string_view<Value>::value &&
	              !std::is_same<
	                  typename String::const_pointer,
	                  typename std::decay<Value>::type
	              >::value, int>::type = 0>
	DEKAF2_NODISCARD
	const String& Persist(Value&& value)
	//-----------------------------------------------------------------------------
	{
		return base::Persist(std::forward<Value>(value));
	}

}; // KPersistStrings

DEKAF2_NAMESPACE_END
