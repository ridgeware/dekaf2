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

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/time/clock/ktime.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace dekaf2 {}
using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Persistent configuration + audit store for ktunnel.
///
/// Uses a single SQLite file. The default path is
///   `$HOME/.config/ktunnel/ktunnel.db` (cf. kGetConfigPath()),
/// but the caller can override it (e.g. from the `-db` CLI flag).
///
/// Schema:
///   - `admins`        : admin login accounts (bcrypt-hashed). Admins are the
///                       only identity that can sign into the web UI.
///   - `nodes`         : outlet accounts (bcrypt-hashed) - the table keeps
///                       its legacy name. Outlets authenticate the ktunnel
///                       control stream and own listener configurations,
///                       but cannot reach the UI.
///   - `inlets`        : inlet accounts (bcrypt-hashed).
///   - `tunnels`       : per-tunnel listener configuration, owned by an outlet.
///   - `events`        : append-only audit log (admin actions, tunnel
///                       lifecycle, login outcomes). Distinguishes the
///                       admin-actor (admin column) from the outlet-actor
///                       (node column) so audit queries are unambiguous.
///                       Outlet event kinds keep their historical
///                       names (node_login_ok, ...) so existing databases
///                       stay consistent; inlet kinds are inlet_*.
///   - `usage_samples` : periodic RX/TX / connection-count snapshots per tunnel.
///
/// All write operations are serialized by an internal mutex — SQLite itself
/// is fine with concurrent reads from multiple threads, but `ExecSQL`-style
/// writes through a single `KSQLite` instance are simpler to reason
/// about with an external lock. Reads are also taken under the lock for
/// simplicity; lock contention is a non-issue for admin UI traffic.
class KTunnelStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Default DB path. Default location (`bPreferSystemPath = false`):
	/// `$HOME/.config/ktunnel/ktunnel.db` for interactive and
	/// user-scope service launches.
	///
	/// When @p bPreferSystemPath is true the caller is asking for a
	/// system-wide path (typically because the process was launched
	/// as a root service by systemd / launchd / SCM):
	///   - Linux / macOS: `/var/lib/ktunnel/ktunnel.db`
	///   - Windows:       `%PROGRAMDATA%\ktunnel\ktunnel.db`
	///
	/// If the system path cannot be built (no PROGRAMDATA on Windows,
	/// or any other unexpected failure) the function silently falls
	/// back to the user path.
	static KString DefaultDatabasePath(bool bPreferSystemPath = false);

	/// Construct around a SQLite file path. The file and its parent
	/// directory are created on demand (mode 0700 for the dir, per
	/// kGetConfigPath()). The ctor runs the schema migration, so
	/// HasError()/GetLastError() can be queried right after construction.
	explicit KTunnelStore (KString sDatabase);

	// --- Admins -----------------------------------------------------------
	//
	// "Admin" replaces the historical "user with is_admin=1" concept. Every
	// row in the admins table can sign into the web UI; there is no other
	// kind of UI identity. Tunnel-endpoint identities live in a separate
	// `nodes` table — see below — and intentionally cannot reach the UI.

	struct Admin
	{
		int64_t   iID           { 0 };
		KString   sUsername;     ///< login name (DB column kept as `username` so
		                         ///< the HTML login form's `name="username"`
		                         ///< plays nicely with browser autofill)
		KString   sPasswordHash; ///< bcrypt hash (60 chars)
		KUnixTime tCreated;
		KUnixTime tLastLogin;    ///< zero if never logged in
	};

	/// Count admins. Handy for bootstrap ("is this a fresh install?").
	std::size_t       CountAdmins ();

	/// Insert an admin row, returns false if the username already exists.
	bool              AddAdmin    (const Admin& admin);

	/// Overwrite the bcrypt hash for an existing admin.
	bool              UpdateAdminPasswordHash (KStringView sUsername, KStringView sBcryptHash);

	/// Remove an admin row.
	bool              DeleteAdmin             (KStringView sUsername);

	/// Fetch one admin. Returns a null unique_ptr if the admin does not exist.
	std::unique_ptr<Admin> GetAdmin           (KStringView sUsername);

	/// Return all admins sorted by username ascending.
	std::vector<Admin> GetAllAdmins           ();

	/// Stamp the last_login_utc column with the given time.
	void              SetAdminLastLogin       (KStringView sUsername, KUnixTime tNow);

	// --- Outlets ------------------------------------------------------------
	//
	// An outlet is the account of the ktunnel that runs inside the target
	// network and dials out to the relay: the credentials it presents in
	// its v2 hello / Basic-auth login frame. Outlets do NOT have any web
	// UI access; they exist only so the relay can authenticate inbound
	// control connections. Disabling an outlet lets an admin temporarily
	// lock out a remote endpoint without losing its configuration.
	// Stored in the legacy `nodes` table.

	struct Outlet
	{
		int64_t   iID           { 0 };
		KString   sName;         ///< endpoint name, unique (used as login id)
		KString   sPasswordHash; ///< bcrypt hash (60 chars)
		bool      bEnabled      { true };
		KUnixTime tCreated;
		KUnixTime tLastLogin;    ///< zero if never logged in
	};

	/// Count outlets (all, regardless of enabled state).
	std::size_t       CountOutlets ();

	/// Insert a node row, returns false if the name already exists.
	bool              AddOutlet  (const Outlet& outlet);

	/// Overwrite the bcrypt hash for an existing node.
	bool              UpdateOutletPasswordHash (KStringView sName, KStringView sBcryptHash);

	/// Toggle the `enabled` bit of an existing node.
	bool              SetOutletEnabled       (KStringView sName, bool bEnabled);

	/// Remove a node row. Tunnels referencing this node keep their FK as a
	/// dangling reference — the admin UI flags such tunnels and the
	/// ReconcileListeners loop refuses to start them.
	bool              DeleteOutlet           (KStringView sName);

	/// Fetch one node. Returns a null unique_ptr if it does not exist.
	std::unique_ptr<Outlet> GetOutlet        (KStringView sName);

	/// Return all nodes sorted by name ascending.
	std::vector<Outlet> GetAllOutlets        ();

	/// Return only the enabled nodes (used by ReconcileListeners and by
	/// the admin UI's tunnel-owner dropdown).
	std::vector<Outlet> GetEnabledOutlets    ();

	/// Stamp the last_login_utc column for the given node with the given
	/// time. Called from the tunnel auth callback on successful login.
	void              SetOutletLastLogin     (KStringView sName, KUnixTime tNow);

	// --- Inlets -----------------------------------------------------------
	//
	// Inlet identities are the third realm, next to `admins` (web UI) and
	// outlets. An inlet is an operator-side ktunnel that connects IN to
	// the relay, authenticates, and asks for a forwarding channel to a
	// *named tunnel*. It never names a host itself, so an inlet account
	// cannot turn the relay into an open proxy.

	struct Inlet
	{
		int64_t   iID           { 0 };
		KString   sName;         ///< client name, unique (used as login id)
		KString   sPasswordHash; ///< bcrypt hash (60 chars)
		bool      bEnabled      { true };
		/// Comma separated list of tunnel names this client may open.
		/// Empty means every configured tunnel.
		KString   sAllowTunnels;
		KUnixTime tCreated;
		KUnixTime tLastLogin;    ///< zero if never logged in
	};

	/// Insert a client row, returns false if the name already exists.
	bool              AddInlet   (const Inlet& inlet);

	/// Overwrite the bcrypt hash for an existing client.
	bool              UpdateInletPasswordHash  (KStringView sName, KStringView sBcryptHash);

	/// Replace the list of tunnels a client may open (empty = all).
	bool              SetInletAllowTunnels     (KStringView sName, KStringView sAllowTunnels);

	/// Toggle the `enabled` bit of an existing client.
	bool              SetInletEnabled          (KStringView sName, bool bEnabled);

	/// Remove a client row.
	bool              DeleteInlet              (KStringView sName);

	/// Fetch one client. Returns a null unique_ptr if it does not exist.
	std::unique_ptr<Inlet> GetInlet            (KStringView sName);

	/// Return all clients sorted by name ascending.
	std::vector<Inlet> GetAllInlets           ();

	/// Stamp the last_login_utc column of a client. Called from the client
	/// tunnel's auth callback on successful login.
	void              SetInletLastLogin        (KStringView sName, KUnixTime tNow);

	// --- Tunnels ----------------------------------------------------------

	struct Tunnel
	{
		int64_t   iID          { 0 };
		KString   sName;                 ///< logical name, unique
		KString   sOutlet;               ///< outlet (stored in the legacy `nodes` table) authorised to drive this listener
		/// Port the exposed host binds to for forwarded downstream
		/// connections, on the interface named by sBindAddress.
		uint16_t  iListenPort  { 0 };
		KString   sTargetHost;           ///< default target, used if client does not pass one
		uint16_t  iTargetPort  { 0 };
		bool      bEnabled     { true };
		/// Interface the listener binds to. Empty means the wildcard
		/// (0.0.0.0 + [::] dual-stack, reachable from everywhere);
		/// "127.0.0.1" restricts it to the exposed host itself, which is
		/// the pattern for reaching the target through `ssh -L` instead of
		/// an open port.
		KString   sBindAddress;
		/// Comma separated list of IPs / CIDR networks that may connect to
		/// the listener. Empty means no restriction. Checked before the
		/// connection is forwarded; a rejected connection is closed and
		/// logged as a "conn_reject" event.
		KString   sAllowFrom;
		KUnixTime tCreated;
		KUnixTime tModified;
	};

	bool              AddTunnel              (const Tunnel& t);
	bool              UpdateTunnel           (const Tunnel& t);
	bool              DeleteTunnel           (KStringView sName);
	bool              SetTunnelEnabled       (KStringView sName, bool bEnabled);

	/// Fetch one tunnel. Returns a null unique_ptr if the tunnel does not exist.
	std::unique_ptr<Tunnel> GetTunnel        (KStringView sName);
	std::vector<Tunnel> GetAllTunnels        ();
	std::vector<Tunnel> GetEnabledTunnels    ();

	// --- Events (audit log) -----------------------------------------------

	struct Event
	{
		int64_t   iID         { 0 };
		KUnixTime tTimestamp;
		KString   sKind;         ///< "admin_login_ok" | "admin_login_fail" | "node_login_ok" | "node_login_fail" | "handshake_fail" | "tunnel_start" | "tunnel_stop" | "tunnel_disconnect" | "tunnel_error" | "config_change" | "bootstrap" | "admin_add" | "admin_del" | "node_add" | "node_del" | "node_disable" | "node_enable" | ...
		KString   sAdmin;        ///< admin who performed this action (UI write paths). Empty for purely automatic events.
		KString   sOutlet;       ///< outlet involved (tunnel lifecycle / handshake events; stored in the `node` column). Empty for admin-only events.
		KString   sTunnelName;   ///< optional
		KString   sRemoteIP;     ///< optional
		KString   sDetail;       ///< free-form
	};

	void               LogEvent              (const Event& ev);
	std::vector<Event> GetRecentEvents       (std::size_t iLimit = 100);

	/// Return events, optionally filtered by exact-match @p sKind. Empty
	/// filter skips the constraint. Results are ordered newest first and
	/// capped at @p iLimit rows. Admin and node columns are both returned.
	std::vector<Event> GetEvents             (KStringView sKind,
	                                          std::size_t iLimit);

	/// Return all distinct `kind` values from the events table, sorted
	/// alphabetically. Used to populate the admin UI filter dropdown.
	std::vector<KString> GetDistinctEventKinds ();

	// --- Usage samples ----------------------------------------------------

	struct UsageSample
	{
		int64_t     iID          { 0 };
		KUnixTime   tTimestamp;
		KString     sTunnelName;
		uint64_t    iBytesRx     { 0 };
		uint64_t    iBytesTx     { 0 };
		std::size_t iConnections { 0 };
	};

	void                     LogUsageSample  (const UsageSample& sample);
	std::vector<UsageSample> GetRecentUsage  (KStringView sTunnelName, std::size_t iLimit = 60);

	// --- Settings (key/value) ----------------------------------------------
	//
	// Small generic key/value store for admin-configurable server settings
	// (e.g. the ACME certificate configuration or the log level). Values are
	// plain strings; the reader supplies the default for missing keys.

	/// Return the value for sKey, or sDefault if the key does not exist.
	KString           GetSetting             (KStringView sKey, KStringView sDefault = "");

	/// Insert or overwrite the value for sKey.
	bool              SetSetting             (KStringView sKey, KStringView sValue);

	// --- Utilities --------------------------------------------------------

	/// Returns the last error message produced by this store. Empty if OK.
	KString           GetLastError           () const { return m_sError; }
	bool              HasError               () const { return !m_sError.empty(); }
	KStringView       GetDatabasePath        () const { return m_sDatabase; }

//----------
private:
//----------

	/// Initial schema migration — idempotent. Called from the ctor.
	bool              InitializeSchema       ();

	/// Open a fresh KSQLite handle for one query. We do not hold
	/// a long-lived handle because that would require the caller to
	/// synchronize its prepared statements; opening on demand is trivial
	/// and, for admin-UI traffic, has no performance impact.
	KSQLite OpenRW ();
	KSQLite OpenRO ();

	void              SetError (KString sError);

	KString           m_sDatabase;
	mutable KString   m_sError;
	mutable std::mutex m_Mutex;

}; // KTunnelStore
