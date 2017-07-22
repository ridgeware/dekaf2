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

template<class T>
class KSubscription : public T
{
public:
	using UnsubscribeCB = std::function<void()>;

	/// copy construction is possible, but does not get the subscribers
	KSubscription(const KSubscription& other)
	    : T(other)
	{
	}

	/// move construction is possible, and gets the subscribers, too
	KSubscription(KSubscription&& other)
	    : T(std::move(other))
	    , m_Subscribers(std::move(other.m_Subscribers))
	{
	}

	/// default constructor and any other
	template<class... Args>
	KSubscription(Args&&... args)
	    : T(std::forward<Args>(args)...)
	{
	}

	/// copy assignment is possible, but does not get the subscribers
	KSubscription& operator=(const KSubscription& other)
	{
		release_subscribers();
		T::operator=(other);
		return *this;
	}

	/// move assignment is possible, and gets the subscribers, too
	KSubscription& operator=(KSubscription&& other)
	{
		release_subscribers();
		T::operator=(std::move(other));
		m_Subscribers = std::move(other);
		return *this;
	}

	/// any assignment that the base class allows is allowed, too - but this does not remove the subscribers
	template<class... Args>
	KSubscription& operator=(Args&&... args)
	{
		T::operator=(std::forward<Args>(args)...);
		return *this;
	}

	~KSubscription()
	{
		release_subscribers();
	}

	const T& get() const
	{
		return *this;
	}

	T& get()
	{
		return *this;
	}

	bool Subscribe(UnsubscribeCB cb)
	{
		m_Subscribers.emplace_back(cb);
	}

	bool Unsubscribe(UnsubscribeCB cb)
	{
		auto it = std::find_if(m_Subscribers.rbegin(), m_Subscribers.rend(),
		                       [&cb](const UnsubscribeCB& val)->bool
		{
			return !memcmp(&cb, &val, sizeof(UnsubscribeCB));
		});

		if (it != m_Subscribers.rend())
		{
			m_Subscribers.erase(it.base());
		}
	}

	size_t Subscriptions() const
	{
		return m_Subscribers.size();
	}

protected:
	using Subscribers_t = std::vector<UnsubscribeCB>;

	void release_subscribers()
	{
		if (!m_Subscribers.empty())
		{
			for (auto& callback : m_Subscribers)
			{
				callback();
			}
			m_Subscribers.clear();
		}
	}

	Subscribers_t m_Subscribers;

}; // KSubscription


template<class T, class S>
class KSubscriber : public T
{
public:
	using Subscription = KSubscription<S>;

	/// default constructor and any other
	template<class... Args>
	KSubscriber(Args&&... args)
	    : T(std::forward<Args>(args)...)
	{
	}

	KSubscriber(const Subscription& subscription)
	    : T(subscription.get())
	    , m_Parent(&subscription)
	{
		subscribe_parent(m_Parent);
	}

	KSubscriber& operator=(const Subscription& subscription)
	{
		release_parent();
		T::operator=(subscription.get());
		m_Parent = &subscription;
		subscribe_parent(m_Parent);
		return *this;
	}

	// the catchall below would take a non-const value with priority
	// about the const operator= above.. The downside of perfect forwarding
	// and variadic templates
	KSubscriber& operator=(Subscription& subscription)
	{
		release_parent();
		T::operator=(subscription.get());
		m_Parent = &subscription;
		subscribe_parent(m_Parent);
		return *this;
	}

	/// any assignment that the base class allows is allowed, too, but it does not change the subscription..
	template<class... Args>
	KSubscriber& operator=(Args&&... args)
	{
		T::operator=(std::forward<Args>(args)...);
		return *this;
	}

	~KSubscriber()
	{
		if (m_Parent)
		{
			if (m_bParentIsOwn)
			{
				delete m_Parent;
			}
			else
			{
				m_Parent->Unsubscribe(std::bind(&KSubscriber::unsubscribe_parent, this));
			}
		}
	}

	inline const T& get() const
	{
		return *this;
	}

	inline T& get()
	{
		return *this;
	}

	size_t Siblings() const
	{
		if (!m_Parent)
		{
			return 0;
		}
		return m_Parent->Subscriptions();
	}

	void Clone()
	{
		if (m_Parent && !m_bParentIsOwn)
		{
			Subscription* Parent(m_Parent);
			clone();
			Parent->Unsubscribe(std::bind(&KSubscriber::unsubscribe_parent, this));
		}
	}

	bool HasSubscription() const
	{
		return (m_Parent && !m_bParentIsOwn);
	}

	bool HasClone() const
	{
		return (m_Parent && m_bParentIsOwn);
	}

protected:

	void release_parent()
	{
		if (m_Parent)
		{
			if (m_bParentIsOwn)
			{
				delete m_Parent;
				m_bParentIsOwn = false;
			}
			else
			{
				m_Parent->Unsubscribe(std::bind(&KSubscriber::unsubscribe_parent, this));
			}
			m_Parent = nullptr;
		}
	}

	void unsubscribe_parent()
	{
		if (m_Parent)
		{
			if (m_bParentIsOwn)
			{
				// this is an error - log it
				KLog().warning("KSubscriber has a request to unsubscribe, but is not subscribed");
			}
			else
			{
				clone();
			}
			m_Parent = nullptr;
		}
	}

	inline void subscribe_parent(Subscription* Parent)
	{
		Parent->Subscribe(std::bind(&KSubscriber::unsubscribe_parent, this));
	}

	/// default clone function - should be overwritten by a template specialisation
	/// if smarter copies are possible
	inline void clone()
	{
		m_Parent = new Subscription(m_Parent->get());
		m_bParentIsOwn = true;
	}

	Subscription* m_Parent{nullptr};
	bool          m_bParentIsOwn{false};
};


} // of namespace dekaf2

