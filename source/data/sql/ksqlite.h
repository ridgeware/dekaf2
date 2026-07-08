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
///
/// Threading model: a Database and everything executing on it belongs to one
/// thread at a time. Simultaneous use of one connection from multiple threads
/// throws ConcurrencyError. For parallel use open one connection per thread -
/// the Database copy constructor opens a new connection to the same file.
///
/// SQL is passed as text. Compiled statements are cached per connection, keyed
/// by the SQL text - a text is admitted into the cache when it is seen for the
/// second time on the connection, so generated one-off SQL cannot flood the
/// cache. Parameters are bound at the execution site:
///
///   auto Result = db.ExecSQL("insert into user (name) values (?1)", sName);
///   auto iID    = Result.LastInsertID();
///
///   for (auto& Row : db.ExecQuery("select id, name from user where age > ?1", 21))
///   {
///       auto iID   = Row.Get<int64_t>(1);
///       auto sName = Row.Col("name").String();
///   }

#include <unordered_map>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <tuple>
#include <utility>
#include <cstdint>
#include <cstddef>

#ifdef DEKAF2
	#include <dekaf2/core/strings/kstring.h>
	#include <dekaf2/core/strings/kstringview.h>
	#include <dekaf2/core/errors/kexception.h>
	#include <dekaf2/containers/associative/kassociative.h> // for KUnorderedMap
#else
	#include <string>
	#include <stdexcept>
	#if __cplusplus >= 201703L
		#if __has_include(<string_view>)
			#include <string_view>
			#define KSQLITE_HAS_STRING_VIEW 1
		#endif
	#endif
	#ifndef DEKAF2_PUBLIC
		#define DEKAF2_PUBLIC
	#endif
	#ifndef DEKAF2_NODISCARD
		#define DEKAF2_NODISCARD
	#endif
#endif

#if defined(__has_include)
	#if (defined(DEKAF2_HAS_CPP_17) || __cplusplus >= 201703L) && __has_include(<optional>)
		#include <optional>
		#define KSQLITE_HAS_OPTIONAL 1
	#endif
#endif

struct sqlite3;
struct sqlite3_stmt;

#ifdef DEKAF2
DEKAF2_NAMESPACE_BEGIN

/// @addtogroup data_sql
/// @{
#endif

namespace KSQLite {

#ifdef DEKAF2
	using String      = KString;
	using StringView  = KStringView;
	using StringViewZ = KStringViewZ;
#else
	using String      = std::string;
	#ifdef KSQLITE_HAS_STRING_VIEW
		using StringView = std::string_view;
	#else
		using StringView = std::string;
	#endif
	// parameters that need NUL termination own their storage in standalone mode
	using StringViewZ = std::string;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// exception thrown on SQL errors when throwing is enabled with SetThrow()
class DEKAF2_PUBLIC Exception
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef DEKAF2
	: public KException
{
	using KException::KException;
#else
	: public std::runtime_error
{
	using std::runtime_error::runtime_error;
#endif

}; // Exception

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// exception thrown on simultaneous use of one connection from multiple
/// threads - always thrown, independent of SetThrow()
class DEKAF2_PUBLIC ConcurrencyError : public Exception
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using Exception::Exception;

}; // ConcurrencyError

//----------
/// connection open mode
enum Mode
//----------
{
	/// open an existing database for reading
	READONLY,
	/// open an existing database for reading and writing
	READWRITE,
	/// open for reading and writing, create the database if it does not exist
	READWRITECREATE
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// connection options, given at construction of a Database
///
/// @code
/// KSQLite::Options Opts;
/// Opts.iMode = KSQLite::Mode::READWRITECREATE;
/// Opts.bWAL  = true;
///
/// KSQLite::Database db("/tmp/test.db", Opts);
/// @endcode
struct DEKAF2_PUBLIC Options
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// open mode
	Mode                      iMode           { READONLY };
	/// busy timeout for concurrent writers
	std::chrono::milliseconds BusyTimeout     { std::chrono::seconds(10) };
	/// switch the database file to write ahead logging. WAL is persistent on
	/// the file, creates -wal/-shm side files, and is unsuitable for network
	/// mounts. Not applied to READONLY connections.
	bool                      bWAL            { false };
	/// enforce foreign key constraints (sqlite default is off)
	bool                      bForeignKeys    { true  };
	/// allow URI filenames like 'file::memory:?cache=shared'
	bool                      bURIFilename    { false };
	/// maximum count of cached compiled statements, 0 disables caching
	uint16_t                  iStatementCache { 64    };
	/// de/encryption key, applied at connect when non-empty
	String                    sKey;

}; // Options

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a BLOB parameter for binding
///
/// @code
/// db.ExecSQL("insert into files (id, data) values (?1, ?2)",
///            iID, KSQLite::Blob(pBuffer, iSize));
/// @endcode
struct DEKAF2_PUBLIC Blob
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// construct from a pointer to the data and its size in bytes
	Blob(const void* _pData, std::size_t _iSize)
	: pData(_pData), iSize(_iSize) {}

	/// start of the BLOB data
	const void* pData;
	/// size of the BLOB data in bytes
	std::size_t iSize;

}; // Blob

namespace detail {

class DBConnector;

#ifdef DEKAF2
// transparent lookup: find() takes a StringView without constructing a key copy
// (std::hash<KString> hashes KStringView, KUnorderedMap compares heterogeneously)
template<class Value>
using StringMap = KUnorderedMap<String, Value>;
#define KSQLITE_HAS_TRANSPARENT_LOOKUP 1
#elif defined(KSQLITE_HAS_STRING_VIEW) && defined(__cpp_lib_generic_unordered_lookup)
// transparent hash so that find() takes a string_view without a key copy
struct StringViewHash
{
	using is_transparent = void;
	std::size_t operator()(StringView sValue) const noexcept
	{ return std::hash<StringView>{}(sValue); }
};
template<class Value>
using StringMap = std::unordered_map<String, Value, StringViewHash, std::equal_to<>>;
#define KSQLITE_HAS_TRANSPARENT_LOOKUP 1
#elif !defined(KSQLITE_HAS_STRING_VIEW)
// StringView is std::string here - find() takes it directly
template<class Value>
using StringMap = std::unordered_map<String, Value>;
#define KSQLITE_HAS_TRANSPARENT_LOOKUP 1
#else
// C++17 without generic unordered lookup - find() needs a String key
template<class Value>
using StringMap = std::unordered_map<String, Value>;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// exclusive use claim on a connection: reentrant for the owning thread,
/// throws ConcurrencyError when another thread uses the connection at the
/// same time - never blocks
class DEKAF2_PUBLIC Claim
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// constructs an empty claim
	Claim() = default;
	/// claims the connection - throws ConcurrencyError when another thread
	/// holds it
	explicit Claim(DBConnector& Connector);
	Claim(const Claim&) = delete;
	Claim& operator=(const Claim&) = delete;
	/// moving transfers the claim
	Claim(Claim&& other) noexcept
	: m_pConnector(other.m_pConnector) { other.m_pConnector = nullptr; }
	/// moving transfers the claim
	Claim& operator=(Claim&& other) noexcept
	{ Release(); m_pConnector = other.m_pConnector; other.m_pConnector = nullptr; return *this; }
	/// releases the claim
	~Claim() { Release(); }

//----------
private:
//----------

	void Release() noexcept;

	DBConnector* m_pConnector { nullptr };

}; // Claim

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a cached compiled statement of a connection
struct DEKAF2_PUBLIC CacheEntry
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	sqlite3_stmt* pStmt    { nullptr };
	uint64_t      iLastUse { 0 };
	bool          bInUse   { false };

}; // CacheEntry

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// handle for a compiled statement, either from the connection's cache or
/// standalone
struct DEKAF2_PUBLIC StmtHandle
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// constructs an empty handle
	StmtHandle() = default;
	/// constructs a handle from a compiled statement and its cache state
	StmtHandle(sqlite3_stmt* pStmt, bool bCached, CacheEntry* pEntry = nullptr)
	: m_pStmt(pStmt), m_bCached(bCached), m_pEntry(pEntry) {}
	StmtHandle(const StmtHandle&) = delete;
	StmtHandle& operator=(const StmtHandle&) = delete;
	/// moving transfers the statement
	StmtHandle(StmtHandle&& other) noexcept
	: m_pStmt(other.m_pStmt), m_bCached(other.m_bCached), m_pEntry(other.m_pEntry)
	{ other.m_pStmt = nullptr; other.m_pEntry = nullptr; }
	/// moving transfers the statement
	StmtHandle& operator=(StmtHandle&& other) noexcept
	{
		m_pStmt = other.m_pStmt; m_bCached = other.m_bCached; m_pEntry = other.m_pEntry;
		other.m_pStmt = nullptr; other.m_pEntry = nullptr;
		return *this;
	}

	/// the compiled statement
	sqlite3_stmt* Stmt()  const noexcept { return m_pStmt;   }
	/// does the handle hold a statement?
	explicit operator bool() const noexcept { return m_pStmt != nullptr; }

	sqlite3_stmt* m_pStmt   { nullptr };
	bool          m_bCached { false   };
	// the cache entry this handle was taken from - stable address, as the
	// entry cannot get evicted while it is in use
	CacheEntry*   m_pEntry  { nullptr };

}; // StmtHandle

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// internal database connector: one sqlite3 connection plus its statement
/// cache and use claim
class DEKAF2_PUBLIC DBConnector
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	/// open a connection to the database file with the given options
	DBConnector(StringViewZ sFilename, Options Opts);
	/// the copy constructor opens a new connection to the same file
	DBConnector(const DBConnector& other);
	DBConnector(DBConnector&&) = delete;
	/// finalizes all cached statements and closes the connection
	~DBConnector();

	/// is the connection established?
	bool          IsOpen()       const noexcept { return m_DB != nullptr; }
	/// the raw connection handle
	sqlite3*      NativeHandle()       noexcept { return m_DB;            }
	/// name of the database file
	const String& Filename()     const noexcept { return m_sFilename;     }
	/// set a busy timeout (to change the one given at creation)
	bool          SetBusyTimeout(std::chrono::milliseconds Timeout);

	/// compiled statement for this SQL, from the cache when it has earned
	/// admission (second sight) - call under a Claim only
	StmtHandle    GetStatement(StringView sSQL);
	/// return a statement handle: cached ones are reset, others finalized
	void          PutStatement(StmtHandle& Handle) noexcept;

	/// sqlite (extended) error code of the last failed call, 0 when good
	int           LastErrorCode() const noexcept;
	/// error description of the last failed call (an owned copy)
	String        LastError()     const;

	/// count of statement compilations (for tests and diagnostics)
	size_type     Compilations()     const noexcept { return m_iCompilations;    }
	/// count of statement cache hits (for tests and diagnostics)
	size_type     CacheHits()        const noexcept { return m_iCacheHits;       }
	/// count of currently cached statements (for tests and diagnostics)
	size_type     CachedStatements() const noexcept { return m_StmtCache.size(); }

	// transaction bookkeeping - only ever touched under a claim
	uint32_t      m_iTransactionDepth { 0 };

	// connection options
	Options       m_Options;

//----------
private:
//----------

	friend class Claim;

	bool          Connect(StringViewZ sFilename);
	void          ApplyOptions();
	sqlite3_stmt* Compile(StringView sSQL);
	void          ClearStatementCache() noexcept;

	String                                 m_sFilename;
	sqlite3*                               m_DB { nullptr };

	StringMap<CacheEntry>                  m_StmtCache;
	std::vector<uint64_t>                  m_Probation;
	size_type                              m_iProbationIdx  { 0 };
	uint64_t                               m_iUseCounter    { 0 };
	size_type                              m_iCompilations  { 0 };
	size_type                              m_iCacheHits     { 0 };

	std::atomic<std::thread::id>           m_Owner;
	uint32_t                               m_iClaimDepth    { 0 };

}; // DBConnector

// parameter binding - one overload per supported type

DEKAF2_PUBLIC bool BindOne(sqlite3_stmt* pStmt, int iOneBased, std::nullptr_t);
DEKAF2_PUBLIC bool BindOne(sqlite3_stmt* pStmt, int iOneBased, int64_t iValue);
DEKAF2_PUBLIC bool BindOne(sqlite3_stmt* pStmt, int iOneBased, double fValue);
DEKAF2_PUBLIC bool BindOne(sqlite3_stmt* pStmt, int iOneBased, StringView sValue);
DEKAF2_PUBLIC bool BindOne(sqlite3_stmt* pStmt, int iOneBased, const Blob& Value);

inline bool BindOne(sqlite3_stmt* pStmt, int iOneBased, float fValue)
{ return BindOne(pStmt, iOneBased, static_cast<double>(fValue)); }

#ifndef DEKAF2
// with std::string as StringView a const char* would be ambiguous
inline bool BindOne(sqlite3_stmt* pStmt, int iOneBased, const char* sValue)
{ return BindOne(pStmt, iOneBased, StringView(sValue ? sValue : "")); }
#endif

template<class Integer,
         typename std::enable_if<std::is_integral<Integer>::value &&
                                !std::is_same<Integer, int64_t>::value, int>::type = 0>
inline bool BindOne(sqlite3_stmt* pStmt, int iOneBased, Integer iValue)
{ return BindOne(pStmt, iOneBased, static_cast<int64_t>(iValue)); }

inline bool BindAll(sqlite3_stmt*, int)
{ return true; }

template<class Value, class... More>
bool BindAll(sqlite3_stmt* pStmt, int iOneBased, Value&& value, More&&... more)
{
	return BindOne(pStmt, iOneBased, std::forward<Value>(value))
	    && BindAll(pStmt, iOneBased + 1, std::forward<More>(more)...);
}

template<class Tuple, std::size_t... Idx>
bool BindTupleImpl(sqlite3_stmt* pStmt, const Tuple& Values, std::index_sequence<Idx...>)
{
	return BindAll(pStmt, 1, std::get<Idx>(Values)...);
}

template<class Tuple>
bool BindTuple(sqlite3_stmt* pStmt, const Tuple& Values)
{
	return BindTupleImpl(pStmt, Values,
	                     std::make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>{});
}

} // end of namespace detail

/// shared ownership of the internal connector - keeps the connection alive
/// for the lifetime of cursors
using SharedConnector = std::shared_ptr<detail::DBConnector>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// result of a statement execution - carries success, affected rows, last
/// insert id and error state, all captured atomically at execution time
///
/// @code
/// auto Result = db.ExecSQL("insert into user (name) values (?1)", "Jill");
///
/// if (!Result)
/// {
///     kDebug(1, "insert failed: {}", Result.Error());
/// }
/// else
/// {
///     auto iID = Result.LastInsertID();
/// }
/// @endcode
class DEKAF2_PUBLIC ExecResult
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	/// success?
	explicit operator bool()   const noexcept { return m_iErrorCode == 0; }
	/// count of rows affected by this execution
	size_type AffectedRows()   const noexcept { return m_iAffectedRows;   }
	/// row ID inserted by this execution, or 0
	size_type LastInsertID()   const noexcept { return m_iLastInsertID;   }
	/// sqlite (extended) error code, 0 on success
	int       ErrorCode()      const noexcept { return m_iErrorCode;      }
	/// error description, empty on success
	const String& Error()      const noexcept { return m_sError;          }

//----------
private:
//----------

	friend class Database;
	friend class Transaction;

	size_type m_iAffectedRows { 0 };
	size_type m_iLastInsertID { 0 };
	int       m_iErrorCode    { 0 };
	String    m_sError;

}; // ExecResult

class Column;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// one row of a result set: a view into the cursor's current position -
/// materialize values with Get<T>(), do not keep the row past the next
/// advance of its cursor
///
/// @code
/// auto iID   = Row.Get<int64_t>(1);              // by index, typed
/// auto sName = Row.Get<KSQLite::String>("name"); // by name
/// auto iAge  = Row.GetOr<int>("age", -1);        // NULL becomes -1
/// auto sCity = Row.Col("city").String();         // explicit conversion
/// auto fRate = Row[5].Double();                  // same, by index
///
/// if (Row["email"].IsNull()) { ... }
/// @endcode
class DEKAF2_PUBLIC Row
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ColIndex = int;

	/// column count of the result set
	ColIndex    size()  const noexcept { return m_iColumnCount;  }
	/// no columns?
	bool        empty() const noexcept { return size() == 0;     }

	/// value of a column (one based) - T is any integer type, floating point
	/// type, enum, or KSQLite::String (materializes TEXT and BLOB binary
	/// safe). NULL converts to T{}, type conversions follow sqlite rules.
	template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
	T           Get(ColIndex iOneBased) const
	{ return static_cast<T>(GetInt64(iOneBased)); }

	/// value of a column (one based) as an enum
	template<class T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
	T           Get(ColIndex iOneBased) const
	{ return static_cast<T>(GetInt64(iOneBased)); }

	/// value of a column (one based) as a floating point type
	template<class T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
	T           Get(ColIndex iOneBased) const
	{ return static_cast<T>(GetDouble(iOneBased)); }

	/// value of a column (one based) as a String - materializes TEXT and BLOB
	/// binary safe
	template<class T, typename std::enable_if<std::is_same<T, String>::value, int>::type = 0>
	T           Get(ColIndex iOneBased) const
	{ return GetString(iOneBased); }

	/// value of a column by name - unknown names yield T{}
	template<class T>
	T           Get(StringView sColName) const
	{ return Get<T>(GetColIndex(sColName)); }

	/// access to a column (one based) for explicit conversion, e.g.
	/// Row.Col(2).Int64() or Row.Col("name").String()
	Column      Col(ColIndex iOneBased) const;

	/// access to a column by name for explicit conversion
	Column      Col(StringView sColName) const;

	/// access to a column (one based), same as Col()
	Column      operator[](ColIndex iOneBased) const;
	/// access to a column by name, same as Col()
	Column      operator[](StringView sColName) const;

	/// value of a column, or Default when the column is NULL
	template<class T>
	T           GetOr(ColIndex iOneBased, T Default) const
	{ return IsNull(iOneBased) ? std::move(Default) : Get<T>(iOneBased); }

	/// value of a column by name, or Default when the column is NULL
	template<class T>
	T           GetOr(StringView sColName, T Default) const
	{ return GetOr<T>(GetColIndex(sColName), std::move(Default)); }

#ifdef KSQLITE_HAS_OPTIONAL
	/// value of a column, or nullopt when the column is NULL
	template<class T>
	std::optional<T> GetOptional(ColIndex iOneBased) const
	{ if (IsNull(iOneBased)) { return std::nullopt; } return Get<T>(iOneBased); }

	/// value of a column by name, or nullopt when the column is NULL
	template<class T>
	std::optional<T> GetOptional(StringView sColName) const
	{ return GetOptional<T>(GetColIndex(sColName)); }
#endif

	/// is the column NULL?
	bool        IsNull(ColIndex iOneBased) const;
	/// is the column NULL? (by name - unknown names count as NULL)
	bool        IsNull(StringView sColName) const
	{ return IsNull(GetColIndex(sColName)); }
	/// column index for the given name, 0 if unknown
	ColIndex    GetColIndex(StringView sColName) const;
	/// column name (as assigned by an 'as' clause)
	StringViewZ GetColName(ColIndex iOneBased) const;
	/// zero copy view of a TEXT or BLOB column - only valid until the cursor
	/// advances, use Get<String>() to keep the value
	StringView  GetRawView(ColIndex iOneBased) const;

//----------
private:
//----------

	friend class Cursor;

	Row() = default;
	Row(const Row&) = delete;
	Row& operator=(const Row&) = delete;
	Row(Row&&) = default;
	Row& operator=(Row&&) = default;

	int64_t GetInt64 (ColIndex iOneBased) const;
	double  GetDouble(ColIndex iOneBased) const;
	String  GetString(ColIndex iOneBased) const;

	using NameMap = detail::StringMap<ColIndex>;

	sqlite3_stmt*   m_pStmt        { nullptr };
	ColIndex        m_iColumnCount { 0 };
	mutable NameMap m_NameMap;

}; // Row

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// access to the value of one column, for explicit conversion in the style of
/// Row.Col(2).Int64() - a lightweight view with the same lifetime rules as
/// its Row
///
/// @code
/// auto sName = Row.Col("name").String();
/// auto iAge  = Row[2].Int64();
/// @endcode
class DEKAF2_PUBLIC Column
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// value as int32_t
	int32_t          Int32  () const { return m_pRow->Get<int32_t> (m_iCol); }
	/// value as uint32_t
	uint32_t         UInt32 () const { return m_pRow->Get<uint32_t>(m_iCol); }
	/// value as int64_t
	int64_t          Int64  () const { return m_pRow->Get<int64_t> (m_iCol); }
	/// value as uint64_t
	uint64_t         UInt64 () const { return m_pRow->Get<uint64_t>(m_iCol); }
	/// value as double
	double           Double () const { return m_pRow->Get<double>  (m_iCol); }
	/// value as bool (false when 0 or NULL)
	bool             Bool   () const { return m_pRow->Get<bool>    (m_iCol); }
	/// materialized TEXT or BLOB content, binary safe
	KSQLite::String  String () const { return m_pRow->Get<KSQLite::String>(m_iCol); }
	/// is the column NULL?
	bool             IsNull () const { return m_pRow->IsNull(m_iCol);        }

//----------
private:
//----------

	friend class Row;

	Column(const Row& row, Row::ColIndex iOneBased)
	: m_pRow(&row), m_iCol(iOneBased) {}

	const Row*    m_pRow;
	Row::ColIndex m_iCol;

}; // Column

//--------------------------------------------------------------------------------
inline Column Row::Col(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	return Column(*this, iOneBased);
}

//--------------------------------------------------------------------------------
inline Column Row::Col(StringView sColName) const
//--------------------------------------------------------------------------------
{
	return Column(*this, GetColIndex(sColName));
}

//--------------------------------------------------------------------------------
inline Column Row::operator[](ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	return Col(iOneBased);
}

//--------------------------------------------------------------------------------
inline Column Row::operator[](StringView sColName) const
//--------------------------------------------------------------------------------
{
	return Col(sColName);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// single pass cursor over a result set. Claims the connection for its
/// lifetime - keep it scoped, do not store it. Moving a cursor to another
/// thread is not supported.
///
/// @code
/// auto Query = db.ExecQuery("select id, name from user where age > ?1", 21);
///
/// // either iterate with a range based for loop ..
/// for (auto& Row : Query)
/// {
///     auto sName = Row.Col("name").String();
/// }
///
/// // .. or stream row by row (the cursor is single pass - pick one)
/// while (Query.Next())
/// {
///     auto& Row = Query.GetRow();
/// }
/// @endcode
class DEKAF2_PUBLIC Cursor
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// input iterator over the rows, typically used in range based for loops
	class iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		/// constructs an end iterator
		iterator() = default;
		/// constructs a begin or end iterator on a cursor - begin advances to
		/// the first row
		iterator(Cursor& Cursor, bool bToEnd);
		/// advance to the next row
		iterator& operator++();
		/// the current row
		Row& operator*()  const { return  m_pCursor->GetRow(); }
		/// the current row
		Row* operator->() const { return &m_pCursor->GetRow(); }
		/// do both iterators point at the same position?
		friend bool operator==(const iterator& left, const iterator& right) { return left.m_pCursor == right.m_pCursor; }
		/// do the iterators point at different positions?
		friend bool operator!=(const iterator& left, const iterator& right) { return !operator==(left, right);          }

	//----------
	private:
	//----------

		Cursor* m_pCursor { nullptr };

	}; // iterator

	using const_iterator = iterator;

	Cursor(const Cursor&) = delete;
	Cursor& operator=(const Cursor&) = delete;
	/// moves the cursor, including its connection claim
	Cursor(Cursor&& other) noexcept;
	Cursor& operator=(Cursor&&) = delete;
	/// returns the statement to the connection's cache and releases the
	/// connection claim
	~Cursor();

	/// advance to the first resp. next row, returns false when the result
	/// set is exhausted (or on error - check ErrorCode())
	bool Next();
	/// the current row
	Row& GetRow()            noexcept { return m_Row;             }
	/// no more rows to fetch?
	bool Done()        const noexcept { return m_bDone;           }
	/// prepared without error?
	explicit operator bool() const noexcept { return m_iErrorCode == 0; }
	/// sqlite (extended) error code, 0 when good
	int  ErrorCode()   const noexcept { return m_iErrorCode;      }
	/// error description, empty when good
	const String& Error() const noexcept { return m_sError;       }

	/// begin iterator, advances to the first row
	iterator begin() { return iterator(*this, false); }
	/// end iterator
	iterator end()   { return iterator(*this, true);  }

//----------
private:
//----------

	friend class Database;
	friend class Transaction;

	Cursor(SharedConnector Connector, detail::Claim Claim, detail::StmtHandle Handle, bool bMayThrow);

	void SetError(StringView sSQL);

	SharedConnector    m_Connector;
	detail::Claim      m_Claim;
	detail::StmtHandle m_Handle;
	Row                m_Row;
	String             m_sError;
	int                m_iErrorCode { 0 };
	bool               m_bMayThrow  { false };
	bool               m_bStarted   { false };
	bool               m_bDone      { false };

}; // Cursor

class Transaction;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Database connection: executes ad-hoc SQL and parameterized statements,
/// creates cursors over result sets. One connection belongs to one thread at
/// a time - copies open their own connection to the same file.
///
/// @code
/// KSQLite::Database db("/tmp/test.db", KSQLite::Mode::READWRITECREATE);
///
/// db.ExecSQL("create table if not exists user ("
///            "    id   integer primary key,"
///            "    name text    not null,"
///            "    age  integer null"
///            ")");
///
/// auto Result = db.ExecSQL("insert into user (name, age) values (?1, ?2)", "Jill", 28);
/// auto iID    = Result.LastInsertID();
///
/// for (auto& Row : db.ExecQuery("select id, name from user where age > ?1", 21))
/// {
///     auto iID   = Row.Get<int64_t>(1);
///     auto sName = Row.Col("name").String();
///     auto iAge  = Row["age"].UInt32();
/// }
///
/// auto iCount = db.SingleIntQuery("select count(*) from user");
///
/// {
///     KSQLite::Transaction Txn(db);
///     Txn.ExecSQL("update user set age=age+1 where id=?1", iID);
///     Txn.Commit(); // without Commit() the destructor rolls back
/// }
/// @endcode
class DEKAF2_PUBLIC Database
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	/// open database file with the given options
	Database(StringViewZ sFilename, Options Opts);
	/// open database file with the given mode and default options
	Database(StringViewZ sFilename, Mode iMode = Mode::READONLY);
	/// the copy constructor opens a new connection to the same file
	Database(const Database& other);
	/// the move constructor takes over the connection
	Database(Database&&) = default;
	/// copy assignment opens a new connection to the same file
	Database& operator=(const Database& other);
	/// move assignment takes over the connection
	Database& operator=(Database&&) = default;

	/// is the connection established?
	bool IsOpen() const noexcept;

	/// execute a statement without result set, binding the given parameters.
	/// Without parameters the SQL may contain multiple statements (which then
	/// bypass the statement cache).
	template<class... Args>
	ExecResult ExecSQL(StringView sSQL, Args&&... args);

	/// execute a query and return a cursor over the result set
	template<class... Args>
	DEKAF2_NODISCARD
	Cursor ExecQuery(StringView sSQL, Args&&... args);

	/// execute one statement for every tuple of parameters in the container,
	/// all inside a single transaction
	template<class Parameters>
	ExecResult ExecMany(StringView sSQL, const Parameters& Params);

	/// first column of the first result row as integer, 0 if none
	template<class... Args>
	int64_t SingleIntQuery(StringView sSQL, Args&&... args);

	/// first column of the first result row as string, empty if none
	template<class... Args>
	String SingleStringQuery(StringView sSQL, Args&&... args);

	/// does a table with the given name exist?
	bool HasTable(StringViewZ sTable);
	/// name of the database file
	const String& Filename() const noexcept;
	/// set a busy timeout (to change the one given at creation)
	bool SetBusyTimeout(std::chrono::milliseconds BusyTimeout);
	/// set de/encryption key for the current database
	bool Key(StringView sKey);
	/// set a new key, a valid Key() must have been set before
	bool Rekey(StringView sKey);
	/// does the database in the given file need a Key()?
	static bool IsEncrypted(StringViewZ sFilename);
	/// does the current database need a Key()?
	bool IsEncrypted() const { return IsEncrypted(StringViewZ(Filename())); }
	/// allow SQL errors to throw Exception. Returns previous throw status
	bool SetThrow(bool bYesNo) noexcept { std::swap(m_bMayThrow, bYesNo); return bYesNo; }
	/// the raw connection handle
	sqlite3* NativeHandle() noexcept;

	/// count of statement compilations on this connection (for tests and diagnostics)
	size_type Compilations()     const noexcept;
	/// count of statement cache hits on this connection (for tests and diagnostics)
	size_type CacheHits()        const noexcept;
	/// count of currently cached statements (for tests and diagnostics)
	size_type CachedStatements() const noexcept;

//----------
private:
//----------

	friend class Transaction;

	const SharedConnector& Connector() const noexcept { return m_Connector; }

	ExecResult NoConnection    ();
	ExecResult PrepareError    (StringView sSQL);
	ExecResult BindError       (StringView sSQL);
	ExecResult IntExecSQL      (StringView sSQL);
	ExecResult ExecHandle      (detail::StmtHandle& Handle, bool bKeepHandle);
	Cursor     MakeErrorCursor ();

	SharedConnector m_Connector;
	bool            m_bMayThrow { false };

}; // Database

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RAII transaction: claims the connection for its scope and rolls back on
/// destruction unless committed. Inside an open transaction a new Transaction
/// nests as a savepoint. Statements from other threads throw ConcurrencyError
/// for the duration of the transaction instead of silently joining it.
///
/// @code
/// {
///     KSQLite::Transaction Txn(db);
///
///     Txn.ExecSQL("insert into user (name) values (?1)", "Jack");
///     Txn.ExecSQL("insert into user (name) values (?1)", "Jill");
///
///     Txn.Commit(); // without Commit() the destructor rolls back
/// }
/// @endcode
class DEKAF2_PUBLIC Transaction
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// how the transaction takes its locks
	enum Kind
	{
		/// take locks lazily on first read resp. write
		DEFERRED,
		/// take the write lock immediately (the right choice with concurrent
		/// writers - waits on the busy timeout instead of failing later)
		IMMEDIATE,
		/// lock out readers as well
		EXCLUSIVE
	};

	/// start a transaction (or a savepoint when nested)
	explicit Transaction(Database& db, Kind iKind = IMMEDIATE);
	Transaction(const Transaction&) = delete;
	Transaction(Transaction&&) = delete;
	Transaction& operator=(const Transaction&) = delete;
	Transaction& operator=(Transaction&&) = delete;
	/// rolls back unless committed
	~Transaction();

	/// execute a statement inside the transaction
	template<class... Args>
	ExecResult ExecSQL(StringView sSQL, Args&&... args)
	{ return m_DB.ExecSQL(sSQL, std::forward<Args>(args)...); }

	/// execute a query inside the transaction
	template<class... Args>
	DEKAF2_NODISCARD
	Cursor ExecQuery(StringView sSQL, Args&&... args)
	{ return m_DB.ExecQuery(sSQL, std::forward<Args>(args)...); }

	/// commit the transaction (or release the savepoint) - further calls no-op
	bool Commit();
	/// is this transaction a nested savepoint?
	bool IsNested() const noexcept { return m_iDepth > 0; }
	/// did the transaction begin successfully? May be false when another process
	/// holds the write lock for longer than the busy timeout.
	bool IsOpen() const noexcept { return m_bOpen; }

//----------
private:
//----------

	String SavepointName() const;

	Database&     m_DB;
	detail::Claim m_Claim;
	Kind          m_iKind      { IMMEDIATE };
	uint32_t      m_iDepth     { 0 };
	bool          m_bOpen      { false };
	bool          m_bCommitted { false };

}; // Transaction

//================================= inline implementation =================================

//--------------------------------------------------------------------------------
template<class... Args>
ExecResult Database::ExecSQL(StringView sSQL, Args&&... args)
//--------------------------------------------------------------------------------
{
	if (!m_Connector)
	{
		return NoConnection();
	}

	detail::Claim Claim(*m_Connector);

	if (sizeof...(Args) == 0)
	{
		// no parameters: allow multiple statements, bypass the statement cache
		return IntExecSQL(sSQL);
	}

	auto Handle = m_Connector->GetStatement(sSQL);

	if (!Handle)
	{
		return PrepareError(sSQL);
	}

	if (!detail::BindAll(Handle.Stmt(), 1, std::forward<Args>(args)...))
	{
		auto Result = BindError(sSQL);
		m_Connector->PutStatement(Handle);
		return Result;
	}

	return ExecHandle(Handle, false);

} // Database::ExecSQL

//--------------------------------------------------------------------------------
template<class... Args>
Cursor Database::ExecQuery(StringView sSQL, Args&&... args)
//--------------------------------------------------------------------------------
{
	if (!m_Connector)
	{
		NoConnection(); // throws when enabled
		return MakeErrorCursor();
	}

	detail::Claim Claim(*m_Connector);

	auto Handle = m_Connector->GetStatement(sSQL);

	Cursor Result(m_Connector, std::move(Claim), std::move(Handle), m_bMayThrow);

	if (!Result.m_Handle)
	{
		Result.SetError(sSQL);
	}
	else if (!detail::BindAll(Result.m_Handle.Stmt(), 1, std::forward<Args>(args)...))
	{
		Result.SetError(sSQL);
	}

	return Result;

} // Database::ExecQuery

//--------------------------------------------------------------------------------
template<class Parameters>
ExecResult Database::ExecMany(StringView sSQL, const Parameters& Params)
//--------------------------------------------------------------------------------
{
	if (!m_Connector)
	{
		return NoConnection();
	}

	detail::Claim Claim(*m_Connector);

	Transaction Txn(*this);

	auto Handle = m_Connector->GetStatement(sSQL);

	if (!Handle)
	{
		return PrepareError(sSQL);
	}

	ExecResult Total;

	for (const auto& Tuple : Params)
	{
		if (!detail::BindTuple(Handle.Stmt(), Tuple))
		{
			Total = BindError(sSQL);
			break;
		}

		auto Result = ExecHandle(Handle, true);

		if (!Result)
		{
			Total = std::move(Result);
			break;
		}

		Total.m_iAffectedRows += Result.AffectedRows();
		Total.m_iLastInsertID  = Result.LastInsertID();
	}

	m_Connector->PutStatement(Handle);

	if (Total)
	{
		Txn.Commit();
	}

	return Total;

} // Database::ExecMany

//--------------------------------------------------------------------------------
template<class... Args>
int64_t Database::SingleIntQuery(StringView sSQL, Args&&... args)
//--------------------------------------------------------------------------------
{
	auto Query = ExecQuery(sSQL, std::forward<Args>(args)...);
	return Query.Next() ? Query.GetRow().template Get<int64_t>(1) : 0;

} // Database::SingleIntQuery

//--------------------------------------------------------------------------------
template<class... Args>
String Database::SingleStringQuery(StringView sSQL, Args&&... args)
//--------------------------------------------------------------------------------
{
	auto Query = ExecQuery(sSQL, std::forward<Args>(args)...);
	return Query.Next() ? Query.GetRow().template Get<String>(1) : String{};

} // Database::SingleStringQuery

} // end of namespace KSQLite

#ifdef DEKAF2

/// @}

DEKAF2_NAMESPACE_END
#endif
