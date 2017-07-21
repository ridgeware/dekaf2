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

#include <atomic>
#include <functional>

namespace dekaf2
{

template<class T, bool bMultiThreaded = false>
class KSharedRef
{
public:
	KSharedRef(const KSharedRef& other) noexcept
	{
		m_ref = other.m_ref;
		inc();
	}
	KSharedRef(KSharedRef& other) noexcept
	{
		m_ref = other.m_ref;
		inc();
	}
	KSharedRef(KSharedRef&& other) noexcept
	{
		m_ref = std::move(other.m_ref);
		other.m_ref = nullptr;
	}
	template<class... Args>
	KSharedRef(Args&&... args)
	{
		m_ref = new Reference(std::forward<Args>(args)...);
	}
	~KSharedRef()
	{
		dec();
	}
	KSharedRef& operator=(const KSharedRef& other) noexcept
	{
		dec();
		m_ref = other.m_ref;
		inc();
		return *this;
	}
	KSharedRef& operator=(KSharedRef&& other) noexcept
	{
		dec();
		m_ref = std::move(other.m_ref);
		other.m_ref = nullptr;
		return *this;
	}
	inline size_t UseCount() const
	{
		return m_ref->m_iRefCount;
	}
	inline const T& get() const
	{
		return m_ref->get();
	}
	inline T& get()
	{
		return m_ref->get();
	}
	inline operator const T&() const
	{
		return get();
	}
	inline operator T&()
	{
		return get();
	}
	inline const T& operator*() const
	{
		return get();
	}
	inline T& operator*()
	{
		return get();
	}
	inline const T* operator->() const
	{
		return &get();
	}
	inline T* operator->()
	{
		return &get();
	}

protected:

	inline void inc()
	{
		if (m_ref)
		{
			m_ref->inc();
		}
	}

	inline void dec()
	{
		if (m_ref && !m_ref->dec())
		{
			delete m_ref;
		}
	}

	struct Reference
	{
	public:
		using RefCount_t = std::conditional_t<bMultiThreaded, std::atomic_size_t, size_t>;

		template<class... Args>
		Reference(Args&&... args)
		    : m_Reference(std::forward<Args>(args)...)
		{
		}
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == true>::type* = nullptr>
		inline void inc()
		{
			m_iRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == false>::type* = nullptr>
		inline void inc()
		{
			++m_iRefCount;
		}
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == true>::type* = nullptr>
		inline size_t dec()
		{
			return m_iRefCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
		}
		template<bool bMT = bMultiThreaded, typename std::enable_if<bMT == false>::type* = nullptr>
		inline size_t dec()
		{
			return --m_iRefCount;
		}
		inline const T& get() const
		{
			return m_Reference;
		}
		inline T& get()
		{
			return m_Reference;
		}

		T m_Reference;
		RefCount_t m_iRefCount{1};
	};

	Reference* m_ref;
};


template<class BaseT, class DependantT, bool bMultiThreaded = false>
class KDependant
{
public:
	using base_type      = BaseT;
	using shared_type    = KSharedRef<base_type, bMultiThreaded>;
	using dependant_type = DependantT;

	KDependant()
	{}
	KDependant(const KDependant& other)
	    : m_rep(other.m_rep)
	    , m_dep(other.m_dep)
	{
	}
	KDependant(KDependant&& other) noexcept
	    : m_rep(std::move(other.m_rep))
	    , m_dep(std::move(other.m_dep))
	{
	}
	KDependant(shared_type& shared)
	    : m_rep(shared)
	    , m_dep(shared)
	{
	}
	template<class Base, class... Dep>
	KDependant(Base&& shared, Dep&&... dep)
	    : m_rep(std::forward<Base>(shared))
	    , m_dep(std::forward<Dep>(dep)...)
	{
	}
	KDependant& operator=(const KDependant& other)
	{
		m_rep = other.m_rep;
		m_dep = other.m_dep;
		return *this;
	}
	KDependant& operator=(KDependant&& other) noexcept
	{
		m_rep = std::move(other.m_rep);
		m_dep = std::move(other.m_dep);
		return *this;
	}
	operator const dependant_type&() const
	{
		return m_dep;
	}
	operator dependant_type&()
	{
		return m_dep;
	}

private:
	shared_type    m_rep;
	dependant_type m_dep;

}; // KDependant


}
