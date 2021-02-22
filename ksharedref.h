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

/// @file ksharedref.h
/// A shared pointer implementation that adapts per template instance for
/// multi/single threading case.
/// Other than a standard shared pointer it allows the user to select between
/// multi or single threading safe operation. This is important because the
/// required atomic synchronization for the multi threading safe implementation
/// costs around 50% of performance for construction / destruction.
///
/// Also provides a template to generate a
/// class of any type that will hold a reference to the shared pointer.

#include <atomic>
#include <type_traits>

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A shared pointer implementation that adapts per template instance for
/// multi/single threading case. Uses perfect forwarding and variadic
/// templates to simplify construction of an instance (no manual
/// object generation is needed).
/// Other than a standard shared pointer it allows the user to select between
/// multi or single threading safe operation. This is important because the
/// required atomic synchronization for the multi threading safe implementation
/// costs around 50% of performance for construction / destruction.
template<class T, bool bMultiThreaded = false>
class KSharedRef
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KSharedRef;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// copy construction is allowed
	KSharedRef(const self_type& other)
	//-----------------------------------------------------------------------------
	: m_ref(other.m_ref)
	{
		inc();
	}

	//-----------------------------------------------------------------------------
	/// move construction is allowed
	KSharedRef(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	: m_ref(std::move(other.m_ref))
	{
		other.m_ref = nullptr;
	}

	//-----------------------------------------------------------------------------
	/// Construction with any arguments the shared type permits. This includes the default constructor.
	// make sure this does not cover the copy constructor by requesting an args count
	// of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KSharedRef(Args&&... args)
	//-----------------------------------------------------------------------------
	: m_ref(new Reference(std::forward<Args>(args)...))
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
	KSharedRef(Arg&& arg)
	//-----------------------------------------------------------------------------
	: m_ref(new Reference(std::forward<Arg>(arg)))
	{
	}

	//-----------------------------------------------------------------------------
	/// destructor
	~KSharedRef()
	//-----------------------------------------------------------------------------
	{
		dec();
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is allowed
	self_type& operator=(const self_type& other) noexcept
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ref = other.m_ref;
		inc();
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment is allowed
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ref = std::move(other.m_ref);
		other.m_ref = nullptr;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// returns the use count of the shared type
	inline size_t use_count() const
	//-----------------------------------------------------------------------------
	{
		return m_ref->use_count();
	}

	//-----------------------------------------------------------------------------
	/// gets a const reference to the shared type
	inline const T& get() const
	//-----------------------------------------------------------------------------
	{
		return m_ref->get();
	}

	//-----------------------------------------------------------------------------
	/// gets a reference to the shared type
	inline T& get()
	//-----------------------------------------------------------------------------
	{
		return m_ref->get();
	}

	//-----------------------------------------------------------------------------
	/// gets a const reference to the shared type
	inline operator const T&() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	/// gets a reference to the shared type
	inline operator T&()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	/// gets a const reference to the shared type
	inline const T& operator*() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	/// gets a reference to the shared type
	inline T& operator*()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	/// gets a const pointer to the shared type
	inline const T* operator->() const
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

	//-----------------------------------------------------------------------------
	/// gets a pointer to the shared type
	inline T* operator->()
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// increase the reference count
	inline void inc()
	//-----------------------------------------------------------------------------
	{
		if (m_ref)
		{
			m_ref->inc();
		}
	}

	//-----------------------------------------------------------------------------
	/// decrease the reference count
	inline void dec()
	//-----------------------------------------------------------------------------
	{
		if (m_ref && !m_ref->dec())
		{
			delete m_ref;
#ifndef NDEBUG
			m_ref = nullptr;
#endif
		}
	}

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A struct that holds shared type and reference count in one instance.
	/// Other than a standard shared pointer it allows the user to select between
	/// multi or single threading safe operation. This is important because the
	/// required atomic synchronization for the multi threading safe implementation
	/// costs around 50% of performance for construction / destruction.
	struct Reference
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		using RefCount_t = typename std::conditional<bMultiThreaded, std::atomic_size_t, size_t>::type;

		//-----------------------------------------------------------------------------
		/// perfect-forwarding constructor. Allows any parameter that is accepted by the
		/// shared type.
		template<class... Args>
		Reference(Args&&... args)
		    : m_Reference(std::forward<Args>(args)...)
		//-----------------------------------------------------------------------------
		{
		}

		//-----------------------------------------------------------------------------
		/// increase reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == true, int>::type = 0>
		inline void inc()
		//-----------------------------------------------------------------------------
		{
			m_iRefCount.fetch_add(1, std::memory_order_relaxed);
		}

		//-----------------------------------------------------------------------------
		/// increase reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == false, int>::type = 0>
		inline void inc()
		//-----------------------------------------------------------------------------
		{
			++m_iRefCount;
		}

		//-----------------------------------------------------------------------------
		/// decrease reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == true, int>::type = 0>
		inline size_t dec()
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
		}

		//-----------------------------------------------------------------------------
		/// decrease reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == false, int>::type = 0>
		inline size_t dec()
		//-----------------------------------------------------------------------------
		{
			return --m_iRefCount;
		}

		//-----------------------------------------------------------------------------
		/// get reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == true, int>::type = 0>
		inline size_t use_count() const
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount.load(std::memory_order_acquire);
		}

		//-----------------------------------------------------------------------------
		/// get reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == false, int>::type = 0>
		inline size_t use_count() const
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount;
		}

		//-----------------------------------------------------------------------------
		/// gets a const reference on the shared type
		inline const T& get() const
		//-----------------------------------------------------------------------------
		{
			return m_Reference;
		}

		//-----------------------------------------------------------------------------
		/// gets a reference on the shared type
		inline T& get()
		//-----------------------------------------------------------------------------
		{
			return m_Reference;
		}

		T m_Reference;
		RefCount_t m_iRefCount{1};

	};

	Reference* m_ref;

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Implements the concept of a "dependant" type on a shared type.
/// The shared type is automatically released when the dependant
/// type goes out of scope. Both types do not have to be of same origin.
template<class BaseT, class DependantT, bool bMultiThreaded = false>
class KDependant
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type      = BaseT;
	using shared_type    = KSharedRef<base_type, bMultiThreaded>;
	using dependant_type = DependantT;

	//-----------------------------------------------------------------------------
	/// default constructor. Constructs an empty instance with no reference to a
	/// shared type.
	KDependant() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy construction is allowed
	KDependant(const KDependant& other)
	//-----------------------------------------------------------------------------
	    : m_rep(other.m_rep)
	    , m_dep(other.m_dep)
	{
	}

	//-----------------------------------------------------------------------------
	/// move construction is allowed
	KDependant(KDependant&& other) noexcept
	//-----------------------------------------------------------------------------
	    : m_rep(std::move(other.m_rep))
	    , m_dep(std::move(other.m_dep))
	{
	}

	//-----------------------------------------------------------------------------
	/// constructs the internal object from the shared object it depends on (if
	/// possible)
	KDependant(shared_type& shared)
	//-----------------------------------------------------------------------------
	    : m_rep(shared)
	    , m_dep(shared)
	{
	}

	//-----------------------------------------------------------------------------
	/// constructs the internal object from a list of arguments. Prefixed by the
	/// shared object it depends on
	template<class Base, class... Dep>
	KDependant(Base&& shared, Dep&&... dep)
	//-----------------------------------------------------------------------------
	    : m_rep(std::forward<Base>(shared))
	    , m_dep(std::forward<Dep>(dep)...)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is allowed
	KDependant& operator=(const KDependant& other)
	//-----------------------------------------------------------------------------
	{
		m_rep = other.m_rep;
		m_dep = other.m_dep;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment is allowed
	KDependant& operator=(KDependant&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		m_rep = std::move(other.m_rep);
		m_dep = std::move(other.m_dep);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// gets a const reference on the internal type
	operator const dependant_type&() const
	//-----------------------------------------------------------------------------
	{
		return m_dep;
	}

	//-----------------------------------------------------------------------------
	/// gets a reference on the internal type
	operator dependant_type&()
	//-----------------------------------------------------------------------------
	{
		return m_dep;
	}

//----------
protected:
//----------

	shared_type    m_rep;
	dependant_type m_dep;

}; // KDependant


}
