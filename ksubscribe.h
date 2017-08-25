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

#include <functional>
#include <vector>
#include <algorithm>
#include "klog.h"

namespace dekaf2
{

template<typename parent_type>
class KSubscriberBase
{
	using self_type = KSubscriberBase;

public:

	virtual ~KSubscriberBase()
	{
	}

	size_t CountSubscriptions() const
	{
		size_t iResult{0};

		self_type* p = m_PrevSubscriber;
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

protected:

	/// do whatever has to be done to release the subscription
	/// - base version is abstract to force a template specialization
	/// for subscriber types
	virtual void ReleaseSubscription(const parent_type& parent) = 0;

	void Subscribe(const self_type* Subscription) const
	{
		m_NextSubscriber = Subscription->m_NextSubscriber;
		Subscription->m_NextSubscriber = const_cast<self_type*>(this);
	}

	void Unsubscribe() const
	{
		if (m_PrevSubscriber)
		{
			m_PrevSubscriber->m_NextSubscriber = m_NextSubscriber;
		}

		if (m_NextSubscriber)
		{
			m_NextSubscriber->m_PrevSubscriber = m_PrevSubscriber;
		}

		m_NextSubscriber = nullptr;
		m_PrevSubscriber = nullptr;
	}

	/// to be called from subscription class only..
	/// (would not harm if done by subscribers, but
	/// is not a design goal)
	void ReleaseSubscribers(const parent_type& parent)
	{
		if (m_NextSubscriber)
		{
			ReleaseSubscription(parent);

			m_NextSubscriber->ReleaseSubscribers(parent);
			m_NextSubscriber = nullptr;
			m_PrevSubscriber = nullptr;
		}
	}

protected:

	mutable self_type* m_NextSubscriber{nullptr};
	mutable self_type* m_PrevSubscriber{nullptr};

};

template<typename parent_type>
class KSubscription : public KSubscriberBase<parent_type>
{
	using base_type = KSubscriberBase<parent_type>;
	using self_type = KSubscription;

public:

	/// copy construction is possible, but does not get the subscribers
	KSubscription(const self_type& other)
	    : m_Rep(other.m_Rep)
	{
	}

	/// move construction is possible, and gets the subscribers, too
	KSubscription(self_type&& other)
	    : base_type(std::move(other))
	    , m_Rep(std::move(other.m_Rep))
	{
	}

	/// default constructor and any other
	template<class... Args>
	KSubscription(Args&&... args)
	    : m_Rep(std::forward<Args>(args)...)
	{
	}

	/// copy assignment is possible, but does not get the subscribers
	/// and we release our own subscribers..
	KSubscription& operator=(const self_type& other)
	{
		base_type::ReleaseSubscribers();
		m_Rep = other.m_Rep;
		return *this;
	}

	/// move assignment is possible, and gets the subscribers, too
	KSubscription& operator=(self_type&& other)
	{
		base_type::ReleaseSubscribers();
		m_Rep = std::move(other.m_Rep);
		base_type::operator=(std::move(other));
		return *this;
	}

	/// any assignment that the base class allows is allowed, too
	/// - but this removes the subscribers
	template<class Arg>
	KSubscription& operator=(Arg&& args)
	{
		base_type::ReleaseSubscribers();
		m_Rep = std::forward<Arg>(args);
		return *this;
	}

	~KSubscription()
	{
		base_type::ReleaseSubscribers(*this);
	}

	const parent_type& get() const
	{
		return m_Rep;
	}

	parent_type& get()
	{
		return m_Rep;
	}

	inline operator const parent_type&() const
	{
		return get();
	}

	inline operator parent_type&()
	{
		return get();
	}

	inline const parent_type& operator*() const
	{
		return get();
	}

	inline parent_type& operator*()
	{
		return get();
	}

	inline const parent_type* operator->() const
	{
		return &get();
	}

	inline parent_type* operator->()
	{
		return &get();
	}

protected:

	// does nothing, needed to make this class non-abstract
	virtual void ReleaseSubscription(const parent_type& parent) override
	{
	}

	parent_type m_Rep;

}; // KSubscription


template<typename subscriber_type, typename parent_type>
class KSubscriber : public KSubscriberBase<parent_type>
{
	using base_type      = KSubscriberBase<parent_type>;
	using self_type      = KSubscriber;
	using subscript_type = KSubscription<parent_type>;

public:

	KSubscriber()
	{
	}

	/// ctor. constructs m_Rep with args and binds to subscription
	template<class... Args>
	KSubscriber(const subscript_type& subscription, Args&&... args)
	    : m_Rep(std::forward<Args>(args)...)
	    , m_Parent(&subscription.get())
	{
		base_type::Subscribe(&subscription);
	}

	template<class... Args>
	self_type& SetSubscription(const subscript_type& subscription, Args&&... args)
	{
		base_type::Unsubscribe();
		m_Parent = &subscription.get();
		base_type::Subscribe(&subscription);
		m_OwnParent = false;
		m_Rep = subscriber_type(std::forward<Args>(args)...);
	}

	/// Assignment. Only works if m_Rep can be constructed
	/// by assigning subscription
	self_type& operator=(const subscript_type& subscription)
	{
		base_type::Unsubscribe();
		m_Parent = &subscription.get();
		base_type::Subscribe(&subscription);
		m_OwnParent = false;
		// assignment from parent to rep must be possible
		m_Rep = *m_Parent;
		return *this;
	}

	// the catchall below would take a non-const value with priority
	// about the const operator= above.. The downside of perfect forwarding
	// and variadic templates
	self_type& operator=(subscript_type& subscription)
	{
		base_type::Unsubscribe();
		m_Parent = &subscription.get();
		base_type::Subscribe(&subscription);
		m_OwnParent = false;
		// assignment from parent to rep must be possible
		m_Rep = *m_Parent;
		return *this;
	}

	/* not sure I want this..
	/// any assignment that the base class allows is allowed,
	/// but it removes the subscription and does not add a new one
	template<class... Args>
	KSubscriber& operator=(Args&&... args)
	{
		Unsubscribe();
		T::operator=(std::forward<Args>(args)...);
		return *this;
	}
	*/

	~KSubscriber()
	{
		if (m_OwnParent)
		{
			delete m_Parent;
		}
		else
		{
			base_type::Unsubscribe();
		}
	}

	inline const subscriber_type& get() const
	{
		return *this;
	}

	inline subscriber_type& get()
	{
		return *this;
	}

	inline operator const subscriber_type&() const
	{
		return get();
	}

	inline operator subscriber_type&()
	{
		return get();
	}

	inline const subscriber_type& operator*() const
	{
		return get();
	}

	inline subscriber_type& operator*()
	{
		return get();
	}

	inline const subscriber_type* operator->() const
	{
		return &get();
	}

	inline subscriber_type* operator->()
	{
		return &get();
	}

/*
 * 	/// default clone function - must be overwritten by a template specialisation
	/// for each type this design should work for
	virtual void ReleaseSubscription()
	{
	}
*/

protected:

	virtual void ReleaseSubscription(const parent_type& parent) override
	{
	}

	subscriber_type    m_Rep;
	const parent_type* m_Parent{nullptr};
	bool               m_OwnParent{false};

};



inline void testfun()
{
	using subscript_t = KSubscription<KString>;
	subscript_t buffer("hallo");

	using sub_t = KSubscriber<KStringView, KString>;
	sub_t view(buffer);
}


} // of namespace dekaf2

