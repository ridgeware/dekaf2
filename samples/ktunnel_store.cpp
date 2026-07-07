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
KString KTunnelStore::DefaultDatabasePath(bool bPreferSystemPath)
//-----------------------------------------------------------------------------
{
	if (bPreferSystemPath)
	{
#if defined(DEKAF2_IS_WINDOWS)
		// Windows services typically run as LocalSystem; %PROGRAMDATA%
		// (C:\ProgramData) is the canonical machine-wide state location.
		auto sProgData = kGetEnv("PROGRAMDATA");

		if (!sProgData.empty())
		{
			return kFormat("{}{}ktunnel{}ktunnel.db",
			               sProgData, kDirSep, kDirSep);
		}
#else
		// FHS-conformant state directory on Linux and macOS. The
		// systemd / launchd unit is expected to create it with
		// appropriate ownership (root:root 0700 / _ktunnel:_ktunnel
		// 0700); we do not chmod from inside the process because
		// that would paper over operator-side misconfiguration.
		return "/var/lib/ktunnel/ktunnel.db";
#endif
		// Fall through to the user path if the system path could
		// not be built.
	}

	// kGetConfigPath() defaults to $HOME/.config/<progname> (ktunnel), creates
	// the directory on demand with mode 0700, and appends a path separator.
	// That matches our "admin store" security expectation (user-private) without
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
	if (!db.IsOpen())
	{
		SetError(kFormat("cannot open '{}' for write", m_sDatabase));
	}
	return db;

} // OpenRW

//-----------------------------------------------------------------------------
KSQLite::Database KTunnelStore::OpenRO ()
//-----------------------------------------------------------------------------
{
	KSQLite::Database db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen())
	{
		SetError(kFormat("cannot open '{}' for read", m_sDatabase));
	}
	return db;

} // OpenRO

//-----------------------------------------------------------------------------
bool KTunnelStore::InitializeSchema ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	// WAL gives us concurrent readers while the admin UI / event logger is
	// writing. It persists as a DB attribute, but setting it is idempotent.
	db.ExecSQL("pragma journal_mode=WAL");
	db.ExecSQL("pragma synchronous=NORMAL");
	db.ExecSQL("pragma foreign_keys=ON");

	static constexpr KStringView s_sDDL[] =
	{
		// Admins: unique username, bcrypt hash. Every row in this table
		// is, by definition, allowed to sign in to the web UI. There is
		// no separate is_admin flag — having a row IS the admin grant.
		"create table if not exists admins ("
		"  id             integer primary key autoincrement,"
		"  username       text    not null unique,"
		"  pw_hash        text    not null,"
		"  created_utc    integer not null default 0,"
		"  last_login_utc integer not null default 0"
		")",

		// Nodes: tunnel-endpoint accounts. A node is the credential the
		// remote ktunnel instance presents during the v2 handshake (or
		// the legacy Basic-auth login frame). Nodes have NO web UI
		// access; the UI is admin-only. The `enabled` bit lets an admin
		// temporarily lock out a remote endpoint without losing its
		// configuration or its tunnel ownership.
		"create table if not exists nodes ("
		"  id             integer primary key autoincrement,"
		"  name           text    not null unique,"
		"  pw_hash        text    not null,"
		"  enabled        integer not null default 1,"
		"  created_utc    integer not null default 0,"
		"  last_login_utc integer not null default 0"
		")",

		// Tunnels: one row per listener the admin configured. A tunnel
		// is keyed by name (human-readable); the owning node decides
		// whose ktunnel login is accepted for this listener.
		//
		// Note: historically there was a listen_host column here, but
		// KTCPServer always binds to the wildcard address (0.0.0.0 +
		// [::] dual-stack), so the value was silently ignored. The
		// column was dropped from the schema; existing DBs keep it at
		// its default '0.0.0.0' but the app never reads it again.
		"create table if not exists tunnels ("
		"  id           integer primary key autoincrement,"
		"  name         text    not null unique,"
		"  node         text    not null default '',"
		"  listen_port  integer not null,"
		"  target_host  text    not null default '',"
		"  target_port  integer not null default 0,"
		"  enabled      integer not null default 1,"
		"  created_utc  integer not null default 0,"
		"  modified_utc integer not null default 0"
		")",

		// Events: append-only audit log. No foreign keys — an admin or
		// a node can be deleted but we want to keep its historical log
		// entries. Two actor columns:
		//   * `admin` for events caused by an admin via the web UI
		//     (admin_add, node_disable, config_change, ...)
		//   * `node`  for events caused by tunnel-endpoint activity
		//     (node_login_ok, node_login_fail, handshake_fail,
		//      tunnel_disconnect, ...)
		// A given event row populates at most one of these; the dual
		// columns make audit queries unambiguous instead of overloading
		// a single "username" string.
		"create table if not exists events ("
		"  id          integer primary key autoincrement,"
		"  ts_utc      integer not null,"
		"  kind        text    not null,"
		"  admin       text    not null default '',"
		"  node        text    not null default '',"
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
		auto Result = db.ExecSQL(sSQL);
		if (!Result)
		{
			// Migration-style statements (ALTER TABLE ...) are best-effort:
			// they fail on older SQLite versions without DROP COLUMN support,
			// on a second run when the column is already gone, and on fresh
			// DBs where the legacy column was never created. None of these
			// are fatal — the app treats the column as non-existent anyway.
			if (sSQL.starts_with("alter table"))
			{
				kDebug(2, "ignoring expected DDL error on '{}': {}", sSQL, Result.Error());
				continue;
			}
			SetError(kFormat("schema init failed: {}: {}", sSQL, Result.Error()));
			return false;
		}
	}

	return true;

} // InitializeSchema

// ============================= Admins ===================================
//
// UI identities. Every row here can sign into /Configure/. Admins are NOT
// usable as tunnel-endpoint identities — those live in the `nodes` table
// and authenticate the v2 handshake / Basic-auth login frame instead.

//-----------------------------------------------------------------------------
std::size_t KTunnelStore::CountAdmins ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from admins"));

} // CountAdmins

//-----------------------------------------------------------------------------
bool KTunnelStore::AddAdmin (const Admin& admin)
//-----------------------------------------------------------------------------
{
	if (admin.sUsername.empty() || admin.sPasswordHash.empty())
	{
		SetError("AddAdmin: empty username or hash");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL(
		"insert into admins (username, pw_hash, created_utc, last_login_utc) "
		"values (?1, ?2, ?3, ?4)",
		admin.sUsername,
		admin.sPasswordHash,
		static_cast<int64_t>((admin.tCreated == KUnixTime{}) ? KUnixTime::now().to_time_t()
		                                                     : admin.tCreated.to_time_t()),
		static_cast<int64_t>(admin.tLastLogin.to_time_t()));

	if (!Result)
	{
		SetError(kFormat("AddAdmin: insert failed: {}", Result.Error()));
		return false;
	}
	return true;

} // AddAdmin

//-----------------------------------------------------------------------------
bool KTunnelStore::UpdateAdminPasswordHash (KStringView sUsername, KStringView sBcryptHash)
//-----------------------------------------------------------------------------
{
	if (sUsername.empty() || sBcryptHash.empty())
	{
		SetError("UpdateAdminPasswordHash: empty args");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("update admins set pw_hash=?1 where username=?2",
	                         sBcryptHash, sUsername);

	if (!Result)
	{
		SetError(kFormat("UpdateAdminPasswordHash: {}", Result.Error()));
		return false;
	}
	return true;

} // UpdateAdminPasswordHash

//-----------------------------------------------------------------------------
bool KTunnelStore::DeleteAdmin (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("delete from admins where username=?1", sUsername);

	if (!Result)
	{
		SetError(kFormat("DeleteAdmin: {}", Result.Error()));
		return false;
	}
	return true;

} // DeleteAdmin

//-----------------------------------------------------------------------------
std::unique_ptr<KTunnelStore::Admin>
KTunnelStore::GetAdmin (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return nullptr;

	auto Query = db.ExecQuery(
		"select id, username, pw_hash, created_utc, last_login_utc "
		"from admins where username=?1", sUsername);

	if (!Query.Next())
	{
		return nullptr;
	}

	auto& Row = Query.GetRow();
	auto a = std::make_unique<Admin>();
	a->iID           = Row.Col(1).Int64();
	a->sUsername     = Row.Col(2).String();
	a->sPasswordHash = Row.Col(3).String();
	a->tCreated      = KUnixTime::from_time_t(Row.Col(4).Int64());
	a->tLastLogin    = KUnixTime::from_time_t(Row.Col(5).Int64());
	return a;

} // GetAdmin

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Admin> KTunnelStore::GetAllAdmins ()
//-----------------------------------------------------------------------------
{
	std::vector<Admin> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	for (auto& Row : db.ExecQuery(
		"select id, username, pw_hash, created_utc, last_login_utc "
		"from admins order by username asc"))
	{
		Admin a;
		a.iID           = Row.Col(1).Int64();
		a.sUsername     = Row.Col(2).String();
		a.sPasswordHash = Row.Col(3).String();
		a.tCreated      = KUnixTime::from_time_t(Row.Col(4).Int64());
		a.tLastLogin    = KUnixTime::from_time_t(Row.Col(5).Int64());
		out.push_back(std::move(a));
	}

	return out;

} // GetAllAdmins

//-----------------------------------------------------------------------------
void KTunnelStore::SetAdminLastLogin (KStringView sUsername, KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return;

	db.ExecSQL("update admins set last_login_utc=?1 where username=?2",
	           static_cast<int64_t>(tNow.to_time_t()), sUsername);

} // SetAdminLastLogin

// ============================= Nodes ====================================
//
// Tunnel-endpoint identities. A node authenticates the ktunnel control
// stream and owns one or more listener configurations in `tunnels`. Nodes
// have NO web UI access; toggling `enabled` lets an admin lock out an
// endpoint without losing its config.

namespace {

KTunnelStore::Node NodeFromRow (const KSQLite::Row& Row)
{
	KTunnelStore::Node n;
	n.iID           = Row.Col(1).Int64();
	n.sName         = Row.Col(2).String();
	n.sPasswordHash = Row.Col(3).String();
	n.bEnabled      = Row.Col(4).Int64() != 0;
	n.tCreated      = KUnixTime::from_time_t(Row.Col(5).Int64());
	n.tLastLogin    = KUnixTime::from_time_t(Row.Col(6).Int64());
	return n;
}

constexpr KStringView s_sNodeCols =
	"id, name, pw_hash, enabled, created_utc, last_login_utc";

} // anonymous

//-----------------------------------------------------------------------------
std::size_t KTunnelStore::CountNodes ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from nodes"));

} // CountNodes

//-----------------------------------------------------------------------------
bool KTunnelStore::AddNode (const Node& node)
//-----------------------------------------------------------------------------
{
	if (node.sName.empty() || node.sPasswordHash.empty())
	{
		SetError("AddNode: empty name or hash");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL(
		"insert into nodes (name, pw_hash, enabled, created_utc, last_login_utc) "
		"values (?1, ?2, ?3, ?4, ?5)",
		node.sName,
		node.sPasswordHash,
		static_cast<int64_t>(node.bEnabled ? 1 : 0),
		static_cast<int64_t>((node.tCreated == KUnixTime{}) ? KUnixTime::now().to_time_t()
		                                                    : node.tCreated.to_time_t()),
		static_cast<int64_t>(node.tLastLogin.to_time_t()));

	if (!Result)
	{
		SetError(kFormat("AddNode: insert failed: {}", Result.Error()));
		return false;
	}
	return true;

} // AddNode

//-----------------------------------------------------------------------------
bool KTunnelStore::UpdateNodePasswordHash (KStringView sName, KStringView sBcryptHash)
//-----------------------------------------------------------------------------
{
	if (sName.empty() || sBcryptHash.empty())
	{
		SetError("UpdateNodePasswordHash: empty args");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("update nodes set pw_hash=?1 where name=?2",
	                         sBcryptHash, sName);

	if (!Result)
	{
		SetError(kFormat("UpdateNodePasswordHash: {}", Result.Error()));
		return false;
	}
	return true;

} // UpdateNodePasswordHash

//-----------------------------------------------------------------------------
bool KTunnelStore::SetNodeEnabled (KStringView sName, bool bEnabled)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("update nodes set enabled=?1 where name=?2",
	                         static_cast<int64_t>(bEnabled ? 1 : 0), sName);

	if (!Result)
	{
		SetError(kFormat("SetNodeEnabled: {}", Result.Error()));
		return false;
	}
	return true;

} // SetNodeEnabled

//-----------------------------------------------------------------------------
bool KTunnelStore::DeleteNode (KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("delete from nodes where name=?1", sName);

	if (!Result)
	{
		SetError(kFormat("DeleteNode: {}", Result.Error()));
		return false;
	}
	return true;

} // DeleteNode

//-----------------------------------------------------------------------------
std::unique_ptr<KTunnelStore::Node>
KTunnelStore::GetNode (KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return nullptr;

	auto Query = db.ExecQuery(kFormat("select {} from nodes where name=?1", s_sNodeCols), sName);

	if (!Query.Next())
	{
		return nullptr;
	}

	return std::make_unique<Node>(NodeFromRow(Query.GetRow()));

} // GetNode

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Node> KTunnelStore::GetAllNodes ()
//-----------------------------------------------------------------------------
{
	std::vector<Node> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(kFormat("select {} from nodes order by name asc", s_sNodeCols));

	while (Query.Next())
	{
		out.push_back(NodeFromRow(Query.GetRow()));
	}

	return out;

} // GetAllNodes

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Node> KTunnelStore::GetEnabledNodes ()
//-----------------------------------------------------------------------------
{
	std::vector<Node> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(kFormat(
		"select {} from nodes where enabled=1 order by name asc", s_sNodeCols));

	while (Query.Next())
	{
		out.push_back(NodeFromRow(Query.GetRow()));
	}

	return out;

} // GetEnabledNodes

//-----------------------------------------------------------------------------
void KTunnelStore::SetNodeLastLogin (KStringView sName, KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return;

	db.ExecSQL("update nodes set last_login_utc=?1 where name=?2",
	           static_cast<int64_t>(tNow.to_time_t()), sName);

} // SetNodeLastLogin

// ============================ Tunnels ===================================

namespace {

KTunnelStore::Tunnel TunnelFromRow (const KSQLite::Row& Row)
{
	KTunnelStore::Tunnel t;
	t.iID          = Row.Col(1).Int64();
	t.sName        = Row.Col(2).String();
	t.sNode        = Row.Col(3).String();
	t.iListenPort  = static_cast<uint16_t>(Row.Col(4).Int64());
	t.sTargetHost  = Row.Col(5).String();
	t.iTargetPort  = static_cast<uint16_t>(Row.Col(6).Int64());
	t.bEnabled     = Row.Col(7).Int64() != 0;
	t.tCreated     = KUnixTime::from_time_t(Row.Col(8).Int64());
	t.tModified    = KUnixTime::from_time_t(Row.Col(9).Int64());
	return t;
}

constexpr KStringView s_sTunnelCols =
	"id, name, node, listen_port, "
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

	// "cli" is reserved for the synthetic listener produced by
	// ExposedServer::GatherDesiredTunnels() when the legacy
	// `-f <port> -t <target>` CLI arguments are present. A DB row with
	// the same name would silently be displaced by that synthetic row
	// in ReconcileListeners, so refuse it up front with a clear error.
	if (t.sName == "cli")
	{
		SetError("AddTunnel: 'cli' is a reserved name for the CLI-driven tunnel");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return false;

	const auto tNow = KUnixTime::now().to_time_t();

	auto Result = db.ExecSQL(
		"insert into tunnels (name, node, listen_port, "
		"                     target_host, target_port, enabled, "
		"                     created_utc, modified_utc) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
		t.sName,
		t.sNode,
		static_cast<int64_t>(t.iListenPort),
		t.sTargetHost,
		static_cast<int64_t>(t.iTargetPort),
		static_cast<int64_t>(t.bEnabled ? 1 : 0),
		static_cast<int64_t>(tNow),
		static_cast<int64_t>(tNow));

	if (!Result)
	{
		SetError(kFormat("AddTunnel: {}", Result.Error()));
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
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL(
		"update tunnels set "
		"  node=?2, listen_port=?3, "
		"  target_host=?4, target_port=?5, enabled=?6, modified_utc=?7 "
		"where name=?1",
		t.sName,
		t.sNode,
		static_cast<int64_t>(t.iListenPort),
		t.sTargetHost,
		static_cast<int64_t>(t.iTargetPort),
		static_cast<int64_t>(t.bEnabled ? 1 : 0),
		static_cast<int64_t>(KUnixTime::now().to_time_t()));

	if (!Result)
	{
		SetError(kFormat("UpdateTunnel: {}", Result.Error()));
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
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL("delete from tunnels where name=?1", sName);

	if (!Result)
	{
		SetError(kFormat("DeleteTunnel: {}", Result.Error()));
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
	if (!db.IsOpen()) return false;

	auto Result = db.ExecSQL(
		"update tunnels set enabled=?1, modified_utc=?2 where name=?3",
		static_cast<int64_t>(bEnabled ? 1 : 0),
		static_cast<int64_t>(KUnixTime::now().to_time_t()),
		sName);

	if (!Result)
	{
		SetError(kFormat("SetTunnelEnabled: {}", Result.Error()));
		return false;
	}
	return true;

} // SetTunnelEnabled

//-----------------------------------------------------------------------------
std::unique_ptr<KTunnelStore::Tunnel>
KTunnelStore::GetTunnel (KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return nullptr;

	auto Query = db.ExecQuery(kFormat("select {} from tunnels where name=?1", s_sTunnelCols), sName);

	if (!Query.Next())
	{
		return nullptr;
	}

	return std::make_unique<Tunnel>(TunnelFromRow(Query.GetRow()));

} // GetTunnel

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Tunnel> KTunnelStore::GetAllTunnels ()
//-----------------------------------------------------------------------------
{
	std::vector<Tunnel> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(kFormat("select {} from tunnels order by name asc", s_sTunnelCols));

	while (Query.Next())
	{
		out.push_back(TunnelFromRow(Query.GetRow()));
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
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(kFormat(
		"select {} from tunnels where enabled=1 order by name asc", s_sTunnelCols));

	while (Query.Next())
	{
		out.push_back(TunnelFromRow(Query.GetRow()));
	}

	return out;

} // GetEnabledTunnels

// ============================= Events ===================================
//
// Two-actor model: every event row carries at most one of (admin, node).
// Admin actions (UI write paths) populate the admin column; tunnel
// lifecycle / handshake events populate the node column. This keeps audit
// queries unambiguous without overloading a single "who" string.

namespace {

constexpr KStringView s_sEventCols =
	"id, ts_utc, kind, admin, node, tunnel_name, remote_ip, detail";

KTunnelStore::Event EventFromRow (const KSQLite::Row& Row)
{
	KTunnelStore::Event e;
	e.iID         = Row.Col(1).Int64();
	e.tTimestamp  = KUnixTime::from_time_t(Row.Col(2).Int64());
	e.sKind       = Row.Col(3).String();
	e.sAdmin      = Row.Col(4).String();
	e.sNode       = Row.Col(5).String();
	e.sTunnelName = Row.Col(6).String();
	e.sRemoteIP   = Row.Col(7).String();
	e.sDetail     = Row.Col(8).String();
	return e;
}

} // anonymous

//-----------------------------------------------------------------------------
void KTunnelStore::LogEvent (const Event& ev)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return;

	const auto tTs = (ev.tTimestamp == KUnixTime{}) ? KUnixTime::now() : ev.tTimestamp;

	auto Result = db.ExecSQL(
		"insert into events (ts_utc, kind, admin, node, tunnel_name, remote_ip, detail) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
		static_cast<int64_t>(tTs.to_time_t()),
		ev.sKind,
		ev.sAdmin,
		ev.sNode,
		ev.sTunnelName,
		ev.sRemoteIP,
		ev.sDetail);

	if (!Result)
	{
		SetError(kFormat("LogEvent: {}", Result.Error()));
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
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(kFormat(
		"select {} from events order by ts_utc desc, id desc limit ?1", s_sEventCols),
		static_cast<int64_t>(iLimit));

	while (Query.Next())
	{
		out.push_back(EventFromRow(Query.GetRow()));
	}

	return out;

} // GetRecentEvents

//-----------------------------------------------------------------------------
std::vector<KTunnelStore::Event>
KTunnelStore::GetEvents (KStringView sKind, std::size_t iLimit)
//-----------------------------------------------------------------------------
{
	std::vector<Event> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	// Empty filter string collapses to "no constraint" via the
	// (?1 = '' OR ...) idiom. Keeps a single prepared statement
	// serving both "all events" and "filter by kind" without string
	// concatenation — and therefore without any SQL injection risk.
	auto Query = db.ExecQuery(kFormat(
		"select {} from events "
		"where (?1 = '' or kind = ?1) "
		"order by ts_utc desc, id desc limit ?2", s_sEventCols),
		sKind, static_cast<int64_t>(iLimit));

	while (Query.Next())
	{
		out.push_back(EventFromRow(Query.GetRow()));
	}

	return out;

} // GetEvents

//-----------------------------------------------------------------------------
std::vector<KString> KTunnelStore::GetDistinctEventKinds ()
//-----------------------------------------------------------------------------
{
	std::vector<KString> out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRO();
	if (!db.IsOpen()) return out;

	for (auto& Row : db.ExecQuery("select distinct kind from events order by kind asc"))
	{
		out.push_back(Row.Col(1).String());
	}

	return out;

} // GetDistinctEventKinds

// =========================== Usage samples ==============================

//-----------------------------------------------------------------------------
void KTunnelStore::LogUsageSample (const UsageSample& sample)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto db = OpenRW();
	if (!db.IsOpen()) return;

	const auto tTs = (sample.tTimestamp == KUnixTime{}) ? KUnixTime::now() : sample.tTimestamp;

	auto Result = db.ExecSQL(
		"insert into usage_samples (ts_utc, tunnel_name, bytes_rx, bytes_tx, connections) "
		"values (?1, ?2, ?3, ?4, ?5)",
		static_cast<int64_t>(tTs.to_time_t()),
		sample.sTunnelName,
		static_cast<int64_t>(sample.iBytesRx),
		static_cast<int64_t>(sample.iBytesTx),
		static_cast<int64_t>(sample.iConnections));

	if (!Result)
	{
		SetError(kFormat("LogUsageSample: {}", Result.Error()));
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
	if (!db.IsOpen()) return out;

	auto Query = db.ExecQuery(
		"select id, ts_utc, tunnel_name, bytes_rx, bytes_tx, connections "
		"from usage_samples where tunnel_name=?1 "
		"order by ts_utc desc, id desc limit ?2",
		sTunnelName, static_cast<int64_t>(iLimit));

	while (Query.Next())
	{
		auto& Row = Query.GetRow();
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
