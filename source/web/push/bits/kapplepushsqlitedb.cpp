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
	if (!db.IsOpen())
	{
		m_sError = kFormat("cannot open database: {}", m_sDatabase);
		return false;
	}

	// One row per (user_id, device_id). Both push tokens (regular and VoIP)
	// live on the same row — apps with PushKit register both as part of one
	// device record. Either may be empty if that channel isn't yet known.
	auto Result = db.ExecSQL(
		"create table if not exists APNS_SUBSCRIPTIONS ("
		"    user_id     text    not null,"
		"    device_id   text    not null,"
		"    token       text    not null default '',"
		"    voip_token  text    not null default '',"
		"    useragent   text    not null default '',"
		"    created_utc integer not null default 0,"
		"    lastmod_utc integer not null default 0,"
		"    primary key(user_id, device_id)"
		")");

	if (!Result)
	{
		m_sError = kFormat("cannot create APNS_SUBSCRIPTIONS table: {}", Result.Error());
		return false;
	}

	return true;

} // CreateTables

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::StoreSubscription(const KApplePush::Subscription& sub)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen())
	{
		m_sError = "cannot open database for StoreSubscription";
		return false;
	}

	auto iNow = KUnixTime::now().to_time_t();

	// INSERT OR REPLACE + COALESCE — same SQLite-3.16-portable upsert idiom
	// used by KWebPushSQLiteDB. Preserves created_utc on update.
	auto Result = db.ExecSQL(
		"insert or replace into APNS_SUBSCRIPTIONS (user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc) "
		"values (?1, ?2, ?3, ?4, ?5, coalesce((select created_utc from APNS_SUBSCRIPTIONS where user_id=?1 and device_id=?2), ?6), ?6)",
		sub.sUser, sub.sDeviceID, sub.sToken, sub.sVoIPToken, sub.sUserAgent, iNow);

	if (!Result)
	{
		m_sError = kFormat("cannot insert APNs subscription: {}", Result.Error());
		return false;
	}
	return true;

} // StoreSubscription

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::RemoveSubscription(KStringView sUser, KStringView sDeviceID)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);
	if (!db.IsOpen())
	{
		m_sError = "cannot open database for RemoveSubscription";
		return false;
	}
	return static_cast<bool>(db.ExecSQL("delete from APNS_SUBSCRIPTIONS where user_id=?1 and device_id=?2",
	                                    sUser, sDeviceID));

} // RemoveSubscription

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::RemoveUserSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITE);
	if (!db.IsOpen())
	{
		m_sError = "cannot open database for RemoveUserSubscriptions";
		return false;
	}
	return static_cast<bool>(db.ExecSQL("delete from APNS_SUBSCRIPTIONS where user_id=?1", sUser));

} // RemoveUserSubscriptions

//-----------------------------------------------------------------------------
std::vector<KApplePush::Subscription> KApplePushSQLiteDB::GetSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<KApplePush::Subscription> out;

	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return out;

	static constexpr KStringView sSelect = "select user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc "
	                                       "from APNS_SUBSCRIPTIONS";

	auto Query = sUser.empty()
		? db.ExecQuery(sSelect)
		: db.ExecQuery(KString(sSelect) + " where user_id=?1", sUser);

	for (auto& row : Query)
	{
		KApplePush::Subscription sub;
		sub.sUser      = row.Get<KString>(1);
		sub.sDeviceID  = row.Get<KString>(2);
		sub.sToken     = row.Get<KString>(3);
		sub.sVoIPToken = row.Get<KString>(4);
		sub.sUserAgent = row.Get<KString>(5);
		sub.tCreated   = KUnixTime(row.Get<int64_t>(6));
		sub.tLastMod   = KUnixTime(row.Get<int64_t>(7));
		out.push_back(std::move(sub));
	}
	return out;

} // GetSubscriptions

//-----------------------------------------------------------------------------
bool KApplePushSQLiteDB::HasSubscriptions()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;
	return db.SingleIntQuery("select count(*) from APNS_SUBSCRIPTIONS") > 0;

} // HasSubscriptions

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
