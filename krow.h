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
		NONE        = 0,   // must stay zero
		MYSQL       = 100, // assume we're connecting to MySQL
		ORACLE6     = 206, // assume we're connecting to an Oracle6 rdbms
		ORACLE7     = 207, // assume we're connecting to an Oracle7 rdbms
		ORACLE8     = 208, // assume we're connecting to an Oracle8 rdbms
		ORACLE      = 200, // use the latest assumptions about Oracle
		SQLSERVER   = 300, // SQLServer with UTF-16 storage in n(var)char
		SQLSERVER15 = 315, // SQLServer with UTF-8 storage in (var)char, from v15 onwards (2019)
		SYBASE      = 400,
		INFORMIX    = 500,
		SQLITE3     = 600  // assume we're connecting to SQLite3
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

	explicit
	KCOL (KString _sValue, Flags iFlags=0, Len iMaxLen=0)
		: sValue(std::move(_sValue))
		, m_iMaxLen(iMaxLen)
		, m_iFlags(iFlags)
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

bool operator==(const KCOL& left, const KCOL& right);
inline
bool operator!=(const KCOL& left, const KCOL& right)
{
	return !operator==(left, right);
}


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

	/// Set the name of the table - could also be done through construction of KROW
	/// @param sTablename the name of the table
	void SetTablename (KStringView sTablename) const
	{
		// this method is const!
		m_sTablename = sTablename;
	}

	/// Create default columms for the list of columns in sColumns
	/// @param sColumns comma separated list of column names to create
	std::size_t CreateColumns(KStringView sColumns);

	/// Create or set a column as the primary key and set its value or default it
	/// @param sKeyColName name of the column for the primary key
	/// @param Value a value for the primary key, or the default value for the type
	template<typename COLTYPE = KStringView>
	bool SetKey(KStringView sKeyColName, COLTYPE Value = COLTYPE{})
	{
		return AddCol(sKeyColName, Value, PKEY);
	}

	/// Create or set a column from a JSON value
	/// @param sColName Name of the column
	/// @param Value JSON value to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, const KJSON& Value, KCOL::Flags iFlags=NOFLAG, KCOL::Len iMaxLen=0);

	/// Create or set a column from a boolean value
	/// @param sColName Name of the column
	/// @param Value boolean to serialize as the column value
	/// @param iFlags special column or name flags, default BOOLEAN
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, bool Value, KCOL::Flags iFlags=BOOLEAN, KCOL::Len iMaxLen=0)
	{
		return (KCOLS::Add (sColName, KCOL(Value ? "true" : "false", (iFlags | BOOLEAN), iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from a const char* value
	/// @param sColName Name of the column
	/// @param Value const char* to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, const char* Value, KCOL::Flags iFlags=NOFLAG, KCOL::Len iMaxLen=0)
	{
		return (KCOLS::Add (sColName, KCOL(Value, iFlags, iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from a string value
	/// @param sColName Name of the column
	/// @param Value string to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	template<typename COLTYPE, typename std::enable_if<detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, KCOL::Flags iFlags=NOFLAG, KCOL::Len iMaxLen=0)
	{
		return (KCOLS::Add (sColName, KCOL(Value, iFlags, iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from any value that can be printed through kFormat (e.g. numbers)
	/// @param sColName Name of the column
	/// @param Value value to serialize as the column value
	/// @param iFlags special column or name flags, default NUMERIC
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	template<typename COLTYPE, typename std::enable_if<!detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE Value, KCOL::Flags iFlags=NUMERIC, KCOL::Len iMaxLen=0)
	{
		/*
		 * JS 2020/05/20: we do not do this check for the moment, because due
		 * to a bug this was inactive in all previous versions of dekaf2
		 * - we have to verify the consequences first
		 *
		if (sizeof(COLTYPE) > 6 && (iFlags & NUMERIC))
		{
			// make sure we flag large integers - this is important when we want to
			// convert them back into JSON integers, which have a limit of 53 bits
			// - values larger than that need to be represented as strings..
			iFlags |= INT64NUMERIC;
		}
		 *
		 */
		return (KCOLS::Add (sColName, KCOL(kFormat("{}", Value), iFlags |= NUMERIC, iMaxLen)) != KCOLS::end());
	}

	/// Just set the value of a column from a string, do not touch flags or max len if already set.
	/// Will create a new column with default type (string) if not existing.
	/// @param sColName Name of the column
	/// @param sValue new string value for the column
	/// @return bool success of operation
	bool SetValue (KStringView sColName, KStringView sValue);

	/// Just set the value of a column from a string, do not touch flags or max len if already set.
	/// Will create a new column with type NUMERIC if not existing.
	/// @param sColName Name of the column
	/// @param iValue new signed 64 bit value for the column
	/// @return bool success of operation
	bool SetValue (KStringView sColName, int64_t iValue);

	/// Just set the flags of a column, do not touch value or max len if already set
	/// @param sColName Name of the column
	/// @param iFlags special column or name flags
	/// @return bool success of operation, fails if column does not exist
	bool SetFlags (KStringView sColName, KCOL::Flags iFlags);

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
	bool FormInsert (KString& sSQL, DBT iDBType, bool bIdentityInsert=false, bool bIgnore=false) const;

	/// Appends the DDL statement by one more row
	bool AppendInsert (KString& sSQL, DBT iDBType, bool bIdentityInsert=false, bool bIgnore=true) const;

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormUpdate (KString& sSQL, DBT iDBType) const;

	/// Appends the DDL statement by one more row
	bool AppendUpdate (KString& sSQL, DBT iDBType) const;

	/// Formats the proper RDBMS DDL statement for selecting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormSelect (KString& sSQL, DBT iDBType, bool bSelectAllColumns = false) const;

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

	/// returns the list of chars that are escaped for a given DBType, or per default for MySQL, as that
	/// provides the superset of escaped characters
	static KStringView EscapedCharacters (DBT iDBType = DBT::MYSQL);

	static bool NeedsEscape (KStringView sCol, KStringView sCharsToEscape);
	static bool NeedsEscape (KStringView sCol, DBT iDBType = DBT::MYSQL)
	{
		return NeedsEscape(sCol, EscapedCharacters(iDBType));
	}

	static KString EscapeChars (KStringView sCol, KStringView sCharsToEscape,
								KString::value_type iEscapeChar = 0);
	static KString EscapeChars (KStringView sCol, DBT iDBType);

	static KString EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape,
								KString::value_type iEscapeChar = 0);
	static KString EscapeChars (const KROW::value_type& Col, DBT iDBType);

	KString ColumnInfoForLogOutput (const KCOLS::value_type& it, Index iCol) const;
	void LogRowLayout(int iLogLevel = 3) const;

	/// Return row as a KJSON object
	KJSON to_json (uint64_t iFlags=0) const;

	/// convert one row to CSV format (or just column headers):
	KString to_csv (bool bheaders=false, uint64_t iFlags=0);

	/// append columns from another krow object
	KROW& operator+=(const KROW& another);

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

	void PrintValuesForInsert(KString& sSQL, DBT iDBType) const;

	mutable KString m_sTablename;
	mutable KString m_sLastError;

}; // KROW

} // namespace dekaf2
