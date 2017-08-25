///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cinttypes>
#include "kstring.h"

using string_view = dekaf2::KStringView;

namespace dekaf2
{

    // https://en.wikipedia.org/wiki/URL
    // scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]

// Suppose you want to parse parts from:
//     hint = 0;
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     KProto::KProto kproto1(URL, hint);
// The zero is a "hint" to start parsing at offset 0.
// When KProto is done with a successful parse, it update hint (to 8 here).
// This is the index immediately following the protocol or scheme.
// Using this hint, we can now ask to parse the domain.
//     KDomain::KDomain kdomain1(URL, hint);
// Other ctors and parse functions work similarly.
// Suppose we have a scrap of URL from which we wish to parse a domain.
//     scrap="jlettvin@github.com"; // strlen(scrap) == 19
//     here = 0;
//     KDomain::KDomain kdomain2(scrap, here);
// here will have the value 19 after this.
// A full URL is divided and parsed by running a sequence:
//     hint = 0;
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     KProto::KProto(URL, hint);    // Updates hint to 8
//     KDomain::KDomain(URL, hint);  // Updates hint to 32
//     KURI::KURI(URL, hint);        // Updates hint to 64
// URL[hint] == '\0';  // End of string

const size_t npos = (size_t)~0ULL;  // otherwise include std::string.

bool unimplemented(const KString& name, const char* __file__, size_t __line__);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// this is a description
/// hint is the starting offset for parsing.
/// If successful, hint is updated to the first offset after the scheme.
/// Otherwise m_error is set true.
/// For typical calls, scheme is at the beginning, so hint should == 0UL.
/// parse methods shall return false on error, true on success.
class KProto
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KProto ();
		KProto (const KString& source);
		KProto (const KString& source, size_t& hint);
		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		bool getProtocol     (string_view& target) const;

		void clear ();

		enum eProto : uint16_t
		{
			UNKNOWN = 0,
			HTTP    = 80,
			HTTPS   = 443,
			FTP     = 23
		};

    private:
		// bool parse (); // do we need this?

		bool m_error{false};
		bool m_mailto{false};
		KString m_proto{""};
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KUserInfo
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KUserInfo ();
	    KUserInfo (const KString& source);
		KUserInfo (const KString& source, size_t& hint);
		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		void clear ();

    private:
		bool m_error{false};
		KString m_user{""};
		KString m_pass{""};
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KDomain
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KDomain ();
	    KDomain (const KString& source);
		KDomain (const KString& source, size_t& hint);
		//KDomain (KString&& source);
		//KDomain (const KDomain& other);
		//KDomain (const string_view& sv);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		const KString&   getHostname ()   const
		{
			return m_sHostName;
		}
		const KString&   getDomain ()     const
		{
			return m_sDomainName;
		}
		const KString&   getTLD ()        const
		{
			return m_sDomainName;
		} // aka getDomain

		const KString&   getBaseDomain () const
		{
			return m_sBaseDomain;
		}
		int              getPortNum ()    const
		{
			return m_iPortNum;
		}

		void             clear ();

    private:
		bool     m_error;
		uint16_t m_iPortNum;
		KString  m_sPortName;
		KString  m_sHostName;
		KString  m_sDomainName;
		KString  m_sBaseDomain;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURIPath
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KURIPath ();
		KURIPath (const KString& source);
		KURIPath (const KString& source, size_t& hint);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		void clear ();
    private:
		bool m_error{false};
		KString m_path{""};

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KQueryParms
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KQueryParms    ();
		KQueryParms    (const KString& source);
		KQueryParms    (const KString& source, size_t& hint);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		bool querystring  (string_view& target) const;

		void clear ();
    private:
		bool m_error{false};
		KString m_query{""};

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KFragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KFragment    ();
	    KFragment    (const KString& source);
		KFragment    (const KString& source, size_t& hint);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		bool fragment  (string_view& target) const;

		void clear ();
    private:
		bool m_error{false};
		KString m_fragment{""};

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURI : public KURIPath, KQueryParms
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KURI    ();
		KURI    (const KString& source);
		KURI    (const KString& source, size_t& hint);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;

		bool getURI    (string_view& target) const;

		void clear ();

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KURL : public KProto, KUserInfo, KDomain, KURI, KFragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
    public:
	    KURL    ();
		KURL    (const KString& source);
		KURL    (const KString& source, size_t& hint);

		bool parse     (const KString& source, size_t& hint);
		bool serialize (      KString& target) const;
		void clear ();
    private:
		bool m_error{false};
};

/*
/////////////////////////////////////////////////////////////////////////////
class KURL
/////////////////////////////////////////////////////////////////////////////
{
public:
		KURL ();
		KURL (const KString& sURL);

		bool      Parse       (const KString& sURL);
		KString   FormURL     () const;

		KString   GetURL ()           { return m_sURL; } const;
		KString   GetProtocol ()      { return m_sProtocol; };
		KString   GetFullPath ()      { return m_sFullPath; };
		KString   GetPath2File ()     { return m_sPath2File; };
		KString   GetFilename ()      { return m_sFilename; };
		KString   GetFileExt ()       { return m_sFileExt; };
		KString   GetQueryString ()   { return m_sQueryString; };

		bool      SetProtocol      (const KString& sProtocol);
		bool      SetProtocol      (KProto& iProtocol);
		bool      SetHostname      (const KString& sHostname);
		bool      SetPortNum       (int iPortNum)   { m_iPortNum = iPortNum; };
		bool      SetQueryString   (const KString& sQueryString);

		static bool IsURL         (const KString& sString);
		static bool IsEmail       (const KString& sString);
		static bool ParseProtocol (const KString& sString, KString& sProtocol, KProto& iProtocol, int& iDefaultPort);
		static bool ParseHostname (const KString& sString, KString& sHostname, KString& sDomainName);

private:
		KString   m_sURL;
		KString   m_sProtocol;
		KProto    m_iProtocol;
		KString   m_sHostname;
		KString   m_sDomainName;
		KString   m_sMailTo;
		int       m_iPortNum;
		KString   m_sFullPath;
		KString   m_sPath2File;
		KString   m_sFilename;
		KString   m_sFileExt;
		KString   m_sQueryString;
		std::map  m_QueryParms;
};

*/

} // of namespace dekaf2
