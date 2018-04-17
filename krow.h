/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

#include "kprops.h"
#include "bits/ktemplate.h"

namespace dekaf2 {

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCommonSQLBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum SQLTYPE
	{
		// the DBT constant is used to govern certain SQL statments:
		DBT_NONE              = 0,
		DBT_MYSQL             = 100,        // assume we're connecting to MySQL
		DBT_ORACLE6           = 206,        // assume we're connecting to an Oracle6 rdbms
		DBT_ORACLE7           = 207,        // assume we're connecting to an Oracle7 rdbms
		DBT_ORACLE8           = 208,        // assume we're connecting to an Oracle8 rdbms
		DBT_ORACLE            = 200,        // use the latest assumptions about Oracle
		DBT_SQLSERVER         = 300,
		DBT_SYBASE            = 400,
		DBT_INFORMIX          = 500
	};

	static void     SetDebugLevel (int32_t iNewLevel) { m_iDebugLevel=iNewLevel; }
	static int32_t  GetDebugLevel () { return m_iDebugLevel; }

//----------
private:
//----------

	static int32_t  m_iDebugLevel;

}; // KCommonSQLBase

}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCOL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KCOL () = default;

	KCOL (KString&& _sValue, uint64_t _iFlags=0, uint32_t _iMaxLen=0)
		: sValue(std::move(_sValue))
		, iFlags(_iFlags)
		, iMaxLen(_iMaxLen)
	{
	}

	KCOL (KStringView _sValue, uint64_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	KCOL (const std::string& _sValue, uint64_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	KCOL (const char* _sValue, uint64_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	KCOL (KCOL&&)                  = default;
	KCOL (const KCOL&)             = default;
	KCOL& operator = (KCOL&&)      = default;
	KCOL& operator = (const KCOL&) = default;

	KString  sValue;  // aka "second"
	uint64_t iFlags{0};
	uint32_t iMaxLen{0};

}; // KCOL

typedef KProps <KString, KCOL,    /*order-matters=*/true, /*unique-keys*/true> KCOLS;

//-----------------------------------------------------------------------------
class KROW : public KCOLS, public detail::KCommonSQLBase
//-----------------------------------------------------------------------------
{
//----------
public:
//----------
	KROW (const char* szTablename)
	{
		m_sTablename =  szTablename;
	}

	KROW (KStringView sTablename)
	{
		m_sTablename =  sTablename;
	}

	KROW (KROW&&)                  = default;
	KROW (const KROW&)             = default;
	KROW& operator = (KROW&&)      = default;
	KROW& operator = (const KROW&) = default;

	template<class... Args>
	KROW(Args&&... args)
	    : KCOLS(std::forward<Args>(args)...)
	{
	}

	void SetTablename (KStringView sTablename)
	{
		m_sTablename =  sTablename;
	}

	template<typename COLTYPE, typename std::enable_if<detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, uint64_t iFlags=0, uint32_t iMaxLen=0)
	{
		KCOL col (Value, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	template<typename COLTYPE, typename std::enable_if<!detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, uint64_t iFlags=0, uint32_t iMaxLen=0)
	{
		KCOL col (kFormat("{}", Value), iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}


	bool SetValue (KStringView sColName, KStringView sValue)
	{
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::end())
		{
			return (KCOLS::Add (sColName, KCOL(sValue)) != KCOLS::end());
		}
		else
		{
			it->second.sValue = sValue;
			return (true);
		}
	}

	bool SetValue (KStringView sColName, int64_t iValue)
	{
		KString sValue; sValue.Format ("{}", iValue);
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::end())
		{
			return (KCOLS::Add (sColName, KCOL(std::move(sValue))) != KCOLS::end());
		}
		else
		{
			it->second.sValue = std::move(sValue);
			return (true);
		}
	}

	bool SetFlags (KStringView sColName, uint64_t iFlags)
	{
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::end())
		{
			return (false);
		}
		else
		{
			it->second.iFlags = iFlags;
			return (true);
		}
	}

	/// Returns the Nth column's name (note: column index starts at 0).
	KStringView GetName (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).first);
	}

	/// Returns the Nth column's value as a string (note: column index starts at 0).  Note that you can map this to literally any data type by using KStringView member functions like .Int32().
	KStringView GetValue (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).second.sValue);
	}

	/// Returns the Nth column's flags (note: column index starts at 0).
	uint64_t GetFlags (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).second.iFlags);
	}

	/// Returns whether or not a particular flag is set on the Nth column (note: column index starts at 0).
	bool IsFlag (size_t iZeroBasedIndex, uint64_t iFlag)
	{
		return ((GetFlags(iZeroBasedIndex) & iFlag) == iFlag);
	}

	/// Returns the maximum character length of the Nth column if it was set (note: column index starts at 0).
	uint32_t MaxLength (uint32_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).second.iMaxLen);
	}

	/// Formats the proper RDBMS DDL statement for inserting one row into the database for the given table and column structure.
	bool FormInsert (KString& sSQL, SQLTYPE iDBType, bool fIdentityInsert=false);

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormUpdate (KString& sSQL, SQLTYPE iDBType);

	/// Formats the proper RDBMS DDL statement for deleting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormDelete (KString& sSQL, SQLTYPE iDBType);

	/// Returns a JSON formatted string representing the KROW object
	KString ToJSON (bool bWrapInCurlies=true, KStringView sLineLeader="\t", KStringView sLineTrailer="\n");

	/// Returns the last RDBMS error message.
	KStringView GetLastError() { return (m_sLastError); }

	/// Returns the tablename of this KROW object (if set).
	KStringView GetTablename() { return (m_sTablename); }

	enum
	{
		PKEY             = 0x0000001,   ///< Indicates given column is part of the primary key.  At least one column must have the PKEY flag to use KROW to do UPDATE and DELETE.
		NONCOLUMN        = 0x0000010,   ///< Indicates given column is not a column and should be included in DDL statements.
		EXPRESSION       = 0x0000100,   ///< Indicates given column is not a column and should be included in DDL statements.
		INSERTONLY       = 0x0001000,   ///< Indicates given column is only to be used in INSERT statements (not UPDATE or DELETE).
		NUMERIC          = 0x0010000,   ///< Indicates given column should not be quoted when forming DDL statements.
		NULL_IS_NOT_NIL  = 0x0100000,   ///< Indicates given column is ???
		BOOLEAN          = 0x1000000    ///< Indicates given column is a boolean (true/false)
	};

	// - - - - - - - - - - - - - - - -
	// helper functions:
	// - - - - - - - - - - - - - - - -
	static void EscapeChars (KStringView sString, KString& sEscaped, SQLTYPE iDBType);
	static void EscapeChars (KStringView sString, KString& sEscaped,
	                         KStringView sCharsToEscape, KString::value_type iEscapeChar=0);

//----------
private:
//----------
	void    SmartClip (KStringView sColName, KString& sValue, size_t iMaxLen);
	KString ColumnInfoForLogOutput (uint32_t ii);

	KString m_sTablename;
	KString m_sLastError;
};

} // namespace dekaf2
