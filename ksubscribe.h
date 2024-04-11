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

/// @file ksubscribe.h
/// subscriber / subscription scheme. The subscriber is forced to build
/// an own instance (or subset) of subscription when subscription goes out of
/// scope.

#include <utility>
#include <memory>
#include "bits/kmake_unique.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Used by both KSubscription and KSubscriber to have a common parent
template<typename parent_type>
class KSubscriberBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using self_type = KSubscriberBase;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	virtual ~KSubscriberBase()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	size_t CountSubscriptions() const
	//-----------------------------------------------------------------------------
	{
		size_t iResult{0};

		auto p = m_PrevSubscriber;
		while (p)
		{
			++iResult;
			p = p->m_PrevSubscriber;
		}

		p = m_NextSubscriber;
		while (p)
		{
			++iResult;
			p = p->m_NextSubscriber;
		}

		return iResult;
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// do whatever has to be done to release the subscription
	/// - base version is abstract
	virtual void ReleaseSubscription(const parent_type& parent) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Subscribe a new KSubscriber to a KSubscription instance
	void Subscribe(const self_type* subscription) const
	//-----------------------------------------------------------------------------
	{
		m_PrevSubscriber = const_cast<self_type*>(subscription);
		m_NextSubscriber = subscription->m_NextSubscriber;
		if (m_NextSubscriber)
		{
			m_NextSubscriber->m_PrevSubscriber = const_cast<self_type*>(this);
		}
		subscription->m_NextSubscriber = const_cast<self_type*>(this);
	}

	//-----------------------------------------------------------------------------
	/// Unsubscribe a new KSubscriber from a KSubscription instance
	void Unsubscribe() const
	//-----------------------------------------------------------------------------
	{
		if (m_PrevSubscriber)
		{
			m_PrevSubscriber->m_NextSubscriber = m_NextSubscriber;
			m_PrevSubscriber = nullptr;
		}

		if (m_NextSubscriber)
		{
			m_NextSubscriber->m_PrevSubscriber = m_PrevSubscriber;
			m_NextSubscriber = nullptr;
		}
	}

	//-----------------------------------------------------------------------------
	/// to be called from subscription class only..
	void ReleaseSubscribers(const parent_type& parent)
	//-----------------------------------------------------------------------------
	{
		auto p = m_NextSubscriber;
		while (p)
		{
			p->ReleaseSubscription(parent);
			auto hp = p->m_NextSubscriber;
			p->m_NextSubscriber = nullptr;
			p->m_PrevSubscriber = nullptr;
			p = hp;
		}
		m_NextSubscriber = nullptr;
		if (m_PrevSubscriber)
		{
			m_PrevSubscriber = nullptr;
			kWarning("error: called from subscriber, not subscription");
		}
	}

//----------
protected:
//----------

	mutable self_type* m_NextSubscriber{nullptr};
	mutable self_type* m_PrevSubscriber{nullptr};

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper class to define a framework for specialized templates that can
/// transform an existing subscriber when its subscription signals end of life.
/// Base template version simply tries to create a new subscription object from
/// the subscriber.
/// There is also a version of the function operator that explains how to
/// construct a subscriber out of a parent.
template <typename subscriber_type, typename parent_type>
class KSubscriberReleaser
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	// specialize this class for types where parent cannot be constructed from the actual subscriber,
	// or add an operator parent_type() to the subscriber..
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// operator that is used when "cloning" a parent that is about to go out of scope.
	std::unique_ptr<parent_type> operator()(subscriber_type& subscriber, const parent_type& parent) const noexcept
	//-----------------------------------------------------------------------------
	{
		auto newParent = std::make_unique<parent_type>(subscriber);
		subscriber = *newParent;
		return newParent;
	}

	//-----------------------------------------------------------------------------
	/// operator that is used to construct a subscriber when the parent is the
	/// only argument to the subscriber constructor
	subscriber_type operator()(const parent_type& parent) const noexcept
	//-----------------------------------------------------------------------------
	{
		return parent;
	}

};

} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Wraps a type that is used by other types as the subscription. When it goes
/// out of life, it tells such to all of its subscribers and forces them to
/// search for alternative storage.
template<typename parent_type>
class KSubscription : public detail::KSubscriberBase<parent_type>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = detail::KSubscriberBase<parent_type>;
	using self_type = KSubscription;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// copy construction is possible, but does not get the subscribers
	KSubscription(const self_type& other)
	//-----------------------------------------------------------------------------
	    : m_Rep(other.m_Rep)
	{
	}

	//-----------------------------------------------------------------------------
	/// move construction is possible, and gets the subscribers, too
	KSubscription(self_type&& other)
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , m_Rep(std::move(other.m_Rep))
	{
	}

	//-----------------------------------------------------------------------------
	/// default constructor and any other
	template<class... Args>
	KSubscription(Args&&... args)
	//-----------------------------------------------------------------------------
	    : m_Rep(std::forward<Args>(args)...)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is possible, but does not get the subscribers
	/// and we release our own subscribers..
	KSubscription& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		base_type::ReleaseSubscribers();
		m_Rep = other.m_Rep;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assignment is possible, and gets the subscribers, too
	KSubscription& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::ReleaseSubscribers();
		m_Rep = std::move(other.m_Rep);
		base_type::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// any assignment that the base class allows is allowed, too
	/// - but this removes the subscribers
	template<class Arg>
	KSubscription& operator=(Arg&& args)
	//-----------------------------------------------------------------------------
	{
		base_type::ReleaseSubscribers();
		m_Rep = std::forward<Arg>(args);
		return *this;
	}

	//-----------------------------------------------------------------------------
	~KSubscription()
	//-----------------------------------------------------------------------------
	{
		base_type::ReleaseSubscribers(*this);
	}

	//-----------------------------------------------------------------------------
	const parent_type& get() const
	//-----------------------------------------------------------------------------
	{
		return m_Rep;
	}

	//-----------------------------------------------------------------------------
	parent_type& get()
	//-----------------------------------------------------------------------------
	{
		return m_Rep;
	}

	//-----------------------------------------------------------------------------
	inline operator const parent_type&() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline operator parent_type&()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline const parent_type& operator*() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline parent_type& operator*()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline const parent_type* operator->() const
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

	//-----------------------------------------------------------------------------
	inline parent_type* operator->()
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	// does nothing, needed to make this class non-abstract
	virtual void ReleaseSubscription(const parent_type& parent) override
	//-----------------------------------------------------------------------------
	{
	}

	parent_type m_Rep;

}; // KSubscription


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Wraps a type that uses another type as a subscription. Think of it as
/// a reference to an object instance. But this type will create its own
/// instance once the referenced object passed end of life.
template<typename subscriber_type, typename parent_type>
class KSubscriber : public detail::KSubscriberBase<parent_type>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type      = detail::KSubscriberBase<parent_type>;
	using self_type      = KSubscriber;
	using subscript_type = KSubscription<parent_type>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	// default ctor
	KSubscriber() noexcept
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// copy ctor is deleted
	KSubscriber(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// move ctor
	KSubscriber(self_type&& other)
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , m_Rep(std::move(other.m_Rep))
	    , m_Parent(std::move(other.m_Parent))
	{
	}

	//-----------------------------------------------------------------------------
	// For some reason it does not work to use a ternary operator testing the
	// size of the parameter pack, therefore we have to "specialize" here and
	// create the constructor that builds from subsciption. Otherwise we could
	// have reused the variadic constructor below alone.
	/// value ctor. constructs m_Rep with subscription and binds to subscription
	KSubscriber(const subscript_type& subscription)
	//-----------------------------------------------------------------------------
	    : m_Rep(detail::KSubscriberReleaser<subscriber_type, parent_type>()(subscription))
	{
		base_type::Subscribe(&subscription);
	}

	//-----------------------------------------------------------------------------
	/// value ctor. constructs m_Rep with args and binds to subscription
	template<class... Args>
	KSubscriber(const subscript_type& subscription, Args&&... args)
	//-----------------------------------------------------------------------------
	    : m_Rep(std::forward<Args>(args)...)
	{
		base_type::Subscribe(&subscription);
	}

	//-----------------------------------------------------------------------------
	/// A sort of a constructor after the initial construction had happened
	template<class... Args>
	self_type& SetSubscription(const subscript_type& subscription, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		base_type::Unsubscribe();
		m_Parent.reset();
		base_type::Subscribe(&subscription);
		m_Rep = (sizeof...(args)==0)
			            ? detail::KSubscriberReleaser<subscriber_type, parent_type>()(subscription)
			            : subscriber_type(std::forward<Args>(args)...) ;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
	    m_Rep = std::move(other.m_Rep);
	    m_Parent = std::move(other.m_Parent);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Assignment. Only works if m_Rep can be constructed
	/// by assigning subscription
	self_type& operator=(const subscript_type& subscription)
	//-----------------------------------------------------------------------------
	{
		base_type::Unsubscribe();
		m_Parent.reset();
		base_type::Subscribe(&subscription);
		// assignment from parent to rep must be possible
		m_Rep = detail::KSubscriberReleaser<subscriber_type, parent_type>()(subscription);
		return *this;
	}

	//-----------------------------------------------------------------------------
	~KSubscriber()
	//-----------------------------------------------------------------------------
	{
		base_type::Unsubscribe();
	}

	//-----------------------------------------------------------------------------
	inline const subscriber_type& get() const
	//-----------------------------------------------------------------------------
	{
		return m_Rep;
	}

	//-----------------------------------------------------------------------------
	inline subscriber_type& get()
	//-----------------------------------------------------------------------------
	{
		return m_Rep;
	}

	//-----------------------------------------------------------------------------
	inline operator subscriber_type&()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline operator const subscriber_type&() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline const subscriber_type& operator*() const
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline subscriber_type& operator*()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	inline const subscriber_type* operator->() const
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

	//-----------------------------------------------------------------------------
	inline subscriber_type* operator->()
	//-----------------------------------------------------------------------------
	{
		return &get();
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	virtual void ReleaseSubscription(const parent_type& parent) override
	//-----------------------------------------------------------------------------
	{
		// call the function operator of (a spezialized) KSubscriberReleaser..
		m_Parent = detail::KSubscriberReleaser<subscriber_type, parent_type>()(m_Rep, parent);
	}

	subscriber_type              m_Rep;
	std::unique_ptr<parent_type> m_Parent{nullptr};

};

DEKAF2_NAMESPACE_END

