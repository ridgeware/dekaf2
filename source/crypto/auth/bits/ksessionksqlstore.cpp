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

#include <dekaf2/crypto/auth/bits/ksessionksqlstore.h>
#include <dekaf2/data/sql/ksql.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/time/clock/ktime.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KSessionKSQLStore::KSessionKSQLStore(KSQL& db, KString sTableName)
//-----------------------------------------------------------------------------
: m_DB(db)
, m_sTableName(std::move(sTableName))
{
	if (m_sTableName.empty())
	{
		m_sTableName = "KSESSION_SESSIONS";
	}

} // ctor

//-----------------------------------------------------------------------------
bool KSessionKSQLStore::Initialize()
//-----------------------------------------------------------------------------
{
	// Column widths are chosen for portability across MySQL/MariaDB,
	// PostgreSQL and SQLite. PostgreSQL does not enforce VARCHAR(n)
	// length beyond the declaration; SQLite ignores it entirely;
	// MySQL truncates. Tokens from KSession are ~45 chars
	// (base64url of 32 random bytes, no padding) so 128 is generous.
	if (!m_DB.ExecSQL(
		"create table if not exists {} ("
		"    token         varchar(128) not null primary key,"
		"    username      varchar(255) not null,"
		"    created_utc   bigint       not null default 0,"
		"    last_seen_utc bigint       not null default 0,"
		"    client_ip     varchar(64)  not null default '',"
		"    user_agent    varchar(512) not null default '',"
		"    extra         text         not null default ''"
		")", m_sTableName))
	{
		return false;
	}

	// Index creation is separate because some engines reject
	// multiple statements in one ExecSQL call.
	// Some DBs error out if the index already exists; failure here is
	// not fatal (tolerated for portability across engines).
	m_DB.ExecSQL("create index {}_user_idx on {}(username)",
	             m_sTableName, m_sTableName);
	m_DB.ExecSQL("create index {}_lastseen_idx on {}(last_seen_utc)",
	             m_sTableName, m_sTableName);

	return true;

} // Initialize

//-----------------------------------------------------------------------------
bool KSessionKSQLStore::Create(const KSession::Record& Rec)
//-----------------------------------------------------------------------------
{
	if (Rec.sToken.empty())
	{
		return false;
	}

	auto iCreated  = static_cast<int64_t>(Rec.tCreated.to_time_t());
	auto iLastSeen = static_cast<int64_t>(Rec.tLastSeen.to_time_t());

	return m_DB.ExecSQL(
		"insert into {} (token, username, created_utc, last_seen_utc, client_ip, user_agent, extra) "
		"values ('{}', '{}', {}, {}, '{}', '{}', '{}')",
		m_sTableName,
		Rec.sToken,
		Rec.sUsername,
		iCreated,
		iLastSeen,
		Rec.sClientIP,
		Rec.sUserAgent,
		Rec.sExtra);

} // Create

//-----------------------------------------------------------------------------
bool KSessionKSQLStore::Lookup(KStringView sToken, KSession::Record* pOut)
//-----------------------------------------------------------------------------
{
	if (!m_DB.ExecQuery(
		"select token, username, created_utc, last_seen_utc, client_ip, user_agent, extra "
		"from {} where token='{}'",
		m_sTableName, sToken))
	{
		return false;
	}

	if (!m_DB.NextRow())
	{
		return false;
	}

	if (pOut)
	{
		pOut->sToken     = m_DB.Get(1);
		pOut->sUsername  = m_DB.Get(2);
		pOut->tCreated   = m_DB.GetUnixTime(3);
		pOut->tLastSeen  = m_DB.GetUnixTime(4);
		pOut->sClientIP  = m_DB.Get(5);
		pOut->sUserAgent = m_DB.Get(6);
		pOut->sExtra     = m_DB.Get(7);
	}

	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSessionKSQLStore::Touch(KStringView sToken, KUnixTime tLastSeen)
//-----------------------------------------------------------------------------
{
	return m_DB.ExecSQL(
		"update {} set last_seen_utc={} where token='{}'",
		m_sTableName,
		static_cast<int64_t>(tLastSeen.to_time_t()),
		sToken);

} // Touch

//-----------------------------------------------------------------------------
bool KSessionKSQLStore::Erase(KStringView sToken)
//-----------------------------------------------------------------------------
{
	if (!m_DB.ExecSQL("delete from {} where token='{}'", m_sTableName, sToken))
	{
		return false;
	}

	return m_DB.GetNumRowsAffected() > 0;

} // Erase

//-----------------------------------------------------------------------------
std::size_t KSessionKSQLStore::EraseAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	if (!m_DB.ExecSQL("delete from {} where username='{}'", m_sTableName, sUsername))
	{
		return 0;
	}

	return static_cast<std::size_t>(m_DB.GetNumRowsAffected());

} // EraseAllFor

//-----------------------------------------------------------------------------
std::size_t KSessionKSQLStore::PurgeExpired(KUnixTime tOldestLastSeen,
                                            KUnixTime tOldestCreated)
//-----------------------------------------------------------------------------
{
	const bool bCheckIdle = tOldestLastSeen.to_time_t() > 0;
	const bool bCheckAbs  = tOldestCreated.to_time_t()  > 0;

	if (!bCheckIdle && !bCheckAbs)
	{
		return 0;
	}

	bool bOK;

	if (bCheckIdle && bCheckAbs)
	{
		bOK = m_DB.ExecSQL(
			"delete from {} where last_seen_utc < {} or created_utc < {}",
			m_sTableName,
			static_cast<int64_t>(tOldestLastSeen.to_time_t()),
			static_cast<int64_t>(tOldestCreated.to_time_t()));
	}
	else if (bCheckIdle)
	{
		bOK = m_DB.ExecSQL(
			"delete from {} where last_seen_utc < {}",
			m_sTableName,
			static_cast<int64_t>(tOldestLastSeen.to_time_t()));
	}
	else
	{
		bOK = m_DB.ExecSQL(
			"delete from {} where created_utc < {}",
			m_sTableName,
			static_cast<int64_t>(tOldestCreated.to_time_t()));
	}

	if (!bOK)
	{
		return 0;
	}

	return static_cast<std::size_t>(m_DB.GetNumRowsAffected());

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSessionKSQLStore::Count() const
//-----------------------------------------------------------------------------
{
	// KSQL::SingleIntQuery is non-const, but logically Count() is a
	// read-only observer, so we cast away const here. Callers are
	// serialized through the same KSQL reference anyway (see note on
	// thread safety in the header).
	auto& db = const_cast<KSQL&>(m_DB);
	auto iVal = db.SingleIntQuery("select count(*) from {}", m_sTableName);
	return (iVal < 0) ? 0 : static_cast<std::size_t>(iVal);

} // Count

//-----------------------------------------------------------------------------
KString KSessionKSQLStore::GetLastError() const
//-----------------------------------------------------------------------------
{
	return m_DB.GetLastError();

} // GetLastError

// --------------------------------------------------------------------------
// KSession convenience constructors that wrap a KSessionKSQLStore.
// These live in this TU (built into the ksql2 library) instead of
// ksession.cpp so that the core dekaf2 library does not need to link
// against KSQL / MySQL / PostgreSQL / SQLite.
// --------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KSession::KSession(KSQL& db, Config cfg)
//-----------------------------------------------------------------------------
: KSession(std::make_unique<KSessionKSQLStore>(db), std::move(cfg))
{
} // ctor (KSQL convenience)

//-----------------------------------------------------------------------------
KSession::KSession(KSQL& db)
//-----------------------------------------------------------------------------
: KSession(db, Config{})
{
} // ctor (KSQL convenience, default config)

DEKAF2_NAMESPACE_END
