//=============================================================================
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
// |\|  other LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|  otherWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
//=============================================================================

#pragma once

#include <cinttypes>
#include "kstring.h"
#include "kstringutils.h"
#include "kprops.h"

namespace dekaf2
{

namespace KURL
{

// https://en.wikipedia.org/wiki/URL
// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]

// Suppose you want to parse fields from:
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     Protocol::Protocol kproto1(URL, 0);
// The 0 is the offset at which to start parsing.
// All these classes store the initial offset internally.
// On successful parse, the offset of the next character past what was parsed
// is stored instead.  This offset is to be retrieved from the class
// in order to provide the next class constructor/parse with a valid offset.
// This design allows independent parsing for an autonomous field or
// sequenced dependent parsing for multi-field strings.
// To see an example, review the parse method for KURL (last in file kurl.cpp).

// For Protocol:
// When Protocol is done with a successful parse, it stores a new offset
// internally at m_iEndOffset (8 for "https://" starting from iOffset==0).
// This is the offset immediately following the protocol or scheme.
// We can now parse UserInfo starting at the stored offset.
// Using the offset from UserInfo, we can now parse the domain.
//     Domain::Domain kdomain1(URL, offset);
// Other ctors and parse functions work similarly.
// Suppose we have a scrap of URL from which we wish to parse a domain.
//     scrap="jlettvin@github.com"; // strlen(scrap) == 19
//     here = 0;
//     Domain::Domain kdomain2(scrap, here);
// here will have the value 19 after this.
// A full URL is divided and parsed by running a sequence:
//     offset = 0;
//     URL="https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
//     Protocol::Protocol(URL, offset);    // update offset to  8 using getEndOffset()
//     Domain::Domain(URL, offset);  // update offset to 32 using getEndOffset()
//     URI::URI(URL, offset);        // update offset to 64 using getEndOffset()
// URL[offset] == '\0';  // End of string

// I think I am turning Hungarian.
// Members m_aHungarian, Statics s_bHungarian, all else is unprefixed Hungarian.

// TODO there is much common code between classes.  virtual might work well.
// Specifically, getEndOffset is always the same.
// All classes have members m_iOffset and m_bError.
// All classes have a parse, serialize, and clear.
// Only these last methods require specialization.
//-----------------------------------------------------------------------------
// iOffset is the starting offset for parsing.
// Class::parse stores iOffset and updates it on successful parse.
// This parse length is stored in m_iEndOffset which value is
// returned by the method size_t iOffset = getEndOffset ();
// If successful m_bError is set false and iOffset will have a larger value.
// Otherwise m_bError is set true and this value is zero.
// For typical calls, scheme is at the beginning, so iOffset should begin 0.
// parse methods shall return false on error, true on success.



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "scheme" portion of w3 URL.
/// @brief RFC3986 3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
/// Implementation: We take all characters until ':'.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
/// ------
/// Protocol extracts and stores a KStringView of a URL "scheme".
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Protocol
{

//------
public:
//------

	enum eProto : uint16_t  // unused at the moment.
	{
		UNDEFINED,
		FTP,
		HTTP,
		HTTPS,
		MAILTO,
		UNKNOWN
	};

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline Protocol () {}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Protocol    (const KStringView& svSource, size_t iOffset=0)
	{
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse         (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Protocol    (const Protocol &  other)
	{
		m_bError     =  other.m_bError;
		m_bMailto    =  other.m_bMailto;
		m_iEndOffset =  other.m_iEndOffset;
		m_sProto     =  other.m_sProto;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Protocol    (      Protocol && other)
	{
		m_bError     =  other.m_bError;
		m_bMailto    =  other.m_bMailto;
		m_iEndOffset =  other.m_iEndOffset;
		m_sProto     =  std::move (other.m_sProto);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Protocol&   operator= (const Protocol &  other) noexcept
	{
		m_bError     =  other.m_bError;
		m_bMailto    =  other.m_bMailto;
		m_iEndOffset =  other.m_iEndOffset;
		m_sProto     =  other.m_sProto;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Protocol&   operator= (      Protocol && other) noexcept
	{
		m_bError     =  other.m_bError;
		m_bMailto    =  other.m_bMailto;
		m_iEndOffset =  other.m_iEndOffset;
		m_sProto     = std::move (other.m_sProto);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		if (!m_bError && m_sProto.size () != 0)
		{
			sTarget += m_sProto;
			sTarget += m_bMailto ? ":" : "://";
			return true;
		}
		return false;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		m_bError  = false;
		m_bMailto = false;
		m_sProto  = KString {};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KString getProtocol () const
	{
		KString sEncoded;
		kUrlEncode (m_sProto, sEncoded);
		return sEncoded;
	}
	inline eProto getProtocolEnum () const
	{
		return m_eProto;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setProtocol (const KStringView& svProto)
	{
		parse (svProto, 0);
	}

	//---------------------------------------------------------------------
	/// identify that "mailto:" was parsed
	inline bool isEmail () const
	{
		return m_bMailto;
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	bool            m_bError     {false};
	bool            m_bDecode    {false};
	bool            m_bMailto    {false};
	KString         m_sProto     {};
	eProto          m_eProto     {UNDEFINED};
	size_t          m_iEndOffset {0};

};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const Protocol& lhs, const Protocol& rhs)
{
	return lhs.getProtocol () == rhs.getProtocol ();
}

//-----------------------------------------------------------------------------
/// compares other instance with this, member-by-member
inline bool operator!= (const Protocol& lhs, const Protocol& rhs)
{
	return lhs.getProtocol () != rhs.getProtocol ();
}



//## use the @brief for the first sentence of your comment, not for the complex
//## detail. BTW, with doxygen, the first sentence of a multiline doc comment
//## is automatically used as the @brief, if you want to save some typing
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "user" and "password" portion of w3 URL.
/// @brief RFC3986 3.2: authority   = [ userinfo "@" ] host [ ":" port ]
/// Implementation: We take all characters until '@'.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///            ----  --------
/// User extracts and stores a KStringView of URL "user" and "password".
//## This colon comment should come directly after the class declaration, so
//## one line further down. Please correct at all occurences.
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class User
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline User ()
	//## put a closing //----- comment exactly here.. please correct all occurences.
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline User    (const KStringView& svSource, size_t iOffset=0)
	{
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse  (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline User              (const User &  other)
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sUser      =  other.m_sUser;
		m_sPass      =  other.m_sPass;
		//## what about m_bDecode ?
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline User              (      User && other)
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sUser      = std::move (other.m_sUser);
		m_sPass      = std::move (other.m_sPass);
		//## what about m_bDecode ?
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline User&   operator= (const User &  other) noexcept
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sUser      =  other.m_sUser;
		m_sPass      =  other.m_sPass;
		//## what about m_bDecode ?
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline User&   operator= (      User && other) noexcept
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sUser      = std::move (other.m_sUser);
		m_sPass      = std::move (other.m_sPass);
		//## what about m_bDecode ?
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		if (m_sUser.size ())
		{
			sTarget += m_sUser;
			if (m_sPass.size ())
			{
				sTarget += ':';
				sTarget += m_sPass;
			}
			sTarget += '@';
		}
		return !m_bError;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		m_bError = false;
		//## use string.clear() (in both cases)
		m_sUser = KString{};
		m_sPass = KString{};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	//## why do you encode the value? how do I get access to the username
	//## without encoding?
	inline KStringView getUser () const
	{
		KString sEncoded;
		kUrlEncode (m_sUser, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setUser (const KStringView& svUser)
	{
		m_sUser = svUser;
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	//## why do you encode the value? how do I get access to the password
	//## without encoding?
	inline KStringView getPass () const
	{
		KString sEncoded;
		kUrlEncode (m_sPass, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setPass (const KStringView& svPass)
	{
		m_sPass = svPass;
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	//## why is m_bError a member variable? there is no
	//## function to query it. Wouldn't it suffice to
	//## use it internally in parse() ?
	bool            m_bError    {false};
	//## same question for m_bDecode
	bool            m_bDecode   {false};
	KString         m_sUser     {};
	KString         m_sPass     {};
	size_t          m_iEndOffset{0};
};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const User& lhs, const User& rhs)
{
	return
		(lhs.getUser () == rhs.getUser ()) &&
		(lhs.getPass () == rhs.getPass ()) ;
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const User& lhs, const User& rhs)
{
	return
		(lhs.getUser () != rhs.getUser ()) ||
		(lhs.getPass () != rhs.getPass ()) ;
}


//## see my comments about briefs and doc comments above, and about the
//## position of the closing //:::::
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "host" and "port" portion of w3 URL.
/// @brief RFC3986 3.2: authority = [ userinfo "@" ] host [ ":" port ]
/// @brief RFC3986 3.2.2: host = IP-literal / IPv4address / reg-name
/// @brief RFC3986 3.2.3: port = *DIGIT
/// Implementation: We take domain from '@'+1 to first of "/:?#\0".
/// Implementation: We take port from ':'+1 to first of "/?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                             ----  ----
/// Domain extracts and stores a KStringView of URL "host" and "port"
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Domain
{

	//------
	public:
	//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline Domain ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Domain  (const KStringView& svSource, size_t iOffset=0)
	{
		//## see questions about this var in the User class.
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse  (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Domain            (const Domain &  other)
	{
		//## misses the encoding flag
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  =  other.m_sHostName;
		m_sBaseName  =  other.m_sBaseName;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Domain            (      Domain && other)
	{
		//## misses the encoding flag
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  = std::move (other.m_sHostName);
		m_sBaseName  = std::move (other.m_sBaseName);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Domain& operator= (const Domain &  other) noexcept
	{
		//## misses the encoding flag
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  =  other.m_sHostName;
		m_sBaseName  =  other.m_sBaseName;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Domain& operator= (      Domain && other) noexcept
	{
		//## misses the encoding flag
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  = std::move (other.m_sHostName);
		m_sBaseName  = std::move (other.m_sBaseName);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		bool bSome = true;
		if (m_sHostName.size ())
		{
			sTarget += m_sHostName;
			if (m_iPortNum)
			{
				sTarget += ':';
				sTarget += KString (std::to_string (m_iPortNum));
			}
		}
		return bSome;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		//## misses the encoding flag, and what about the offset?
		m_bError       = false;
		m_iPortNum     = 0;
		m_sHostName   = KString{};
		m_sBaseName   = KString{};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView   getHostName ()   const
	{
		//## why should this return encoded
		KString sEncoded;
		kUrlEncode (m_sHostName, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void           setHostName (const KStringView& svHostName)
	{
		parseHostName (svHostName, 0);  // data extraction
	}

	//---------------------------------------------------------------------
	/// Convert member and return it as uppercased string
	inline KString       getBaseDomain () const // No set function
	{
		//## why should this return encoded
		KString sEncoded;
		kUrlEncode (m_sBaseName, sEncoded);
		return KString (sEncoded).MakeUpper ();
	}

	//---------------------------------------------------------------------
	/// return member by value
	inline uint16_t             getPortNum ()    const
	{
		return m_iPortNum;
	}

	//---------------------------------------------------------------------
	/// set member by value
	inline void           setPortNum (const uint16_t iPortNum)
	{
		m_iPortNum = iPortNum;
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	bool             m_bError      {false};
	bool             m_bDecode     {false};
	uint16_t         m_iPortNum    {0};
	KString          m_sHostName   {};
	KString          m_sBaseName   {};
	size_t           m_iEndOffset  {0};

	//---------------------------------------------------------------------
	bool parseHostName (const KStringView& svSource, size_t iOffset);
};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const Domain& lhs, const Domain& rhs)
{
	return
		(lhs.getHostName () == rhs.getHostName ()) &&
		(lhs.getPortNum  () == rhs.getPortNum  ()) ;
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const Domain& lhs, const Domain& rhs)
{
	return
		(lhs.getHostName () != rhs.getHostName ()) ||
		(lhs.getPortNum ()  != rhs.getPortNum ()) ;
}


//## see my earlier comments
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "path" portion of w3 URL.
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                          -----   -----   --------
/// Path extracts and stores a KStringView of URL "path"
/// Path also encapsulates Query and Fragment
///
/// The aggregation of /path?query#fragment without individual path
/// is a design decision; not arbitrary.
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Path
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline Path        ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Path        (const KStringView& svSource, size_t iOffset=0)
	{
		//## same question about decode as a member variable for ths class as well
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse      (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Path              (const Path &  other)
	{
		//## encode is missing
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sPath      =  other.m_sPath  ;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Path              (      Path && other)
	{
		//## encode is missing
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sPath      = std::move (other.m_sPath);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Path  & operator= (const Path &  other) noexcept
	{
		//## encode is missing
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sPath      =  other.m_sPath  ;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Path  & operator= (      Path && other) noexcept
	{
		//## encode is missing
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sPath      = std::move (other.m_sPath);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		bool bResult = true;

		if (m_sPath.size ())
		{
			sTarget += m_sPath;
		}
		return bResult;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		//## use string.clear(), and what about m_iEndOffset
		m_sPath   = KString{};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView   getPath ()        const
	{
		//## why encoded return
		KString sEncoded;
		kUrlEncode (m_sPath, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setPath (const KStringView& svPath)
	{
		parse (svPath, 0);
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	bool        m_bError     {false};
	bool        m_bDecode    {false};
	KString     m_sPath      {};
	size_t      m_iEndOffset {0};
};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const Path& lhs, const Path& rhs)
{
	return lhs.getPath () == rhs.getPath ();
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const Path& lhs, const Path& rhs)
{
	return lhs.getPath () != rhs.getPath ();
}


//## see above
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "query" portion of w3 URL.
/// It is also responsible for parsing the query into a private property map.
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                                  -----
/// Query extracts and stores a KStringView of URL "query"
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Query
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// Simplify local declaration of property map
	typedef KProps<KString, KString, true, false> KProp_t;

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline Query ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Query   (const KStringView& svSource, size_t iOffset=0)
	{
		//## same as above
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse  (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Query             (const Query &  other)
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sQuery     =  other.m_sQuery;
		//## KProps has operator=() since some time
		//m_kpQuery    =  other.m_kpQuery;      // Missing kprops::operator=
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Query             (      Query && other)
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sQuery     =  std::move (other.m_sQuery);
		//## KProps has operator=(&&) since some time (use move!)
		//m_kpQuery    =  other.m_kpQuery;      // Missing kprops::operator=
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Query & operator= (const Query &  other) noexcept
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sQuery     =  other.m_sQuery;
		//m_kpQuery    =  other.m_kpQuery;      // Missing kprops::operator=
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Query & operator= (      Query && other) noexcept
	{
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sQuery     =  std::move (other.m_sQuery);
		//m_kpQuery    =  other.m_kpQuery;      // Missing kprops::operator=
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	bool serialize (KString& sTarget) const;

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		//# use string.clear(), also clear KProps, what about iEndOffset?
		m_bError = false;
		m_sQuery = KString{};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView  getQuery () const
	{
		//## why encoded, and should this not get the KProps serialization?
		KString sEncoded;
		kUrlEncode (m_sQuery, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setQuery (const KStringView& svQuery)
	{
		parse (svQuery, 0);
	}

	//---------------------------------------------------------------------
	/// return the property map
	inline const KProp_t& getProperties () const
	{
		return m_kpQuery;
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	//## why is error a member var?
	bool        m_bError        {false};
	//## what about decode?
	bool        m_bDecode       {false};
	KString     m_sQuery        {};
	//## why both, sQuery and kpQuery
	KProp_t     m_kpQuery       {};
	size_t      m_iEndOffset    {0};

	// make and use JIT translator for URL coded strings.
	// "+"     translates to " "
	// "%FF"   translates to "\xFF"
	// other   remains untranslated
	bool    decode ();

};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const Query& lhs, const Query& rhs)
{
	return lhs.getQuery () == rhs.getQuery ();
	// TODO compare kprops when operator== is ready
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const Query& lhs, const Query& rhs)
{
	return lhs.getQuery () != rhs.getQuery ();
	// TODO compare kprops when operator== is ready
}



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain "fragment" portion of w3 URL.
// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                                          --------
/// Fragment extracts and stores a KStringView of URL "fragment"
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Fragment
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline Fragment ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Fragment  (const KStringView& svSource, size_t iOffset=0)
	{
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Fragment             (const Fragment &  other)
	{
		//## misses decode
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sFragment  =  other.m_sFragment;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Fragment             (      Fragment && other)
	{
		//## misses decode
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sFragment  = std::move (other.m_sFragment);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Fragment & operator= (const Fragment &  other) noexcept
	{
		//## misses decode
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sFragment  =  other.m_sFragment;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Fragment & operator= (      Fragment && other) noexcept
	{
		//## misses decode
		m_bError     =  other.m_bError;
		m_iEndOffset =  other.m_iEndOffset;
		m_sFragment  =  std::move (other.m_sFragment);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		if (m_sFragment.size ())
		{
			//sTarget += '#';
			sTarget += m_sFragment;
		}
		return true;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		//## misses decode, iEndOffset
		m_bError = false;
		//## use string.clear()
		m_sFragment = KString{};
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getFragment () const
	{
		//## why encoded, why with leading #
		KString sEncoded;
		kUrlEncode (m_sFragment, sEncoded);
		return sEncoded;
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	bool            m_bError    {false};
	bool            m_bDecode   {false};
	KString         m_sFragment {};
	size_t          m_iEndOffset{0};

};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const Fragment& lhs, const Fragment& rhs)
{
	return lhs.getFragment () == rhs.getFragment ();
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const Fragment& lhs, const Fragment& rhs)
{
	return lhs.getFragment () != rhs.getFragment ();
}



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain aggregate TransPerfect URI portion of w3 URL.
/// It includes the "path", "query", and "fragment" portions.
/// The aggregation of /path?query#fragment without individual path
/// is a TransPerfect design decision; not arbitrary.
/// @brief RFC3986 3.3: (See RFC)
/// Implementation: All characters after domain from '/' to 1st of "?#\0".
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
///                                          -----   -----   --------
/// URI extracts and stores a KStringView of URL "path"
/// URI also encapsulates Query and Fragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URI : public Path, public Query, public Fragment
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline URI        ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline URI        (const KStringView& svSource, size_t iOffset=0)
	{
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse  (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URI                     (const URI &  other)
//##	Please use code like below instead of your convoluted type casting
//##	    : Path(other)
//##	    , Query(other)
//##	    , Fragment(other)
	{
		Path     & lPath     = *this; const Path     & rPath     = other;
		Query    & lQuery    = *this; const Query    & rQuery    = other;
		Fragment & lFragment = *this; const Fragment & rFragment = other;
		lPath     = rPath;
		lQuery    = rQuery;
		lFragment = rFragment;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URI                     (      URI && other)
//## please see above, but additionally use std::move() (the below code is incorrect)
	{
		Path     & lPath     = *this; const Path     & rPath     = other;
		Query    & lQuery    = *this; const Query    & rQuery    = other;
		Fragment & lFragment = *this; const Fragment & rFragment = other;
		lPath     = rPath;
		lQuery    = rQuery;
		lFragment = rFragment;
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline URI &         operator= (const URI &  other) noexcept
	{
//## please see above, except that here the pattern is
//##	Path::operator=(other);
//##	Query::operator=(other);
//##	Fragment::operator=(other);
		Path     & lPath     = *this; const Path     & rPath     = other;
		Query    & lQuery    = *this; const Query    & rQuery    = other;
		Fragment & lFragment = *this; const Fragment & rFragment = other;
		lPath     = rPath;
		lQuery    = rQuery;
		lFragment = rFragment;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline URI &         operator= (      URI && other) noexcept
	{
//## please see above, except that here the pattern is
//##	Path::operator=(std::move(other));
//##	Query::operator=(std::move(other));
//##	Fragment::operator=(std::move(other));
		Path     & lPath     = *this; const Path     & rPath     = other;
		Query    & lQuery    = *this; const Query    & rQuery    = other;
		Fragment & lFragment = *this; const Fragment & rFragment = other;
		lPath     = rPath;
		lQuery    = rQuery;
		lFragment = rFragment;
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		bool bResult = true;

		if (m_sPath.size ())
		{
			sTarget += m_sPath;
		}
		bResult = Path                  ::serialize (sTarget);
		bResult = bResult && Query      ::serialize (sTarget);
		bResult = bResult && Fragment   ::serialize (sTarget);
		return bResult;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		//## use m_sPath.clear();
		//## and what about the other member variables?
		m_sPath   = KString{};
		Path        ::clear ();
		Query       ::clear ();
		Fragment    ::clear ();
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

//------
private:
//------

	bool        m_bError     {false};
	bool        m_bDecode    {false};
	KString     m_sPath      {};
	size_t      m_iEndOffset {0};
};

//-----------------------------------------------------------------------------
//## make this a member function, and then use the pattern
//## return Path::operator==(other) && Query::operator==(other) && Fragment::operator==(other);
/// compares other instance with this, member-by-member
inline bool operator== (const URI& lhs, const URI& rhs)
{
	return Path(lhs) == Path(rhs);
	const Path     & lPath       = lhs, & rPath        = rhs;
	const Query    & lQuery      = lhs, & rQuery       = rhs;
	const Fragment & lFragment   = lhs, & rFragment    = rhs;
	return
		lPath       == rPath    &&
		lQuery      == rQuery   &&
		lFragment   == rFragment;
}

//-----------------------------------------------------------------------------
//## see before
/// compares other instance with this, member-by-member
inline bool operator!= (const URI& lhs, const URI& rhs)
{
	const Path     & lPath       = lhs, & rPath        = rhs;
	const Query    & lQuery      = lhs, & rQuery       = rhs;
	const Fragment & lFragment   = lhs, & rFragment    = rhs;
	return
		lPath       != rPath    ||
		lQuery      != rQuery   ||
		lFragment   != rFragment;
}



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class to parse and maintain aggregate w3 URL.
/// URL contains the "scheme", "user", "password", "host", "port" and URI.
/// This is a complete accounting for all fields of the w3 URL.
/// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
/// ------     ----  --------   ----  ----   -----   -----   --------
/// URL extracts and stores all elements of a URL.
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URL : public Protocol, public User, public Domain, public URI
{

//------
public:
//------

	//---------------------------------------------------------------------
	/// constructs empty instance.
	inline URL        ()
	{
	}

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline URL        (const KStringView& svSource, size_t iOffset=0)
	{
		m_bDecode = true;
		parse (svSource, iOffset);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	bool parse  (const KStringView& sSource, size_t iOffset=0);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URL                     (const URL &  other)
	{
		//## see before
		Protocol& lProtocol   = *this; const Protocol& rProtocol   = other;
		User    & lUser       = *this; const User    & rUser       = other;
		Domain  & lDomain     = *this; const Domain  & rDomain     = other;
		URI     & lURI        = *this; const URI     & rURI        = other;

		lProtocol   = rProtocol;
		lUser       = rUser;
		lDomain     = rDomain;
		lURI        = rURI;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URL                     (      URL && other)
	{
		//## see before
		Protocol& lProtocol   = *this; const Protocol& rProtocol   = other;
		User    & lUser       = *this; const User    & rUser       = other;
		Domain  & lDomain     = *this; const Domain  & rDomain     = other;
		URI     & lURI        = *this; const URI     & rURI        = other;

		lProtocol   = rProtocol;
		lUser       = rUser;
		lDomain     = rDomain;
		lURI        = rURI;
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline URL &         operator= (const URL &  other) noexcept
	{
		//## see before
		Protocol& lProtocol   = *this; const Protocol& rProtocol   = other;
		User    & lUser       = *this; const User    & rUser       = other;
		Domain  & lDomain     = *this; const Domain  & rDomain     = other;
		URI     & lURI        = *this; const URI     & rURI        = other;

		lProtocol   = rProtocol;
		lUser       = rUser;
		lDomain     = rDomain;
		lURI        = rURI;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline URL &         operator= (      URL && other) noexcept
	{
		//## see before
		Protocol& lProtocol   = *this; const Protocol& rProtocol   = other;
		User    & lUser       = *this; const User    & rUser       = other;
		Domain  & lDomain     = *this; const Domain  & rDomain     = other;
		URI     & lURI        = *this; const URI     & rURI        = other;

		lProtocol   = rProtocol;
		lUser       = rUser;
		lDomain     = rDomain;
		lURI        = rURI;
		return *this;
	}

	//---------------------------------------------------------------------
	/// Operator form of serialize
	//## why only for this class, and not for all preceeding as well?
	inline operator KString ()
	{
		const Protocol& lProtocol   = *this;
		const User&     lUser       = *this;
		const Domain&   lDomain     = *this;
		const URI&      lURI        = *this;

		KString result;

		lProtocol.serialize (result);
		lUser    .serialize (result);
		lDomain  .serialize (result);
		lURI     .serialize (result);
		return result;
	}

	//---------------------------------------------------------------------
	/// Serialize a URL
	//## why don't the others then have this operator? and what is the syntax to use this?
	const URL& operator>> (KString& target)
	{
		const URL      & source     = *this;
		const Protocol& lProtocol   = source;
		const User&     lUser       = source;
		const Domain&   lDomain     = source;
		const URI&      lURI        = source;

		lProtocol.serialize (target);
		lUser    .serialize (target);
		lDomain  .serialize (target);
		lURI     .serialize (target);

		return *this;
	}

	//---------------------------------------------------------------------
	//## see above
	/// parse a URL
	URL& operator<< (KString& source)
	{
		parse (source);
		return *this;
	}


	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool serialize (KString& sTarget) const
	{
		bool bResult = true;

		bResult &= Protocol ::serialize (sTarget);
		bResult &= User     ::serialize (sTarget);
		bResult &= Domain   ::serialize (sTarget);
		bResult &= URI      ::serialize (sTarget);

		return bResult;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		Protocol    ::clear ();
		User        ::clear ();
		Domain      ::clear ();
		URI         ::clear ();
		//## you forget to clear self
	}

	//---------------------------------------------------------------------
	/// return offset to end of parse in last parsed string
	inline size_t getEndOffset () const
	{
		return m_iEndOffset;
	}

	//---------------------------------------------------------------------
	/// identify that parse found mandatory elements of URL
	bool inline sURL () const
	{
		return (getProtocol ().size () > 0);
	}

	//---------------------------------------------------------------------
	/// identify that "mailto:" was parsed
	bool isEmailinline () const
	{
		return (Protocol::isEmail ());
	}

//------
private:
//------

	bool    m_bError    {false};
	bool    m_bDecode   {false};
	size_t  m_iEndOffset{0};
};

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator== (const URL& lhs, const URL& rhs)
{
	//## use the upcast pattern shown before
	const Protocol& lProtocol   = lhs, & rProtocol   = rhs;
	const User&     lUser       = lhs, & rUser       = rhs;
	const Domain&   lDomain     = lhs, & rDomain     = rhs;
	const URI&      lURI        = lhs, & rURI        = rhs;
	return
		lProtocol.getProtocolEnum () == rProtocol.getProtocolEnum () &&
		lProtocol                    == rProtocol &&
		lUser                        == rUser     &&
		lDomain                      == rDomain   &&
		lURI                         == rURI      ;
}

//-----------------------------------------------------------------------------
//## make this a member function
/// compares other instance with this, member-by-member
inline bool operator!= (const URL& lhs, const URL& rhs)
{
	//## use the upcast pattern shown before
	const Protocol& lProtocol   = lhs, & rProtocol   = rhs;
	const User&     lUser       = lhs, & rUser       = rhs;
	const Domain&   lDomain     = lhs, & rDomain     = rhs;
	const URI&      lURI        = lhs, & rURI        = rhs;
	return
		lProtocol.getProtocolEnum () != rProtocol.getProtocolEnum () ||
		lProtocol                    != rProtocol ||
		lUser                        != rUser     ||
		lDomain                      != rDomain   ||
		lURI                         != rURI      ;
}


} // of namespace KURL

} // of namespace dekaf2
