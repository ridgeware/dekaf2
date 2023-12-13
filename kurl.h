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

/// @file kurl.h
/// URL and URL component handling

#include "kurlencode.h"
#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "kstream.h"
#include "kformat.h"
#include "ktemplate.h"
#include <cinttypes>

DEKAF2_NAMESPACE_BEGIN

namespace url {

/// @param sHostName hostname to be converted into the domain suffix
/// @return the domain suffix
DEKAF2_PUBLIC KStringView GetDomainSuffix   (KStringView sHostName);

DEKAF2_PUBLIC KString     GetDomainIdentity (KStringView sHostName);

DEKAF2_PUBLIC KStringView GetRootDomain     (KStringView sHostName);

DEKAF2_PUBLIC KStringView GetSubDomain      (KStringView sHostName);

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A generalized template class to hold all components of a URI except the protocol component.
/// We have three different storage types for the KURI components:
/// URLEncodedString
/// URLEncodedQuery
/// URLEncodedUInt
/// @tparam Storage the storage class, URLEncodedString, URLEncodedQuery (KProps), or URLEncodedUInt
/// @tparam Component  the URL component type
/// @tparam StartToken the start token, one of @:/;?#
/// @tparam RemoveStartSeparator whether to remove the start token when parsing
/// @tparam RemoveEndSeparator whether to remove the end token when parsing
/// @tparam IsString true if this is a string
template<
	class Storage,
	URIPart Component,
	const char StartToken,
	bool RemoveStartSeparator,
	bool RemoveEndSeparator,
	bool IsString
>
class URIComponent
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self_type = URIComponent<Storage, Component, StartToken, RemoveStartSeparator, RemoveEndSeparator, IsString>;

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	URIComponent () = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	URIComponent (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// constructs from integer types
	template<typename T, typename U = typename Storage::value_type,
	         typename std::enable_if<DEKAF2_PREFIX detail::is_pod<U>::value &&
	                                 std::is_convertible<T, U>::value == true, int>::type = 0>
	URIComponent (const T& t)
	//-------------------------------------------------------------------------
	{
		get() = t;
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance. bAppend is only used for Query
	/// components.
	KStringView Parse (KStringView svSource, bool bRequiresPrefix = false, bool bAppend = false)
	//-------------------------------------------------------------------------
	{
		if (Component != URIPart::Query || !bAppend)
		{
			// Query component can be appended through multiple parse runs if bAppend == true,
			// other components can't
			clear();
		}

		if (!svSource.empty())
		{
			if (Component == URIPart::Path)
			{
				// we always need a slash at the beginning of a Path to accept it
				// otherwise it would not be bidirectional to create a Path from a string
				// and to read back a Path into a string, as the slash is always needed to
				// separate the hostname / port from the path
				bRequiresPrefix = true;
			}

			if (!StartToken || (!bRequiresPrefix || svSource.front() == StartToken))
			{
				if (RemoveStartSeparator && svSource.front() == StartToken)
				{
					svSource.remove_prefix(1);
					m_bHadStartSeparator = true;
				}

				// this switch gets optimized away completely
				const char* NextToken;
				switch (Component)
				{
					case URIPart::User:
						// we do not search for a ':' here on purpose, instead
						// we search backwards for it from a found '@'
						NextToken = "@/;?#";
						break;
					case URIPart::Password:
						NextToken = "@:/;?#";
						break;
					case URIPart::Domain:
						NextToken = ":/;?#";
						break;
					case URIPart::Port:
						NextToken = "/;?#";
						break;
					case URIPart::Path:
						NextToken = "?#";
						break;
					case URIPart::Query:
						NextToken = "#";
						break;
					default:
					case URIPart::Fragment:
						NextToken = "";
						break;
				}

				size_t iFound;

				if (Component == URIPart::Domain && !svSource.empty() && svSource.front() == '[')
				{
					// an IPv6 address
					iFound = svSource.find(']');
					if (iFound == KStringView::npos)
					{
						// unterminated IPv6 address, bail out
						return svSource;
					}

					// we want to include the closing ] into the hostname string
					++iFound;
				}
				else
				{
					// anything else than an IPv6 address
					iFound = svSource.find_first_of(NextToken);
				}

				if (iFound == KStringView::npos)
				{
					if (Component == URIPart::User || Component == URIPart::Password)
					{
						// bail out - we need to find the @ for User or Password
						// or else this is no User or Password component of a KURI
						return svSource;
					}
					else
					{
						iFound = svSource.size();
					}
				}

				if (Component == URIPart::User || Component == URIPart::Password)
				{
					if (svSource[iFound] != '@')
					{
						// bail out - we need to find the @ for User or Password
						// or else this is no User or Password component of a KURI
						return svSource;
					}

					if (Component == URIPart::User)
					{
						// search backwards to check if there is a password separator
						auto iPass = svSource.rfind(':', iFound);
						if (iPass < iFound)
						{
							iFound  = iPass;
						}
					}
				}

				m_sStorage.Parse(svSource.substr(0, iFound), Component);

				svSource.remove_prefix(iFound);

				if (RemoveEndSeparator)
				{
					svSource.remove_prefix(1);
				}
			}
		}

		return svSource;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const URIComponent& operator>> (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	URIComponent& operator<< (KStringView sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// add the start separator (:, /, ?, #) when serializing? Is normally
	/// determined by the parsing stage, but needs to be set manually after
	/// setting the values in unencoded form.
	void WantStartSeparator() const
	//-------------------------------------------------------------------------
	{
		// only enable for those instances that actually also remove it
		m_bHadStartSeparator = RemoveStartSeparator;
	}

	//-------------------------------------------------------------------------
	/// serialize content into string from members (not user or password)
	template<URIPart C = Component, typename std::enable_if<C != URIPart::User && C != URIPart::Password, int>::type = 0 >
	bool Serialize (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		if (!m_sStorage.empty())
		{
			if (RemoveStartSeparator && m_bHadStartSeparator)
			{
				sTarget += StartToken;
			}

			m_sStorage.Serialize(sTarget, Component);
		}

		return true;
	}

	//-------------------------------------------------------------------------
	/// serialize content into string from user or password
	template<URIPart C = Component, typename std::enable_if<C == URIPart::User || C == URIPart::Password, int>::type = 0 >
	bool Serialize (KStringRef& sTarget, char chSuffix = '@') const
	//-------------------------------------------------------------------------
	{
		if (!m_sStorage.empty())
		{
			if (Component == URIPart::Password)
			{
				if (!sTarget.empty())
				{
					if (sTarget.back() == '@')
					{
						sTarget.erase(sTarget.size()-1, 1);
						sTarget += ':';
					}
				}
			}

			m_sStorage.Serialize(sTarget, Component);

			if (chSuffix != '\0')
			{
				sTarget += chSuffix;
			}
		}

		return true;
	}

	//-------------------------------------------------------------------------
	/// serialize content into stream from members (not user or password)
	template<URIPart C = Component, typename std::enable_if<C != URIPart::User && C != URIPart::Password, int>::type = 0 >
	bool Serialize (KOutStream& sTarget) const
	//-------------------------------------------------------------------------
	{
		if (!m_sStorage.empty())
		{
			if (RemoveStartSeparator && m_bHadStartSeparator)
			{
				sTarget += StartToken;
			}

			m_sStorage.Serialize(sTarget, Component);
		}

		return true;
	}

	//-------------------------------------------------------------------------
	/// serialize content into stream from user or password
	template<URIPart C = Component, typename std::enable_if<C == URIPart::User || C == URIPart::Password, int>::type = 0 >
	bool Serialize (KOutStream& sTarget, char chSuffix = '@') const
	//-------------------------------------------------------------------------
	{
		if (!m_sStorage.empty())
		{
			m_sStorage.Serialize(sTarget, Component);

			if (chSuffix != '\0')
			{
				sTarget += chSuffix;
			}
		}

		return true;
	}

	//-------------------------------------------------------------------------
	/// return encoded content, without leading separator
	KString Serialize() const
	//-------------------------------------------------------------------------
	{
		return m_sStorage.Serialize(Component);
	}

	//-------------------------------------------------------------------------
	/// restore instance to unpopulated state
	void clear ()
	//-------------------------------------------------------------------------
	{
		m_sStorage.clear();
		m_bHadStartSeparator = false;
	}

	//-------------------------------------------------------------------------
	/// return a const reference of the member
	const typename Storage::value_type& get () const
	//-------------------------------------------------------------------------
	{
		return m_sStorage.get();
	}

	//-------------------------------------------------------------------------
	/// return a reference of the member
	typename Storage::value_type& get ()
	//-------------------------------------------------------------------------
	{
		return m_sStorage.get();
	}

	//-------------------------------------------------------------------------
	/// return a const pointer of the member
	const typename Storage::value_type* operator-> () const
	//-------------------------------------------------------------------------
	{
		return &get();
	}

	//-------------------------------------------------------------------------
	/// return a pointer of the member
	typename Storage::value_type* operator-> ()
	//-------------------------------------------------------------------------
	{
		return &get();
	}

	//-------------------------------------------------------------------------
	/// return the key-value const value
	template<typename F, bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	auto& operator[] (F&& Search) const
	//-------------------------------------------------------------------------
	{
		return get()[std::forward<F>(Search)];
	}

	//-------------------------------------------------------------------------
	/// return the key-value value
	template<typename F, bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	auto& operator[] (F&& Search)
	//-------------------------------------------------------------------------
	{
		return get()[std::forward<F>(Search)];
	}

	//-------------------------------------------------------------------------
	/// modify member by setting argument
	template<bool X = IsString, typename std::enable_if<X, int>::type = 0 >
	void set (KStringView sv)
	//-------------------------------------------------------------------------
	{
		m_sStorage.set(sv);
	}

	//-------------------------------------------------------------------------
	/// operator KStringView () returns the decoded string
	template<bool X = IsString, typename std::enable_if<X, int>::type = 0 >
	operator KStringView () const
	//-------------------------------------------------------------------------
	{
		return get();
	}

	//-------------------------------------------------------------------------
	/// operator KStringView () returns the decoded string
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	operator typename Storage::value_type () const
	//-------------------------------------------------------------------------
	{
		return get();
	}

	//-------------------------------------------------------------------------
	/// return percent-encoded content
	KString Encoded() const
	//-------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// return percent-decoded content (for query part)
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	KString Decoded() const
	//-------------------------------------------------------------------------
	{
		// key-value pair strings _have_ to be percent encoded, as that is the
		// natural flat-string representation for them
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// return begin iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto begin()
	//-------------------------------------------------------------------------
	{
		return get().begin();
	}

	//-------------------------------------------------------------------------
	/// return begin iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto begin() const
	//-------------------------------------------------------------------------
	{
		return get().begin();
	}

	//-------------------------------------------------------------------------
	/// return end iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto end()
	//-------------------------------------------------------------------------
	{
		return get().end();
	}

	//-------------------------------------------------------------------------
	/// return end iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto end() const
	//-------------------------------------------------------------------------
	{
		return get().end();
	}

	//-------------------------------------------------------------------------
	/// search element
	template<typename F, typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto find(F&& Search)
	//-------------------------------------------------------------------------
	{
		return get().find(std::forward<F>(Search));
	}

	//-------------------------------------------------------------------------
	/// search element, const
	template<typename F, typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto find(F&& Search) const
	//-------------------------------------------------------------------------
	{
		return get().find(std::forward<F>(Search));
	}

	//-------------------------------------------------------------------------
	/// contains element, const
	template<typename F, bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	auto contains(F&& Search) const
	//-------------------------------------------------------------------------
	{
		return get().find(std::forward<F>(Search)) != end();
	}

	//-------------------------------------------------------------------------
	/// contains element, const
	template<typename F, bool X = IsString, typename std::enable_if<X, int>::type = 0 >
	auto contains(F&& Search) const
	//-------------------------------------------------------------------------
	{
		return get().find(std::forward<F>(Search)) != KString::npos;
	}

	//-------------------------------------------------------------------------
	/// return size
	template<typename T = typename Storage::value_type, typename std::enable_if<DEKAF2_PREFIX detail::is_pod<T>::value == false, int>::type = 0 >
	auto size() const
	//-------------------------------------------------------------------------
	{
		return get().size();
	}

	//-------------------------------------------------------------------------
	/// return percent-decoded content (for string parts)
	template<typename T = Storage, bool X = IsString, typename std::enable_if<X, int>::type = 0 >
	const typename T::value_type& Decoded() const
	//-------------------------------------------------------------------------
	{
		return get();
	}

	//-------------------------------------------------------------------------
	/// operator+=(KStringView) for the query part parses the encoded string
	/// and appends to existing query parms
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	URIComponent& operator+=(KStringView sv)
	//-------------------------------------------------------------------------
	{
		Parse (sv, false, true);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// operator+=(url::KQuery) appends to existing query parms
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	URIComponent& operator+=(self_type Query)
	//-------------------------------------------------------------------------
	{
		for (auto& it : Query.get())
		{
			get().Add(std::move(it.first), std::move(it.second));
		}
		return *this;
	}

	//-------------------------------------------------------------------------
	/// operator=(KStringView) for the query part parses the encoded string
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	URIComponent& operator=(KStringView sv)
	//-------------------------------------------------------------------------
	{
		Parse (sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// operator=(KStringView) sets the decoded string
	template<bool X = IsString, typename std::enable_if<X, int>::type = 0 >
	URIComponent& operator=(KStringView sv)
	//-------------------------------------------------------------------------
	{
		m_sStorage.set(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Predicate: Are there contents?
	bool empty () const
	//-------------------------------------------------------------------------
	{
		return m_sStorage.empty();
	}

	//-------------------------------------------------------------------------
	friend bool operator==(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.m_sStorage == right.m_sStorage;
	}

	//-------------------------------------------------------------------------
	friend bool operator!=(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.m_sStorage < right.m_sStorage;
	}

	//-------------------------------------------------------------------------
	friend bool operator> (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.m_sStorage > right.m_sStorage;
	}

//------
private:
//------

	Storage m_sStorage;
	mutable bool m_bHadStartSeparator { false };

}; // URIComponent

/*
template<
	class Storage,
	URIPart Component,
	const char StartToken,
	bool RemoveStartSeparator,
	bool RemoveEndSeparator,
	bool IsString
 >
*/

extern template class URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
extern template class URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
extern template class URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
extern template class URIComponent<URLEncodedUInt,   URIPart::Port,     ':',  true,  false, false>;
extern template class URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
extern template class URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
extern template class URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;

DEKAF2_NAMESPACE_END

using KUser     = detail::URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
using KPassword = detail::URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
using KDomain   = detail::URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
using KPort     = detail::URIComponent<URLEncodedUInt,   URIPart::Port,     ':',  true,  false, false>;
using KPath     = detail::URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
using KQuery    = detail::URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
using KFragment = detail::URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;
using KQueryParms = URLEncodedQuery::value_type;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The URL Schema is a bit different from the other URI components, therefore
/// we handle it manually
class DEKAF2_PUBLIC KProtocol
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum eProto : uint16_t
	{
		// Explicit values to guarantee map to m_sCanonical.
		UNDEFINED =  0,
		MAILTO    =  1, // MAILTO _has_ to stay at the second position after UNDEFINED!
		AUTO      =  2, // the "//" in HTML attributes (AUTO has to stay after MAILTO)
		HTTP      =  3,
		HTTPS     =  4,
		FILE      =  5,
		FTP       =  6,
		GIT       =  7,
		SVN       =  8,
		IRC       =  9,
		NEWS      = 10,
		NNTP      = 11,
		TELNET    = 12,
		GOPHER    = 13,
		UNIX      = 14,
		SMTP      = 15,
		SMTPS     = 16,
		WS        = 17,
		WSS       = 18,
		UNKNOWN   = 19  // UNKNOWN _has_ to be the last value
	};

	//-------------------------------------------------------------------------
	/// default constructor
	KProtocol () = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct an instance for enumerated protocol.
	KProtocol (eProto iProto)
	    : m_eProto {iProto}
	    //-------------------------------------------------------------------------
	{}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	template<typename T,
	        typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	KProtocol (const T& svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource, true);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance - if bAcceptWithoutColon is true
	/// also strings like "http" will be consumed, otherwise only strings followed
	/// by a colon, like "http:" or "http://".
	KStringView Parse (KStringView svSource, bool bAcceptWithoutColon = false);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// Serialize internal rep into arg KString
	const KProtocol& operator>> (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse arg into internal rep
	KProtocol& operator<< (KStringView sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// generate content into string from members
	bool Serialize (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += Serialize();
		return true;
	}

	//-------------------------------------------------------------------------
	/// generate content into string from members
	bool Serialize (KOutStream& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget.Write(Serialize());
		return true;
	}

	//-------------------------------------------------------------------------
	/// return encoded content
	KStringView Serialize() const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return percent-encoded content
	KStringView Encoded() const
	//-------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// return percent-decoded content
	KStringView Decoded() const
	//-------------------------------------------------------------------------
	{
		return get();
	}

	//-------------------------------------------------------------------------
	/// operator KStringView returns the decoded string
	operator KStringView() const
	//-------------------------------------------------------------------------
	{
		return Decoded();
	}

	//-------------------------------------------------------------------------
	/// restore instance to unpopulated state
	void clear ();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return a view of the member
	KStringView get () const
	//-------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// return the numeric scheme identifier
	eProto getProtocol () const
	//-------------------------------------------------------------------------
	{
		return m_eProto;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument - accepts also strings without following
	/// colon, like "http"
	void set (KStringView svProto)
	//-------------------------------------------------------------------------
	{
		Parse (svProto, true);
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this
	friend bool operator== (const KProtocol& left, const KProtocol& right)
	//-------------------------------------------------------------------------
	{
		if (left.m_eProto != right.m_eProto)
		{
			return false;
		}

		if (DEKAF2_UNLIKELY(left.m_eProto == UNKNOWN))
		{
			return left.m_sProto == right.m_sProto;
		}

		return true;
	}

	//-------------------------------------------------------------------------
	friend bool operator<(const KProtocol& left, const KProtocol& right)
	//-------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(left.m_eProto == UNKNOWN))
		{
			return left.m_sProto < right.m_sProto;
		}

		return left.m_eProto < right.m_eProto;
	}

	//-------------------------------------------------------------------------
	/// returns true if protocol was set/parsed
	bool empty () const
	//-------------------------------------------------------------------------
	{
		return (m_eProto == UNDEFINED);
	}

	//-------------------------------------------------------------------------
	/// returns the default port for the protocol, or 0 if unknown
	uint16_t DefaultPort() const;
	//-------------------------------------------------------------------------

//------
private:
//------

	void SetProto(KStringView svProto);

	KString m_sProto;
	eProto  m_eProto { UNDEFINED };

}; // KProtocol

DEKAF2_COMPARISON_OPERATORS(KProtocol)

/// returns true if the second parameter is a subdomain of the first
/// @param Domain the base domain
/// @param SubDomain the sub domain
DEKAF2_PUBLIC
bool IsSubDomainOf(const KDomain& Domain, const KDomain& SubDomain);

} // end of namespace url


//-------------------------------------------------------------------------
/// returns true if the second parameter is a subdomain of the first
/// @param Domain the base domain
/// @param SubDomain the sub domain
DEKAF2_PUBLIC inline
bool kIsSubDomainOf(const url::KDomain& Domain, const url::KDomain& SubDomain) { return url::IsSubDomainOf(Domain, SubDomain); }
//-------------------------------------------------------------------------


// forward declaration
class KURL;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps the Path and Query components of a URL
class DEKAF2_PUBLIC KResource
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// default ctor
	KResource() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct from any type that can be assigned to a KStringView
	template<typename T,
			 typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KResource(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	/// construct from a KURL
	KResource(const KURL& url);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// assign from any type that can be assigned to a KStringView
	template<typename T,
			 typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KResource& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// parse from a string view
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// clear object
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// serialize into a string
	/// @return always true
	bool Serialize(KStringRef& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// serialize into a stream
	/// @return always true
	bool Serialize(KOutStream& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const KResource& operator>> (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// return percent-encoded content
	KString Serialize() const
	//-------------------------------------------------------------------------
	{
		KString sReturn;
		Serialize(sReturn);
		return sReturn;
	}

	//-------------------------------------------------------------------------
	/// return percent-encoded content (same as Serialize())
	KString Encoded() const
	//-------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	KResource& operator<< (KStringView sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// is this resource object empty?
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return Path.empty();
	}

	//-------------------------------------------------------------------------
	friend bool operator==(const KResource& left, const KResource& right);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	friend bool operator!=(const KResource& left, const KResource& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	bool operator<(const KResource& other) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool operator>(const KResource& other) const
	//-------------------------------------------------------------------------
	{
		return other.operator<(*this);
	}

	//-------------------------------------------------------------------------
	bool operator<=(const KResource& other) const
	//-------------------------------------------------------------------------
	{
		return !operator>(other);
	}

	//-------------------------------------------------------------------------
	bool operator>=(const KResource& other) const
	//-------------------------------------------------------------------------
	{
		return !operator<(other);
	}


	url::KPath      Path;
	url::KQuery     Query;

//------
protected:
//------

	//-------------------------------------------------------------------------
	KStringView ParseResourcePart(KStringView svSource);
	//-------------------------------------------------------------------------

}; // KResource


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// keeps all parts of a URL
class DEKAF2_PUBLIC KURL : public KResource
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// default ctor
	KURL() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct from any type that can be assigned to a KStringView
	template<typename T,
	         typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KURL(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	/// construct from a KResource object
	KURL(KResource Resource)
	//-------------------------------------------------------------------------
		: KResource(std::move(Resource))
	{
	}

	//-------------------------------------------------------------------------
	/// assign from any type that can be assigned to a KStringView
	template<typename T,
	         typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KURL& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// parse from a string view
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// clear the object
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// generate content into encoded string from members
	/// @return always true
	bool Serialize(KStringRef& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// generate content into encoded stream from members
	/// @return always true
	bool Serialize (KOutStream& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return percent-encoded content
	KString Serialize() const
	//-------------------------------------------------------------------------
	{
		KString sReturn;
		Serialize(sReturn);
		return sReturn;
	}

	//-------------------------------------------------------------------------
	/// return percent-encoded content (same as Serialize())
	KString Encoded() const
	//-------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-------------------------------------------------------------------------
	/// return Path, Query as a URL encoded string (for backward compatibility with dekaf1)
	KString GetURI() const
	//-------------------------------------------------------------------------
	{
		return KResource::Serialize();
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const KURL& operator>> (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	KURL& operator<< (KStringView sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// is this a valid URL?
	bool IsURL () const
	//-------------------------------------------------------------------------
	{
		return !Protocol.empty()
		        && (!Domain.empty()
		            || (Protocol == url::KProtocol::FILE
		                && !Path.empty()));
	}

	//-------------------------------------------------------------------------
	/// is this a valid HTTP / HTTPS URL?
	bool IsHttpURL () const
	//-------------------------------------------------------------------------
	{
		return (Protocol == url::KProtocol::HTTPS ||
				Protocol == url::KProtocol::HTTP  ||
				Protocol == url::KProtocol::AUTO)
		        && !Domain.empty();
	}

	//-------------------------------------------------------------------------
	/// is this a valid HTTPS URL?
	bool IsHttpsURL () const
	//-------------------------------------------------------------------------
	{
		return Protocol == url::KProtocol::HTTPS
		       && !Domain.empty();
	}

	//-------------------------------------------------------------------------
	/// is this object empty?
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return Protocol.empty() && Domain.empty() && Path.empty();
	}

	//-------------------------------------------------------------------------
	friend bool operator==(const KURL& left, const KURL& right);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	friend bool operator!=(const KURL& left, const KURL& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	bool operator<(const KURL& other) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool operator>(const KURL& other) const
	//-------------------------------------------------------------------------
	{
		return other.operator<(*this);
	}

	//-------------------------------------------------------------------------
	bool operator<=(const KURL& other) const
	//-------------------------------------------------------------------------
	{
		return !operator>(other);
	}

	//-------------------------------------------------------------------------
	bool operator>=(const KURL& other) const
	//-------------------------------------------------------------------------
	{
		return !operator<(other);
	}

	//-------------------------------------------------------------------------
	/// returns the "base" domain as from kGetBaseDomain() for this URL
	KString GetBaseDomain() const
	//-------------------------------------------------------------------------
	{
		return url::GetDomainIdentity(Domain.get());
	}

	url::KProtocol  Protocol;
	url::KUser      User;
	url::KPassword  Password;
	url::KDomain    Domain;
	url::KPort      Port;
	// KResource::Path
	// KResource::Query
	url::KFragment  Fragment;

}; // KURL

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// keeps a domain and port number
class DEKAF2_PUBLIC KTCPEndPoint
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// default ctor
	KTCPEndPoint() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct from any type that can be assigned to a KStringView
	template<typename T,
	         typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KTCPEndPoint(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	/// construct from a KURL object
	KTCPEndPoint(const KURL& URL);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct from a domain and port
	KTCPEndPoint(url::KDomain domain, url::KPort port)
	//-------------------------------------------------------------------------
	    : Domain(std::move(domain))
	    , Port(std::move(port))
	{}

	//-------------------------------------------------------------------------
	/// construct from a domain and port
	KTCPEndPoint(KStringView sDomain, uint16_t iPort)
	//-------------------------------------------------------------------------
	    : Domain(sDomain)
		, Port(KString::to_string(iPort))
	{}

	//-------------------------------------------------------------------------
	/// assign from any type that can be assigned to a KStringView
	template<typename T,
	         typename std::enable_if<DEKAF2_PREFIX detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	KTCPEndPoint& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// parse from a string view
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// clear the object
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// is this object empty?
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return Domain.empty();
	}

	//-------------------------------------------------------------------------
	/// serialize into a string
	/// @return always true
	bool Serialize(KStringRef& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// serialize into a stream
	/// @return always true
	bool Serialize (KOutStream& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return encoded content
	KString Serialize() const
	//-------------------------------------------------------------------------
	{
		KString sReturn;
		Serialize(sReturn);
		return sReturn;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const KTCPEndPoint& operator>> (KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	friend bool operator==(const KTCPEndPoint& left, const KTCPEndPoint& right);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	friend bool operator!=(const KTCPEndPoint& left, const KTCPEndPoint& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	bool operator<(const KTCPEndPoint& other) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool operator>(const KTCPEndPoint& other) const
	//-------------------------------------------------------------------------
	{
		return other.operator<(*this);
	}

	//-------------------------------------------------------------------------
	bool operator<=(const KTCPEndPoint& other) const
	//-------------------------------------------------------------------------
	{
		return !operator>(other);
	}

	//-------------------------------------------------------------------------
	bool operator>=(const KTCPEndPoint& other) const
	//-------------------------------------------------------------------------
	{
		return !operator<(other);
	}
	
	url::KDomain    Domain;
	url::KPort      Port;
	bool            bIsUnixDomain { false };

}; // KTCPEndPoint

/// Convert into a special "Base" domain
/// @param sHostName hostname to be converted into the base domain
DEKAF2_PUBLIC inline
KString kGetBaseDomain (KStringView sHostName) { return url::GetDomainIdentity(sHostName); }

DEKAF2_NAMESPACE_END

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// kFormat formatters

namespace fmt
{

template <>
struct formatter<DEKAF2_PREFIX url::KUser> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KUser& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KPassword> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KPassword& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KDomain> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KDomain& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KPort> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KPort& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KPath> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KPath& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KQuery> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KQuery& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KFragment> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KFragment& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX url::KProtocol> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX url::KProtocol& URICompoment, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KResource> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KResource& Resource, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Resource.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KURL> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KURL& URL, FormatContext& ctx) const
	{
		return formatter<string_view>::format(URL.Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KTCPEndPoint> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KTCPEndPoint& EndPoint, FormatContext& ctx) const
	{
		return formatter<string_view>::format(EndPoint.Serialize(), ctx);
	}
};

} // end of namespace fmt
