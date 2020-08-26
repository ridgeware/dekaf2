/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

/// @file ksharedptr.h
/// A shared pointer implementation that adapts per template instance for
/// multi/single threading case.
/// Other than a standard shared pointer it allows the user to select between
/// multi or single threading safe operation. This is important because the
/// required atomic synchronization for the multi threading safe implementation
/// costs around 50% of performance for construction / destruction. Also, this
/// shared pointer can be configured to use sequential-consistent memory access
/// for the use counter, which is slower, but 100% reliable in multi threding.
/// The std::shared_ptr uses relaxed memory access, which is unreliable.

#include <atomic>
#include <functional>
#include <thread>

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
/// costs around 50% of performance for construction / destruction. Also, this
/// shared pointer can be configured to use sequential-consistent memory access
/// for the use counter, which is slower, but 100% reliable in multi threding.
/// The std::shared_ptr uses relaxed memory access, which is unreliable.
template<class T, bool bMultiThreaded = true, bool bSequential = true>
class KSharedPtr
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

    template<class T_, bool bMultiThreaded_, bool bSequential_, class... Args>
    friend KSharedPtr<T_, bMultiThreaded_, bSequential_> kMakeShared(Args&&... args);

//----------
public:
//----------

	using self_type    = KSharedPtr;
	using element_type = typename std::remove_extent<T>::type;

	KSharedPtr() = default;

	//-----------------------------------------------------------------------------
	/// copy construction is allowed
	KSharedPtr(const self_type& other) noexcept
	//-----------------------------------------------------------------------------
	: m_ptr(other.m_ptr)
	, m_Control(other.m_Control)
	{
		inc();
	}

	//-----------------------------------------------------------------------------
	/// move construction is allowed
	KSharedPtr(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	: m_ptr(std::move(other.m_ptr))
	, m_Control(std::move(other.m_Control))
	{
		other.m_ptr = nullptr;
		other.m_Control = nullptr;
	}

	//-----------------------------------------------------------------------------
	template<class Y>
	explicit KSharedPtr(Y* ptr)
	//-----------------------------------------------------------------------------
	: m_ptr(ptr)
	, m_Control(new ControlImpl<Y, DefaultDeleter<Y>>(ptr, DefaultDeleter<Y>()))
	{
	}

	//-----------------------------------------------------------------------------
	template<class Y, class Deleter>
    KSharedPtr(Y* ptr, Deleter deleter)
	//-----------------------------------------------------------------------------
	: m_ptr(ptr)
	, m_Control(new ControlImpl<Y, Deleter>(ptr, deleter))
	{
	}

	//-----------------------------------------------------------------------------
	/// destructor
	~KSharedPtr()
	//-----------------------------------------------------------------------------
	{
		dec();
	}

	//-----------------------------------------------------------------------------
	void reset() noexcept
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ptr = nullptr;
		m_Control = nullptr;
	}

	//-----------------------------------------------------------------------------
	template<typename Y>
	void reset(Y* ptr)
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ptr = ptr;
		m_Control = new ControlImpl<Y, DefaultDeleter<Y>>(ptr, DefaultDeleter<Y>());
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is allowed
	self_type& operator=(const self_type& other) noexcept
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ptr = other.m_ptr;
		m_Control = other.m_Control;
		inc();
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment is allowed
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		dec();
		m_ptr = other.m_ptr;
		m_Control = other.m_Control;
		other.m_ptr = nullptr;
		other.m_Control = nullptr;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// returns the use count of the shared type
	size_t use_count() const
	//-----------------------------------------------------------------------------
	{
		return (!m_Control) ? 0 : m_Control->use_count();
	}

	//-----------------------------------------------------------------------------
	/// returns true if use count == 1
	bool unique() const
	//-----------------------------------------------------------------------------
	{
		return use_count() == 1;
	}

	//-----------------------------------------------------------------------------
	/// gets a pointer to the shared type
	element_type* get() const noexcept
	//-----------------------------------------------------------------------------
	{
		return m_ptr;
	}

	//-----------------------------------------------------------------------------
	/// gets a pointer to the shared type
	operator element_type*() const noexcept
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	/// gets a reference to the shared type
	element_type& operator*() const noexcept
	//-----------------------------------------------------------------------------
	{
		return *get();
	}

	//-----------------------------------------------------------------------------
	/// gets a pointer to the shared type
	element_type* operator->() const noexcept
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	operator bool() const noexcept
	//-----------------------------------------------------------------------------
	{
		return get() != nullptr;
	}

	//-----------------------------------------------------------------------------
	void swap(KSharedPtr& other) noexcept
	//-----------------------------------------------------------------------------
	{
		std::swap(m_ptr, other.m_ptr);
		std::swap(m_Control, other.m_Control);
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// increase the reference count
	void inc() noexcept
	//-----------------------------------------------------------------------------
	{
		if (m_Control)
		{
			m_Control->inc();
		}
	}

	//-----------------------------------------------------------------------------
	/// decrease the reference count
	void dec() noexcept
	//-----------------------------------------------------------------------------
	{
		if (m_Control && !m_Control->dec())
		{
			// this actually deletes both the pointed-to object
			// and the control block with the reference count
			m_Control->dispose();
			// we do not need to set m_Control and m_ptr to 0, as
			// every dec() is always followed by an assignment to
			// those members. Only when that changes make sure to
			// reset the values here..
//			m_Control = nullptr;
//			m_ptr = nullptr;
		}
	}

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Control
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		using RefCount_t = std::conditional_t<bMultiThreaded, std::atomic_size_t, size_t>;

		Control() = default;

		//-----------------------------------------------------------------------------
		virtual ~Control()
		//-----------------------------------------------------------------------------
		{
		}

		//-----------------------------------------------------------------------------
		/// increase reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == true>* = nullptr>
		void inc() noexcept
		//-----------------------------------------------------------------------------
		{
			m_iRefCount.fetch_add(1, bSequential ? std::memory_order_seq_cst : std::memory_order_relaxed);
		}

		//-----------------------------------------------------------------------------
		/// increase reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == false>* = nullptr>
		void inc() noexcept
		//-----------------------------------------------------------------------------
		{
			++m_iRefCount;
		}

		//-----------------------------------------------------------------------------
		/// decrease reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == true>* = nullptr>
		size_t dec() noexcept
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount.fetch_sub(1, bSequential ? std::memory_order_seq_cst : std::memory_order_acq_rel) - 1;
		}

		//-----------------------------------------------------------------------------
		/// decrease reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == false>* = nullptr>
		size_t dec() noexcept
		//-----------------------------------------------------------------------------
		{
			return --m_iRefCount;
		}

		//-----------------------------------------------------------------------------
		/// return reference count for the multi threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == true>* = nullptr>
		size_t use_count() noexcept
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount.load(bSequential ? std::memory_order_seq_cst : std::memory_order_relaxed);
		}

		//-----------------------------------------------------------------------------
		/// return reference count for the single threaded case
		template<bool bMT = bMultiThreaded, typename std::enable_if_t<bMT == false>* = nullptr>
		size_t use_count() noexcept
		//-----------------------------------------------------------------------------
		{
			return m_iRefCount;
		}

		virtual void dispose() noexcept = 0;

		RefCount_t m_iRefCount { 1 };
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	template<class Y, class Deleter>
	struct ControlImpl : public Control
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		//-----------------------------------------------------------------------------
		ControlImpl(Y* ptr, Deleter deleter)
		//-----------------------------------------------------------------------------
		: m_ptr(ptr)
		, m_deleter(deleter)
		{
		}

		//-----------------------------------------------------------------------------
		virtual void dispose() noexcept override final
		//-----------------------------------------------------------------------------
		{
			m_deleter(m_ptr);
		}

		Y* m_ptr;
		Deleter m_deleter;
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper struct to allocate object and ref count with one call to new()
	template<class Y>
	struct ObjectAndControl : public Control
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		//-----------------------------------------------------------------------------
		template<class... Args>
		ObjectAndControl(Args&&... args)
		//-----------------------------------------------------------------------------
		: m_Object(std::forward<Args>(args)...)
		{
		}

		//-----------------------------------------------------------------------------
		virtual void dispose() noexcept override final
		//-----------------------------------------------------------------------------
		{
		}

		Y m_Object;
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	template<class Y>
    struct DefaultDeleter
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    {
		//-----------------------------------------------------------------------------
        void operator()(Y* ptr) const noexcept
		//-----------------------------------------------------------------------------
		{
			delete ptr;
		}
    };

	element_type* m_ptr { nullptr };
	Control* m_Control  { nullptr };

}; // KSharedPtr

//-----------------------------------------------------------------------------
/// create a KSharedPtr with only one allocation
template<class T, bool bMultiThreaded = true, bool bSequential = true, class... Args>
KSharedPtr<T, bMultiThreaded, bSequential> kMakeShared(Args&&... args)
//-----------------------------------------------------------------------------
{
    KSharedPtr<T, bMultiThreaded, bSequential> ptr;
	auto tmp_object = new typename KSharedPtr<T, bMultiThreaded, bSequential>::template ObjectAndControl<T>(std::forward<Args>(args)...);
    ptr.m_ptr = &(tmp_object->m_Object);
    ptr.m_Control = tmp_object;

    return ptr;

} // kMakeShared

} // end of namespace dekaf2
