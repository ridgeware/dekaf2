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

/// @file ksqlite.h
/// wraps the SQLite3 API into C++

#include <unordered_map>
#include <vector>

#ifdef DEKAF2
	#include "kstring.h"
	#include "kstringview.h"
#else
	#include <string>
	#if __cplusplus > 201402L
		#if __has_include(<string_view>)
			#include <string_view>
			#define KSQLITE_HAS_STRING_VIEW
		#elif __has_include(<experimental/string_view>)
			#include <experimental/string_view>
			#define KSQLITE_HAS_EXPERIMENTAL_STRING_VIEW
		#endif
	#endif
#endif

struct sqlite3;
struct sqlite3_stmt;

#ifdef DEKAF2
namespace dekaf2 {
#endif

namespace KSQLite {

#ifdef DEKAF2
	using String = KString;
	using StringView = KStringView;
	using StringViewZ = KStringViewZ;
#else
	using String = std::string;
	#ifdef KSQLITE_HAS_STRING_VIEW
		using StringView = std::string_view;
	#elif KSQLITE_HAS_EXPERIMENTAL_STRING_VIEW
		using StringView = std::experimental::string_view;
	#else
		/// tiny but nearly complete string_view implementation - it only does not have rfind() nor reverse iterators nor find_first/last_(not)_of()
		#if !defined(constexpr_2014)
			#if __cplusplus >= 201402L
				#define constexpr_2014 constexpr
			#else
				#define constexpr_2014
			#endif
		#endif
		class StringView {
		public:
			using CharT = char; using Traits = std::char_traits<CharT>; using traits_type = Traits;
			using value_type = CharT; using size_type = std::size_t; using const_pointer = const CharT*;
			using const_reference = const CharT&; using const_iterator = const_pointer; using iterator = const_iterator;
			static constexpr size_type npos = size_type(-1);
			constexpr_2014 StringView() noexcept : m_pszString(std::addressof(m_chEmpty)), m_iSize(0) {}
			constexpr_2014 StringView(const StringView& other) noexcept : m_pszString(other.m_pszString), m_iSize(other.m_iSize) {}
			constexpr_2014 StringView(const std::string& strStr) : m_pszString(strStr.c_str()), m_iSize(strStr.size()) {}
			constexpr_2014 StringView(const char* pszStr) : m_pszString(pszStr ? pszStr : std::addressof(m_chEmpty)), m_iSize(pszStr ? std::strlen(pszStr) : 0) {}
			constexpr_2014 StringView(const char* pszStr, size_type iSize) : m_pszString(pszStr ? pszStr : std::addressof(m_chEmpty)), m_iSize(pszStr ? iSize : 0) {}
			constexpr_2014 StringView& operator=(const StringView& other) noexcept { m_pszString = other.m_pszString; m_iSize = other.m_iSize; return *this; }
			constexpr_2014 void clear() noexcept { m_pszString = std::addressof(m_chEmpty); m_iSize = 0; }
			constexpr_2014 size_type max_size() const noexcept { return size_type(-1) - 1; }
			constexpr_2014 size_type size() const noexcept { return m_iSize; } size_type length() const { return size(); }
			constexpr_2014 bool empty() const noexcept { return !size(); }
			constexpr_2014 iterator begin() const noexcept { return m_pszString; }
			constexpr_2014 iterator end() const noexcept { return m_pszString + m_iSize; }
			constexpr_2014 const_reference front() const noexcept { return *begin(); }
			constexpr_2014 const_reference back() const noexcept { return *(end() - 1); }
			constexpr_2014 const_pointer data() const noexcept { return begin(); }
			constexpr_2014 const_reference operator[](size_type pos) const { return *(begin() + pos); }
			constexpr_2014 const_reference at(size_type pos) const { if (pos >= size()) { throw std::out_of_range({}); } return operator[](pos); }
			constexpr_2014 void remove_prefix(size_type n) { m_pszString += n; m_iSize -= n; }
			constexpr_2014 void remove_suffix(size_type n) { m_iSize -= n; }
			void swap(StringView& other) noexcept { using std::swap; swap(*this, other); }
			constexpr_2014 StringView substr(size_type pos = 0, size_type count = npos) const { return { m_pszString + pos, std::min(count, size() - pos) }; }
			constexpr_2014 int compare(StringView other) const noexcept { auto cmp = Traits::compare(data(), other.data(), std::min(size(), other.size())); return (cmp) ? cmp : size() - other.size(); }
			constexpr_2014 size_type find(StringView needle, size_type pos = 0) const noexcept { if (needle.size() == 1) { return find(needle.front(), pos); }
				if (pos >= size() || needle.empty() || needle.size() > (size() - pos)) { return npos; }
				auto found = static_cast<const char*>(::memmem(data() + pos, size() - pos, needle.data(), needle.size()));
				return (found) ? static_cast<size_t>(found - data()) : npos; }
			constexpr_2014 size_type find(CharT ch, size_type pos = 0) const noexcept { if (pos > size()) { return npos; }
				auto found = static_cast<const char*>(::memchr(data() + pos, ch, size() - pos));
				return (found) ? static_cast<size_t>(found - data()) : npos; }
			constexpr_2014 size_type find(const CharT* s, size_type pos, size_type count) const { return find(StringView(s, count), pos); }
			constexpr_2014 size_type find(const CharT* s, size_type pos) const { return find(StringView(s), pos); }
		private:
			static const char m_chEmpty = 0;
			const char* m_pszString;
			std::size_t m_iSize;
		}; // StringView
		constexpr_2014 inline bool operator==(StringView left, StringView right) noexcept { return left.size() == right.size() && (left.data() == right.data() || left.size() == 0 || left.compare(right) == 0); }
		constexpr_2014 inline bool operator!=(StringView left, StringView right) noexcept { return !(left == right); }
		constexpr_2014 inline bool operator< (StringView left, StringView right) noexcept { StringView::size_type min_size = std::min(left.size(), right.size()); int r = min_size == 0 ? 0 : left.compare(right); return (r < 0) || (r == 0 && left.size() < right.size()); }
		constexpr_2014 inline bool operator> (StringView left, StringView right) noexcept { return right < left; }
		constexpr_2014 inline bool operator<=(StringView left, StringView right) noexcept { return !(left > right); }
		constexpr_2014 inline bool operator>=(StringView left, StringView right) noexcept { return !(left < right); }
	#endif
	/// string view implementation that guarantees a trailing NUL
	class StringViewZ : private StringView
	{
	public:
		constexpr2014 StringViewZ(const char* s = "") noexcept : StringView { s } {}
		StringViewZ(const std::string& str) noexcept : StringView { str } {}
		StringViewZ(StringView sv) = delete;
		constexpr_2014 const_pointer c_str() const noexcept { return begin(); }
		constexpr_2014 StringView substr(size_type pos, size_type count) const { return StringView::substr(pos, count); }
		constexpr_2014 StringViewZ substr(size_type pos) const { if (pos < size()) { return { data() + pos, size() - pos }; } else { return {}; } }
	}; // StringViewZ
#endif

//----------
enum Mode
//----------
{
	READONLY,
	READWRITE,
	READWRITECREATE
};

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// internal database connector class
struct DBConnector
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using size_type = std::size_t;

	DBConnector() = default;
	// the copy constructor actually establishes a new connection
	DBConnector(const DBConnector& other);
	DBConnector(DBConnector&&) = default;
	// ctor
	DBConnector(StringViewZ sFilename, Mode iMode);
	// dtor
	~DBConnector();

	/// Connect to database (fails if connection is already established)
	bool Connect(StringViewZ sFilename, Mode iMode);
	/// Returns count of rows affected by last query
	size_type AffectedRows();
	/// Returns row ID of last inserted row or 0
	size_type LastInsertID();
	/// Returns the filename of the connected database
	StringViewZ Filename() const;

	/// Returns last error (if any)
	StringViewZ Error() const;
	/// Returns non-zero in case of error
	int IsError() const;
	/// Returns true if no error
	bool Good() const { return !IsError(); }

	operator sqlite3*() noexcept { return m_DB; }

	sqlite3* m_DB { nullptr };
	Mode m_iMode { READONLY };

}; // DBConnector

} // end of namespace detail

using SharedConnector = std::shared_ptr<detail::DBConnector>;

class Statement;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Database connection class - creates the database connector, and allows
/// for high level tasks. Creates prepared statements and can be used for
/// ad-hoc SQL queries.
class Database
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type       = std::size_t;
	using result_type_row = std::unordered_map<String, String>;
	using result_type     = std::vector<result_type_row>;

	Database() = default;
	// the copy constructor creates a new Connector
	Database(const Database& other);
	Database(Database&&) = default;
	Database& operator=(Database&&) = default;
	/// Open database in file with given name, use given mode to open
	Database(StringViewZ sFilename, Mode iMode = Mode::READONLY);

	/// Connect to database - closes an established connection
	bool Connect(StringViewZ sFilename, Mode iMode = Mode::READONLY);
	/// Create a prepared statement
	Statement Prepare(StringView sQuery);
	/// Execute an ad-hoc query, returns success
	bool ExecuteVoid(StringViewZ sQuery);
	/// Execute an ad-hoc query, return all result rows
	result_type Execute(StringViewZ sQuery);
	/// Returns count of rows affected by last query
	size_type AffectedRows() { return Connector()->AffectedRows(); }
	/// Returns row ID of last inserted row or 0
	size_type LastInsertID() { return Connector()->LastInsertID(); }
	/// Returns true if table with given name exists
	bool HasTable(StringViewZ sTable) const;
	/// Returns last error (if any)
	StringViewZ Error() const { return Connector()->Error(); }
	/// Returns non-zero in case of error
	int IsError() const { return Connector()->IsError(); }
	/// Returns true if no error
	bool Good() const { return Connector()->Good(); }
	/// Returns name of database file
	StringViewZ Filename() const noexcept { return Connector()->Filename(); }
	/// Set de/encryption key for current database
	bool Key(StringView sKey);
	/// Set new key for current database, valid Key() must have been set before
	bool Rekey(StringView sKey);
	/// Checks if database needs a Key()
	static bool IsEncrypted(StringViewZ sFilename);
	/// Checks if current database needs a Key()
	bool IsEncrypted() const { return IsEncrypted(Filename()); }

	operator sqlite3*() noexcept { return *Connector(); }
	operator SharedConnector&()  { return Connector();  }

//----------
private:
//----------

	const SharedConnector& Connector() const noexcept { return m_Connector; }
	SharedConnector& Connector() noexcept { return m_Connector; }

	bool Check(int iReturn);

	SharedConnector m_Connector;

}; // Database

class Column;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds information about the result set structure, and gives access to any
/// of the columns in the result set, either by index or name.
class Row
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ColIndex  = int;
	using size_type = std::size_t;

	Row();
	Row(const Row&) = default;
	Row(Row&&) = default;
	Row& operator=(const Row&) = default;
	Row& operator=(Row&&) = default;

	Row(Database& database, StringView sQuery);

	/// Get a column from the result row by index
	Column Col(ColIndex iOneBasedIndex);
	/// Get a column from the result row by name
	Column Col(StringView sColName);
	/// Returns column index for given name
	ColIndex GetColIndex(StringView sColName);

	Column operator[](ColIndex iOneBasedIndex);
	Column operator[](StringView sColName);

	/// Get the Query used to build the statement
	StringViewZ GetQuery() const;
	/// Returns column count of the result set
	ColIndex size() const noexcept { return m_Row->m_iColumnCount; }
	/// Returns true if row is not valid or column count is 0
	bool empty() const noexcept { return !m_Row->m_bIsValid || !size(); }
	/// Returns true if there are no more rows to fetch
	bool Done() const noexcept { return m_Row->m_bIsDone; }
	/// Reset underlying prepared statement for another query. Normally called through Statement().
	bool Reset(bool bClearBindings = false) noexcept;
	/// Advance to next row in result set
	bool Next();
	/// Advance to next row in result set
	Row& operator++();

	/// Return const sqlite3 object pointer
	const sqlite3_stmt* Statement() const noexcept { return m_Row->m_Statement; }
	/// Return sqlite3 object pointer
	sqlite3_stmt* Statement() noexcept { return m_Row->m_Statement; }
	operator sqlite3_stmt*()  noexcept { return Statement();        }

//----------
private:
//----------

	void SetIsValid(bool bYesno) const noexcept { m_Row->m_bIsValid = bYesno; }
	void SetIsDone(bool bYesno)  const noexcept { m_Row->m_bIsDone  = bYesno; }

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// internal struct to maintain all statement- and row-related data in one
	/// place
	struct RowBase
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		using ColIndex = Row::ColIndex;

		RowBase() = default;
		RowBase(const RowBase&) = delete;
		RowBase(RowBase&&) = default;
		RowBase& operator=(const RowBase&) = delete;
		RowBase& operator=(RowBase&&) = default;
		RowBase(Database& database, StringView sQuery);
		~RowBase();

		operator sqlite3_stmt*() noexcept { return m_Statement; }

		using NameMap = std::unordered_map<String, ColIndex>;

		// we basically keep an instance of the connector here to avoid
		// it going out of scope while we still reference to it through
		// the statement object
		SharedConnector m_Connector;
		sqlite3_stmt* m_Statement { nullptr };
		NameMap m_NameMap;
		ColIndex m_iColumnCount { 0 };
		bool m_bIsValid { false };
		bool m_bIsDone { false };

	}; // RowBase

	using SharedRow = std::shared_ptr<RowBase>;

	SharedRow m_Row;

}; // Row

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Access on the data of one column
class Column
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class Row;

//----------
public:
//----------

	using ColIndex  = Row::ColIndex;
	using size_type = std::size_t;

	enum ColType
	{
		Integer,
		Float,
		Text,
		BLOB,
		Null
	};

	Column() = default;

	/// Get original column name
	StringViewZ GetName();
	/// Get column name as assigned by an 'as' clause
	StringViewZ GetNameAs();

	/// Return column as an int32_t
	int32_t Int32();
	/// Return column as a uint32_t
	uint32_t UInt32();
	/// Return column as an int64_t
	int64_t Int64();
	/// Return column as a uint64_t
	uint64_t UInt64();
	/// Return column as a double
	double Double();
	/// Return column as a string (from a TEXT or BLOB)
	StringView String();

	/// Size of result string
	size_type size();
	/// Size of result string (same as size())
	size_type length() { return size(); }

	/// Returns type of column (Integer, Float, Text, BLOB, or Null)
	ColType Type();
	/// Is column an integer?
	bool IsInteger() { return Type() == ColType::Integer; }
	/// Is column a floating point value?
	bool IsFloat()   { return Type() == ColType::Float;   }
	/// Is column a string?
	bool IsText()    { return Type() == ColType::Text;    }
	/// Is column a BLOB?
	bool IsBLOB()    { return Type() == ColType::BLOB;    }
	/// Is column Null?
	bool IsNull()    { return Type() == ColType::Null;    }

//----------
private:
//----------

	Column(Row& row, ColIndex iOneBasedIndex)
	: m_Row(row), m_Index(iOneBasedIndex-1) {}

	Row m_Row;
	ColIndex m_Index { 0 };

}; // Column

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Prepared statement class for SQLite
class Statement
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Iterator for a range of rows. Only supports prefix increment, typically
	/// to be used in range based for loops.
	class iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		iterator(Row& row, bool bToEnd = false);
		iterator& operator++();
		Row& operator*() { return *m_pRow; }
		Row* operator->() { return m_pRow; }
		bool operator==(const iterator& other) { return m_pRow == other.m_pRow; }
		bool operator!=(const iterator& other) { return !operator==(other);     }

	//----------
	private:
	//----------

		Row* m_pRow { nullptr };

	}; // iterator

	using const_iterator = iterator;
	using size_type      = std::size_t;
	using ColIndex       = Row::ColIndex;
	using ParIndex       = int;

	Statement() = default;
	Statement(const Statement&) = delete;
	Statement(Statement&&) = default;
	Statement& operator=(const Statement&) = delete;
	Statement& operator=(Statement&&) = default;

	/// Constructs a prepared statement from an SQL query
	Statement(Database& database, StringView sQuery);

	/// Bind an i64_t value
	bool Bind(ParIndex iOneBasedIndex, int64_t iValue);
	/// Bind a double value
	bool Bind(ParIndex iOneBasedIndex, double fValue);
	/// Bind a float value
	bool Bind(ParIndex iOneBasedIndex, float fValue) { return Bind(iOneBasedIndex, double(fValue)); }
	/// Bind a string value. If bCopy is true the string will be copied internally, to avoid issues with temporary strings.
	bool Bind(ParIndex iOneBasedIndex, StringView sValue, bool bCopy = true);
	/// Bind a BLOB value. If bCopy is true the BLOB will be copied internally, to avoid issues with temporary buffers.
	bool Bind(ParIndex iOneBasedIndex, void* pValue, size_type iSize, bool bCopy = true);
	/// Bind a NULL value.
	bool Bind(ParIndex iOneBasedIndex);
	/// Bind any integral value.
	template<class Integer, typename std::enable_if<std::is_integral<Integer>::value, int>::type = 0>
	bool Bind(ParIndex iOneBasedIndex, Integer iValue)
	{ return Bind(iOneBasedIndex, static_cast<int64_t>(iValue)); }

	/// Bind a string value by parameter name. If bCopy is true the string will be copied internally, to avoid issues with temporary strings.
	bool Bind(StringViewZ sParName, StringView sValue, bool bCopy = true)
	{ return Bind(GetParIndex(sParName), sValue, bCopy); }
	/// Bind a BLOB value by parameter name. If bCopy is true the BLOB will be copied internally, to avoid issues with temporary buffers.
	bool Bind(StringViewZ sParName, void* pValue, size_type iSize, bool bCopy = true)
	{ return Bind(GetParIndex(sParName), pValue, iSize, bCopy); }
	/// Bind a NULL value by parameter name.
	bool Bind(StringViewZ sParName)
	{ return Bind(GetParIndex(sParName)); }
	/// Bind any integral value by parameter name.
	template<class Parm>
	bool Bind(StringViewZ sParName, Parm iValue)
	{ return Bind(GetParIndex(sParName), iValue); }

	/// Reset statement. If bClearBindings is true, clear all existing bindings as well, which his in general not necessary, permits reuse of unchanged bindings.
	bool Reset(bool bClearBindings = false);
	/// Advance to next row
	bool NextRow();
	/// Execute a statement (same as NextRow(), except that it returns true on empty results)
	bool Execute();
	/// Get current row
	Row GetRow() { return m_Row; }
	/// Do we have a result set?
	bool empty() const { return m_Row.empty(); }
	/// Get the Query used to build the statement
	StringViewZ GetQuery() const { return m_Row.GetQuery(); }
	/// Returns parameter index for given name
	ParIndex GetParIndex(StringViewZ sParName);

	/// Get start iterator over rows after execution of a prepared statement
	iterator begin() { return iterator(m_Row, false); }
	/// Get end iterator over rows
	iterator end()   { return iterator(m_Row, true);  }

//----------
private:
//----------

	class Row m_Row;

}; // Statement

} // end of namespace KSQLite

#ifdef DEKAF2
} // end of namespace dekaf2
#endif
