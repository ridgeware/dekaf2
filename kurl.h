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
#include "kurl.h"


namespace dekaf2
{

namespace KURL
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Protocol
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum eProto : uint16_t
	{
		// Explicit values to guarantee map to m_sCanonical.
		UNDEFINED = 0,
		FILE      = 1,
		FTP       = 2,
		HTTP      = 3,
		HTTPS     = 4,
		MAILTO    = 5,
		UNKNOWN   = 6
	};

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline Protocol () {}
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// Allow instance by enumeration including
	/// Protocol proto (Protocol::UNKNOWN, "opaquelocktoken");
	inline Protocol (eProto iProto, KStringView svProto = "")
		: m_sProto {svProto}
		, m_eProto {iProto}
	//-------------------------------------------------------------------------
	{}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Protocol (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Protocol (const Protocol& other)
	//-------------------------------------------------------------------------
		: m_sProto (other.m_sProto)
		, m_eProto (other.m_eProto)
	{
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Protocol (Protocol&& other)
		: m_sProto (std::move (other.m_sProto))
		, m_eProto (std::move (other.m_eProto))
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline Protocol& operator= (const Protocol& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sProto = other.m_sProto;
		m_eProto = other.m_eProto;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline Protocol& operator= (Protocol&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sProto = std::move (other.m_sProto);
		m_eProto = std::move (other.m_eProto);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Convert internal rep to KString
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize internal rep into arg KString
	const Protocol& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse arg into internal rep
	Protocol& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	void Clear ();
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KString getProtocol () const
	//-------------------------------------------------------------------------
	{
		KString sEncoded;
		Serialize (sEncoded);
		return sEncoded;
	}

	//-------------------------------------------------------------------------
	/// return the numeric scheme identifier
	inline eProto getProtocolEnum () const
	//-------------------------------------------------------------------------
	{
		return m_eProto;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setProtocol (KStringView svProto)
	//-------------------------------------------------------------------------
	{
		Parse (svProto);
	}

	//-------------------------------------------------------------------------
	/// identify that "mailto:" was parsed
	inline bool isEmail () const
	//-------------------------------------------------------------------------
	{
		return (m_eProto == MAILTO);
	}

	//-------------------------------------------------------------------------
	inline bool operator== (eProto iProto) const
	//-------------------------------------------------------------------------
	{
		return (iProto == m_eProto);
	}

	//-------------------------------------------------------------------------
	inline bool operator!= (eProto iProto) const
	//-------------------------------------------------------------------------
	{
		return (iProto != m_eProto);
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this
	inline bool operator== (const Protocol& rhs) const
	//-------------------------------------------------------------------------
	{
		bool bEqual = (m_eProto == rhs.m_eProto);
		if (bEqual && m_eProto == UNKNOWN)
		{
			bEqual = (m_sProto == rhs.m_sProto);
		}
		return bEqual;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this
	//-------------------------------------------------------------------------
	inline bool operator!= (const Protocol& rhs) const
	{
		bool bEqual = (m_eProto == rhs.m_eProto);
		if (bEqual && m_eProto == UNKNOWN)
		{
			bEqual = (m_sProto == rhs.m_sProto);
		}
		return !bEqual;
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		size_t iRet{0};
		if (m_eProto >= UNKNOWN)
		{
			iRet = m_sProto.size () + 3; // 3 for the "://"
		}
		else if (m_eProto)
		{
			iRet = m_sKnown[m_eProto].size ();
		}
		return iRet;
	}

//------
private:
//------

	KString m_sProto {};
	eProto  m_eProto {UNDEFINED};
	static KString m_sKnown [UNKNOWN+1];

};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class User
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
	inline User (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline User (const User& other)
		: m_sUser (other.m_sUser)
		, m_sPass (other.m_sPass)
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline User (User&& other)
		: m_sUser (std::move (other.m_sUser))
		, m_sPass (std::move (other.m_sPass))
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline User& operator= (const User& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sUser = other.m_sUser;
		m_sPass = other.m_sPass;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline User& operator= (User&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sUser = std::move (other.m_sUser);
		m_sPass = std::move (other.m_sPass);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize and return as KString
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const User& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	User& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		m_sUser.clear ();
		m_sPass.clear ();
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
	inline void setUser (KStringView svUser)
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
	inline void setPass (KStringView svPass)
	//-------------------------------------------------------------------------
	{
		m_sPass = svPass;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const User& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			(getUser () == rhs.getUser ()) &&
			(getPass () == rhs.getPass ()) ;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const User& rhs) const
	//-------------------------------------------------------------------------
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
			m_sUser.size()
			? (
				m_sUser.size () +
				m_sPass.size () +
				(m_sPass.size() != 0) + // account for ':' if any
				1                       // account for '@'
			)
			: 0;
	}

//------
private:
//------

	KString m_sUser {};
	KString m_sPass {};
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Domain
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	//------
	public:
	//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline Domain ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Domain (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Domain (const Domain& other)
	//-------------------------------------------------------------------------
	{
		m_iPortNum  = other.m_iPortNum;
		m_sHostName = other.m_sHostName;
		m_sBaseName = other.m_sBaseName;
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Domain (Domain&& other)
	//-------------------------------------------------------------------------
	{
		m_iPortNum  = other.m_iPortNum;
		m_sHostName = std::move (other.m_sHostName);
		m_sBaseName = std::move (other.m_sBaseName);
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline Domain& operator= (const Domain& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_iPortNum  = other.m_iPortNum;
		m_sHostName = other.m_sHostName;
		m_sBaseName = other.m_sBaseName;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline Domain& operator= (Domain&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_iPortNum  = other.m_iPortNum;
		m_sHostName = std::move (other.m_sHostName);
		m_sBaseName = std::move (other.m_sBaseName);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize in KString style
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize in stream style
	const Domain& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse in stream style
	Domain& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		m_iPortNum = 0;
		m_sHostName.clear ();
		m_sBaseName.clear ();
	}

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getHostName () const
	//-------------------------------------------------------------------------
	{
		return m_sHostName;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setHostName (KStringView svHostName)
	//-------------------------------------------------------------------------
	{
		ParseHostName (svHostName);  // data extraction
	}

	//-------------------------------------------------------------------------
	/// Convert member and return it as uppercased string
	inline KString getBaseDomain () const // No set function because derived
	//-------------------------------------------------------------------------
	{
		return m_sBaseName.ToUpper ();
	}

	//-------------------------------------------------------------------------
	/// return member by value
	inline uint16_t getPortNum () const
	//-------------------------------------------------------------------------
	{
		return m_iPortNum;
	}

	//-------------------------------------------------------------------------
	/// set member by value
	inline void setPortNum (uint16_t iPortNum)
	//-------------------------------------------------------------------------
	{
		m_iPortNum = iPortNum;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Domain& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			(getHostName () == rhs.getHostName ()) &&
			(getPortNum  () == rhs.getPortNum  ()) ;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Domain& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			(getHostName () != rhs.getHostName ()) ||
			(getPortNum ()  != rhs.getPortNum ()) ;
	}

//------
private:
//------

	uint16_t m_iPortNum  {0};
	KString  m_sHostName {};
	KString  m_sBaseName {};

	//-------------------------------------------------------------------------
	KStringView ParseHostName (KStringView svSource);
	//-------------------------------------------------------------------------
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Path
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline Path ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Path (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Path (const Path& other)
	//-------------------------------------------------------------------------
	{
		m_sPath = other.m_sPath ;
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Path (Path&& other)
	//-------------------------------------------------------------------------
	{
		m_sPath = std::move (other.m_sPath);
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline Path& operator= (const Path& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sPath = other.m_sPath ;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline Path& operator= (Path&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sPath = std::move (other.m_sPath);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize KString style
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const Path& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	Path& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		m_sPath.clear ();
	}

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getPath () const
	//-------------------------------------------------------------------------
	{
		return m_sPath;
	}

	//-------------------------------------------------------------------------
	/// modify member by parsing argument
	inline void setPath (KStringView svPath)
	//-------------------------------------------------------------------------
	{
		Parse (svPath);
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Path& rhs) const
	//-------------------------------------------------------------------------
	{
		return getPath () == rhs.getPath ();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Path& rhs) const
	//-------------------------------------------------------------------------
	{
		return getPath () != rhs.getPath ();
	}

//------
private:
//------

	KString m_sPath {};
};


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Query
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// Simplify local declaration of property map
	typedef KProps<KString, KString, true, false> KProp_t;

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline Query ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Query (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Query (const Query& other)
	//-------------------------------------------------------------------------
	{
		m_kpQuery = other.m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Query (Query&& other)
	//-------------------------------------------------------------------------
	{
		m_kpQuery = other.m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline Query& operator= (const Query& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_kpQuery = other.m_kpQuery;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline Query& operator= (Query&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_kpQuery = other.m_kpQuery;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize into KString
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const Query& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	Query& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		m_kpQuery.clear ();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Query& rhs) const
	//-------------------------------------------------------------------------
	{
		return m_kpQuery == rhs.m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Query& rhs) const
	//-------------------------------------------------------------------------
	{
		return m_kpQuery != rhs.m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// Size of stored parse results.
	inline size_t size() const
	//-------------------------------------------------------------------------
	{
		//?? Should this calculate the size of the encoded query string?
		//?? Or should it return the number of key:value pairs?
		return m_kpQuery.size();
	}

	//-------------------------------------------------------------------------
	/// Returns entire KProp member const style
	inline const KProp_t& GetQuery() const
	//-------------------------------------------------------------------------
	{
		return m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// Returns entire KProp member non-const style
	inline KProp_t& GetQuery()
	//-------------------------------------------------------------------------
	{
		return m_kpQuery;
	}

	//-------------------------------------------------------------------------
	/// Fetch value of key:value pair
	inline KStringView operator[](KStringView svKey) const
	//-------------------------------------------------------------------------
	{
		return m_kpQuery[svKey];
	}

//------
private:
//------

	KProp_t m_kpQuery {};

	//-------------------------------------------------------------------------
	// make and use JIT translator for URL coded strings.
	// "+"   translates to " "
	// "%FF" translates to "\xFF"
	// other remains untranslated
	bool decode (KStringView);
	//-------------------------------------------------------------------------

};



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Fragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline Fragment ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline Fragment (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		svSource = Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline Fragment (const Fragment& other)
	//-------------------------------------------------------------------------
	{
		m_sFragment = other.m_sFragment;
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline Fragment (Fragment&& other)
	//-------------------------------------------------------------------------
	{
		m_sFragment = std::move (other.m_sFragment);
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline Fragment& operator= (const Fragment& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sFragment = other.m_sFragment;
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline Fragment& operator= (Fragment&& other) noexcept
	//-------------------------------------------------------------------------
	{
		m_sFragment = std::move (other.m_sFragment);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize KString style
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const Fragment& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	Fragment& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		m_sFragment.clear ();
	}

	//-------------------------------------------------------------------------
	/// return a view of the member
	inline KStringView getFragment () const
	//-------------------------------------------------------------------------
	{
		return m_sFragment;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const Fragment& rhs) const
	//-------------------------------------------------------------------------
	{
		return getFragment () == rhs.getFragment ();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const Fragment& rhs) const
	//-------------------------------------------------------------------------
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

	KString m_sFragment {};

};



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URI : public Path, public Query, public Fragment
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline URI ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline URI (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		svSource = Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URI (const URI& other)
		: Path     (other)
		, Query    (other)
		, Fragment (other)
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URI (URI&& other)
		: Path     (std::move (other))
		, Query    (std::move (other))
		, Fragment (std::move (other))
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline URI& operator= (const URI& other) noexcept
	//-------------------------------------------------------------------------
	{
		Path    ::operator= (other);
		Query   ::operator= (other);
		Fragment::operator= (other);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline URI& operator= (URI&& other) noexcept
	//-------------------------------------------------------------------------
	{
		Path    ::operator= (std::move (other));
		Query   ::operator= (std::move (other));
		Fragment::operator= (std::move (other));
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize into KString
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const Fragment& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Parse stream style
	Fragment& operator<< (KStringView sSource)
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
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		Path    ::Clear ();
		Query   ::Clear ();
		Fragment::Clear ();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const URI& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			Path    ::operator== (rhs) &&
			Query   ::operator== (rhs) &&
			Fragment::operator== (rhs) ;
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const URI& rhs) const
	//-------------------------------------------------------------------------
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



//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class URL : public Protocol, public User, public Domain, public URI
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-------------------------------------------------------------------------
	/// constructs empty instance.
	inline URL ()
	//-------------------------------------------------------------------------
	{
	}

	//-------------------------------------------------------------------------
	/// constructs instance and parses source into members
	inline URL (KStringView svSource)
	//-------------------------------------------------------------------------
	{
		svSource = Parse (svSource);
	}

	//-------------------------------------------------------------------------
	/// parses source into members of instance
	KStringView Parse (KStringView sSource);
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// construct new instance and copy members from old instance
	inline URL (const URL& other)
	//-------------------------------------------------------------------------
		: Protocol (other)
		, User   (other)
		, Domain (other)
		, URI    (other)
	{
	}

	//-------------------------------------------------------------------------
	/// construct new instance and move members from old instance
	inline URL (URL&& other)
	//-------------------------------------------------------------------------
		: Protocol (std::move (other))
		, User     (std::move (other))
		, Domain   (std::move (other))
		, URI      (std::move (other))
	{
	}

	//-------------------------------------------------------------------------
	/// copies members from other instance into this
	inline URL& operator= (const URL& other) noexcept
	//-------------------------------------------------------------------------
	{
		Path  ::operator= (other);
		User  ::operator= (other);
		Domain::operator= (other);
		URI   ::operator= (other);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// moves members from other instance into this
	inline URL& operator= (URL&& other) noexcept
	//-------------------------------------------------------------------------
	{
		Path  ::operator= (std::move (other));
		User  ::operator= (std::move (other));
		Domain::operator= (std::move (other));
		URI   ::operator= (std::move (other));
		return *this;
	}

	//-------------------------------------------------------------------------
	/// Serialize into KString
	inline operator KString () const
	//-------------------------------------------------------------------------
	{
		KString sResult;
		Serialize (sResult);
		return sResult;
	}

	//-------------------------------------------------------------------------
	/// Serialize stream style
	const URL& operator>> (KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		const URL& source = *this;
		Serialize (sTarget);
		return *this;
	}

	//-------------------------------------------------------------------------
	/// parse stream style
	URL& operator<< (KStringView source)
	//-------------------------------------------------------------------------
	{
		Parse (source);
		return *this;
	}


	//-------------------------------------------------------------------------
	/// generate content into string from members
	bool Serialize (KString& sTarget) const;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// restore instance to unpopulated state
	inline void Clear ()
	//-------------------------------------------------------------------------
	{
		Protocol::Clear ();
		User    ::Clear ();
		Domain  ::Clear ();
		URI     ::Clear ();
	}

	//-------------------------------------------------------------------------
	/// identify that parse found mandatory elements of URL
	bool inline sURL () const
	//-------------------------------------------------------------------------
	{
		return (getProtocol ().size () > 0);
	}

	//-------------------------------------------------------------------------
	size_t size() const
	//-------------------------------------------------------------------------
	{
		return Protocol::size();
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator== (const URL& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			Protocol::operator== (rhs) &&
			User    ::operator== (rhs) &&
			Domain  ::operator== (rhs) &&
			URI     ::operator== (rhs);
	}

	//-------------------------------------------------------------------------
	/// compares other instance with this, member-by-member
	inline bool operator!= (const URL& rhs) const
	//-------------------------------------------------------------------------
	{
		return
			Protocol::operator!= (rhs) ||
			User    ::operator!= (rhs) ||
			Domain  ::operator!= (rhs) ||
			URI     ::operator!= (rhs);
	}

//------
private:
//------

};


} // of namespace KURL

} // of namespace dekaf2