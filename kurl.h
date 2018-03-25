/*
//=============================================================================
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
//=============================================================================
*/

#pragma once

#include <cinttypes>

#include "kurlencode.h"
#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "kstream.h"


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
    /// copy constructor
    URIComponent (const URIComponent& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// move constructor
    URIComponent (URIComponent&& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// copy assignment
    URIComponent& operator= (const URIComponent& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// move assignment
    URIComponent& operator= (URIComponent&& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// parses source into members of instance
    KStringView Parse (KStringView svSource, bool bRequiresPrefix = false)
    //-------------------------------------------------------------------------
    {
        clear();

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
                        NextToken = "@";
                        break;
                    case URIPart::Password:
                        NextToken = "@";
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
                    iFound = svSource.size();
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
    /// add the start separator (/, ?, #) when serializing? Is normally
    /// determined by the parsing stage, but needs to be set manually after
    /// setting the values in unencoded form.
    void WantStartSeparator() const
    //-------------------------------------------------------------------------
    {
        // only enable for those instances that actually also remove it
        m_bHadStartSeparator = RemoveStartSeparator;
    }

    //-------------------------------------------------------------------------
    /// generate content into string from members
    bool Serialize (KString& sTarget) const
    //-------------------------------------------------------------------------
    {
        if (m_bHadStartSeparator && !m_sStorage.empty())
        {
            sTarget += StartToken;
        }

        if (!m_sStorage.empty())
        {
            if (Component == URIPart::Password)
            {
                if (!sTarget.empty())
                {
                    if (sTarget.back() == '@')
                    {
                        sTarget.erase(sTarget.size()-1, 1);
                    }

                    sTarget += ':';
                }
            }

            m_sStorage.Serialize(sTarget, Component);

            if (Component == URIPart::User || Component == URIPart::Password)
            {
                sTarget += '@';
            }
        }

        return true;
    }

    //-------------------------------------------------------------------------
    /// generate content into string from members
    bool Serialize (KOutStream& sTarget) const
    //-------------------------------------------------------------------------
    {
        if (m_bHadStartSeparator && !m_sStorage.empty())
        {
            sTarget += StartToken;
        }

        if (!m_sStorage.empty())
        {
            if (Component == URIPart::Password)
            {
                // we should throw here or output an error as we cannot
                // add a password to an existing stream (because we would
                // have to rewind by one to remove the @ previously output).
                kWarning("cannot serialize a password to a stream")
                return false;
            }

            m_sStorage.Serialize(sTarget, Component);

            if (Component == URIPart::User)
            {
                sTarget += '@';
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
    /// operator=(KStringView) parses the decoded string
    URIComponent& operator=(KStringView sv)
    //-------------------------------------------------------------------------
    {
        Parse(sv);
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
        return operator<(right, left);
    }

//------
private:
//------

    Storage m_sStorage;
    mutable bool m_bHadStartSeparator{false};

};

#ifndef __clang__
extern template class URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
extern template class URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
extern template class URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
extern template class URIComponent<URLEncodedString, URIPart::Port,     ':',  true,  false, true >;
extern template class URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
extern template class URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
extern template class URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;
#endif

} // end of namespace dekaf2::url::detail

using KUser     = detail::URIComponent<URLEncodedString, URIPart::User,     '\0', false, true,  true >;
using KPassword = detail::URIComponent<URLEncodedString, URIPart::Password, '\0', false, true,  true >;
using KDomain   = detail::URIComponent<URLEncodedString, URIPart::Domain,   '\0', false, false, true >;
using KPort     = detail::URIComponent<URLEncodedString, URIPart::Port,     ':',  true,  false, true >;
using KPath     = detail::URIComponent<URLEncodedString, URIPart::Path,     '/',  false, false, true >;
using KQuery    = detail::URIComponent<URLEncodedQuery,  URIPart::Query,    '?',  true,  false, false>;
using KFragment = detail::URIComponent<URLEncodedString, URIPart::Fragment, '#',  true,  false, true >;

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
        HTTP      =  2,
        HTTPS     =  3,
        FILE      =  4,
        FTP       =  5,
        GIT       =  6,
        SVN       =  7,
        IRC       =  8,
        NEWS      =  9,
        NNTP      = 10,
        TELNET    = 11,
        GOPHER    = 12,
        UNKNOWN   = 13  // UNKNOWN _has_ to be the last value
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
        Parse (svSource);
    }

    //-------------------------------------------------------------------------
    /// parses source into members of instance - if bAcceptWithoutColon is true
    /// also strings like "http" will be consumed, otherwise only strings followed
    /// by a colon, like "http:" or "http://".
    KStringView Parse (KStringView svSource, bool bAcceptWithoutColon = false);
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// copy constructor
    KProtocol (const KProtocol& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// move constructor
    KProtocol (KProtocol&& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// copy assignment
    KProtocol& operator= (const KProtocol& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// moves members from other instance into this
    KProtocol& operator= (KProtocol&& other) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// Convert internal rep to KString
    operator KString () const
    //-------------------------------------------------------------------------
    {
        KString sResult;
        Serialize (sResult);
        return sResult;
    }

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
    bool Serialize (KString& sTarget) const;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// generate content into string from members
    bool Serialize (KOutStream& sTarget) const
    //-------------------------------------------------------------------------
    {
        KString str;
        Serialize(str);
        sTarget.Write(str);
        return true;
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
    /// restore instance to unpopulated state
    void clear ();
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    /// return a view of the member
    KString get () const
    //-------------------------------------------------------------------------
    {
        KString sEncoded;
        Serialize (sEncoded);
        return sEncoded;
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
    /// operator=(KStringView) parses the argument
    KProtocol& operator=(KStringView sv)
    //-------------------------------------------------------------------------
    {
        set(sv);
        return *this;
    }
    //-------------------------------------------------------------------------
    bool operator== (eProto iProto) const
    //-------------------------------------------------------------------------
    {
        return iProto == m_eProto;
    }

    //-------------------------------------------------------------------------
     bool operator!= (eProto iProto) const
    //-------------------------------------------------------------------------
    {
        return !operator== (iProto);
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
            return left.m_sProto == left.m_sProto;
        }

        return true;
    }

    //-------------------------------------------------------------------------
    /// compares other instance with this
    //-------------------------------------------------------------------------
    friend bool operator!= (const KProtocol& left, const KProtocol& right)
    {
        return !(left == right);
    }

    //-------------------------------------------------------------------------
    /// Predicate: Are there contents?
    inline bool empty () const
    //-------------------------------------------------------------------------
    {
        return (m_eProto == UNDEFINED);
    }

    //-------------------------------------------------------------------------
    uint16_t DefaultPort() const
    //-------------------------------------------------------------------------
    {
        return m_sCanonical[m_eProto].port;
    }

//------
private:
//------

    KString m_sProto {};
    eProto  m_eProto {UNDEFINED};
    struct Protocols
    {
        const uint16_t port;
        const KStringView::value_type* name;
    };
    static const Protocols m_sCanonical [UNKNOWN+1];

};

} // end of namespace url



// forward declaration
class KURL;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURI
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

    //-------------------------------------------------------------------------
    KURI() = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURI(KStringView sv)
    //-------------------------------------------------------------------------
    {
        Parse(sv);
    }

    //-------------------------------------------------------------------------
    KURI(const KString& str)
    //-------------------------------------------------------------------------
    {
        Parse(str);
    }

    //-------------------------------------------------------------------------
    KURI(const char* sp)
    //-------------------------------------------------------------------------
    : KURI(KStringView(sp))
    {
    }

    //-------------------------------------------------------------------------
    KURI(const KURI&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURI(KURI&&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURI& operator=(const KURI&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURI& operator=(KURI&&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURI& operator=(const KURL& url);
    //-------------------------------------------------------------------------

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
    const KURI& operator>> (KString& sTarget) const
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
    /// Parse stream style
    KURI& operator<< (KStringView sSource)
    //-------------------------------------------------------------------------
    {
        Parse (sSource);
        return *this;
    }

    url::KPath      Path;
    url::KQuery     Query;
    url::KFragment  Fragment;

}; // KURI

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

    //-------------------------------------------------------------------------
    KURL() = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURL(KStringView sv)
    //-------------------------------------------------------------------------
    {
        Parse(sv);
    }

    //-------------------------------------------------------------------------
    KURL(const KString& str)
    //-------------------------------------------------------------------------
    {
        Parse(str);
    }

    //-------------------------------------------------------------------------
    KURL(const char* sp)
    //-------------------------------------------------------------------------
    : KURL(KStringView(sp))
    {
    }

    //-------------------------------------------------------------------------
    KURL(const KURL&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURL(KURL&&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURL& operator=(const KURL&) = default;
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    KURL& operator=(KURL&&) = default;
    //-------------------------------------------------------------------------

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
        return (Protocol == url::KProtocol::HTTP || Protocol == url::KProtocol::HTTPS)
                && (!Domain.empty())
;
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
    KStringView getBaseDomain() const;
    //-------------------------------------------------------------------------

    url::KProtocol  Protocol;
    url::KUser      User;
    url::KPassword  Password;
    url::KDomain    Domain;
    url::KPort      Port;
    url::KPath      Path;
    url::KQuery     Query;
    url::KFragment  Fragment;

//------
protected:
//------

    mutable KString BaseDomain;

}; // KURL

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KTCPEndPoint
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

    KTCPEndPoint() = default;
    KTCPEndPoint(const KTCPEndPoint&) = default;
    KTCPEndPoint(KTCPEndPoint&&) = default;

	KTCPEndPoint(KStringView sv)
	: KTCPEndPoint(KURL(sv))
	{}

	KTCPEndPoint(const KString& str)
	: KTCPEndPoint(KURL(str))
	{}

	KTCPEndPoint(const char* sp)
	: KTCPEndPoint(KURL(sp))
	{}

    KTCPEndPoint(const KURL& URL)
    : Domain(URL.Domain)
    , Port(URL.Port)
    {}

    KTCPEndPoint(const url::KDomain& domain, const url::KPort& port)
    : Domain(domain)
    , Port(port)
    {}

    KTCPEndPoint& operator=(const KTCPEndPoint&) = default;
    KTCPEndPoint& operator=(KTCPEndPoint&&) = default;

    //-------------------------------------------------------------------------
    friend bool operator==(const KTCPEndPoint& left, const KTCPEndPoint& right);
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    friend bool operator!=(const KTCPEndPoint& left, const KTCPEndPoint& right)
    //-------------------------------------------------------------------------
    {
        return !operator==(left, right);
    }

    url::KDomain    Domain;
    url::KPort      Port;

}; // KTCPEndPoint

} // of namespace dekaf2
