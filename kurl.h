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

#include "kurlencode.h"
#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "kstream.h"
#include "kformat.h"
#include <cinttypes>


namespace dekaf2 {

//-------------------------------------------------------------------------
KString kGetBaseDomain (KStringView sHostName);
//-------------------------------------------------------------------------

namespace url {
namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// we have two different storage types for the KURI components:
// URLEncodedString
// KProps
// For some of the class methods we need specializations
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

	using self_type      = URIComponent<Storage, Component, StartToken, RemoveStartSeparator, RemoveEndSeparator, IsString>;

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
	template<typename T, typename U = typename Storage::value_type,
	         typename std::enable_if<std::is_pod<U>::value == true &&
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
	const URIComponent& operator>> (KString& sTarget) const
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
	/// generate content into string from members (not user or password)
	template<URIPart C = Component, typename std::enable_if<C != URIPart::User && C != URIPart::Password, int>::type = 0 >
	bool Serialize (KString& sTarget) const
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
	/// generate content into string from user or password
	template<URIPart C = Component, typename std::enable_if<C == URIPart::User || C == URIPart::Password, int>::type = 0 >
	bool Serialize (KString& sTarget, char chSuffix = '@') const
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
	/// generate content into stream from members (not user or password)
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
	/// generate content into stream from user or password
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
	/// return the key-value value
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	const KString& operator[] (KStringView sv) const
	//-------------------------------------------------------------------------
	{
		return get()[sv];
	}

	//-------------------------------------------------------------------------
	/// return the key-value value
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	KString& operator[] (KStringView sv)
	//-------------------------------------------------------------------------
	{
		return get()[sv];
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
	template<typename T = typename Storage::value_type, typename std::enable_if<std::is_pod<T>::value == false, int>::type = 0 >
	auto begin()
	//-------------------------------------------------------------------------
	{
		return get().begin();
	}

	//-------------------------------------------------------------------------
	/// return begin iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<std::is_pod<T>::value == false, int>::type = 0 >
	auto begin() const
	//-------------------------------------------------------------------------
	{
		return get().begin();
	}

	//-------------------------------------------------------------------------
	/// return end iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<std::is_pod<T>::value == false, int>::type = 0 >
	auto end()
	//-------------------------------------------------------------------------
	{
		return get().end();
	}

	//-------------------------------------------------------------------------
	/// return end iterator
	template<typename T = typename Storage::value_type, typename std::enable_if<std::is_pod<T>::value == false, int>::type = 0 >
	auto end() const
	//-------------------------------------------------------------------------
	{
		return get().end();
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
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
	friend bool operator==(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.m_sStorage == right.m_sStorage;
	}

	//-------------------------------------------------------------------------
	template<bool X = IsString, typename std::enable_if<!X, int>::type = 0 >
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

} // end of namespace dekaf2::url::detail

using KUser     = detail::URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
using KPassword = detail::URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
using KDomain   = detail::URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
using KPort     = detail::URIComponent<URLEncodedUInt,   URIPart::Port,     ':',  true,  false, false>;
using KPath     = detail::URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
using KQuery    = detail::URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
using KFragment = detail::URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;
using KQueryParms = URLEncodedQuery::value_type;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// The URL Schema is a bit different from the other URI components, therefore
// we handle it manually
class KProtocol
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
	KProtocol (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource, true);
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	KProtocol (const KString& sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource, true);
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	KProtocol (const char* sSource)
	//-------------------------------------------------------------------------
	{
		Parse (sSource, true);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance - if bAcceptWithoutColon is true
	/// also strings like "http" will be consumed, otherwise only strings followed
	/// by a colon, like "http:" or "http://".
	KStringView Parse (KStringView svSource, bool bAcceptWithoutColon = false);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// Serialize internal rep into arg KString
	const KProtocol& operator>> (KString& sTarget) const
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
	bool Serialize (KString& sTarget) const
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
	/// compares other instance with this
	friend bool operator!= (const KProtocol& left, const KProtocol& right)
	//-------------------------------------------------------------------------
	{
		return !(left == right);
	}

	//-------------------------------------------------------------------------
	bool operator<(const KProtocol& other) const
	//-------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(m_eProto == UNKNOWN))
		{
			return m_sProto < other.m_sProto;
		}

		return m_eProto < other.m_eProto;
	}

	//-------------------------------------------------------------------------
	bool operator>(const KProtocol& other) const
	//-------------------------------------------------------------------------
	{
		return other.operator<(*this);
	}

	//-------------------------------------------------------------------------
	bool operator<=(const KProtocol& other) const
	//-------------------------------------------------------------------------
	{
		return !operator>(other);
	}

	//-------------------------------------------------------------------------
	bool operator>=(const KProtocol& other) const
	//-------------------------------------------------------------------------
	{
		return !operator<(other);
	}
	//-------------------------------------------------------------------------
	/// returns true if protocol was set/parsed
	bool empty () const
	//-------------------------------------------------------------------------
	{
		return (m_eProto == UNDEFINED);
	}

	//-------------------------------------------------------------------------
	uint16_t DefaultPort() const;
	//-------------------------------------------------------------------------

//------
private:
//------

	void SetProto(KStringView svProto);

	KString m_sProto;
	eProto  m_eProto { UNDEFINED };

}; // KProtocol

} // end of namespace url


//-------------------------------------------------------------------------
/// returns true if the second parameter is a subdomain of the first
/// @param Domain the base domain
/// @param SubDomain the sub domain
bool kIsSubDomainOf(const url::KDomain& Domain, const url::KDomain& SubDomain);
//-------------------------------------------------------------------------


// forward declaration
class KURL;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KResource
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	KResource() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KResource(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	KResource(const KURL& url);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KResource& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool Serialize(KString& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool Serialize(KOutStream& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const KResource& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

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
class KURL : public KResource
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	KURL() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KURL(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	KURL(KResource Resource)
	//-------------------------------------------------------------------------
		: KResource(std::move(Resource))
	{
	}

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KURL& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// generate content into encoded string from members
	bool Serialize(KString& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// generate content into encoded stream from members
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
	const KURL& operator>> (KString& sTarget) const
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
		return (Protocol == url::KProtocol::HTTP || Protocol == url::KProtocol::HTTPS || Protocol == url::KProtocol::AUTO)
		        && (!Domain.empty());
	}

	//-------------------------------------------------------------------------
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
	KString GetBaseDomain() const
	//-------------------------------------------------------------------------
	{
		return kGetBaseDomain(Domain.get());
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
class KTCPEndPoint
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	KTCPEndPoint() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KTCPEndPoint(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
	}

	//-------------------------------------------------------------------------
	KTCPEndPoint(const KURL& URL);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KTCPEndPoint(url::KDomain domain, url::KPort port)
	//-------------------------------------------------------------------------
	    : Domain(std::move(domain))
	    , Port(std::move(port))
	{}

	//-------------------------------------------------------------------------
	KTCPEndPoint(KStringView sDomain, uint16_t iPort)
	//-------------------------------------------------------------------------
	    : Domain(sDomain)
		, Port(KString::to_string(iPort))
	{}

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	KTCPEndPoint& operator=(const T& sv)
	//-------------------------------------------------------------------------
	{
		Parse(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	KStringView Parse(KStringView svSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	void clear();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return Domain.empty();
	}

	//-------------------------------------------------------------------------
	bool Serialize(KString& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// generate content into string from members
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
	const KTCPEndPoint& operator>> (KString& sTarget) const
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

}; // KTCPEndPoint

} // of namespace dekaf2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// kFormat formatters

template <>
struct fmt::formatter<dekaf2::url::KUser> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KUser& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KPassword> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KPassword& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KDomain> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KDomain& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KPort> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KPort& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KPath> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KPath& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KQuery> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KQuery& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KFragment> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KFragment& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::url::KProtocol> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::url::KProtocol& URICompoment, FormatContext& ctx)
	{
		return formatter<string_view>::format(URICompoment.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::KResource> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KResource& Resource, FormatContext& ctx)
	{
		return formatter<string_view>::format(Resource.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::KURL> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KURL& URL, FormatContext& ctx)
	{
		return formatter<string_view>::format(URL.Serialize(), ctx);
	}
};

template <>
struct fmt::formatter<dekaf2::KTCPEndPoint> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KTCPEndPoint& EndPoint, FormatContext& ctx)
	{
		return formatter<string_view>::format(EndPoint.Serialize(), ctx);
	}
};
