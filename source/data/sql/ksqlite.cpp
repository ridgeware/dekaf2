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

#include <dekaf2/data/sql/ksqlite.h>
#include <fstream>
#include <cstring>
#include <functional>
#include <sqlite3.h>

#ifdef DEKAF2
#include <dekaf2/core/logging/klog.h>
DEKAF2_NAMESPACE_BEGIN
#endif

namespace KSQLite {

//--------------------------------------------------------------------------------
inline bool Success(int ec)
//--------------------------------------------------------------------------------
{
	return (ec == SQLITE_OK);
}

//--------------------------------------------------------------------------------
inline uint64_t HashSQL(StringView sSQL)
//--------------------------------------------------------------------------------
{
#ifdef DEKAF2
	auto iHash = static_cast<uint64_t>(sSQL.Hash());
#else
	auto iHash = static_cast<uint64_t>(std::hash<StringView>{}(sSQL));
#endif
	// 0 is the empty slot marker of the probation ring
	return iHash ? iHash : 1;
}

//=================================== Claim ======================================

//--------------------------------------------------------------------------------
detail::Claim::Claim(DBConnector& Connector)
//--------------------------------------------------------------------------------
	: m_pConnector(&Connector)
{
	auto Me = std::this_thread::get_id();

	if (Connector.m_Owner.load(std::memory_order_acquire) == Me)
	{
		// reentrant claim by the owning thread
		++Connector.m_iClaimDepth;
		return;
	}

	std::thread::id None;

	if (!Connector.m_Owner.compare_exchange_strong(None, Me, std::memory_order_acq_rel))
	{
		m_pConnector = nullptr;
		throw ConcurrencyError("KSQLite: simultaneous use of one connection from multiple threads"
		                       " - use one Database per thread (copies open their own connection)");
	}

	Connector.m_iClaimDepth = 1;

} // ctor Claim

//--------------------------------------------------------------------------------
void detail::Claim::Release() noexcept
//--------------------------------------------------------------------------------
{
	if (m_pConnector)
	{
		if (--m_pConnector->m_iClaimDepth == 0)
		{
			m_pConnector->m_Owner.store(std::thread::id(), std::memory_order_release);
		}
		m_pConnector = nullptr;
	}

} // Claim::Release

//================================= DBConnector ==================================

//--------------------------------------------------------------------------------
detail::DBConnector::DBConnector(StringViewZ sFilename, Options Opts)
//--------------------------------------------------------------------------------
	: m_Options(std::move(Opts))
{
	Connect(sFilename);

} // ctor DBConnector

//--------------------------------------------------------------------------------
detail::DBConnector::DBConnector(const DBConnector& other)
//--------------------------------------------------------------------------------
	: m_Options(other.m_Options)
{
	Connect(StringViewZ(other.m_sFilename));

} // copy ctor DBConnector

//--------------------------------------------------------------------------------
detail::DBConnector::~DBConnector()
//--------------------------------------------------------------------------------
{
	ClearStatementCache();
	sqlite3_close(m_DB);

} // dtor DBConnector

//--------------------------------------------------------------------------------
bool detail::DBConnector::Connect(StringViewZ sFilename)
//--------------------------------------------------------------------------------
{
	if (m_DB != nullptr)
	{
		return false;
	}

	m_sFilename = String(sFilename);

	int iFlags;

	switch (m_Options.iMode)
	{
		default:
		case READONLY:
			iFlags = SQLITE_OPEN_READONLY;
			break;

		case READWRITE:
			iFlags = SQLITE_OPEN_READWRITE;
			break;

		case READWRITECREATE:
			iFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			break;
	}

	if (m_Options.bURIFilename)
	{
		iFlags |= SQLITE_OPEN_URI;
	}

	auto ec = sqlite3_open_v2(sFilename.c_str(), &m_DB, iFlags, nullptr);

	if (!Success(ec))
	{
#ifdef DEKAF2
		kDebug(1, "error: {}", sqlite3_errstr(ec));
#endif
		sqlite3_close(m_DB);
		m_DB = nullptr;
		return false;
	}

	ApplyOptions();

	return true;

} // Connect

//--------------------------------------------------------------------------------
void detail::DBConnector::ApplyOptions()
//--------------------------------------------------------------------------------
{
	sqlite3_busy_timeout(m_DB, static_cast<int>(m_Options.BusyTimeout.count()));

#ifdef SQLITE_HAS_CODEC
	if (!m_Options.sKey.empty())
	{
		sqlite3_key(m_DB, m_Options.sKey.data(), static_cast<int>(m_Options.sKey.size()));
	}
#endif

	if (m_Options.bForeignKeys)
	{
		sqlite3_exec(m_DB, "PRAGMA foreign_keys=ON", nullptr, nullptr, nullptr);
	}

	if (m_Options.bWAL && m_Options.iMode != READONLY)
	{
		sqlite3_exec(m_DB, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
	}

	m_Probation.assign(m_Options.iStatementCache ? m_Options.iStatementCache * std::size_t(2) : 0, 0);

} // ApplyOptions

//--------------------------------------------------------------------------------
bool detail::DBConnector::SetBusyTimeout(std::chrono::milliseconds Timeout)
//--------------------------------------------------------------------------------
{
	m_Options.BusyTimeout = Timeout;

	if (!m_DB)
	{
		return false;
	}

	return Success(sqlite3_busy_timeout(m_DB, static_cast<int>(Timeout.count())));

} // SetBusyTimeout

//--------------------------------------------------------------------------------
sqlite3_stmt* detail::DBConnector::Compile(StringView sSQL)
//--------------------------------------------------------------------------------
{
	sqlite3_stmt* pStmt = nullptr;

	auto ec = sqlite3_prepare_v2(m_DB, sSQL.data(), static_cast<int>(sSQL.size()), &pStmt, nullptr);

	if (!Success(ec))
	{
#ifdef DEKAF2
		kDebug(1, "error: {}", sqlite3_errstr(ec));
		kDebug(1, sSQL);
#endif
		return nullptr;
	}

	if (pStmt)
	{
		++m_iCompilations;
	}

	// pStmt is null for whitespace-only SQL
	return pStmt;

} // Compile

//--------------------------------------------------------------------------------
detail::StmtHandle detail::DBConnector::GetStatement(StringView sSQL)
//--------------------------------------------------------------------------------
{
	if (!m_DB)
	{
		return {};
	}

	if (m_Options.iStatementCache == 0)
	{
		return { Compile(sSQL), false };
	}

#ifdef KSQLITE_HAS_TRANSPARENT_LOOKUP
	// no key copy on the hot path
	auto it = m_StmtCache.find(sSQL);
#else
	auto it = m_StmtCache.find(String(sSQL));
#endif

	if (it != m_StmtCache.end())
	{
		if (!it->second.bInUse)
		{
			it->second.bInUse   = true;
			it->second.iLastUse = ++m_iUseCounter;
			++m_iCacheHits;
			return { it->second.pStmt, true, &it->second };
		}

		// the same SQL is nested inside an open cursor - compile a second,
		// uncached instance
		return { Compile(sSQL), false };
	}

	auto* pStmt = Compile(sSQL);

	if (!pStmt)
	{
		return {};
	}

	// admission on second sight: only a text whose hash is already in the
	// probation ring gets cached - one-off SQL cannot flood the cache
	auto iHash = HashSQL(sSQL);
	bool bSeenBefore = false;

	for (auto& iSlot : m_Probation)
	{
		if (iSlot == iHash)
		{
			iSlot = 0;
			bSeenBefore = true;
			break;
		}
	}

	if (!bSeenBefore)
	{
		if (!m_Probation.empty())
		{
			m_Probation[m_iProbationIdx] = iHash;
			m_iProbationIdx = (m_iProbationIdx + 1) % m_Probation.size();
		}
		return { pStmt, false };
	}

	if (m_StmtCache.size() >= m_Options.iStatementCache)
	{
		// evict the least recently used entry that is not in use
		auto itEvict = m_StmtCache.end();

		for (auto itc = m_StmtCache.begin(); itc != m_StmtCache.end(); ++itc)
		{
			if (!itc->second.bInUse &&
				(itEvict == m_StmtCache.end() || itc->second.iLastUse < itEvict->second.iLastUse))
			{
				itEvict = itc;
			}
		}

		if (itEvict == m_StmtCache.end())
		{
			// all cached statements are held by open cursors - run uncached
			return { pStmt, false };
		}

		sqlite3_finalize(itEvict->second.pStmt);
		m_StmtCache.erase(itEvict);
	}

	CacheEntry Entry;
	Entry.pStmt    = pStmt;
	Entry.iLastUse = ++m_iUseCounter;
	Entry.bInUse   = true;

	auto Inserted = m_StmtCache.emplace(String(sSQL), Entry);

	return { pStmt, true, &Inserted.first->second };

} // GetStatement

//--------------------------------------------------------------------------------
void detail::DBConnector::PutStatement(StmtHandle& Handle) noexcept
//--------------------------------------------------------------------------------
{
	if (!Handle.m_pStmt)
	{
		return;
	}

	if (Handle.m_bCached)
	{
		sqlite3_reset(Handle.m_pStmt);
		sqlite3_clear_bindings(Handle.m_pStmt);

		if (Handle.m_pEntry)
		{
			Handle.m_pEntry->bInUse = false;
		}
	}
	else
	{
		sqlite3_finalize(Handle.m_pStmt);
	}

	Handle.m_pStmt  = nullptr;
	Handle.m_pEntry = nullptr;

} // PutStatement

//--------------------------------------------------------------------------------
void detail::DBConnector::ClearStatementCache() noexcept
//--------------------------------------------------------------------------------
{
	for (auto& Entry : m_StmtCache)
	{
		sqlite3_finalize(Entry.second.pStmt);
	}

	m_StmtCache.clear();

} // ClearStatementCache

//--------------------------------------------------------------------------------
int detail::DBConnector::LastErrorCode() const noexcept
//--------------------------------------------------------------------------------
{
	return m_DB ? sqlite3_extended_errcode(m_DB) : SQLITE_CANTOPEN;
}

//--------------------------------------------------------------------------------
String detail::DBConnector::LastError() const
//--------------------------------------------------------------------------------
{
	if (!m_DB)
	{
		return String("no database connection");
	}

	auto* sMessage = sqlite3_errmsg(m_DB);

	return String(sMessage ? sMessage : "");

} // LastError

//================================== binding =====================================

//--------------------------------------------------------------------------------
bool detail::BindOne(sqlite3_stmt* pStmt, int iOneBased, std::nullptr_t)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_null(pStmt, iOneBased));
}

//--------------------------------------------------------------------------------
bool detail::BindOne(sqlite3_stmt* pStmt, int iOneBased, int64_t iValue)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_int64(pStmt, iOneBased, iValue));
}

//--------------------------------------------------------------------------------
bool detail::BindOne(sqlite3_stmt* pStmt, int iOneBased, double fValue)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_double(pStmt, iOneBased, fValue));
}

//--------------------------------------------------------------------------------
bool detail::BindOne(sqlite3_stmt* pStmt, int iOneBased, StringView sValue)
//--------------------------------------------------------------------------------
{
	// a default constructed string view has data() == nullptr, and
	// sqlite3_bind_text() binds SQL NULL for a null pointer - bind the empty
	// string instead; callers that want SQL NULL bind nullptr
	return Success(sqlite3_bind_text(pStmt, iOneBased,
	                                 sValue.data() ? sValue.data() : "",
	                                 static_cast<int>(sValue.size()),
	                                 SQLITE_TRANSIENT));
}

//--------------------------------------------------------------------------------
bool detail::BindOne(sqlite3_stmt* pStmt, int iOneBased, const Blob& Value)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_blob(pStmt, iOneBased, Value.pData,
	                                 static_cast<int>(Value.iSize), SQLITE_TRANSIENT));
}

//===================================== Row ======================================

//--------------------------------------------------------------------------------
inline bool ValidColumn(sqlite3_stmt* pStmt, Row::ColIndex iOneBased, Row::ColIndex iColumnCount)
//--------------------------------------------------------------------------------
{
	return pStmt && iOneBased >= 1 && iOneBased <= iColumnCount;
}

//--------------------------------------------------------------------------------
int64_t Row::GetInt64(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	return ValidColumn(m_pStmt, iOneBased, m_iColumnCount)
	    ? sqlite3_column_int64(m_pStmt, iOneBased - 1)
	    : 0;
}

//--------------------------------------------------------------------------------
double Row::GetDouble(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	return ValidColumn(m_pStmt, iOneBased, m_iColumnCount)
	    ? sqlite3_column_double(m_pStmt, iOneBased - 1)
	    : 0.0;
}

//--------------------------------------------------------------------------------
String Row::GetString(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	if (!ValidColumn(m_pStmt, iOneBased, m_iColumnCount))
	{
		return String();
	}

	// sqlite3_column_blob returns TEXT and BLOB content unmodified and NUL safe,
	// other types are converted to their text representation first
	auto* pData = sqlite3_column_blob (m_pStmt, iOneBased - 1);
	auto  iSize = sqlite3_column_bytes(m_pStmt, iOneBased - 1);

	return pData ? String(static_cast<const char*>(pData), iSize) : String();

} // Row::GetString

//--------------------------------------------------------------------------------
StringView Row::GetRawView(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	if (!ValidColumn(m_pStmt, iOneBased, m_iColumnCount))
	{
		return StringView();
	}

	auto* pData = sqlite3_column_blob (m_pStmt, iOneBased - 1);
	auto  iSize = sqlite3_column_bytes(m_pStmt, iOneBased - 1);

	return pData ? StringView(static_cast<const char*>(pData), iSize) : StringView();

} // Row::GetRawView

//--------------------------------------------------------------------------------
bool Row::IsNull(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	return !ValidColumn(m_pStmt, iOneBased, m_iColumnCount)
	    || sqlite3_column_type(m_pStmt, iOneBased - 1) == SQLITE_NULL;
}

//--------------------------------------------------------------------------------
Row::ColIndex Row::GetColIndex(StringView sColName) const
//--------------------------------------------------------------------------------
{
	if (m_NameMap.empty() && m_pStmt)
	{
		for (ColIndex iCol = 0; iCol < m_iColumnCount; ++iCol)
		{
			auto* sName = sqlite3_column_name(m_pStmt, iCol);
			m_NameMap.emplace(String(sName ? sName : ""), iCol + 1);
		}
	}

#ifdef KSQLITE_HAS_TRANSPARENT_LOOKUP
	auto it = m_NameMap.find(sColName);
#else
	auto it = m_NameMap.find(String(sColName));
#endif

	return (it != m_NameMap.end()) ? it->second : 0;

} // Row::GetColIndex

//--------------------------------------------------------------------------------
StringViewZ Row::GetColName(ColIndex iOneBased) const
//--------------------------------------------------------------------------------
{
	if (ValidColumn(m_pStmt, iOneBased, m_iColumnCount))
	{
		auto* sName = sqlite3_column_name(m_pStmt, iOneBased - 1);

		if (sName)
		{
			return StringViewZ(sName);
		}
	}

	return StringViewZ();

} // Row::GetColName

//==================================== Cursor ====================================

//--------------------------------------------------------------------------------
Cursor::Cursor(SharedConnector Connector, detail::Claim Claim, detail::StmtHandle Handle, bool bMayThrow)
//--------------------------------------------------------------------------------
	: m_Connector (std::move(Connector))
	, m_Claim     (std::move(Claim))
	, m_Handle    (std::move(Handle))
	, m_bMayThrow (bMayThrow)
{
	if (m_Handle)
	{
		m_Row.m_pStmt        = m_Handle.Stmt();
		m_Row.m_iColumnCount = sqlite3_column_count(m_Handle.Stmt());
	}
	else
	{
		m_bDone = true;
	}

} // ctor Cursor

//--------------------------------------------------------------------------------
Cursor::Cursor(Cursor&& other) noexcept
//--------------------------------------------------------------------------------
	: m_Connector  (std::move(other.m_Connector))
	, m_Claim      (std::move(other.m_Claim))
	, m_Handle     (std::move(other.m_Handle))
	, m_Row        (std::move(other.m_Row))
	, m_sError     (std::move(other.m_sError))
	, m_iErrorCode (other.m_iErrorCode)
	, m_bMayThrow  (other.m_bMayThrow)
	, m_bStarted   (other.m_bStarted)
	, m_bDone      (other.m_bDone)
{
	other.m_Row.m_pStmt        = nullptr;
	other.m_Row.m_iColumnCount = 0;
	other.m_bDone              = true;

} // move ctor Cursor

//--------------------------------------------------------------------------------
Cursor::~Cursor()
//--------------------------------------------------------------------------------
{
	if (m_Handle && m_Connector)
	{
		m_Connector->PutStatement(m_Handle);
	}
	// the claim is released by its own destructor after the statement returned

} // dtor Cursor

//--------------------------------------------------------------------------------
void Cursor::SetError(StringView sSQL)
//--------------------------------------------------------------------------------
{
	m_iErrorCode = m_Connector ? m_Connector->LastErrorCode() : SQLITE_CANTOPEN;
	m_sError     = m_Connector ? m_Connector->LastError() : String("no database connection");

	if (m_iErrorCode == 0)
	{
		m_iErrorCode = SQLITE_ERROR;
		m_sError     = String("empty statement");
	}

#ifdef DEKAF2
	kDebug(1, "error: {}", m_sError);
	kDebug(1, sSQL);
#endif

	if (m_Handle && m_Connector)
	{
		m_Connector->PutStatement(m_Handle);
	}

	m_Row.m_pStmt        = nullptr;
	m_Row.m_iColumnCount = 0;
	m_bDone              = true;

	if (m_bMayThrow)
	{
		throw Exception(m_sError);
	}

} // Cursor::SetError

//--------------------------------------------------------------------------------
bool Cursor::Next()
//--------------------------------------------------------------------------------
{
	if (!m_Handle || m_bDone)
	{
		return false;
	}

	auto ec = sqlite3_step(m_Handle.Stmt());

	m_bStarted = true;

	if (ec == SQLITE_ROW)
	{
		return true;
	}

	m_bDone = true;

	if (ec != SQLITE_DONE)
	{
		m_iErrorCode = m_Connector->LastErrorCode();
		m_sError     = m_Connector->LastError();
#ifdef DEKAF2
		kDebug(1, "error: {}", m_sError);
#endif
		if (m_bMayThrow)
		{
			throw Exception(m_sError);
		}
	}

	return false;

} // Cursor::Next

//--------------------------------------------------------------------------------
Cursor::iterator::iterator(Cursor& Cursor, bool bToEnd)
//--------------------------------------------------------------------------------
	: m_pCursor(bToEnd ? nullptr : &Cursor)
{
	if (m_pCursor)
	{
		if (!m_pCursor->m_bStarted)
		{
			if (!m_pCursor->Next())
			{
				m_pCursor = nullptr;
			}
		}
		else if (m_pCursor->Done())
		{
			m_pCursor = nullptr;
		}
	}

} // ctor Cursor::iterator

//--------------------------------------------------------------------------------
Cursor::iterator& Cursor::iterator::operator++()
//--------------------------------------------------------------------------------
{
	if (m_pCursor && !m_pCursor->Next())
	{
		m_pCursor = nullptr;
	}

	return *this;

} // Cursor::iterator::operator++

//=================================== Database ===================================

//--------------------------------------------------------------------------------
Database::Database(StringViewZ sFilename, Options Opts)
//--------------------------------------------------------------------------------
	: m_Connector(std::make_shared<detail::DBConnector>(sFilename, std::move(Opts)))
{
} // ctor Database

//--------------------------------------------------------------------------------
Database::Database(StringViewZ sFilename, Mode iMode)
//--------------------------------------------------------------------------------
{
	Options Opts;
	Opts.iMode  = iMode;
	m_Connector = std::make_shared<detail::DBConnector>(sFilename, std::move(Opts));

} // ctor Database

//--------------------------------------------------------------------------------
Database::Database(const Database& other)
//--------------------------------------------------------------------------------
	: m_Connector(other.m_Connector ? std::make_shared<detail::DBConnector>(*other.m_Connector) : nullptr)
	, m_bMayThrow(other.m_bMayThrow)
{
} // copy ctor Database

//--------------------------------------------------------------------------------
Database& Database::operator=(const Database& other)
//--------------------------------------------------------------------------------
{
	m_Connector = other.m_Connector ? std::make_shared<detail::DBConnector>(*other.m_Connector) : nullptr;
	m_bMayThrow = other.m_bMayThrow;
	return *this;

} // Database::operator=

//--------------------------------------------------------------------------------
bool Database::IsOpen() const noexcept
//--------------------------------------------------------------------------------
{
	return m_Connector && m_Connector->IsOpen();
}

//--------------------------------------------------------------------------------
ExecResult Database::NoConnection()
//--------------------------------------------------------------------------------
{
	ExecResult Result;
	Result.m_iErrorCode = SQLITE_CANTOPEN;
	Result.m_sError     = String("no database connection");

	if (m_bMayThrow)
	{
		throw Exception(Result.m_sError);
	}

	return Result;

} // Database::NoConnection

//--------------------------------------------------------------------------------
ExecResult Database::PrepareError(StringView sSQL)
//--------------------------------------------------------------------------------
{
	ExecResult Result;
	Result.m_iErrorCode = m_Connector->LastErrorCode();
	Result.m_sError     = m_Connector->LastError();

	if (Result.m_iErrorCode == 0)
	{
		Result.m_iErrorCode = SQLITE_ERROR;
		Result.m_sError     = String("empty statement");
	}

#ifdef DEKAF2
	kDebug(1, "error: {}", Result.m_sError);
	kDebug(1, sSQL);
#endif

	if (m_bMayThrow)
	{
		throw Exception(Result.m_sError);
	}

	return Result;

} // Database::PrepareError

//--------------------------------------------------------------------------------
ExecResult Database::BindError(StringView sSQL)
//--------------------------------------------------------------------------------
{
	ExecResult Result;
	Result.m_iErrorCode = m_Connector->LastErrorCode();
	Result.m_sError     = m_Connector->LastError();

	if (Result.m_iErrorCode == 0)
	{
		Result.m_iErrorCode = SQLITE_ERROR;
		Result.m_sError     = String("parameter bind failed");
	}

#ifdef DEKAF2
	kDebug(1, "error: {}", Result.m_sError);
	kDebug(1, sSQL);
#endif

	if (m_bMayThrow)
	{
		throw Exception(Result.m_sError);
	}

	return Result;

} // Database::BindError

//--------------------------------------------------------------------------------
Cursor Database::MakeErrorCursor()
//--------------------------------------------------------------------------------
{
	Cursor Result(nullptr, detail::Claim(), detail::StmtHandle(), m_bMayThrow);

	Result.m_iErrorCode = SQLITE_CANTOPEN;
	Result.m_sError     = String("no database connection");

	return Result;

} // Database::MakeErrorCursor

//--------------------------------------------------------------------------------
ExecResult Database::ExecHandle(detail::StmtHandle& Handle, bool bKeepHandle)
//--------------------------------------------------------------------------------
{
	ExecResult Result;

	int ec;

	do
	{
		ec = sqlite3_step(Handle.Stmt());
	}
	while (ec == SQLITE_ROW);

	if (ec != SQLITE_DONE)
	{
		Result.m_iErrorCode = m_Connector->LastErrorCode();
		Result.m_sError     = m_Connector->LastError();
#ifdef DEKAF2
		kDebug(1, "error: {}", Result.m_sError);
#endif
	}
	else
	{
		auto* db = m_Connector->NativeHandle();
		Result.m_iAffectedRows = static_cast<ExecResult::size_type>(sqlite3_changes(db));
		Result.m_iLastInsertID = static_cast<ExecResult::size_type>(sqlite3_last_insert_rowid(db));
	}

	if (bKeepHandle)
	{
		sqlite3_reset(Handle.Stmt());
	}
	else
	{
		m_Connector->PutStatement(Handle);
	}

	if (!Result && m_bMayThrow)
	{
		throw Exception(Result.m_sError);
	}

	return Result;

} // Database::ExecHandle

//--------------------------------------------------------------------------------
ExecResult Database::IntExecSQL(StringView sSQL)
//--------------------------------------------------------------------------------
{
	ExecResult Result;

	auto* db = m_Connector->NativeHandle();

	if (!db)
	{
		return NoConnection();
	}

	sqlite3_stmt* pStmt = nullptr;
	const char*   pTail = sSQL.data();
	const char*   pEnd  = pTail + sSQL.size();

	// sqlite3_prepare_v2 parses the first statement and advances pTail past it,
	// so the loop handles multi-statement SQL without manual ';' splitting
	while (pTail < pEnd)
	{
		auto ec = sqlite3_prepare_v2(db, pTail, static_cast<int>(pEnd - pTail), &pStmt, &pTail);

		if (!Success(ec))
		{
			Result.m_iErrorCode = m_Connector->LastErrorCode();
			Result.m_sError     = m_Connector->LastError();
#ifdef DEKAF2
			kDebug(1, "error: {}", Result.m_sError);
			kDebug(1, sSQL);
#endif
			break;
		}

		if (!pStmt)
		{
			// null when input contained only whitespace or comments
			break;
		}

		do
		{
			ec = sqlite3_step(pStmt);
		}
		while (ec == SQLITE_ROW);

		sqlite3_finalize(pStmt);

		if (ec != SQLITE_DONE)
		{
			Result.m_iErrorCode = m_Connector->LastErrorCode();
			Result.m_sError     = m_Connector->LastError();
#ifdef DEKAF2
			kDebug(1, "error: {}", Result.m_sError);
			kDebug(1, sSQL);
#endif
			break;
		}

		Result.m_iAffectedRows = static_cast<ExecResult::size_type>(sqlite3_changes(db));
		Result.m_iLastInsertID = static_cast<ExecResult::size_type>(sqlite3_last_insert_rowid(db));
	}

	if (!Result && m_bMayThrow)
	{
		throw Exception(Result.m_sError);
	}

	return Result;

} // Database::IntExecSQL

//--------------------------------------------------------------------------------
bool Database::HasTable(StringViewZ sTable)
//--------------------------------------------------------------------------------
{
	return SingleIntQuery("select count(*) from sqlite_master where type='table' and name=?1",
	                      StringView(sTable)) > 0;

} // Database::HasTable

//--------------------------------------------------------------------------------
const String& Database::Filename() const noexcept
//--------------------------------------------------------------------------------
{
	static const String s_sEmpty;

	return m_Connector ? m_Connector->Filename() : s_sEmpty;

} // Database::Filename

//--------------------------------------------------------------------------------
bool Database::SetBusyTimeout(std::chrono::milliseconds BusyTimeout)
//--------------------------------------------------------------------------------
{
	return m_Connector ? m_Connector->SetBusyTimeout(BusyTimeout) : false;
}

//--------------------------------------------------------------------------------
sqlite3* Database::NativeHandle() noexcept
//--------------------------------------------------------------------------------
{
	return m_Connector ? m_Connector->NativeHandle() : nullptr;
}

//--------------------------------------------------------------------------------
Database::size_type Database::Compilations() const noexcept
//--------------------------------------------------------------------------------
{
	return m_Connector ? m_Connector->Compilations() : 0;
}

//--------------------------------------------------------------------------------
Database::size_type Database::CacheHits() const noexcept
//--------------------------------------------------------------------------------
{
	return m_Connector ? m_Connector->CacheHits() : 0;
}

//--------------------------------------------------------------------------------
Database::size_type Database::CachedStatements() const noexcept
//--------------------------------------------------------------------------------
{
	return m_Connector ? m_Connector->CachedStatements() : 0;
}

//--------------------------------------------------------------------------------
bool Database::Key(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	return m_Connector && m_Connector->IsOpen()
	    && Success(sqlite3_key(m_Connector->NativeHandle(), sKey.data(), static_cast<int>(sKey.size())));
#else
	(void)sKey;
	return false;
#endif

} // Database::Key

//--------------------------------------------------------------------------------
bool Database::Rekey(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	return m_Connector && m_Connector->IsOpen()
	    && Success(sqlite3_rekey(m_Connector->NativeHandle(), sKey.data(), static_cast<int>(sKey.size())));
#else
	(void)sKey;
	return false;
#endif

} // Database::Rekey

//--------------------------------------------------------------------------------
bool Database::IsEncrypted(StringViewZ sFilename)
//--------------------------------------------------------------------------------
{
	if (!sFilename.empty())
	{
		std::ifstream File(sFilename.c_str(), std::ios::in | std::ios::binary);
		char header[16];
		if (File.is_open())
		{
			File.seekg(0, std::ios::beg);
			File.getline(header, 16);
			return strncmp(header, "SQLite format 3\000", 16) != 0;
		}
	}
	return false;

} // Database::IsEncrypted

//================================== Transaction =================================

//--------------------------------------------------------------------------------
Transaction::Transaction(Database& db, Kind iKind)
//--------------------------------------------------------------------------------
	: m_DB(db)
	, m_Claim(db.m_Connector ? detail::Claim(*db.m_Connector) : detail::Claim())
	, m_iKind(iKind)
{
	if (!db.m_Connector || !db.m_Connector->IsOpen())
	{
		db.NoConnection(); // throws when enabled
		return;
	}

	m_iDepth = db.m_Connector->m_iTransactionDepth;

	ExecResult Result;

	if (m_iDepth == 0)
	{
		switch (m_iKind)
		{
			case DEFERRED:
				Result = db.ExecSQL("BEGIN DEFERRED");
				break;
			default:
			case IMMEDIATE:
				Result = db.ExecSQL("BEGIN IMMEDIATE");
				break;
			case EXCLUSIVE:
				Result = db.ExecSQL("BEGIN EXCLUSIVE");
				break;
		}
	}
	else
	{
		String sSQL("SAVEPOINT ");
		sSQL += SavepointName();
		Result = db.ExecSQL(StringView(sSQL));
	}

	if (Result)
	{
		m_bOpen = true;
		++db.m_Connector->m_iTransactionDepth;
	}

} // ctor Transaction

//--------------------------------------------------------------------------------
Transaction::~Transaction()
//--------------------------------------------------------------------------------
{
	if (m_bOpen && !m_bCommitted)
	{
		// never throw from the destructor
		auto bMayThrow = m_DB.SetThrow(false);

		if (m_iDepth == 0)
		{
			m_DB.ExecSQL("ROLLBACK");
		}
		else
		{
			// ROLLBACK TO rewinds the savepoint but keeps it on the stack,
			// RELEASE then removes it
			String sRollback("ROLLBACK TO ");
			sRollback += SavepointName();
			m_DB.ExecSQL(StringView(sRollback));

			String sRelease("RELEASE ");
			sRelease += SavepointName();
			m_DB.ExecSQL(StringView(sRelease));
		}

		m_DB.SetThrow(bMayThrow);

		if (m_DB.m_Connector)
		{
			--m_DB.m_Connector->m_iTransactionDepth;
		}
	}

} // dtor Transaction

//--------------------------------------------------------------------------------
bool Transaction::Commit()
//--------------------------------------------------------------------------------
{
	if (!m_bOpen || m_bCommitted)
	{
		return m_bCommitted;
	}

	ExecResult Result;

	if (m_iDepth == 0)
	{
		Result = m_DB.ExecSQL("COMMIT");
	}
	else
	{
		String sSQL("RELEASE ");
		sSQL += SavepointName();
		Result = m_DB.ExecSQL(StringView(sSQL));
	}

	if (Result)
	{
		m_bCommitted = true;

		if (m_DB.m_Connector)
		{
			--m_DB.m_Connector->m_iTransactionDepth;
		}
	}

	return m_bCommitted;

} // Transaction::Commit

//--------------------------------------------------------------------------------
String Transaction::SavepointName() const
//--------------------------------------------------------------------------------
{
	String sName("ksqlite_sp_");
#ifdef DEKAF2
	sName += KString::to_string(m_iDepth);
#else
	sName += std::to_string(m_iDepth);
#endif
	return sName;

} // Transaction::SavepointName

} // end of namespace KSQLite

#ifdef DEKAF2
DEKAF2_NAMESPACE_END
#endif
