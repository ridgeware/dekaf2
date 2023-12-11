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

#include "kdefinitions.h"
#include "ktemplate.h"
#include "kprops.h"
#include "kjson.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
///  a string type that guarantees that all string values (from string var formatting) are SQL escaped
class DEKAF2_PUBLIC KSQLString
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KROW;
	friend class KSQL;

//----------
public:
//----------

	using string_type = KString;
	using self        = KSQLString;

	KSQLString() = default;

	// at runtime this constructor will only accept const char* from the data segment,
	// not dynamic string::c_str() - it will throw otherwise
	KSQLString(const string_type::value_type* sContent)
	{
		AssignFromView(sContent);
	}

	// at runtime this constructor will only accept KStringView from the data segment,
	// not any transformations from dynamic storage - it will throw otherwise
	template<class ConstantString,
			 typename std::enable_if<std::is_same<KStringView,  typename std::decay<ConstantString>::type>::value ||
	                                 std::is_same<KStringViewZ, typename std::decay<ConstantString>::type>::value, int>::type = 0>
	KSQLString(ConstantString sContent)
	{
		AssignFromView(sContent);
	}

	self& operator+=(const KSQLString& sOther)
	{
		m_sContent += sOther.str();
		return *this;
	}

	// at runtime this operator will only accept const char* from the data segment,
	// not dynamic string::c_str() - it will throw otherwise
	self& operator+=(const string_type::value_type* sOther);

	self operator+(const KSQLString& sOther)
	{
		KSQLString temp(*this);
		return temp += sOther;
	}

	const string_type& str() const
	{
		return m_sContent;
	}

	void resize(std::size_t iSize)
	{
		m_sContent.resize(iSize);
	}

	// can be misinterpreted as char !
	explicit operator bool() const
	{
		return !empty();
	}

	void clear()
	{
		m_sContent.clear();
	}

	bool empty() const
	{
		return m_sContent.empty();
	}

	std::size_t size() const
	{
		return m_sContent.size();
	}

	const string_type::value_type* data() const
	{
		return m_sContent.data();
	}

	std::size_t Hash() const
	{
		return m_sContent.Hash();
	}

	bool contains(const KStringView sWhat) const
	{
		return m_sContent.contains(sWhat);
	}

	bool remove_suffix(KStringView sWhich)
	{
		return m_sContent.remove_suffix(sWhich);
	}

	// it is on purpose that there is no way to replace single characters.. use string literals instead ..
	std::size_t Replace(const KSQLString& sOrig, const KSQLString& sReplace, std::size_t pos = 0, bool bReplaceAll = true)
	{
		return m_sContent.Replace(sOrig.str(), sReplace.str(), pos, bReplaceAll);
	}

	/// this is a very special version of Split, it will only split at non-quote encompassed delimiters,
	/// and the delimiter char may not be one of the quotable chars - throws otherwise. Use it to
	/// split chained commands into multiple single commands.
	std::vector<self> Split(const char chDelimit = ';', bool bTrimWhiteSpace = true) const;

//----------
private:
//----------

	string_type& ref()
	{
		return m_sContent;
	}

	void AssignFromView(KStringView sContent);

	void ThrowWarning(KStringView sContent);

	string_type m_sContent;

}; // KSQLString

template<typename T,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator==(const KSQLString& left, const T& right)
{
	return KStringView(left.str()).Equal(KStringView(right));
}

template<typename T,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator==(const T& left, const KSQLString& right)
{
	return operator==(right, left);
}

template<typename T,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator!=(const KSQLString& left, const T& right)
{
	return !operator==(left, right);
}

template<typename T,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator!=(const T& left, const KSQLString& right)
{
	return operator!=(right, left);
}

// leaving an alias to the previous name
using KSQLInjectionSafeString = KSQLString;

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KCommonSQLBase
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
		OCI6     = 26000, // connect to Oracle using the v6 OCI (older set)
		OCI8     = 28000, // connect to Oracle using the v8 OCI (the default)
		SQLITE3  = 30000, // connect to SQLite3 using their custom APIs
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
class DEKAF2_PUBLIC KCOL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// column type flags
	enum Flags
	{
		NOFLAG           = 0,        ///< Reset flags, column will be seen as string value
		PKEY             = 1 << 0,   ///< part of the primary key.  At least one column must have the PKEY flag to use KROW to do UPDATE and DELETE.
		NONCOLUMN        = 1 << 1,   ///< not a column and should be included in DDL statements.
		EXPRESSION       = 1 << 2,   ///< not a column and should be included in DDL statements.
		INSERTONLY       = 1 << 3,   ///< only to be used in INSERT statements (not UPDATE or DELETE).
		NUMERIC          = 1 << 4,   ///< should not be quoted when forming DDL statements.
		MONEY            = 1 << 5,   ///< numeric variant: assume 2 decimal points when serializing
		BOOLEAN          = 1 << 6,   ///< a boolean (true/false)
		JSON             = 1 << 7,   ///< a JSON object
		INT64NUMERIC     = 1 << 8,   ///< a NUMERIC, but would overflow in JSON - NUMERIC is also always set when this flag is true
		INCREMENT        = 1 << 9,   ///< during UPDATES the value in the existing row should be INCREMENTED by the value passed in
		NULL_IS_NOT_NIL  = 1 << 10,  ///< ???

		TYPE_FLAGS       = NUMERIC | BOOLEAN | JSON | INT64NUMERIC,
		MODE_FLAGS       = PKEY | NONCOLUMN | EXPRESSION | INSERTONLY | NULL_IS_NOT_NIL | INCREMENT
	};

	/// Column size, max size type
	using Len = uint32_t;

	KCOL () = default;

	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KString>::value == true, int>::type = 0>
	KCOL (T&& _sValue, Flags iFlags = Flags::NOFLAG, Len iMaxLen = 0)
		: sValue(std::forward<T>(_sValue))
		, m_iMaxLen(iMaxLen)
		, m_Flags(iFlags)
	{
	}

	operator KStringView() const
	{
		return sValue;
	}

	/// clear the column
	void clear();

	/// returns columm flags
	Flags GetFlags() const
	{
		return m_Flags;
	}

	/// set column flags
	void SetFlags(Flags Flags)
	{
		m_Flags = Flags;
	}

	/// add a column flag
	void AddFlags(Flags Flags);

	/// has exactly this flag, and no other
	bool IsFlag(Flags Flags) const;

	/// has this flag, and maybe others
	bool HasFlag(Flags Flags) const;

	/// clear all column flags
	void ClearFlags()
	{
		SetFlags(Flags::NOFLAG);
	}

	/// return max column length
	Len GetMaxLen() const
	{
		return m_iMaxLen;
	}

	/// set max column length
	void SetMaxLen(Len iMaxLen)
	{
		m_iMaxLen = iMaxLen;
	}

	/// print column flags as string, static function
	static KString FlagsToString (Flags iFlags);

	/// print column flags as string, member function
	KString FlagsToString () const { return FlagsToString(m_Flags); }

	KString  sValue;

//----------
private:
//----------

	Len   m_iMaxLen { 0 };
	Flags m_Flags   { Flags::NOFLAG };

}; // KCOL

DEKAF2_ENUM_IS_FLAG(KCOL::Flags)

inline void KCOL::AddFlags(Flags Flags)
{
	m_Flags |= Flags;
}

inline bool KCOL::IsFlag(Flags Flags) const
{
	return (m_Flags & Flags) == Flags;
}

inline bool KCOL::HasFlag(Flags Flags) const
{
	return (m_Flags & Flags) != 0;
}

DEKAF2_PUBLIC
bool operator==(const KCOL& left, const KCOL& right);
inline DEKAF2_PUBLIC
bool operator!=(const KCOL& left, const KCOL& right)
{
	return !operator==(left, right);
}

using KCOLS = KProps <KString, KCOL, /*order-matters=*/true, /*unique-keys*/true>;

//-----------------------------------------------------------------------------
/// KSQL's row type
class DEKAF2_PUBLIC KROW : public KCOLS, public detail::KCommonSQLBase
//-----------------------------------------------------------------------------
{

//----------
public:
//----------

	/// Column index type
	using Index = std::size_t;

	enum CONVERSION
	{
		NO_CONVERSION,  ///< no conversion in to_json()
		KEYS_TO_LOWER,  ///< Used in to_json() to map json keys to lowercase in the event that columns are mixed
		KEYS_TO_UPPER   ///< Used in to_json() to map json keys to uppercase in the event that columns are mixed
	};

	KROW () = default;

	template<typename T, typename std::enable_if<std::is_constructible<KStringView, T>::value && !std::is_same<KJSON, typename std::decay<T>::type>::value, int>::type = 0>
	KROW (T sTablename)
	    : m_sTablename(std::move(sTablename))
	{
	}

	KROW (const LJSON& json)
	{
		operator+=(json);
	}

#if !DEKAF2_KJSON2_IS_DISABLED
	KROW (const KJSON2& json)
	{
		operator+=(json);
	}
#endif

	using KCOLS::KCOLS;

	/// Set the name of the table - could also be done through construction of KROW
	/// @param sTablename the name of the table
	void SetTablename (KString sTablename) const
	{
		// this method is const!
		m_sTablename = std::move(sTablename);
	}

	/// Create default columms for the list of columns in sColumns
	/// @param sColumns comma separated list of column names to create
	std::size_t CreateColumns(KStringView sColumns);

	/// Create or set a column as the primary key and set its value or default it
	/// @param sKeyColName name of the column for the primary key
	/// @param Value a value for the primary key, or the default value for the type
	template<typename COLTYPE = KStringView>
	bool SetKey(KStringView sKeyColName, COLTYPE&& Value = COLTYPE{})
	{
		return AddCol(sKeyColName, std::forward<COLTYPE>(Value), KCOL::Flags::PKEY);
	}

	/// Create or set a column from a JSON value
	/// @param sColName Name of the column
	/// @param Value JSON value to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, const LJSON& Value, KCOL::Flags Flags = KCOL::Flags::NOFLAG, KCOL::Len iMaxLen = 0);

#if !DEKAF2_KJSON2_IS_DISABLED
	/// Create or set a column from a JSON value
	/// @param sColName Name of the column
	/// @param Value JSON value to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, const KJSON2& Value, KCOL::Flags Flags = KCOL::Flags::NOFLAG, KCOL::Len iMaxLen = 0)
	{
		return AddCol(sColName, Value.ToBase(), Flags, iMaxLen);
	}
#endif

	/// Create or set a column from a boolean value
	/// @param sColName Name of the column
	/// @param Value boolean to serialize as the column value
	/// @param iFlags special column or name flags, default BOOLEAN
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, bool Value, KCOL::Flags Flags = KCOL::Flags::BOOLEAN, KCOL::Len iMaxLen = 0)
	{
		return (KCOLS::Add (sColName, KCOL(Value ? "true" : "false", (Flags | KCOL::Flags::BOOLEAN), iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from a const char* value
	/// @param sColName Name of the column
	/// @param Value const char* to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	bool AddCol (KStringView sColName, const char* Value, KCOL::Flags Flags = KCOL::Flags::NOFLAG, KCOL::Len iMaxLen = 0)
	{
		return (KCOLS::Add (sColName, KCOL(Value, Flags, iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from a string value
	/// @param sColName Name of the column
	/// @param Value string to serialize as the column value
	/// @param iFlags special column or name flags, default none
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	template<typename COLTYPE, typename std::enable_if<detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, COLTYPE&& Value, KCOL::Flags Flags = KCOL::Flags::NOFLAG, KCOL::Len iMaxLen = 0)
	{
		return (KCOLS::Add (sColName, KCOL(std::forward<COLTYPE>(Value), Flags, iMaxLen)) != KCOLS::end());
	}

	/// Create or set a column from any value that can be printed through kFormat (e.g. numbers)
	/// @param sColName Name of the column
	/// @param Value value to serialize as the column value
	/// @param iFlags special column or name flags, default NUMERIC
	/// @param iLen limit size of column string (after string escape), default 0 = unlimited
	/// @return bool success of operation
	template<typename COLTYPE, typename std::enable_if<!detail::is_narrow_cpp_str<COLTYPE>::value, int>::type = 0>
	bool AddCol (KStringView sColName, const COLTYPE& Value, KCOL::Flags Flags = KCOL::Flags::NUMERIC, KCOL::Len iMaxLen = 0)
	{
		/*
		 * JS 2020/05/20: we do not do this check for the moment, because due
		 * to a bug this was inactive in all previous versions of dekaf2
		 * - we have to verify the consequences first
		 *
		if (sizeof(COLTYPE) > 6 && (Flags & NUMERIC))
		{
			// make sure we flag large integers - this is important when we want to
			// convert them back into JSON integers, which have a limit of 53 bits
			// - values larger than that need to be represented as strings..
			Flags |= INT64NUMERIC;
		}
		 *
		 */
		return (KCOLS::Add (sColName, KCOL(kFormat("{}", Value), Flags |= KCOL::Flags::NUMERIC, iMaxLen)) != KCOLS::end());
	}

	/// Just set the value of a column from a string, do not touch flags or max len if already set.
	/// Will create a new column with default type (string) if not existing.
	/// @param sColName Name of the column
	/// @param sValue new string value for the column
	/// @return bool success of operation
	bool SetValue (KStringView sColName, KStringView sValue);

	/// Just set the value of a column from an integer, do not touch flags or max len if already set.
	/// Will create a new column with type NUMERIC if not existing.
	/// @param sColName Name of the column
	/// @param iValue new signed 64 bit value for the column
	/// @return bool success of operation
	bool SetValue (KStringView sColName, int64_t iValue);

	/// Just set the value of a column from a decimal (assume money), do not touch flags or max len if already set.
	/// Will create a new column with type NUMERIC if not existing.
	/// @param sColName Name of the column
	/// @param iValue new signed 64 bit value for the column
	/// @return bool success of operation
	bool SetValue (KStringView sColName, double nValue);

	/// Just set the flags of a column, do not touch value or max len if already set
	/// @param sColName Name of the column
	/// @param iFlags special column or name flags
	/// @return bool success of operation, fails if column does not exist
	bool SetFlags (KStringView sColName, KCOL::Flags Flags);

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
	KSQLString FormInsert (DBT iDBType, bool bIdentityInsert=false, bool bIgnore=false) const;

	/// Appends the DDL statement by one more row
	bool AppendInsert (KSQLString& sSQL, DBT iDBType, bool bIdentityInsert=false, bool bIgnore=true) const;

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	KSQLString FormUpdate (DBT iDBType) const;

	/// Appends the DDL statement by one more row
	bool AppendUpdate (KSQLString& sSQL, DBT iDBType) const;

	/// Formats the proper RDBMS DDL statement for selecting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	KSQLString FormSelect (DBT iDBType, bool bSelectAllColumns = false) const;

	/// Formats the proper RDBMS DDL statement for deleting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	KSQLString FormDelete (DBT iDBType) const;

	/// Returns the last RDBMS error message.
	KStringView GetLastError() const { return (m_sLastError); }

	/// Returns the tablename of this KROW object (if set).
	KStringView GetTablename() const { return (m_sTablename); }

	/// Returns true if column is part of the row object.
	bool Exists (KStringView sColName) const;

	/// returns the list of chars that are escaped for a given DBType, or per default for MySQL, as that
	/// provides the superset of escaped characters
	static KStringView EscapedCharacters (DBT iDBType = DBT::MYSQL);

	static bool NeedsEscape (KStringView sCol, KStringView sCharsToEscape);
	static bool NeedsEscape (KStringView sCol, DBT iDBType = DBT::MYSQL)
	{
		return NeedsEscape(sCol, EscapedCharacters(iDBType));
	}

	static KSQLString EscapeChars (KStringView sCol, KStringView sCharsToEscape,
								   KString::value_type iEscapeChar = 0);
	static KSQLString EscapeChars (KStringView sCol, DBT iDBType);

	static KSQLString EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape,
								   KString::value_type iEscapeChar = 0);
	static KSQLString EscapeChars (const KROW::value_type& Col, DBT iDBType);

	void LogRowLayout(int iLogLevel = 3) const;

	/// Return row as a KJSON object
	LJSON to_json (CONVERSION Flags = CONVERSION::NO_CONVERSION) const;

	/// convert one row to CSV format (or just column headers):
	KString to_csv (bool bheaders=false, CONVERSION Flags = CONVERSION::NO_CONVERSION) const;

	/// append columns from another KROW object
	KROW& operator+=(const KROW& another);

	/// append a LJSON object to a krow
	KROW& operator+=(const LJSON& json);

	/// assign a LJSON object to a krow
	KROW& operator=(const LJSON& json)
	{
		clear();
		return operator+=(json);
	}

	/// Load row from a LJSON object
	KROW& from_json(const LJSON& json)
	{
		return operator=(json);
	}

	operator LJSON ()
	{
		return to_json();
	}

#if !DEKAF2_KJSON2_IS_DISABLED
	/// append a KJSON2 object to a krow
	KROW& operator+=(const KJSON2& json)
	{
		return operator+=(json.ToBase());
	}

	/// assign a KJSON2 object to a krow
	KROW& operator=(const KJSON2& json)
	{
		return operator=(json.ToBase());
	}

	/// Load row from a KJSON2 object
	KROW& from_json(const KJSON2& json)
	{
		return operator=(json);
	}

	operator KJSON2 ()
	{
		return to_json();
	}
#endif

//----------
private:
//----------

	DEKAF2_PRIVATE
	void PrintValuesForInsert(KSQLString& sSQL, DBT iDBType) const;

	mutable KString m_sTablename;
	mutable KString m_sLastError;

}; // KROW

//-----------------------------------------------------------------------------
/// ADL resolver for KROW to KJSON
inline void to_json(LJSON& j, const KROW& row)
//-----------------------------------------------------------------------------
{
	j = row.to_json();
}

//-----------------------------------------------------------------------------
/// ADL resolver for KROW to KJSON
inline void from_json(const LJSON& j, KROW& row)
//-----------------------------------------------------------------------------
{
	row.from_json(j);
}

} // namespace dekaf2

namespace fmt
{

template <>
struct formatter<dekaf2::KCOL::Flags> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KCOL::Flags& Flags, FormatContext& ctx) const
	{
		return formatter<string_view>::format(dekaf2::KCOL::FlagsToString(Flags), ctx);
	}
};

} // end of namespace fmt

namespace std
{

template<typename T, typename std::enable_if<std::is_same<typename std::decay<T>::type, dekaf2::KSQLString>::value>::type = 0>
std::ostream& operator <<(std::ostream& stream, const T& sString)
{
	stream.write(sString.data(), sString.size());
	return stream;
}

} // namespace std

namespace fmt
{

template <>
struct formatter<dekaf2::KSQLString> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KSQLString& sString, FormatContext& ctx) const
	{
		return formatter<string_view>::format(sString.str(), ctx);
	}
};

} // namespace fmt
