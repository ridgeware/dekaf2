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

#include <dekaf2/web/push/bits/kwebpushksqldb.h>
#include <dekaf2/data/sql/ksql.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KWebPushKSQLDB::KWebPushKSQLDB(KSQL& db)
//-----------------------------------------------------------------------------
: m_DB(db)
{
} // ctor

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::CreateTables()
//-----------------------------------------------------------------------------
{
	if (!m_DB.ExecSQL(
		"create table if not exists PUSH_SUBSCRIPTIONS ("
		"    user_id     varchar(255) not null,"
		"    endpoint    varchar(2048) not null,"
		"    p256dh      varchar(255) not null,"
		"    auth        varchar(255) not null,"
		"    useragent   varchar(512) not null default '',"
		"    created_utc bigint       not null default 0,"
		"    lastmod_utc bigint       not null default 0,"
		"    primary key (endpoint(255))"
		")"))
	{
		return false;
	}

	if (!m_DB.ExecSQL(
		"create table if not exists VAPID_KEYS ("
		"    key         varchar(64)   not null primary key,"
		"    value       text          not null,"
		"    created_utc bigint        not null default 0,"
		"    lastmod_utc bigint        not null default 0"
		")"))
	{
		return false;
	}

	return true;

} // CreateTables

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::StoreVAPIDKey(KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	auto iNow = static_cast<int64_t>(KUnixTime::now().to_time_t());

	// portable upsert: try update, then insert if no row was affected
	if (!m_DB.ExecSQL("update VAPID_KEYS set value='{}', lastmod_utc={} where key='{}'", sValue, iNow, sKey))
	{
		return false;
	}

	if (m_DB.GetNumRowsAffected() == 0)
	{
		return m_DB.ExecSQL("insert into VAPID_KEYS (key, value, created_utc, lastmod_utc) values ('{}', '{}', {}, {})", sKey, sValue, iNow, iNow);
	}

	return true;

} // StoreVAPIDKey

//-----------------------------------------------------------------------------
KString KWebPushKSQLDB::LoadVAPIDKey(KStringView sKey)
//-----------------------------------------------------------------------------
{
	return m_DB.SingleStringQuery("select value from VAPID_KEYS where key='{}'", sKey);

} // LoadVAPIDKey

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::StoreSubscription(const KWebPush::Subscription& sub)
//-----------------------------------------------------------------------------
{
	auto iNow = static_cast<int64_t>(KUnixTime::now().to_time_t());

	// portable upsert: try update, then insert if no row was affected
	if (!m_DB.ExecSQL(
		"update PUSH_SUBSCRIPTIONS set user_id='{}', p256dh='{}', auth='{}', useragent='{}', lastmod_utc={} "
		"where endpoint='{}'",
		sub.sUser, sub.sP256dh, sub.sAuth, sub.sUserAgent, iNow, sub.sEndpoint))
	{
		return false;
	}

	if (m_DB.GetNumRowsAffected() == 0)
	{
		return m_DB.ExecSQL(
			"insert into PUSH_SUBSCRIPTIONS (user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc) "
			"values ('{}', '{}', '{}', '{}', '{}', {}, {})",
			sub.sUser, sub.sEndpoint, sub.sP256dh, sub.sAuth, sub.sUserAgent,
			iNow, iNow);
	}

	return true;

} // StoreSubscription

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::RemoveSubscription(KStringView sEndpoint)
//-----------------------------------------------------------------------------
{
	return m_DB.ExecSQL("delete from PUSH_SUBSCRIPTIONS where endpoint='{}'", sEndpoint);

} // RemoveSubscription

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::RemoveUserSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	return m_DB.ExecSQL("delete from PUSH_SUBSCRIPTIONS where user_id='{}'", sUser);

} // RemoveUserSubscriptions

//-----------------------------------------------------------------------------
std::vector<KWebPush::Subscription> KWebPushKSQLDB::GetSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<KWebPush::Subscription> Subs;

	bool bOK;

	if (sUser.empty())
	{
		bOK = m_DB.ExecQuery("select user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc from PUSH_SUBSCRIPTIONS");
	}
	else
	{
		bOK = m_DB.ExecQuery("select user_id, endpoint, p256dh, auth, useragent, created_utc, lastmod_utc from PUSH_SUBSCRIPTIONS where user_id='{}'", sUser);
	}

	if (!bOK)
	{
		return Subs;
	}

	while (m_DB.NextRow())
	{
		KWebPush::Subscription sub;
		sub.sUser      = m_DB.Get(1);
		sub.sEndpoint  = m_DB.Get(2);
		sub.sP256dh    = m_DB.Get(3);
		sub.sAuth      = m_DB.Get(4);
		sub.sUserAgent = m_DB.Get(5);
		sub.tCreated   = m_DB.GetUnixTime(6);
		sub.tLastMod   = m_DB.GetUnixTime(7);
		Subs.push_back(std::move(sub));
	}

	return Subs;

} // GetSubscriptions

//-----------------------------------------------------------------------------
bool KWebPushKSQLDB::HasSubscriptions()
//-----------------------------------------------------------------------------
{
	return m_DB.SingleIntQuery("select count(*) from PUSH_SUBSCRIPTIONS") > 0;

} // HasSubscriptions

//-----------------------------------------------------------------------------
KString KWebPushKSQLDB::GetLastError() const
//-----------------------------------------------------------------------------
{
	return m_DB.GetLastError();

} // GetLastError

DEKAF2_NAMESPACE_END
