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

#include <dekaf2/web/push/bits/kapplepushksqldb.h>
#include <dekaf2/data/sql/ksql.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

// ---- KApplePush KSQL convenience constructor -----------------------------
// Mirrors KWebPush's split: this constructor lives here (not in
// kapplepush.cpp) so that kapplepush.o carries no references to KSQL
// symbols. Consumers that do not link ksql2 keep working with the SQLite
// or custom-DB backends.

//-----------------------------------------------------------------------------
KApplePush::KApplePush(KSQL& db, Config cfg)
//-----------------------------------------------------------------------------
: m_DB(std::make_unique<KApplePushKSQLDB>(db))
, m_Config(std::move(cfg))
{
	Init();

} // ctor (KSQL convenience)

// ---- KApplePushKSQLDB ----------------------------------------------------

//-----------------------------------------------------------------------------
KApplePushKSQLDB::KApplePushKSQLDB(KSQL& db)
//-----------------------------------------------------------------------------
: m_DB(db)
{
}

//-----------------------------------------------------------------------------
bool KApplePushKSQLDB::CreateTables()
//-----------------------------------------------------------------------------
{
	if (!m_DB.ExecSQL(
		"create table if not exists APNS_SUBSCRIPTIONS ("
		"    user_id     varchar(255)  not null,"
		"    device_id   varchar(128)  not null,"
		"    token       varchar(255)  not null default '',"
		"    voip_token  varchar(255)  not null default '',"
		"    useragent   varchar(512)  not null default '',"
		"    created_utc bigint        not null default 0,"
		"    lastmod_utc bigint        not null default 0,"
		"    primary key (user_id, device_id)"
		")"))
	{
		return false;
	}
	return true;

} // CreateTables

//-----------------------------------------------------------------------------
bool KApplePushKSQLDB::StoreSubscription(const KApplePush::Subscription& sub)
//-----------------------------------------------------------------------------
{
	auto iNow = static_cast<int64_t>(KUnixTime::now().to_time_t());

	// portable upsert: update first, insert if no row was affected
	if (!m_DB.ExecSQL(
					  "update APNS_SUBSCRIPTIONS set token='{}', voip_token='{}', useragent='{}', lastmod_utc={} "
					  "where user_id='{}' and device_id='{}'",
					  sub.sToken, sub.sVoIPToken, sub.sUserAgent, iNow, sub.sUser, sub.sDeviceID))
	{
		return false;
	}

	if (m_DB.GetNumRowsAffected() == 0)
	{
		return m_DB.ExecSQL(
							"insert into APNS_SUBSCRIPTIONS (user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc) "
							"values ('{}', '{}', '{}', '{}', '{}', {}, {})",
							sub.sUser, sub.sDeviceID, sub.sToken, sub.sVoIPToken, sub.sUserAgent, iNow, iNow);
	}
	return true;

} // StoreSubscription

//-----------------------------------------------------------------------------
bool KApplePushKSQLDB::RemoveSubscription(KStringView sUser, KStringView sDeviceID)
//-----------------------------------------------------------------------------
{
	return m_DB.ExecSQL("delete from APNS_SUBSCRIPTIONS where user_id='{}' and device_id='{}'",
	                    sUser, sDeviceID);

} // RemoveSubscription

//-----------------------------------------------------------------------------
bool KApplePushKSQLDB::RemoveUserSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	return m_DB.ExecSQL("delete from APNS_SUBSCRIPTIONS where user_id='{}'", sUser);
}

//-----------------------------------------------------------------------------
std::vector<KApplePush::Subscription> KApplePushKSQLDB::GetSubscriptions(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<KApplePush::Subscription> Subs;
	bool bOK;
	if (sUser.empty())
	{
		bOK = m_DB.ExecQuery(
			"select user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc "
			"from APNS_SUBSCRIPTIONS");
	}
	else
	{
		bOK = m_DB.ExecQuery(
			"select user_id, device_id, token, voip_token, useragent, created_utc, lastmod_utc "
			"from APNS_SUBSCRIPTIONS where user_id='{}'", sUser);
	}
	if (!bOK) return Subs;

	while (m_DB.NextRow())
	{
		KApplePush::Subscription sub;
		sub.sUser      = m_DB.Get(1);
		sub.sDeviceID  = m_DB.Get(2);
		sub.sToken     = m_DB.Get(3);
		sub.sVoIPToken = m_DB.Get(4);
		sub.sUserAgent = m_DB.Get(5);
		sub.tCreated   = m_DB.GetUnixTime(6);
		sub.tLastMod   = m_DB.GetUnixTime(7);
		Subs.push_back(std::move(sub));
	}
	return Subs;

} // GetSubscriptions

//-----------------------------------------------------------------------------
bool KApplePushKSQLDB::HasSubscriptions()
//-----------------------------------------------------------------------------
{
	return m_DB.SingleIntQuery("select count(*) from APNS_SUBSCRIPTIONS") > 0;
}

//-----------------------------------------------------------------------------
KString KApplePushKSQLDB::GetLastError() const
//-----------------------------------------------------------------------------
{
	return m_DB.GetLastError();
}

DEKAF2_NAMESPACE_END
