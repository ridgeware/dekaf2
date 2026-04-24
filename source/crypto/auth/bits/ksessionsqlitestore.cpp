/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |\|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 //
 */

#include <dekaf2/crypto/auth/bits/ksessionsqlitestore.h>

#if DEKAF2_HAS_SQLITE3

#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KSessionSQLiteStore::KSessionSQLiteStore(KString sDatabase, KString sTableName)
//-----------------------------------------------------------------------------
: m_sDatabase(std::move(sDatabase))
, m_sTableName(std::move(sTableName))
{
	if (m_sTableName.empty())
	{
		m_sTableName = "KSESSION_SESSIONS";
	}

} // ctor

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Initialize()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database '{}': {}", m_sDatabase, db.Error());
		return false;
	}

	// one table, session records keyed by token.
	// all timestamps stored as unix seconds (INTEGER) — date arithmetic
	// is then a simple subtraction.
	// indexes support the two hot paths: EraseAllFor(username) and
	// PurgeExpired (scan on last_seen_utc).
	auto sSQL = kFormat(
		"create table if not exists {} ("
		"    token         text    not null primary key,"
		"    username      text    not null,"
		"    created_utc   integer not null default 0,"
		"    last_seen_utc integer not null default 0,"
		"    client_ip     text    not null default '',"
		"    user_agent    text    not null default '',"
		"    extra         text    not null default ''"
		")", m_sTableName);

	if (!db.ExecuteVoid(sSQL))
	{
		m_sError = kFormat("cannot create table '{}': {}", m_sTableName, db.Error());
		return false;
	}

	if (!db.ExecuteVoid(kFormat(
		"create index if not exists {}_user_idx on {}(username)",
		m_sTableName, m_sTableName)))
	{
		m_sError = kFormat("cannot create user index: {}", db.Error());
		return false;
	}

	if (!db.ExecuteVoid(kFormat(
		"create index if not exists {}_lastseen_idx on {}(last_seen_utc)",
		m_sTableName, m_sTableName)))
	{
		m_sError = kFormat("cannot create last_seen index: {}", db.Error());
		return false;
	}

	return true;

} // Initialize

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Create(const KSession::Record& Rec)
//-----------------------------------------------------------------------------
{
	if (Rec.sToken.empty())
	{
		m_sError = "empty token";
		return false;
	}

	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for Create: {}", db.Error());
		return false;
	}

	auto stmt = db.Prepare(kFormat(
		"insert into {} (token, username, created_utc, last_seen_utc, client_ip, user_agent, extra) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
		m_sTableName));

	stmt.Bind(1, Rec.sToken,     false);
	stmt.Bind(2, Rec.sUsername,  false);
	stmt.Bind(3, Rec.tCreated.to_time_t());
	stmt.Bind(4, Rec.tLastSeen.to_time_t());
	stmt.Bind(5, Rec.sClientIP,  false);
	stmt.Bind(6, Rec.sUserAgent, false);
	stmt.Bind(7, Rec.sExtra,     false);

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot insert session: {}", db.Error());
		return false;
	}

	return true;

} // Create

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Lookup(KStringView sToken, KSession::Record* pOut)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for Lookup: {}", db.Error());
		return false;
	}

	auto stmt = db.Prepare(kFormat(
		"select token, username, created_utc, last_seen_utc, client_ip, user_agent, extra "
		"from {} where token=?1", m_sTableName));

	stmt.Bind(1, sToken, false);

	if (!stmt.NextRow())
	{
		return false;
	}

	if (pOut)
	{
		auto Row = stmt.GetRow();
		pOut->sToken     = Row.Col(1).String();
		pOut->sUsername  = Row.Col(2).String();
		pOut->tCreated   = KUnixTime::from_time_t(Row.Col(3).Int64());
		pOut->tLastSeen  = KUnixTime::from_time_t(Row.Col(4).Int64());
		pOut->sClientIP  = Row.Col(5).String();
		pOut->sUserAgent = Row.Col(6).String();
		pOut->sExtra     = Row.Col(7).String();
	}

	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Touch(KStringView sToken, KUnixTime tLastSeen)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for Touch: {}", db.Error());
		return false;
	}

	auto stmt = db.Prepare(kFormat(
		"update {} set last_seen_utc=?1 where token=?2", m_sTableName));

	stmt.Bind(1, tLastSeen.to_time_t());
	stmt.Bind(2, sToken, false);

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot update last_seen: {}", db.Error());
		return false;
	}

	return true;

} // Touch

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Erase(KStringView sToken)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for Erase: {}", db.Error());
		return false;
	}

	auto stmt = db.Prepare(kFormat(
		"delete from {} where token=?1", m_sTableName));

	stmt.Bind(1, sToken, false);

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot delete session: {}", db.Error());
		return false;
	}

	return db.AffectedRows() > 0;

} // Erase

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::EraseAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for EraseAllFor: {}", db.Error());
		return 0;
	}

	auto stmt = db.Prepare(kFormat(
		"delete from {} where username=?1", m_sTableName));

	stmt.Bind(1, sUsername, false);

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot delete user sessions: {}", db.Error());
		return 0;
	}

	return static_cast<std::size_t>(db.AffectedRows());

} // EraseAllFor

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::PurgeExpired(KUnixTime tOldestLastSeen,
                                              KUnixTime tOldestCreated)
//-----------------------------------------------------------------------------
{
	// A cutoff at epoch (to_time_t() == 0) means the corresponding timeout
	// is disabled in the KSession config; skip that predicate.
	const bool bCheckIdle = tOldestLastSeen.to_time_t() > 0;
	const bool bCheckAbs  = tOldestCreated.to_time_t()  > 0;

	if (!bCheckIdle && !bCheckAbs)
	{
		return 0;
	}

	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for PurgeExpired: {}", db.Error());
		return 0;
	}

	KString sSQL = kFormat("delete from {} where ", m_sTableName);

	if (bCheckIdle && bCheckAbs)
	{
		sSQL += "last_seen_utc < ?1 or created_utc < ?2";
	}
	else if (bCheckIdle)
	{
		sSQL += "last_seen_utc < ?1";
	}
	else
	{
		sSQL += "created_utc < ?1";
	}

	auto stmt = db.Prepare(sSQL);

	if (bCheckIdle && bCheckAbs)
	{
		stmt.Bind(1, tOldestLastSeen.to_time_t());
		stmt.Bind(2, tOldestCreated.to_time_t());
	}
	else if (bCheckIdle)
	{
		stmt.Bind(1, tOldestLastSeen.to_time_t());
	}
	else
	{
		stmt.Bind(1, tOldestCreated.to_time_t());
	}

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot purge expired: {}", db.Error());
		return 0;
	}

	return static_cast<std::size_t>(db.AffectedRows());

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::Count() const
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (db.IsError())
	{
		m_sError = kFormat("cannot open database for Count: {}", db.Error());
		return 0;
	}

	auto stmt = db.Prepare(kFormat("select count(*) from {}", m_sTableName));

	if (stmt.NextRow())
	{
		return static_cast<std::size_t>(stmt.GetRow().Col(1).Int64());
	}

	return 0;

} // Count

// --------------------------------------------------------------------------
// KSession convenience constructors that wrap a KSessionSQLiteStore.
// These live in this TU (built into the ksqlite library) instead of
// ksession.cpp so that the core dekaf2 library does not need to link
// against SQLite.
// --------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KSession::KSession(KStringViewZ sDatabase, Config cfg)
//-----------------------------------------------------------------------------
: KSession(std::make_unique<KSessionSQLiteStore>(KString(sDatabase)), std::move(cfg))
{
} // ctor (SQLite convenience)

//-----------------------------------------------------------------------------
KSession::KSession(KStringViewZ sDatabase)
//-----------------------------------------------------------------------------
: KSession(sDatabase, Config{})
{
} // ctor (SQLite convenience, default config)

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
