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
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
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

/// @defgroup KURL
/// KURL implements all classes for parsing, maintaining, and serializing URLs.
/// Example URL:
///     "https://jlettvin@github.com:8080/experiment/UTF8?page=home#title";
///           ^^^        ^          ^    ^               ^         ^
/// Characters identified by ^ above are expected to NOT be URL encoded.
/// @{
namespace KURL
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Protocol
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum eProto : uint16_t
	{
		UNDEFINED,
		FILE,
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

	//## add a constructor that takes a eProto. That allows you to do a comparison
	//## like if (Protocol == KURL::Protocol::HTTP) {}
	//## Also you then do not need to store the m_sProto string for the
	//## known protocols.. that saves a lot of copies and allocations.
	//## Only store the m_sProto string for the UNKNOWN case...
	//## (BTW, we discussed that weeks ago)

	//---------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Protocol    (KStringView svSource)
	//---------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);
	//---------------------------------------------------------------------
	//## please change to Parse() - we have the convention to have PascalStyleFunctionNames()
	//## please do this with all function names except std:: ones like size(), end(), empty() etc.

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	//## please use a consistent style for spaces around & and &&. Best use
	//## what the rest of us does: glue the ref to the var, and have one space after it.
	//## please correct that throughout the file.
	inline Protocol    (const Protocol &  other)
	//---------------------------------------------------------------------
		: m_bMailto     (other.m_bMailto)
		, m_sProto      (other.m_sProto)
		, m_eProto      (other.m_eProto)
	{
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	//## same like above, and why those random spaces.. do not try to
	//## balance whitespaces if you do not do it consistently (and I am
	//## actually of the opinion that it is much harder to read when
	//## there's so much white space)
	inline Protocol    (      Protocol && other)
		: m_bMailto     (std::move (other.m_bMailto))
		, m_sProto      (std::move (other.m_sProto))
		, m_eProto      (std::move (other.m_eProto))
	//---------------------------------------------------------------------
	{
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Protocol&   operator= (const Protocol &  other) noexcept
	//## there's a //----- comment missing here.
	{
		m_bMailto   = other.m_bMailto;
		m_sProto    = other.m_sProto;
		m_eProto    = other.m_eProto;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Protocol&   operator= (      Protocol && other) noexcept
	//## there's a //----- comment missing here.
	{
		m_bMailto   = std::move (other.m_bMailto);
		m_sProto    = std::move (other.m_sProto);
		m_eProto    = std::move (other.m_eProto);
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Protocol& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	//## shouldn't the parameter here better be a KStringView ? Or at least a const KString&
	Protocol& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
	//---------------------------------------------------------------------
	//## there's a //----- comment missing here.
	//## I would actually move this one into the .cpp
	{
		/*
		switch (m_eProto)
		{
			case FILE:
				sTarget += "file" + (m_eProto == UNDEFINED) ? "" : "://";
				break;
			case FTP:
				sTarget += "ftp" + (m_eProto == UNDEFINED) ? "" : "://";
				break;

		FILE,
		FTP,
		HTTP,
		HTTPS,
		MAILTO,
		UNKNOWN
		}
		*/
		if (m_sProto.size () != 0)
		{
			//KString sEncoded;
			//kUrlEncode (m_sProto, sEncoded);
			sTarget += m_sProto; //sEncoded;
			sTarget += m_sPost;
			//## as the basis of the serialization use m_eProto. Only if it
			//## is UNDEFINED use m_sProto.
			return true;
		}
		return false;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	//---------------------------------------------------------------------
	//## I would actually move this one into the .cpp
	{
		m_bMailto       = false;
		//## please do not do those artistics with spaces. it confuses the reader.
		//## I initially thought you would call clear() recursively..
		m_sProto        .clear ();
		m_eProto        = UNDEFINED;
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KString getProtocol () const
	{
		KString sEncoded;
		//## call Serialize() instead
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
		Parse (svProto);
	}

	//---------------------------------------------------------------------
	/// identify that "mailto:" was parsed
	inline bool isEmail () const
	{
		return m_bMailto;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member //## comment is incorrect
	inline bool operator== (const Protocol& rhs) const
	{
		//## compare emums first, and only if both equal and == UNKNOWN
		//## compare strings
		return getProtocol () == rhs.getProtocol ();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member //## comment is incorrect
	inline bool operator!= (const Protocol& rhs) const
	{
		//## see above
		return getProtocol () != rhs.getProtocol ();
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		return m_sProto.size () + m_sPost.size ();
	}

//------
private:
//------

	bool            m_bMailto   {false};
	KString         m_sProto    {};
	KString         m_sPost     {};
	eProto          m_eProto    {UNDEFINED};
	//## why do you need a member variable and a special method for
	//## mailto? Why not using enum MAILTO ?

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class User
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline User ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline User    (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline User              (const User &  other)
		: m_sUser       (other.m_sUser)
		, m_sPass       (other.m_sPass)
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline User              (      User && other)
		: m_sUser       (std::move (other.m_sUser))
		, m_sPass       (std::move (other.m_sPass))
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline User&   operator= (const User &  other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sUser     = other.m_sUser;
		m_sPass     = other.m_sPass;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline User&   operator= (      User && other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sUser     = std::move (other.m_sUser);
		m_sPass     = std::move (other.m_sPass);
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const User& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	//## make parameter a KStringView
	User& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// generate content into string from members
	///## move the body into the cpp
	inline bool Serialize (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		// TODO Should username/password be url encoded?
		//## well, I would say so. How would you otherwise represent accented chars
		//## (and you urldecode at parsing actually)
		if (m_sUser.size ())
		{
			sTarget += m_sUser;

			if (m_sPass.size ())
			{
				sTarget += ':';
				sTarget += m_sPass;
			}
			sTarget += m_sPost;
		}
		return true;
	}

	//-------------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	//-------------------------------------------------------------------------
	{
		//## don't separate the method operator from its type.
		m_sUser         .clear ();
		m_sPass         .clear ();
	}

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getUser () const
	//-------------------------------------------------------------------------
	{
		return m_sUser;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setUser (const KStringView& svUser)
	//-------------------------------------------------------------------------
	{
		m_sUser = svUser;
	}

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getPass () const
	//-------------------------------------------------------------------------
	{
		return m_sPass;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setPass (const KStringView& svPass)
	//-------------------------------------------------------------------------
	{
		m_sPass = svPass;
	}

	/// compares other instance with this, member-by-member
	inline bool operator== (const User& rhs) const
	{
		return
			(getUser () == rhs.getUser ()) &&
			(getPass () == rhs.getPass ()) ;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const User& rhs) const
	{
		return
			(getUser () != rhs.getUser ()) ||
			(getPass () != rhs.getPass ()) ;
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		return
			m_sUser.size () +
			m_sPass.size () +
			(m_sPass.size() != 0) + // account for ':' if any
			m_sPost.size () ;       // account for '@' if any
	}

//------
private:
//------

	KString         m_sUser     {};
	KString         m_sPass     {};
	KString         m_sPost     {};
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Domain
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline Domain  (KStringView svSource)
	{
		Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Domain            (const Domain &  other)
	{
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  =  other.m_sHostName;
		m_sBaseName  =  other.m_sBaseName;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Domain            (      Domain && other)
	{
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  = std::move (other.m_sHostName);
		m_sBaseName  = std::move (other.m_sBaseName);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Domain& operator= (const Domain &  other) noexcept
	{
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  =  other.m_sHostName;
		m_sBaseName  =  other.m_sBaseName;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Domain& operator= (      Domain && other) noexcept
	{
		m_iPortNum   =  other.m_iPortNum;
		m_sHostName  = std::move (other.m_sHostName);
		m_sBaseName  = std::move (other.m_sBaseName);
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Domain& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	//## KStringView
	Domain& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
	{
		bool bSome = true;
		if (m_sHostName.size ())
		{
			sTarget += m_sHostName;
			if (m_iPortNum)
			{
				sTarget += ':';
				//## no need to construct another string. to_string returns one, and
				//## KString::operator+= accepts std:strings.
				sTarget += KString (std::to_string (m_iPortNum));
			}
		}
		return bSome;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		m_iPortNum     = 0;
		m_sHostName.clear ();
		m_sBaseName.clear ();
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView   getHostName ()   const
	{
		return m_sHostName;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void           setHostName (const KStringView& svHostName)
	{
		ParseHostName (svHostName);  // data extraction
	}

	//---------------------------------------------------------------------
	/// Convert member and return it as uppercased string
	inline KString       getBaseDomain () const // No set function
	{
		//## use return m_sBaseName.ToUpper();
		return KString (m_sBaseName).MakeUpper ();
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

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Domain& rhs) const
	{
		return
			(getHostName () == rhs.getHostName ()) &&
			(getPortNum  () == rhs.getPortNum  ()) ;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Domain& rhs) const
	{
		return
			(getHostName () != rhs.getHostName ()) ||
			(getPortNum ()  != rhs.getPortNum ()) ;
	}

//------
private:
//------

	uint16_t         m_iPortNum    {0};
	KString          m_sHostName   {};
	KString          m_sBaseName   {};

	//---------------------------------------------------------------------
	KStringView ParseHostName (KStringView svSource);
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Path
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline Path        (KStringView svSource)
	{
		Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse      (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Path              (const Path &  other)
	{
		m_sPath      =  other.m_sPath  ;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Path              (      Path && other)
	{
		m_sPath      = std::move (other.m_sPath);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Path  & operator= (const Path &  other) noexcept
	{
		m_sPath      =  other.m_sPath  ;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Path  & operator= (      Path && other) noexcept
	{
		m_sPath      = std::move (other.m_sPath);
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Path& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	//## KStringView
	Path& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
	{
		//## no need to have a variable for the return as it is always true..
		bool bResult = true;

		if (m_sPath.size ())
		{
			//## but you have to urlencode the path..
			sTarget += m_sPath;
		}
		return bResult;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		m_sPath.clear ();
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView   getPath ()        const
	{
		return m_sPath;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setPath (KStringView svPath)
	{
		Parse (svPath);
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Path& rhs) const
	{
		return getPath () == rhs.getPath ();
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Path& rhs) const
	{
		return getPath () != rhs.getPath ();
	}

//------
private:
//------

	KString     m_sPath      {};
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Query
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline Query   (KStringView svSource)
	{
		Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Query             (const Query &  other)
	{
		m_kpQuery    =  other.m_kpQuery;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Query             (      Query && other)
	{
		m_kpQuery    =  other.m_kpQuery;
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Query & operator= (const Query &  other) noexcept
	{
		m_kpQuery    =  other.m_kpQuery;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Query & operator= (      Query && other) noexcept
	{
		m_kpQuery    =  other.m_kpQuery;
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Query& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	Query& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	bool Serialize (KString& sTarget) const;

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		m_kpQuery.clear ();
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	//## replace this as explained above
	inline KString  getQuery () const
	{
		KString sSerialized;
		Serialize (sSerialized);
		return sSerialized;
	}

	//---------------------------------------------------------------------
	/// modify member by parsing argument
	//## remove this - user can call Parse().
	inline void setQuery (const KStringView& svQuery)
	{
		Parse (svQuery);
	}

	//---------------------------------------------------------------------
	/// return the property map
	//## remove this
	inline const KProp_t& getProperties () const
	{
		return m_kpQuery;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Query& rhs) const
	{
		return m_kpQuery == rhs.m_kpQuery;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Query& rhs) const
	{
		return m_kpQuery != rhs.m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		return m_kpQuery.size();
	}

//------
private:
//------

	KProp_t     m_kpQuery       {};

	// make and use JIT translator for URL coded strings.
	// "+"     translates to " "
	// "%FF"   translates to "\xFF"
	// other   remains untranslated
	bool    decode (KStringView);

};



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Fragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline Fragment  (KStringView svSource)
	{
		svSource = Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Fragment             (const Fragment &  other)
	{
		m_sFragment  =  other.m_sFragment;
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Fragment             (      Fragment && other)
	{
		m_sFragment  = std::move (other.m_sFragment);
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline Fragment & operator= (const Fragment &  other) noexcept
	{
		m_sFragment  =  other.m_sFragment;
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline Fragment & operator= (      Fragment && other) noexcept
	{
		m_sFragment  =  std::move (other.m_sFragment);
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Fragment& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	Fragment& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
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
		m_sFragment.clear ();
	}

	//---------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getFragment () const
	{
		//## I still think it is incorrect to return the fragment including the leading #
		return m_sFragment;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Fragment& rhs) const
	{
		return getFragment () == rhs.getFragment ();
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Fragment& rhs) const
	{
		return getFragment () != rhs.getFragment ();
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		return m_sFragment.size();
	}

//------
private:
//------

	KString         m_sFragment {};

};



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URI : public Path, public Query, public Fragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline URI        (KStringView svSource)
	{
		svSource = Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URI                     (const URI &  other)
		: Path      (other)
		, Query     (other)
		, Fragment  (other)
	{
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URI                     (      URI && other)
		: Path      (std::move (other))
		, Query     (std::move (other))
		, Fragment  (std::move (other))
	{
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline URI &         operator= (const URI &  other) noexcept
	{
		Path    ::operator= (other);
		Query   ::operator= (other);
		Fragment::operator= (other);
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline URI &         operator= (      URI && other) noexcept
	{
		Path    ::operator= (std::move (other));
		Query   ::operator= (std::move (other));
		Fragment::operator= (std::move (other));
		return *this;
	}

	//---------------------------------------------------------------------
	inline operator KString () const
	//---------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	const Fragment& operator>> (KString& sTarget)
	//---------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	Fragment& operator<< (KString& sSource)
	//---------------------------------------------------------------------
	{
		Parse (sSource);
		return *this;
	}

	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
	{
		bool bResult = true;

		bResult = Path                  ::Serialize (sTarget);
		bResult = bResult && Query      ::Serialize (sTarget);
		bResult = bResult && Fragment   ::Serialize (sTarget);
		return bResult;
	}

	//---------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void clear ()
	{
		Path        ::clear ();
		Query       ::clear ();
		Fragment    ::clear ();
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const URI& rhs) const
	{
		return
			Path    ::operator== (rhs) &&
			Query   ::operator== (rhs) &&
			Fragment::operator== (rhs) ;
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const URI& rhs) const
	{
		return
			Path    ::operator!= (rhs) ||
			Query   ::operator!= (rhs) ||
			Fragment::operator!= (rhs) ;
	}

//------
private:
//------

};



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URL : public Protocol, public User, public Domain, public URI
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline URL        (KStringView svSource)
	{
		svSource = Parse (svSource);
	}

	//---------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse  (KStringView sSource);

	//---------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URL                     (const URL &  other)
		: Protocol  (other)
		, User      (other)
		, Domain    (other)
		, URI       (other)
	{
	}

	//---------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URL                     (      URL && other)
		: Protocol  (std::move (other))
		, User      (std::move (other))
		, Domain    (std::move (other))
		, URI       (std::move (other))
	{
	}

	//---------------------------------------------------------------------
	/// copies members from other instance into this
	inline URL &         operator= (const URL &  other) noexcept
	{
		Path    ::operator= (other);
		User    ::operator= (other);
		Domain  ::operator= (other);
		URI     ::operator= (other);
		return *this;
	}

	//---------------------------------------------------------------------
	/// moves members from other instance into this
	inline URL &         operator= (      URL && other) noexcept
	{
		Path    ::operator= (std::move (other));
		User    ::operator= (std::move (other));
		Domain  ::operator= (std::move (other));
		URI     ::operator= (std::move (other));
		return *this;
	}

	//---------------------------------------------------------------------
	/// Operator form of serialize
	inline operator KString ()
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//---------------------------------------------------------------------
	/// Serialize a URL
	//## why don't the others then have this operator? and what is the syntax to use this?
	const URL& operator>> (KString& sTarget)
	{
		const URL      & source = *this;
		Serialize (sTarget);
		return *this;
	}

	//---------------------------------------------------------------------
	/// parse a URL
	URL& operator<< (KString& source)
	{
		Parse (source);
		return *this;
	}


	//---------------------------------------------------------------------
	/// generate content into string from members
	inline bool Serialize (KString& sTarget) const
	{
		bool bResult = true;

		bResult &= Protocol ::Serialize (sTarget);
		bResult &= User     ::Serialize (sTarget);
		bResult &= Domain   ::Serialize (sTarget);
		bResult &= URI      ::Serialize (sTarget);

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
		m_iEndOffset = 0;
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
	size_t size() const
	//---------------------------------------------------------------------
	{
		return m_iEndOffset + Protocol::size();
	}

	//---------------------------------------------------------------------
	/// identify that "mailto:" was parsed
	//## remove
	bool isEmailinline () const
	{
		return (Protocol::isEmail ());
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const URL& rhs) const
	{
		return
			//## move that functionality into Protocol's operator==()..
			Protocol    ::getProtocolEnum () == rhs.getProtocolEnum () &&
			Protocol    ::operator== (rhs) &&
			User        ::operator== (rhs) &&
			Domain      ::operator== (rhs) &&
			URI         ::operator== (rhs);
	}

	//-----------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const URL& rhs) const
	{
		return
			//## move that functionality into Protocol's operator==()..
			Protocol    ::getProtocolEnum () != rhs.getProtocolEnum () ||
			Protocol    ::operator!= (rhs) ||
			User        ::operator!= (rhs) ||
			Domain      ::operator!= (rhs) ||
			URI         ::operator!= (rhs);
	}

//------
private:
//------

	size_t  m_iEndOffset{0};
};


} // of namespace KURL
/** @} */ // End of group KURL

} // of namespace dekaf2
