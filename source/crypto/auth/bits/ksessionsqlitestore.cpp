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

	if (!db.IsOpen())
	{
		m_sError = kFormat("cannot open database '{}'", m_sDatabase);
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

	auto Result = db.ExecSQL(sSQL);

	if (!Result)
	{
		m_sError = kFormat("cannot create table '{}': {}", m_sTableName, Result.Error());
		return false;
	}

	Result = db.ExecSQL(kFormat(
		"create index if not exists {}_user_idx on {}(username)",
		m_sTableName, m_sTableName));

	if (!Result)
	{
		m_sError = kFormat("cannot create user index: {}", Result.Error());
		return false;
	}

	Result = db.ExecSQL(kFormat(
		"create index if not exists {}_lastseen_idx on {}(last_seen_utc)",
		m_sTableName, m_sTableName));

	if (!Result)
	{
		m_sError = kFormat("cannot create last_seen index: {}", Result.Error());
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

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for Create";
		return false;
	}

	auto Result = db.ExecSQL(kFormat(
		"insert into {} (token, username, created_utc, last_seen_utc, client_ip, user_agent, extra) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
		m_sTableName),
		Rec.sToken, Rec.sUsername,
		Rec.tCreated.to_time_t(), Rec.tLastSeen.to_time_t(),
		Rec.sClientIP, Rec.sUserAgent, Rec.sExtra);

	if (!Result)
	{
		m_sError = kFormat("cannot insert session: {}", Result.Error());
		return false;
	}

	return true;

} // Create

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Lookup(KStringView sToken, KSession::Record* pOut)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for Lookup";
		return false;
	}

	auto Query = db.ExecQuery(kFormat(
		"select token, username, created_utc, last_seen_utc, client_ip, user_agent, extra "
		"from {} where token=?1", m_sTableName),
		sToken);

	if (!Query.Next())
	{
		return false;
	}

	if (pOut)
	{
		auto& Row = Query.GetRow();
		pOut->sToken     = Row.Get<KString>(1);
		pOut->sUsername  = Row.Get<KString>(2);
		pOut->tCreated   = KUnixTime::from_time_t(Row.Get<int64_t>(3));
		pOut->tLastSeen  = KUnixTime::from_time_t(Row.Get<int64_t>(4));
		pOut->sClientIP  = Row.Get<KString>(5);
		pOut->sUserAgent = Row.Get<KString>(6);
		pOut->sExtra     = Row.Get<KString>(7);
	}

	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Touch(KStringView sToken, KUnixTime tLastSeen)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for Touch";
		return false;
	}

	auto Result = db.ExecSQL(kFormat(
		"update {} set last_seen_utc=?1 where token=?2", m_sTableName),
		tLastSeen.to_time_t(), sToken);

	if (!Result)
	{
		m_sError = kFormat("cannot update last_seen: {}", Result.Error());
		return false;
	}

	return true;

} // Touch

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::UpdateExtra(KStringView sToken, KStringView sExtra)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for UpdateExtra";
		return false;
	}

	auto Result = db.ExecSQL(kFormat(
		"update {} set extra=?1 where token=?2", m_sTableName),
		sExtra, sToken);

	if (!Result)
	{
		m_sError = kFormat("cannot update extra: {}", Result.Error());
		return false;
	}

	return true;

} // UpdateExtra

//-----------------------------------------------------------------------------
bool KSessionSQLiteStore::Erase(KStringView sToken)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for Erase";
		return false;
	}

	auto Result = db.ExecSQL(kFormat(
		"delete from {} where token=?1", m_sTableName),
		sToken);

	if (!Result)
	{
		m_sError = kFormat("cannot delete session: {}", Result.Error());
		return false;
	}

	return Result.AffectedRows() > 0;

} // Erase

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::EraseAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for EraseAllFor";
		return 0;
	}

	auto Result = db.ExecSQL(kFormat(
		"delete from {} where username=?1", m_sTableName),
		sUsername);

	if (!Result)
	{
		m_sError = kFormat("cannot delete user sessions: {}", Result.Error());
		return 0;
	}

	return static_cast<std::size_t>(Result.AffectedRows());

} // EraseAllFor

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::ListFor(KStringView sUsername, std::vector<KSession::Record>& Out)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for ListFor";
		return 0;
	}

	auto Query = db.ExecQuery(kFormat(
		"select token, username, created_utc, last_seen_utc, client_ip, user_agent, extra "
		"from {} where username=?1 order by last_seen_utc desc", m_sTableName),
		sUsername);

	std::size_t iAdded = 0;

	for (auto& Row : Query)
	{
		KSession::Record Rec;
		Rec.sToken     = Row.Get<KString>(1);
		Rec.sUsername  = Row.Get<KString>(2);
		Rec.tCreated   = KUnixTime::from_time_t(Row.Get<int64_t>(3));
		Rec.tLastSeen  = KUnixTime::from_time_t(Row.Get<int64_t>(4));
		Rec.sClientIP  = Row.Get<KString>(5);
		Rec.sUserAgent = Row.Get<KString>(6);
		Rec.sExtra     = Row.Get<KString>(7);
		Out.push_back(std::move(Rec));
		++iAdded;
	}

	return iAdded;

} // ListFor

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

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for PurgeExpired";
		return 0;
	}

	KString sSQL = kFormat("delete from {} where ", m_sTableName);

	KSQLite::ExecResult Result;

	if (bCheckIdle && bCheckAbs)
	{
		sSQL  += "last_seen_utc < ?1 or created_utc < ?2";
		Result = db.ExecSQL(sSQL, tOldestLastSeen.to_time_t(), tOldestCreated.to_time_t());
	}
	else if (bCheckIdle)
	{
		sSQL  += "last_seen_utc < ?1";
		Result = db.ExecSQL(sSQL, tOldestLastSeen.to_time_t());
	}
	else
	{
		sSQL  += "created_utc < ?1";
		Result = db.ExecSQL(sSQL, tOldestCreated.to_time_t());
	}

	if (!Result)
	{
		m_sError = kFormat("cannot purge expired: {}", Result.Error());
		return 0;
	}

	return static_cast<std::size_t>(Result.AffectedRows());

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSessionSQLiteStore::Count() const
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for Count";
		return 0;
	}

	return static_cast<std::size_t>(db.SingleIntQuery(kFormat("select count(*) from {}", m_sTableName)));

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
