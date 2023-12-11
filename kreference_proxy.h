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

/// @file kreference_proxy.h
/// wrap object reference and access it through a proxy object

#include "kcompatibility.h"
#include "ktemplate.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to wrap one object reference and access it through a proxy object
template<class T>
class KReferenceProxy
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KReferenceProxy(T& proxied) : m_proxied(proxied) {};

	/// get reference on object
	T& get()
	{
		return m_proxied;
	}

	/// get pointer on object
	T* operator->()
	{
		return &get();
	}

	/// get reference on object
	T& operator*()
	{
		return get();
	}

	/// get reference on object
	operator T&()
	{
		return get();
	}

	/// helper for subscript access - acts as a proxy for the real object
	template<typename KeyType, typename Map = T,
			 typename std::enable_if<detail::is_map_type<Map>::value == true, int>::type = 0>
	typename Map::mapped_type& operator[](KeyType&& Key)
	{
		return get().operator[](std::forward<KeyType>(Key));
	}

	template<typename KeyType, typename Array = T,
			 typename std::enable_if<detail::is_std_array<Array>::value == true, int>::type = 0>
	typename Array::value_type& operator[](KeyType&& Key)
	{
		return get().operator[](std::forward<KeyType>(Key));
	}

//----------
private:
//----------

	T& m_proxied;

}; // KReferenceProxy

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to const wrap one object reference and access it through a proxy object
template<class T>
class KConstReferenceProxy
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KConstReferenceProxy(const T& proxied) : m_proxied(proxied) {};

	/// get const reference on object
	const T& get() const
	{
		return m_proxied;
	}

	/// get const pointer on object
	const T* operator->() const
	{
		return &get();
	}

	/// get const reference on object
	const T& operator*() const
	{
		return get();
	}

	/// get const reference on object
	operator const T&() const
	{
		return get();
	}

	/// helper for subscript access - acts as a proxy for the real object
	template<typename KeyType, typename Map = T,
			 typename std::enable_if<detail::is_map_type<Map>::value == true, int>::type = 0>
	const typename Map::mapped_type& operator[](KeyType&& Key)
	{
		return get().at(std::forward<KeyType>(Key));
	}

	template<typename KeyType, typename Array = T,
			 typename std::enable_if<detail::is_std_array<Array>::value == true, int>::type = 0>
	const typename Array::value_type& operator[](KeyType&& Key)
	{
		return get().at(std::forward<KeyType>(Key));
	}

//----------
private:
//----------

	const T& m_proxied;

}; // KConstReferenceProxy

DEKAF2_NAMESPACE_END
