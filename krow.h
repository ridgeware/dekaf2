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

	enum class DBT
	{
		// the DBT constant is used to govern certain SQL statments:
		// never change the absolute values, they are persisted in config files
		NONE      = 0,   // must stay zero
		MYSQL     = 100, // assume we're connecting to MySQL
		ORACLE6   = 206, // assume we're connecting to an Oracle6 rdbms
		ORACLE7   = 207, // assume we're connecting to an Oracle7 rdbms
		ORACLE8   = 208, // assume we're connecting to an Oracle8 rdbms
		ORACLE    = 200, // use the latest assumptions about Oracle
		SQLSERVER = 300,
		SYBASE    = 400,
		INFORMIX  = 500,
		SQLITE3   = 600  // assume we're connecting to SQLite3
	};

	enum class API
	{
		// the API constant determines how to connect to the database
		// never change the absolute values, they are persisted in config files
		NONE     = 0,     // must stay zero
		MYSQL    = 10000, // connect to MYSQL using their custom APIs
		SQLITE3  = 30000, // connect to SQLite3 using their custom APIs
		OCI6     = 26000, // connect to Oracle using the v6 OCI (older set)
		OCI8     = 28000, // connect to Oracle using the v8 OCI (the default)
		DBLIB    = 40000, // connect to SQLServer or Sybase using DBLIB
		CTLIB    = 50000, // connect to SQLServer or Sybase using CTLIB
		INFORMIX = 80000, // connect to Informix using their API
		ODBC     = 90000  // connect to something using ODBC APIs
	};

	static void     SetDebugLevel (int16_t iNewLevel) { m_iDebugLevel = iNewLevel; }
	static int16_t  GetDebugLevel () { return m_iDebugLevel; }

//----------
private:
//----------

	static int16_t m_iDebugLevel;

}; // KCommonSQLBase

} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCOL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------


//----------
public:
//----------

	/// KSQL generic column type flags
	using Flags = uint16_t;
	/// Column size, max size type
	using Len   = uint32_t;

	KCOL () = default;

	KCOL (KString&& _sValue, Flags iFlags=0, Len iMaxLen=0)
		: sValue(std::move(_sValue))
		, m_iMaxLen(iMaxLen)
		, m_iFlags(iFlags)
	{
	}

	KCOL (KStringView _sValue, Flags iFlags=0, Len iMaxLen=0)
		: KCOL(KString(_sValue), iFlags, iMaxLen)
	{
	}

	KCOL (const std::string& _sValue, Flags iFlags=0, Len iMaxLen=0)
		: KCOL(KString(_sValue), iFlags, iMaxLen)
	{
	}

	KCOL (const char* _sValue, Flags iFlags=0, Len iMaxLen=0)
		: KCOL(KString(_sValue), iFlags, iMaxLen)
	{
	}

	void clear()
	{
		sValue.clear();
		m_iMaxLen = 0;
		m_iFlags  = 0;
	}

	Flags GetFlags() const
	{
		return m_iFlags;
	}

	void SetFlags(Flags iFlags)
	{
		m_iFlags = iFlags;
	}

	void AddFlags(Flags iFlags)
	{
		m_iFlags |= iFlags;
	}

	bool IsFlag(Flags iFlags) const
	{
		return (m_iFlags & iFlags) == iFlags;
	}

	bool HasFlag(Flags iFlags) const
	{
		return (m_iFlags & iFlags) != 0;
	}

	void ClearFlags()
	{
		SetFlags(0);
	}

	Len GetMaxLen() const
	{
		return m_iMaxLen;
	}

	void SetMaxLen(Len iMaxLen)
	{
		m_iMaxLen = iMaxLen;
	}

	KString  sValue;  // aka "second"

//----------
private:
//----------

	Len   m_iMaxLen { 0 };
	Flags m_iFlags  { 0 };

}; // KCOL

using KCOLS = KProps <KString, KCOL, /*order-matters=*/true, /*unique-keys*/true>;

//-----------------------------------------------------------------------------
class KROW : public KCOLS, public detail::KCommonSQLBase
//-----------------------------------------------------------------------------
{

//----------
public:
//----------

	/// Column index type
	using Index = std::size_t;

	KROW () = default;

	KROW (const char* szTablename)
	    : m_sTablename(szTablename)
	{
	}

	KROW (KStringView sTablename)
	    : m_sTablename(sTablename)
	{
	}

	using KCOLS::KCOLS;

	void SetTablename (KStringView sTablename)
	{
		m_sTablename = sTablename;
	}

	bool AddCol (KStringView sColName, const KJSON& Value, KCOL::Flags iFlags=NOFLAG, KCOL::Len iMaxLen=0);

	bool AddCol (KStringView sColName, bool Value, KCOL::Flags iFlags=BOOLEAN, KCOL::Len iMaxLen=0)
	{
		KCOL col (kFormat("{}", Value), iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	bool AddCol (KStringView sColName, const char* Value, KCOL::Flags iFlags=NOFLAG, KCOL::Len iMaxLen=0)
	{
		KCOL col (Value, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	template<typename COLTYPE, typename std::enable_if<detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, KCOL::Flags iFlags=0, KCOL::Len iMaxLen=0)
	{
		KCOL col (Value, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, std::move(col)) != KCOLS::end());
	}

	template<typename COLTYPE, typename std::enable_if<!detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, KCOL::Flags iFlags=NUMERIC, KCOL::Len iMaxLen=0)
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

	// TODO remove if possible, it does not set the KSQL column type properly
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

	bool SetFlags (KStringView sColName, KCOL::Flags iFlags)
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
	const KString& GetName (Index iZeroBasedIndex) const
	{
		return (at (iZeroBasedIndex).first);
	}

	/// Returns the Nth column's value as a string (note: column index starts at 0).  Note that you can map this to literally any data type by using KStringView member functions like .Int32().
	const KString& GetValue (Index iZeroBasedIndex) const
	{
		return (at (iZeroBasedIndex).second.sValue);
	}

	/// Formats the proper RDBMS DDL statement for inserting one row into the database for the given table and column structure.
	bool FormInsert (KString& sSQL, DBT iDBType, bool fIdentityInsert=false) const;

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormUpdate (KString& sSQL, DBT iDBType) const;

	/// Formats the proper RDBMS DDL statement for deleting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormDelete (KString& sSQL, DBT iDBType) const;

	/// Returns the last RDBMS error message.
	KStringView GetLastError() const { return (m_sLastError); }

	/// Returns the tablename of this KROW object (if set).
	KStringView GetTablename() const { return (m_sTablename); }

	/// Returns true if column is part of the row object.
	bool Exists (KStringView sColName) const;

	enum
	{
		NOFLAG           = 0,        ///< Reset flags, column will be seen as string value
		PKEY             = 1 << 0,   ///< Indicates given column is part of the primary key.  At least one column must have the PKEY flag to use KROW to do UPDATE and DELETE.
		NONCOLUMN        = 1 << 1,   ///< Indicates given column is not a column and should be included in DDL statements.
		EXPRESSION       = 1 << 2,   ///< Indicates given column is not a column and should be included in DDL statements.
		INSERTONLY       = 1 << 3,   ///< Indicates given column is only to be used in INSERT statements (not UPDATE or DELETE).
		NUMERIC          = 1 << 4,   ///< Indicates given column should not be quoted when forming DDL statements.
		NULL_IS_NOT_NIL  = 1 << 5,   ///< Indicates given column is ???
		BOOLEAN          = 1 << 6,   ///< Indicates given column is a boolean (true/false)
		JSON             = 1 << 7,   ///< Indicates given column is a JSON object
		INT64NUMERIC     = 1 << 8,   ///< Indicates given column is a NUMERIC, but would overflow in JSON - NUMERIC is also always set when this flag is true

		KEYS_TO_LOWER    = 1 << 9,   ///< Used in to_json to map json keys to lowercase in the event that columns are mixed
		KEYS_TO_UPPER    = 1 << 10,  ///< Used in to_json to map json keys to uppercase in the event that columns are mixed

		TYPE_FLAGS       = NUMERIC | BOOLEAN | JSON | INT64NUMERIC,
		MODE_FLAGS       = PKEY | NONCOLUMN | EXPRESSION | INSERTONLY | NULL_IS_NOT_NIL,

		// keep list of flags in synch with KROW::FlagsToString() helper function
	};

	// - - - - - - - - - - - - - - - -
	// helper functions:
	// - - - - - - - - - - - - - - - -
	static KString FlagsToString (uint64_t iFlags);
	static KString EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape,
								KString::value_type iEscapeChar = 0);
	static KString EscapeChars (const KROW::value_type& Col, DBT iDBType);

	KString ColumnInfoForLogOutput (const KCOLS::value_type& it, Index iCol) const;
	void LogRowLayout(int iLogLevel = 3) const;

	/// Return row as a KJSON object
	KJSON to_json (uint64_t iFlags=0) const;

	/// append a json object with a krow
	KROW& operator+=(const KJSON& json);

	/// assign a json object from a krow
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
	
	KString m_sTablename;
	mutable KString m_sLastError;
};

} // namespace dekaf2
