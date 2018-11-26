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
	#if !defined(constexpr_2014)
		#if __cplusplus >= 201402L
			#define constexpr_2014 constexpr
		#else
			#define constexpr_2014
		#endif
	#endif
	using String = std::string;
	#ifdef KSQLITE_HAS_STRING_VIEW
		using StringView = std::string_view;
	#else
	/// tiny but nearly complete string_view implementation - it only does not have rfind() nor reverse iterators nor find_first/last_(not)_of()
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
		constexpr_2014 void remove_prefix(size_type n) { m_pszString += n; }
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
	class StringViewZ : public StringView
	{
	public:
		using StringView::StringView;

		constexpr_2014 const_pointer c_str() const noexcept { return begin(); }

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

class Column;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Database
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type   = std::size_t;
	using result_type_row = std::unordered_map<String, String>;
	using result_type = std::vector<result_type_row>;

	Database() = default;
	Database(const Database&) = delete;
	Database(Database&&) = default;
	Database& operator=(const Database&) = delete;
	Database& operator=(Database&&) = default;

	/// Open database in file with given name, use given mode to open
	Database(StringView sFilename, Mode iMode = Mode::READONLY);
	// dtor
	~Database();

	/// Execute an ad-hoc query, returns success
	bool ExecuteVoid(StringViewZ sQuery);
	/// Execute an ad-hoc query, return all result rows
	result_type Execute(StringViewZ sQuery);
	/// Returns count of rows affected by last query
	size_type AffectedRows();
	/// Returns row ID of last inserted row or 0
	size_type LastInsertID() const noexcept;
	/// Returns true if table with given name exists
	bool HasTable(StringViewZ sTable) const;
	/// Returns last error (if any)
	const String& Error() noexcept { return m_sError; }
	/// Returns name of database file
	const String& GetFilename() const noexcept { return m_sFilename; }
	/// Set de/encryption key for current database
	bool Key(StringView sKey);
	/// Set new key for current database, valid Key() must have been set before
	bool Rekey(StringView sKey);
	/// Checks if database needs a Key()
	static bool IsEncrypted(StringViewZ sFilename);
	/// Checks if current database needs a Key()
	bool IsEncrypted() const { return IsEncrypted(GetFilename()); }

	operator sqlite3*() { return m_DB; }

//----------
private:
//----------

	bool SetError(StringView sError) const;
	bool SetError() const;
	bool Check(int iReturn);

	String m_sFilename;
	mutable String m_sError;
	sqlite3* m_DB { nullptr };

}; // Database

class Row;

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct RowBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using ColIndex = int;

	RowBase() = default;
	RowBase(Database& database, StringView sQuery);
	~RowBase();

	using NameMap = std::unordered_map<String, ColIndex>;

	NameMap m_NameMap;
	sqlite3* m_DB { nullptr };
	sqlite3_stmt* m_Statement { nullptr };
	String m_sQuery;
	ColIndex m_iColumnCount { 0 };
	bool m_bIsValid { false };

}; // RowBase

} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Row
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using BaseType  = detail::RowBase;
	using ColIndex  = detail::RowBase::ColIndex;
	using size_type = std::size_t;

	Row(Database& database, StringView sQuery);
	Row() = default;
	Row(const Row&) = default;
	Row(Row&&) = default;
	Row& operator=(const Row&) = default;
	Row& operator=(Row&&) = default;

	/// Get a column from the result row by index
	Column GetColumn(ColIndex iZeroBasedIndex);
	/// Get a column from the result row by name
	Column GetColumn(StringView sColName);
	/// Returns column index for given name
	ColIndex GetColIndex(StringView sColName);

	Column operator[](ColIndex iZeroBasedIndex);
	Column operator[](StringView sColName);

	const String& GetQuery() const noexcept { return m_Row->m_sQuery; }
	ColIndex size() const noexcept { return m_Row->m_iColumnCount; }
	bool empty() const noexcept { return !m_Row->m_bIsValid || !size(); }

	void SetIsValid(bool bYesno) const noexcept { m_Row->m_bIsValid = bYesno; }

	sqlite3* DB() const noexcept { return m_Row->m_DB; }
	sqlite3_stmt* Statement() const noexcept { return m_Row->m_Statement; }

//----------
private:
//----------

	using RowPtr = std::shared_ptr<detail::RowBase>;

	RowPtr m_Row;

}; // Row

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Statement
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;
	using ColIndex  = Row::ColIndex;
	using ParIndex  = int;

	Statement() = default;
	Statement(const Statement&) = delete;
	Statement(Statement&&) = default;
	Statement& operator=(const Statement&) = delete;
	Statement& operator=(Statement&&) = default;

	Statement(Database& database, StringView sQuery);

	bool Bind(ParIndex iOneBasedIndex, int64_t iValue);
	bool Bind(ParIndex iOneBasedIndex, double fValue);
	bool Bind(ParIndex iOneBasedIndex, float fValue) { return Bind(iOneBasedIndex, double(fValue)); }
	bool Bind(ParIndex iOneBasedIndex, StringView sValue);
	bool Bind(ParIndex iOneBasedIndex, void* pValue, size_type iSize);
	bool Bind(ParIndex iOneBasedIndex);
	template<class Integer, typename std::enable_if<std::is_integral<Integer>::value, int>::type = 0>
	bool Bind(ParIndex iOneBasedIndex, Integer iValue)
	{ return Bind(iOneBasedIndex, static_cast<int64_t>(iValue)); }

	bool Bind(StringViewZ sParName, void* pValue, size_type iSize)
	{ return Bind(GetParIndex(sParName), pValue, iSize); }
	bool Bind(StringViewZ sParName)
	{ return Bind(GetParIndex(sParName)); }
	template<class Parm>
	bool Bind(StringViewZ sParName, Parm iValue)
	{ return Bind(GetParIndex(sParName), iValue); }

	/// Advance to next row
	bool NextRow();
	/// Execute a statement (same as NextRow() )
	bool Execute() { return NextRow(); }
	/// Get current row
	Row GetRow() { return m_Row; }
	/// Do we have a result set?
	bool empty() const { return m_Row.empty(); }
	/// Get the Query used to build the statement
	const String& GetQuery() const noexcept { return m_Row.GetQuery(); }
	/// Returns parameter index for given name
	ParIndex GetParIndex(StringViewZ sParName);

//----------
private:
//----------

	bool CheckColIndex(ColIndex iZeroBasedIndex);

	Row m_Row;

}; // Statement

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Column
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ColIndex  = Row::ColIndex;
	using size_type = std::size_t;

	enum Type
	{
		Integer,
		Float,
		Text,
		BLOB,
		Null
	};

	Column() = default;
	Column(const Column&) = default;
	Column(Column&&) = default;
	Column& operator=(const Column&) = default;
	Column& operator=(Column&&) = default;

	Column(Row& row, ColIndex iZeroBasedIndex)
	: m_Row(row), m_Index(iZeroBasedIndex) {}


	/// Get original column name
	KStringViewZ GetName();
	/// Get column name as assigned by an 'as' clause
	KStringViewZ GetNameAs();

	int32_t      GetInt32();
	uint32_t     GetUInt32();
	int64_t      GetInt64();
	uint64_t     GetUInt64();
	double       GetDouble();
	KStringView  GetText();
	KStringView  GetBLOB();

	size_type    size();
	size_type    length() { return size(); }

	Type GetType();
	bool IsInteger() { return GetType() == Type::Integer; }
	bool IsFloat()   { return GetType() == Type::Float;   }
	bool IsText()    { return GetType() == Type::Text;    }
	bool IsBLOB()    { return GetType() == Type::BLOB;    }
	bool IsNull()    { return GetType() == Type::Null;    }

//----------
private:
//----------

	Row m_Row;
	ColIndex m_Index { 0 };

}; // Column

} // end of namespace KSQLite

#ifdef DEKAF2
} // end of namespace dekaf2
#endif
