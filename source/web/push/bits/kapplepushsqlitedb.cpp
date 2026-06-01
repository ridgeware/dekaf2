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

#include <dekaf2/web/push/bits/kapplepushsqlitedb.h>

#if DEKAF2_HAS_SQLITE3

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/time/clock/ktime.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KApplePushSQLiteDB::KApplePushSQLiteDB(KString sDatabase)
//-----------------------------------------------------------------------------
: m_sDatabase(std::move(sDatabase))
{
}

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::CreateTables()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (db.IsError())
	{
		m_sError = kFormat("cannot open database: {}", m_sDatabase);
		return false;
	}

	// One row per (user_id, device_id). Both push tokens (regular and VoIP)
	// live on the same row — apps with PushKit register both as part of one
	// device record. Either may be empty if that channel isn't yet known.
	if (!db.ExecuteVoid(
		"create table if not exists APNS_SUBSCRIPTIONS ("
		"    user_id     text    not null,"
		"    device_id   text    not null,"
		"    token       text    not null default '',"
		"    voip_token  text    not null default '',"
		"    useragent   text    not null default '',"
		"    created_utc integer not null default 0,"
		"    lastmod_utc integer not null default 0,"
		"    primary key(user_id, device_id)"
		")"))
	{
		m_sError = kFormat("cannot create APNS_SUBSCRIPTIONS table: {}", db.Error());
		return false;
	}

	return true;

} // CreateTables

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::StoreSubscription(const KApplePush::Subscription& sub)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (db.IsError())
	{
		m_sError = "cannot open database for StoreSubscription";
		return false;
	}

	auto iNow = KUnixTime::now().to_time_t();

	// INSERT OR REPLACE + COALESCE — same SQLite-3.16-portable upsert idiom
	// used by KWebPushSQLiteDB. Preserves created_utc on update.
	auto stmt = db.Prepare(
		"insert or replace into APNS_SUBSCRIPTIONS (user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc) "
		"values (?1, ?2, ?3, ?4, ?5, coalesce((select created_utc from APNS_SUBSCRIPTIONS where user_id=?1 and device_id=?2), ?6), ?6)");
	stmt.Bind(1, sub.sUser,      false);
	stmt.Bind(2, sub.sDeviceID,  false);
	stmt.Bind(3, sub.sToken,     false);
	stmt.Bind(4, sub.sVoIPToken, false);
	stmt.Bind(5, sub.sUserAgent, false);
	stmt.Bind(6, iNow);

	if (!stmt.Execute())
	{
		m_sError = kFormat("cannot insert APNs subscription: {}", db.Error());
		return false;
	}
	return true;

} // StoreSubscription

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::RemoveSubscription(KStringView sUser, KStringView sDeviceID)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);
	if (db.IsError())
	{
		m_sError = "cannot open database for RemoveSubscription";
		return false;
	}
	auto stmt = db.Prepare("delete from APNS_SUBSCRIPTIONS where user_id=?1 and device_id=?2");
	stmt.Bind(1, sUser,     false);
	stmt.Bind(2, sDeviceID, false);
	return stmt.Execute();

} // RemoveSubscription

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::RemoveUserSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);
	if (db.IsError())
	{
		m_sError = "cannot open database for RemoveUserSubscriptions";
		return false;
	}
	auto stmt = db.Prepare("delete from APNS_SUBSCRIPTIONS where user_id=?1");
	stmt.Bind(1, sUser, false);
	return stmt.Execute();

} // RemoveUserSubscriptions

//-----------------------------------------------------------------------------
std::vector<KApplePush::Subscription> KApplePushSQLiteDB::GetSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<KApplePush::Subscription> out;

	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (db.IsError()) return out;

	KString sQuery = "select user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc "
	                 "from APNS_SUBSCRIPTIONS";
	if (!sUser.empty()) sQuery += " where user_id=?1";

	auto stmt = db.Prepare(sQuery);
	if (!sUser.empty()) stmt.Bind(1, sUser, false);

	while (stmt.NextRow())
	{
		auto row = stmt.GetRow();
		KApplePush::Subscription sub;
		sub.sUser      = row.Col(1).String();
		sub.sDeviceID  = row.Col(2).String();
		sub.sToken     = row.Col(3).String();
		sub.sVoIPToken = row.Col(4).String();
		sub.sUserAgent = row.Col(5).String();
		sub.tCreated   = KUnixTime(row.Col(6).Int64());
		sub.tLastMod   = KUnixTime(row.Col(7).Int64());
		out.push_back(std::move(sub));
	}
	return out;

} // GetSubscriptions

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::HasSubscriptions()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (db.IsError()) return false;
	auto stmt = db.Prepare("select count(*) from APNS_SUBSCRIPTIONS");
	if (stmt.NextRow())
	{
		return stmt.GetRow().Col(1).Int64() > 0;
	}
	return false;

} // HasSubscriptions

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
