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

#include "bits/ktemplate.h"
#include "kprops.h"
#include "kjson.h"

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

	KCOL (KString&& _sValue, uint16_t _iFlags=0, uint32_t _iMaxLen=0)
		: sValue(std::move(_sValue))
		, iMaxLen(_iMaxLen)
		, iFlags(_iFlags)
	{
	}

	KCOL (KStringView _sValue, uint16_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	KCOL (const std::string& _sValue, uint16_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	KCOL (const char* _sValue, uint16_t _iFlags=0, uint32_t _iMaxLen=0)
		: KCOL(KString(_sValue), _iFlags, _iMaxLen)
	{
	}

	uint16_t GetFlags() const
	{
		return iFlags;
	}

	void SetFlags(uint16_t _iFlags)
	{
		iFlags = _iFlags;
	}

	void AddFlags(uint16_t _iFlags)
	{
		iFlags |= _iFlags;
	}

	bool IsFlag(uint16_t _iFlags) const
	{
		return (iFlags & _iFlags) == _iFlags;
	}

	bool HasFlag(uint16_t _iFlags) const
	{
		return (iFlags & _iFlags) != 0;
	}

	void ClearFlags()
	{
		SetFlags(0);
	}

	uint32_t GetMaxLen() const
	{
		return iMaxLen;
	}

	void SetMaxLen(uint32_t _iMaxLen)
	{
		iMaxLen = _iMaxLen;
	}

	KString  sValue;  // aka "second"

//----------
private:
//----------

	uint32_t iMaxLen{0};
	uint16_t iFlags{0};

}; // KCOL

using KCOLS = KProps <KString, KCOL, /*order-matters=*/true, /*unique-keys*/true>;

//-----------------------------------------------------------------------------
class KROW : public KCOLS, public detail::KCommonSQLBase
//-----------------------------------------------------------------------------
{
//----------
public:
//----------

	KROW () = default;

	KROW (const char* szTablename)
	{
		m_sTablename =  szTablename;
	}

	KROW (KStringView sTablename)
	{
		m_sTablename =  sTablename;
	}

	using KCOLS::KCOLS;

	void SetTablename (KStringView sTablename)
	{
		m_sTablename =  sTablename;
	}

	bool AddCol (KStringView sColName, const KJSON& Value, uint16_t iFlags=JSON, uint32_t iMaxLen=0);

	bool AddCol (KStringView sColName, bool Value, uint16_t iFlags=BOOLEAN, uint32_t iMaxLen=0)
	{
		KCOL col (kFormat("{}", Value), iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	bool AddCol (KStringView sColName, const char* Value, uint16_t iFlags=0, uint32_t iMaxLen=0)
	{
		KCOL col (Value, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	template<typename COLTYPE, typename std::enable_if<detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, uint16_t iFlags=0, uint32_t iMaxLen=0)
	{
		KCOL col (Value, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	template<typename COLTYPE, typename std::enable_if<!detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, uint16_t iFlags=NUMERIC, uint32_t iMaxLen=0)
	{
		KCOL col (kFormat("{}", Value), iFlags, iMaxLen);
		if (sizeof(COLTYPE) > 6 && (iFlags & NUMERIC))
		{
			// make sure we flag large integers - this is important when we want to
			// convert them back into JSON integers, which have a limit of 53 bits
			// - values larger than that need to be represented as strings..
			iFlags |= INT64NUMERIC;
		}
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

	bool SetFlags (KStringView sColName, uint16_t iFlags)
	{
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::end())
		{
			return (false);
		}
		else
		{
			it->second.SetFlags(iFlags);
			return (true);
		}
	}

	/// association arrays, e.g. row["column_name"] --> the value for that columm
	KString& operator[] (KStringView sColName)              { return KCOLS::operator[](sColName).sValue; }
	const KString& operator[] (KStringView sColName) const  { return KCOLS::operator[](sColName).sValue; }

	/// Returns the Nth column's name (note: column index starts at 0).
	const KString& GetName (size_t iZeroBasedIndex) const
	{
		return (at (iZeroBasedIndex).first);
	}

	/// Returns the Nth column's value as a string (note: column index starts at 0).  Note that you can map this to literally any data type by using KStringView member functions like .Int32().
	const KString& GetValue (size_t iZeroBasedIndex) const
	{
		return (at (iZeroBasedIndex).second.sValue);
	}

	/// Formats the proper RDBMS DDL statement for inserting one row into the database for the given table and column structure.
	bool FormInsert (KString& sSQL, SQLTYPE iDBType, bool fIdentityInsert=false) const;

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormUpdate (KString& sSQL, SQLTYPE iDBType) const;

	/// Formats the proper RDBMS DDL statement for deleting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormDelete (KString& sSQL, SQLTYPE iDBType) const;

	/// Returns the last RDBMS error message.
	KStringView GetLastError() const { return (m_sLastError); }

	/// Returns the tablename of this KROW object (if set).
	KStringView GetTablename() const { return (m_sTablename); }

	enum
	{
		PKEY             = 1 << 0,   ///< Indicates given column is part of the primary key.  At least one column must have the PKEY flag to use KROW to do UPDATE and DELETE.
		NONCOLUMN        = 1 << 1,   ///< Indicates given column is not a column and should be included in DDL statements.
		EXPRESSION       = 1 << 2,   ///< Indicates given column is not a column and should be included in DDL statements.
		INSERTONLY       = 1 << 3,   ///< Indicates given column is only to be used in INSERT statements (not UPDATE or DELETE).
		NUMERIC          = 1 << 4,   ///< Indicates given column should not be quoted when forming DDL statements.
		NULL_IS_NOT_NIL  = 1 << 5,   ///< Indicates given column is ???
		BOOLEAN          = 1 << 6,   ///< Indicates given column is a boolean (true/false)
		JSON             = 1 << 7,   ///< Indicates given column is a JSON object
		INT64NUMERIC     = 1 << 8    ///< Indicates given column is a NUMERIC, but would overflow in JSON - NUMERIC is also always set when this flag is true
	};

	// - - - - - - - - - - - - - - - -
	// helper functions:
	// - - - - - - - - - - - - - - - -
	static void EscapeChars (KStringView sString, KString& sEscaped, SQLTYPE iDBType);
	static void EscapeChars (KStringView sString, KString& sEscaped,
	                         KStringView sCharsToEscape, KString::value_type iEscapeChar=0);

	KString ColumnInfoForLogOutput (const KCOLS::value_type& it, int16_t iCol = -1) const;

	/// Return row as a KJSON object
	KJSON to_json() const;

	KROW& operator+=(const KJSON& json);

	KROW& operator=(const KJSON& json)
	{
		clear();
		return operator+=(json);
	}

	/// Load row from a KJSON object
	KROW& from_json(const KJSON& json)
	{
		return operator=(json);
	}

//----------
private:
//----------
	
	void    SmartClip (KStringView sColName, KString& sValue, size_t iMaxLen) const;

	KString m_sTablename;
	mutable KString m_sLastError;
};

} // namespace dekaf2
