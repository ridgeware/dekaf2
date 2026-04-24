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
 */

#include "ktunnel_store.h"

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>

//-----------------------------------------------------------------------------
KString KTunnelStore::DefaultDatabasePath()
//-----------------------------------------------------------------------------
{
	// kGetConfigPath() defaults to $HOME/.config/<progname> (ktunnel), creates
	// it with mode 0700 on first use. We append a fixed filename so operators
	// can switch the location via XDG_CONFIG_HOME or the -db flag instead of
	// needing any extra configuration here.
	return kFormat("{}{}ktunnel.db", kGetConfigPath(true), kDirSep);

} // DefaultDatabasePath

//-----------------------------------------------------------------------------
KTunnelStore::KTunnelStore (KString sDatabase)
//-----------------------------------------------------------------------------
: m_sDatabase(std::move(sDatabase))
{
	if (m_sDatabase.empty())
	{
		m_sDatabase = DefaultDatabasePath();
	}

	// Make sure the parent directory exists (the config path normally gets
	// created by kGetConfigPath(), but if the caller passed an explicit -db
	// override we have to honor that too). kDirname() returns a KStringView
	// into m_sDatabase, but kDirExists/kCreateDir want a KStringViewZ, so
	// we materialize a KString first.
	KString sDir { kDirname(m_sDatabase) };
	if (!sDir.empty() && !kDirExists(sDir))
	{
		kCreateDir(sDir, DEKAF2_MODE_CREATE_CONFIG_DIR, true);
	}

	InitializeSchema();

} // ctor

//-----------------------------------------------------------------------------
void KTunnelStore::SetError (KString sError)
//-----------------------------------------------------------------------------
{
	if (!sError.empty())
	{
		kDebug(1, "KTunnelStore: {}", sError);
	}
	m_sError = std::move(sError);

} // SetError

//-----------------------------------------------------------------------------
KSQLite::Database KTunnelStore::OpenRW ()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (db.IsError())
	{
		SetError(kFormat("cannot open '{}' for write: {}", m_sDatabase, db.Error()));
	}
	return db;

} // OpenRW

//-----------------------------------------------------------------------------
KSQLite::Database KTunnelStore::OpenRO ()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (db.IsError())
	{
		SetError(kFormat("cannot open '{}' for read: {}", m_sDatabase, db.Error()));
	}
	return db;

} // OpenRO

//-----------------------------------------------------------------------------
bool KTunnelStore::InitializeSchema ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	// WAL gives us concurrent readers while the admin UI / event logger is
	// writing. It persists as a DB attribute, but setting it is idempotent.
	db.ExecuteVoid("pragma journal_mode=WAL");
	db.ExecuteVoid("pragma synchronous=NORMAL");
	db.ExecuteVoid("pragma foreign_keys=ON");

	static constexpr KStringView s_sDDL[] =
	{
		// Users: unique username, bcrypt hash, admin bit
		"create table if not exists users ("
		"  id             integer primary key autoincrement,"
		"  username       text    not null unique,"
		"  pw_hash        text    not null,"
		"  is_admin       integer not null default 0,"
		"  created_utc    integer not null default 0,"
		"  last_login_utc integer not null default 0"
		")",

		// Tunnels: one row per listener the admin configured. A tunnel
		// is keyed by name (human-readable); the owning user decides
		// whose ktunnel login is accepted for this listener.
		"create table if not exists tunnels ("
		"  id           integer primary key autoincrement,"
		"  name         text    not null unique,"
		"  owner_user   text    not null default '',"
		"  listen_host  text    not null default '0.0.0.0',"
		"  listen_port  integer not null,"
		"  target_host  text    not null default '',"
		"  target_port  integer not null default 0,"
		"  enabled      integer not null default 1,"
		"  created_utc  integer not null default 0,"
		"  modified_utc integer not null default 0"
		")",

		// Events: append-only audit log. No foreign keys — a user can
		// be deleted but we want to keep its historical log entries.
		"create table if not exists events ("
		"  id          integer primary key autoincrement,"
		"  ts_utc      integer not null,"
		"  kind        text    not null,"
		"  username    text    not null default '',"
		"  tunnel_name text    not null default '',"
		"  remote_ip   text    not null default '',"
		"  detail      text    not null default ''"
		")",
		"create index if not exists events_ts_idx   on events(ts_utc)",
		"create index if not exists events_kind_idx on events(kind)",

		// Usage samples: one row per periodic snapshot per tunnel.
		// Keep cheap: no foreign keys, just an index on (tunnel, ts).
		"create table if not exists usage_samples ("
		"  id          integer primary key autoincrement,"
		"  ts_utc      integer not null,"
		"  tunnel_name text    not null,"
		"  bytes_rx    integer not null default 0,"
		"  bytes_tx    integer not null default 0,"
		"  connections integer not null default 0"
		")",
		"create index if not exists usage_samples_idx on usage_samples(tunnel_name, ts_utc)",
	};

	for (const auto sSQL : s_sDDL)
	{
		if (!db.ExecuteVoid(sSQL))
		{
			SetError(kFormat("schema init failed: {}: {}", sSQL, db.Error()));
			return false;
		}
	}

	return true;

} // InitializeSchema

// ============================= Users ====================================

//-----------------------------------------------------------------------------
std::size_t KTunnelStore::CountUsers ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return 0;

	auto stmt = db.Prepare("select count(*) from users");
	if (stmt.NextRow())
	{
		return static_cast<std::size_t>(stmt.GetRow().Col(1).Int64());
	}
	return 0;

} // CountUsers

//-----------------------------------------------------------------------------
bool KTunnelStore::AddUser (const User& user)
//-----------------------------------------------------------------------------
{
	if (user.sUsername.empty() || user.sPasswordHash.empty())
	{
		SetError("AddUser: empty username or hash");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare(
		"insert into users (username, pw_hash, is_admin, created_utc, last_login_utc) "
		"values (?1, ?2, ?3, ?4, ?5)");

	stmt.Bind(1, user.sUsername,     false);
	stmt.Bind(2, user.sPasswordHash, false);
	stmt.Bind(3, static_cast<int64_t>(user.bIsAdmin ? 1 : 0));
	stmt.Bind(4, static_cast<int64_t>((user.tCreated   == KUnixTime{}) ? KUnixTime::now().to_time_t()
	                                                                   : user.tCreated.to_time_t()));
	stmt.Bind(5, static_cast<int64_t>(user.tLastLogin.to_time_t()));

	if (!stmt.Execute())
	{
		SetError(kFormat("AddUser: insert failed: {}", db.Error()));
		return false;
	}
	return true;

} // AddUser

//-----------------------------------------------------------------------------
bool KTunnelStore::UpdateUserPasswordHash (KStringView sUsername, KStringView sBcryptHash)
//-----------------------------------------------------------------------------
{
	if (sUsername.empty() || sBcryptHash.empty())
	{
		SetError("UpdateUserPasswordHash: empty args");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare("update users set pw_hash=?1 where username=?2");
	stmt.Bind(1, sBcryptHash, false);
	stmt.Bind(2, sUsername,   false);

	if (!stmt.Execute())
	{
		SetError(kFormat("UpdateUserPasswordHash: {}", db.Error()));
		return false;
	}
	return true;

} // UpdateUserPasswordHash

//-----------------------------------------------------------------------------
bool KTunnelStore::UpdateUserAdminFlag (KStringView sUsername, bool bIsAdmin)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare("update users set is_admin=?1 where username=?2");
	stmt.Bind(1, static_cast<int64_t>(bIsAdmin ? 1 : 0));
	stmt.Bind(2, sUsername, false);

	if (!stmt.Execute())
	{
		SetError(kFormat("UpdateUserAdminFlag: {}", db.Error()));
		return false;
	}
	return true;

} // UpdateUserAdminFlag

//-----------------------------------------------------------------------------
bool KTunnelStore::DeleteUser (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare("delete from users where username=?1");
	stmt.Bind(1, sUsername, false);

	if (!stmt.Execute())
	{
		SetError(kFormat("DeleteUser: {}", db.Error()));
		return false;
	}
	return true;

} // DeleteUser

//-----------------------------------------------------------------------------
std::optional<KTunnelStore::User>
KTunnelStore::GetUser (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return std::nullopt;

	auto stmt = db.Prepare(
		"select id, username, pw_hash, is_admin, created_utc, last_login_utc "
		"from users where username=?1");
	stmt.Bind(1, sUsername, false);

	if (!stmt.NextRow())
	{
		return std::nullopt;
	}

	auto Row = stmt.GetRow();
	User u;
	u.iID           = Row.Col(1).Int64();
	u.sUsername     = Row.Col(2).String();
	u.sPasswordHash = Row.Col(3).String();
	u.bIsAdmin      = Row.Col(4).Int64() != 0;
	u.tCreated      = KUnixTime::from_time_t(Row.Col(5).Int64());
	u.tLastLogin    = KUnixTime::from_time_t(Row.Col(6).Int64());
	return u;

} // GetUser

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::User> KTunnelStore::GetAllUsers ()
//-----------------------------------------------------------------------------
{
	std::vector<User> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare(
		"select id, username, pw_hash, is_admin, created_utc, last_login_utc "
		"from users order by username asc");

	while (stmt.NextRow())
	{
		auto Row = stmt.GetRow();
		User u;
		u.iID           = Row.Col(1).Int64();
		u.sUsername     = Row.Col(2).String();
		u.sPasswordHash = Row.Col(3).String();
		u.bIsAdmin      = Row.Col(4).Int64() != 0;
		u.tCreated      = KUnixTime::from_time_t(Row.Col(5).Int64());
		u.tLastLogin    = KUnixTime::from_time_t(Row.Col(6).Int64());
		out.push_back(std::move(u));
	}

	return out;

} // GetAllUsers

//-----------------------------------------------------------------------------
void KTunnelStore::SetLastLogin (KStringView sUsername, KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return;

	auto stmt = db.Prepare("update users set last_login_utc=?1 where username=?2");
	stmt.Bind(1, static_cast<int64_t>(tNow.to_time_t()));
	stmt.Bind(2, sUsername, false);
	stmt.Execute();

} // SetLastLogin

// ============================ Tunnels ===================================

namespace {

KTunnelStore::Tunnel TunnelFromRow (KSQLite::Row Row)
{
	KTunnelStore::Tunnel t;
	t.iID          = Row.Col(1).Int64();
	t.sName        = Row.Col(2).String();
	t.sOwnerUser   = Row.Col(3).String();
	t.sListenHost  = Row.Col(4).String();
	t.iListenPort  = static_cast<uint16_t>(Row.Col(5).Int64());
	t.sTargetHost  = Row.Col(6).String();
	t.iTargetPort  = static_cast<uint16_t>(Row.Col(7).Int64());
	t.bEnabled     = Row.Col(8).Int64() != 0;
	t.tCreated     = KUnixTime::from_time_t(Row.Col(9).Int64());
	t.tModified    = KUnixTime::from_time_t(Row.Col(10).Int64());
	return t;
}

constexpr KStringView s_sTunnelCols =
	"id, name, owner_user, listen_host, listen_port, "
	"target_host, target_port, enabled, created_utc, modified_utc";

} // anonymous

//-----------------------------------------------------------------------------
bool KTunnelStore::AddTunnel (const Tunnel& t)
//-----------------------------------------------------------------------------
{
	if (t.sName.empty() || t.iListenPort == 0)
	{
		SetError("AddTunnel: name and listen_port are required");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	const auto tNow = KUnixTime::now().to_time_t();

	auto stmt = db.Prepare(
		"insert into tunnels (name, owner_user, listen_host, listen_port, "
		"                     target_host, target_port, enabled, "
		"                     created_utc, modified_utc) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");

	stmt.Bind(1, t.sName,        false);
	stmt.Bind(2, t.sOwnerUser,   false);
	stmt.Bind(3, t.sListenHost.empty() ? KStringView("0.0.0.0") : KStringView(t.sListenHost), false);
	stmt.Bind(4, static_cast<int64_t>(t.iListenPort));
	stmt.Bind(5, t.sTargetHost,  false);
	stmt.Bind(6, static_cast<int64_t>(t.iTargetPort));
	stmt.Bind(7, static_cast<int64_t>(t.bEnabled ? 1 : 0));
	stmt.Bind(8, static_cast<int64_t>(tNow));
	stmt.Bind(9, static_cast<int64_t>(tNow));

	if (!stmt.Execute())
	{
		SetError(kFormat("AddTunnel: {}", db.Error()));
		return false;
	}
	return true;

} // AddTunnel

//-----------------------------------------------------------------------------
bool KTunnelStore::UpdateTunnel (const Tunnel& t)
//-----------------------------------------------------------------------------
{
	if (t.sName.empty())
	{
		SetError("UpdateTunnel: empty name");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare(
		"update tunnels set "
		"  owner_user=?2, listen_host=?3, listen_port=?4, "
		"  target_host=?5, target_port=?6, enabled=?7, modified_utc=?8 "
		"where name=?1");

	stmt.Bind(1, t.sName,        false);
	stmt.Bind(2, t.sOwnerUser,   false);
	stmt.Bind(3, t.sListenHost,  false);
	stmt.Bind(4, static_cast<int64_t>(t.iListenPort));
	stmt.Bind(5, t.sTargetHost,  false);
	stmt.Bind(6, static_cast<int64_t>(t.iTargetPort));
	stmt.Bind(7, static_cast<int64_t>(t.bEnabled ? 1 : 0));
	stmt.Bind(8, static_cast<int64_t>(KUnixTime::now().to_time_t()));

	if (!stmt.Execute())
	{
		SetError(kFormat("UpdateTunnel: {}", db.Error()));
		return false;
	}
	return true;

} // UpdateTunnel

//-----------------------------------------------------------------------------
bool KTunnelStore::DeleteTunnel (KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare("delete from tunnels where name=?1");
	stmt.Bind(1, sName, false);

	if (!stmt.Execute())
	{
		SetError(kFormat("DeleteTunnel: {}", db.Error()));
		return false;
	}
	return true;

} // DeleteTunnel

//-----------------------------------------------------------------------------
bool KTunnelStore::SetTunnelEnabled (KStringView sName, bool bEnabled)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return false;

	auto stmt = db.Prepare(
		"update tunnels set enabled=?1, modified_utc=?2 where name=?3");
	stmt.Bind(1, static_cast<int64_t>(bEnabled ? 1 : 0));
	stmt.Bind(2, static_cast<int64_t>(KUnixTime::now().to_time_t()));
	stmt.Bind(3, sName, false);

	if (!stmt.Execute())
	{
		SetError(kFormat("SetTunnelEnabled: {}", db.Error()));
		return false;
	}
	return true;

} // SetTunnelEnabled

//-----------------------------------------------------------------------------
std::optional<KTunnelStore::Tunnel>
KTunnelStore::GetTunnel (KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return std::nullopt;

	auto stmt = db.Prepare(kFormat("select {} from tunnels where name=?1", s_sTunnelCols));
	stmt.Bind(1, sName, false);

	if (!stmt.NextRow())
	{
		return std::nullopt;
	}

	return TunnelFromRow(stmt.GetRow());

} // GetTunnel

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Tunnel> KTunnelStore::GetAllTunnels ()
//-----------------------------------------------------------------------------
{
	std::vector<Tunnel> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare(kFormat("select {} from tunnels order by name asc", s_sTunnelCols));

	while (stmt.NextRow())
	{
		out.push_back(TunnelFromRow(stmt.GetRow()));
	}

	return out;

} // GetAllTunnels

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Tunnel> KTunnelStore::GetEnabledTunnels ()
//-----------------------------------------------------------------------------
{
	std::vector<Tunnel> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare(kFormat(
		"select {} from tunnels where enabled=1 order by name asc", s_sTunnelCols));

	while (stmt.NextRow())
	{
		out.push_back(TunnelFromRow(stmt.GetRow()));
	}

	return out;

} // GetEnabledTunnels

// ============================= Events ===================================

//-----------------------------------------------------------------------------
void KTunnelStore::LogEvent (const Event& ev)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return;

	auto stmt = db.Prepare(
		"insert into events (ts_utc, kind, username, tunnel_name, remote_ip, detail) "
		"values (?1, ?2, ?3, ?4, ?5, ?6)");

	const auto tTs = (ev.tTimestamp == KUnixTime{}) ? KUnixTime::now() : ev.tTimestamp;

	stmt.Bind(1, static_cast<int64_t>(tTs.to_time_t()));
	stmt.Bind(2, ev.sKind,       false);
	stmt.Bind(3, ev.sUsername,   false);
	stmt.Bind(4, ev.sTunnelName, false);
	stmt.Bind(5, ev.sRemoteIP,   false);
	stmt.Bind(6, ev.sDetail,     false);

	if (!stmt.Execute())
	{
		SetError(kFormat("LogEvent: {}", db.Error()));
	}

} // LogEvent

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Event>
KTunnelStore::GetRecentEvents (std::size_t iLimit)
//-----------------------------------------------------------------------------
{
	std::vector<Event> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare(
		"select id, ts_utc, kind, username, tunnel_name, remote_ip, detail "
		"from events order by ts_utc desc, id desc limit ?1");
	stmt.Bind(1, static_cast<int64_t>(iLimit));

	while (stmt.NextRow())
	{
		auto Row = stmt.GetRow();
		Event e;
		e.iID         = Row.Col(1).Int64();
		e.tTimestamp  = KUnixTime::from_time_t(Row.Col(2).Int64());
		e.sKind       = Row.Col(3).String();
		e.sUsername   = Row.Col(4).String();
		e.sTunnelName = Row.Col(5).String();
		e.sRemoteIP   = Row.Col(6).String();
		e.sDetail     = Row.Col(7).String();
		out.push_back(std::move(e));
	}

	return out;

} // GetRecentEvents

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Event>
KTunnelStore::GetEvents (KStringView sKind, KStringView sUsername, std::size_t iLimit)
//-----------------------------------------------------------------------------
{
	std::vector<Event> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	// Empty filter strings collapse to "no constraint" via the
	// (?1 = '' OR ...) idiom. This keeps a single prepared statement
	// serving all four (kind, user) combinations without string
	// concatenation — and therefore without any SQL injection risk.
	auto stmt = db.Prepare(
		"select id, ts_utc, kind, username, tunnel_name, remote_ip, detail "
		"from events "
		"where (?1 = '' or kind = ?1) "
		"  and (?2 = '' or username = ?2) "
		"order by ts_utc desc, id desc limit ?3");

	stmt.Bind(1, KString(sKind));
	stmt.Bind(2, KString(sUsername));
	stmt.Bind(3, static_cast<int64_t>(iLimit));

	while (stmt.NextRow())
	{
		auto Row = stmt.GetRow();
		Event e;
		e.iID         = Row.Col(1).Int64();
		e.tTimestamp  = KUnixTime::from_time_t(Row.Col(2).Int64());
		e.sKind       = Row.Col(3).String();
		e.sUsername   = Row.Col(4).String();
		e.sTunnelName = Row.Col(5).String();
		e.sRemoteIP   = Row.Col(6).String();
		e.sDetail     = Row.Col(7).String();
		out.push_back(std::move(e));
	}

	return out;

} // GetEvents

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Event>
KTunnelStore::GetRecentEventsForOwner (KStringView sUsername, std::size_t iLimit)
//-----------------------------------------------------------------------------
{
	std::vector<Event> out;

	// An empty username would match all events where username=''. We
	// refuse that here — callers must supply a real identity. Dashboard
	// code checks IsLoggedIn() before getting here.
	if (sUsername.empty()) return out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	// Two-prong match: the event was either caused by this user
	// (events.username) or concerns one of its tunnels (tunnel owned
	// by this user). The subquery is cheap because `tunnels.name` is
	// the primary key and the row count is small.
	auto stmt = db.Prepare(
		"select id, ts_utc, kind, username, tunnel_name, remote_ip, detail "
		"from events "
		"where username = ?1 "
		"   or tunnel_name in (select name from tunnels where owner_user = ?1) "
		"order by ts_utc desc, id desc limit ?2");

	stmt.Bind(1, KString(sUsername));
	stmt.Bind(2, static_cast<int64_t>(iLimit));

	while (stmt.NextRow())
	{
		auto Row = stmt.GetRow();
		Event e;
		e.iID         = Row.Col(1).Int64();
		e.tTimestamp  = KUnixTime::from_time_t(Row.Col(2).Int64());
		e.sKind       = Row.Col(3).String();
		e.sUsername   = Row.Col(4).String();
		e.sTunnelName = Row.Col(5).String();
		e.sRemoteIP   = Row.Col(6).String();
		e.sDetail     = Row.Col(7).String();
		out.push_back(std::move(e));
	}

	return out;

} // GetRecentEventsForOwner

//-----------------------------------------------------------------------------
std::vector<KString> KTunnelStore::GetDistinctEventKinds ()
//-----------------------------------------------------------------------------
{
	std::vector<KString> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare("select distinct kind from events order by kind asc");
	while (stmt.NextRow()) out.push_back(stmt.GetRow().Col(1).String());

	return out;

} // GetDistinctEventKinds

// =========================== Usage samples ==============================

//-----------------------------------------------------------------------------
void KTunnelStore::LogUsageSample (const UsageSample& sample)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (db.IsError()) return;

	auto stmt = db.Prepare(
		"insert into usage_samples (ts_utc, tunnel_name, bytes_rx, bytes_tx, connections) "
		"values (?1, ?2, ?3, ?4, ?5)");

	const auto tTs = (sample.tTimestamp == KUnixTime{}) ? KUnixTime::now() : sample.tTimestamp;

	stmt.Bind(1, static_cast<int64_t>(tTs.to_time_t()));
	stmt.Bind(2, sample.sTunnelName, false);
	stmt.Bind(3, static_cast<int64_t>(sample.iBytesRx));
	stmt.Bind(4, static_cast<int64_t>(sample.iBytesTx));
	stmt.Bind(5, static_cast<int64_t>(sample.iConnections));

	if (!stmt.Execute())
	{
		SetError(kFormat("LogUsageSample: {}", db.Error()));
	}

} // LogUsageSample

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::UsageSample>
KTunnelStore::GetRecentUsage (KStringView sTunnelName, std::size_t iLimit)
//-----------------------------------------------------------------------------
{
	std::vector<UsageSample> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (db.IsError()) return out;

	auto stmt = db.Prepare(
		"select id, ts_utc, tunnel_name, bytes_rx, bytes_tx, connections "
		"from usage_samples where tunnel_name=?1 "
		"order by ts_utc desc, id desc limit ?2");
	stmt.Bind(1, sTunnelName, false);
	stmt.Bind(2, static_cast<int64_t>(iLimit));

	while (stmt.NextRow())
	{
		auto Row = stmt.GetRow();
		UsageSample s;
		s.iID          = Row.Col(1).Int64();
		s.tTimestamp   = KUnixTime::from_time_t(Row.Col(2).Int64());
		s.sTunnelName  = Row.Col(3).String();
		s.iBytesRx     = static_cast<uint64_t>(Row.Col(4).Int64());
		s.iBytesTx     = static_cast<uint64_t>(Row.Col(5).Int64());
		s.iConnections = static_cast<std::size_t>(Row.Col(6).Int64());
		out.push_back(std::move(s));
	}

	return out;

} // GetRecentUsage
