/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#pragma once

/// @file katomic_object.h
/// generic atomic wrapping for larger objects through an atomic pointer - access on the object is read only,
/// and updates must happen less frequent than maximum lifetime of the unwrapped object

#include "kthreadsafe.h"
#include <memory>
#include <mutex>
#include <atomic>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// generic atomic wrapping for larger objects through an atomic pointer - access on the object is read only,
/// and updates must happen less frequent than maximum lifetime of the unwrapped object
template<class Object>
class KAtomicObject
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self_type = KAtomicObject;

	//-----------------------------------------------------------------------------
	/// Construction with any arguments the object type permits. Is also the default constructor.
	// make sure this does not cover the copy constructor by requesting an args count
	// of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KAtomicObject(Args&&... args)
	//-----------------------------------------------------------------------------
	: m_Objects(ObjectStorage(std::make_unique<Object>(std::forward<Args>(args)...), &m_pObject))
	{
	}

	//-----------------------------------------------------------------------------
	/// Construction of the object type with any single argument.
	// make sure this does not cover the copy constructor by requesting the single
	// arg being of a different type than self_type
	template<class Arg,
		typename std::enable_if<
			!std::is_same<
				typename std::decay<Arg>::type, self_type
			>::value, int
		>::type = 0
	>
	KAtomicObject(Arg&& arg)
	//-----------------------------------------------------------------------------
	: m_Objects(ObjectStorage(std::make_unique<Object>(std::forward<Arg>(arg)), &m_pObject))
	{
	}

	KAtomicObject(const KAtomicObject&) = delete;
	KAtomicObject(KAtomicObject&&) = default;

	KAtomicObject& operator=(const KAtomicObject&) = delete;
	KAtomicObject& operator=(KAtomicObject&&) = default;

	//-----------------------------------------------------------------------------
	/// get pointer on Object
	const Object* get(bool bRelaxed = true) const
	//-----------------------------------------------------------------------------
	{
		return m_pObject.load(bRelaxed ? std::memory_order_relaxed : std::memory_order_acquire);
	}

	//-----------------------------------------------------------------------------
	/// get reference on Object
	const Object& getRef(bool bRelaxed = true) const
	//-----------------------------------------------------------------------------
	{
		return *get(bRelaxed);
	}

	//-----------------------------------------------------------------------------
	operator const Object*() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	operator const Object&() const
	//-----------------------------------------------------------------------------
	{
		return getRef();
	}

	//-----------------------------------------------------------------------------
	/// Reset Object by constructing a new one, either default constructed or with parameters
	template<class... Args>
	void reset(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		auto NewObject            = std::make_unique<Object>(std::forward<Args>(args)...);
		auto Objects              = m_Objects.unique();
		m_pObject                 = NewObject.get();
		Objects->m_DecayingObject = std::move(Objects->m_ActiveObject);
		Objects->m_ActiveObject   = std::move(NewObject);

	} // reset

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct ObjectStorage
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		ObjectStorage() = default;

		// special constructor to avoid using the mutex during parent object construction
		ObjectStorage(std::unique_ptr<Object>&& ActiveObject, std::atomic<Object*>* ObjectPointer)
		: m_ActiveObject(std::move(ActiveObject))
		{
			if (ObjectPointer)
			{
				*ObjectPointer = m_ActiveObject.get();
			}
		}

		std::unique_ptr<Object> m_ActiveObject;
		std::unique_ptr<Object> m_DecayingObject;

	}; // ObjectStorage

	std::atomic<Object*> m_pObject;
	KThreadSafe<ObjectStorage, std::mutex> m_Objects;

}; // KAtomicObject

} // end of namespace dekaf2
