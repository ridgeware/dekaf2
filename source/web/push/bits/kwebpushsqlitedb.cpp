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

#include <dekaf2/web/push/bits/kwebpushsqlitedb.h>

#if DEKAF2_HAS_SQLITE3

#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KWebPushSQLiteDB::KWebPushSQLiteDB(KString sDatabase)
//-----------------------------------------------------------------------------
: m_sDatabase(std::move(sDatabase))
{
} // ctor

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::CreateTables()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);

	if (!db.IsOpen())
	{
		m_sError = kFormat("cannot open database: {}", m_sDatabase);
		return false;
	}

	auto Result = db.ExecSQL(
		"create table if not exists PUSH_SUBSCRIPTIONS ("
		"    user_id     text    not null,"
		"    endpoint    text    not null,"
		"    p256dh      text    not null,"
		"    auth        text    not null,"
		"    useragent   text    not null default '',"
		"    created_utc integer not null default 0,"
		"    lastmod_utc integer not null default 0,"
		"    primary key(endpoint)"
		")");

	if (!Result)
	{
		m_sError = kFormat("cannot create PUSH_SUBSCRIPTIONS table: {}", Result.Error());
		return false;
	}

	Result = db.ExecSQL(
		"create table if not exists VAPID_KEYS ("
		"    key         text    not null primary key,"
		"    value       text    not null,"
		"    created_utc integer not null default 0,"
		"    lastmod_utc integer not null default 0"
		")");

	if (!Result)
	{
		m_sError = kFormat("cannot create VAPID_KEYS table: {}", Result.Error());
		return false;
	}

	return true;

} // CreateTables

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::StoreVAPIDKey(KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for storing VAPID key";
		return false;
	}

	auto iNow = KUnixTime::now().to_time_t();

	// Use INSERT OR REPLACE + COALESCE instead of "ON CONFLICT DO UPDATE"
	// (UPSERT), which was only added in SQLite 3.24.0 (2018-06). Debian
	// stretch ships SQLite 3.16.2. The scalar subquery preserves the
	// original created_utc on an existing row; for a new row it evaluates
	// to NULL and COALESCE falls back to the current timestamp.
	auto Result = db.ExecSQL(
		"insert or replace into VAPID_KEYS (key, value, created_utc, lastmod_utc) "
		"values (?1, ?2, coalesce((select created_utc from VAPID_KEYS where key=?1), ?3), ?3)",
		sKey, sValue, iNow);

	if (!Result)
	{
		m_sError = kFormat("cannot store VAPID key '{}': {}", sKey, Result.Error());
		return false;
	}

	return true;

} // StoreVAPIDKey

//-----------------------------------------------------------------------------
KString KWebPushSQLiteDB::LoadVAPIDKey(KStringView sKey)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		return {};
	}

	return db.SingleStringQuery("select value from VAPID_KEYS where key=?1", sKey);

} // LoadVAPIDKey

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::StoreSubscription(const KWebPush::Subscription& sub)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for StoreSubscription";
		return false;
	}

	auto iNow = KUnixTime::now().to_time_t();

	// INSERT OR REPLACE + COALESCE — see portability comment in StoreVAPIDKey().
	// The scalar subquery preserves the original created_utc for an existing
	// subscription (keyed by endpoint), and defaults to the current time on
	// first insert.
	auto Result = db.ExecSQL(
		"insert or replace into PUSH_SUBSCRIPTIONS (user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc) "
		"values (?1, ?2, ?3, ?4, ?5, coalesce((select created_utc from PUSH_SUBSCRIPTIONS where endpoint=?2), ?6), ?6)",
		sub.sUser, sub.sEndpoint, sub.sP256dh, sub.sAuth, sub.sUserAgent, iNow);

	if (!Result)
	{
		m_sError = kFormat("cannot insert subscription: {}", Result.Error());
		return false;
	}

	return true;

} // StoreSubscription

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::RemoveSubscription(KStringView sEndpoint)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for RemoveSubscription";
		return false;
	}

	auto Result = db.ExecSQL("delete from PUSH_SUBSCRIPTIONS where endpoint=?1", sEndpoint);

	if (!Result)
	{
		m_sError = kFormat("cannot delete subscription: {}", Result.Error());
		return false;
	}

	return true;

} // RemoveSubscription

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::RemoveUserSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);

	if (!db.IsOpen())
	{
		m_sError = "cannot open database for RemoveUserSubscriptions";
		return false;
	}

	auto Result = db.ExecSQL("delete from PUSH_SUBSCRIPTIONS where user_id=?1", sUser);

	if (!Result)
	{
		m_sError = kFormat("cannot delete subscriptions for user '{}': {}", sUser, Result.Error());
		return false;
	}

	return true;

} // RemoveUserSubscriptions

//-----------------------------------------------------------------------------
std::vector<KWebPush::Subscription> KWebPushSQLiteDB::GetSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<KWebPush::Subscription> Subs;

	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		return Subs;
	}

	auto Query = sUser.empty()
		? db.ExecQuery("select user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc from PUSH_SUBSCRIPTIONS")
		: db.ExecQuery("select user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc from PUSH_SUBSCRIPTIONS where user_id=?1", sUser);

	for (auto& Row : Query)
	{
		KWebPush::Subscription sub;
		sub.sUser      = Row.Get<KString>(1);
		sub.sEndpoint  = Row.Get<KString>(2);
		sub.sP256dh    = Row.Get<KString>(3);
		sub.sAuth      = Row.Get<KString>(4);
		sub.sUserAgent = Row.Get<KString>(5);
		sub.tCreated   = KUnixTime::from_time_t(Row.Get<int64_t>(6));
		sub.tLastMod   = KUnixTime::from_time_t(Row.Get<int64_t>(7));
		Subs.push_back(std::move(sub));
	}

	return Subs;

} // GetSubscriptions

//-----------------------------------------------------------------------------
bool KWebPushSQLiteDB::HasSubscriptions()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);

	if (!db.IsOpen())
	{
		return false;
	}

	return db.SingleIntQuery("select count(*) from PUSH_SUBSCRIPTIONS") > 0;

} // HasSubscriptions

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
