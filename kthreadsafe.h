/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

/// @file kthreadsafe.h
/// generic threadsafe sharing for non-atomic types

#include <mutex>
#include "bits/kcppcompat.h"
#include "bits/ktemplate.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Template for generic threadsafe sharing for non-atomic types
template<class T, class MutexType = std::shared_mutex>
class KThreadSafe
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self_type = KThreadSafe;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper type for unique locked access
	class UniqueLocked : public detail::ReferenceProxy<T>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		UniqueLocked(KThreadSafe& Parent)
		: detail::ReferenceProxy<T>(Parent.m_Shared)
		, m_Lock(Parent.m_Mutex)
		{
		}

	//----------
	private:
	//----------

		std::unique_lock<MutexType> m_Lock;

	}; // UniqueLocked

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper type for shared locked access
	class SharedLocked : public detail::ConstReferenceProxy<T>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		SharedLocked(const KThreadSafe& Parent)
		: detail::ConstReferenceProxy<T>(Parent.m_Shared)
		, m_Lock(Parent.m_Mutex)
		{
		}

	//----------
	private:
	//----------

		std::shared_lock<MutexType> m_Lock;

	}; // SharedLocked

	//-----------------------------------------------------------------------------
	KThreadSafe(const KThreadSafe& other)
	//-----------------------------------------------------------------------------
	: m_Shared(other.shared().get())
	{
	}

	//-----------------------------------------------------------------------------
	KThreadSafe(KThreadSafe&& other) noexcept
	//-----------------------------------------------------------------------------
	: m_Shared(std::move(other.unique().get()))
	{
	}

	//-----------------------------------------------------------------------------
	KThreadSafe& operator=(const KThreadSafe& other)
	//-----------------------------------------------------------------------------
	{
		unique().get() = other.shared().get();
		return *this;
	}

	//-----------------------------------------------------------------------------
	KThreadSafe& operator=(KThreadSafe&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		unique().get() = std::move(other.unique().get());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Construction with any arguments the shared type permits. Is also the default constructor.
	// make sure this does not cover the copy constructor by requesting an args count
	// of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KThreadSafe(Args&&... args)
	//-----------------------------------------------------------------------------
	: m_Shared(std::forward<Args>(args)...)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construction of the shared type with any single argument.
	// make sure this does not cover the copy constructor by requesting the single
	// arg being of a different type than self_type
	template<class Arg,
		typename std::enable_if<
			!std::is_same<
				typename std::decay<Arg>::type, self_type
			>::value, int
		>::type = 0
	>
	KThreadSafe(Arg&& arg)
	//-----------------------------------------------------------------------------
	: m_Shared(std::forward<Arg>(arg))
	{
	}

	//-----------------------------------------------------------------------------
	/// Reset shared type by constructing a new one, either default constructed or with parameters
	template<class... Args>
	void reset(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		m_Shared = self_type(std::forward<Args>(args)...);
	}

	//-----------------------------------------------------------------------------
	/// Get an accessor on the shared type with a unique lock (good for reading and writing)
	UniqueLocked unique()
	//-----------------------------------------------------------------------------
	{
		return UniqueLocked(*this);
	}

	//-----------------------------------------------------------------------------
	/// Get an accessor on the shared type with a shared lock (only good for reading)
	SharedLocked shared() const
	//-----------------------------------------------------------------------------
	{
		return SharedLocked(*this);
	}

//----------
private:
//----------

	T m_Shared;
	mutable MutexType m_Mutex;

}; // KThreadSafe

} // of namespace dekaf2

