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
///   - `users`         : admin / login accounts, bcrypt-hashed passwords
///   - `tunnels`       : per-tunnel listener configuration
///   - `events`        : append-only audit log (logins, start/stop, config)
///   - `usage_samples` : periodic RX/TX / connection-count snapshots per tunnel
///
/// All write operations are serialized by an internal mutex — SQLite itself
/// is fine with concurrent reads from multiple threads, but `ExecSQL`-style
/// writes through a single `KSQLite::Database` instance are simpler to reason
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

	// --- Users ------------------------------------------------------------

	struct User
	{
		int64_t   iID           { 0 };
		KString   sUsername;
		KString   sPasswordHash; ///< bcrypt hash (60 chars)
		bool      bIsAdmin      { false };
		KUnixTime tCreated;
		KUnixTime tLastLogin;   ///< zero if never logged in
	};

	/// Count users. Handy for bootstrap ("is this a fresh install?").
	std::size_t       CountUsers ();

	/// Insert a user row, returns false if the username already exists.
	bool              AddUser    (const User& user);

	/// Overwrite the bcrypt hash for an existing user.
	bool              UpdateUserPasswordHash (KStringView sUsername, KStringView sBcryptHash);

	/// Flip the admin bit of an existing user.
	bool              UpdateUserAdminFlag    (KStringView sUsername, bool bIsAdmin);

	/// Remove a user row.
	bool              DeleteUser             (KStringView sUsername);

	/// Fetch one user. Returns a null unique_ptr if the user does not exist.
	std::unique_ptr<User> GetUser            (KStringView sUsername);

	/// Return all users sorted by username ascending.
	std::vector<User> GetAllUsers            ();

	/// Stamp the last_login_utc column with the given time.
	void              SetLastLogin           (KStringView sUsername, KUnixTime tNow);

	// --- Tunnels ----------------------------------------------------------

	struct Tunnel
	{
		int64_t   iID          { 0 };
		KString   sName;                 ///< logical name, unique
		KString   sOwnerUser;            ///< the ktunnel login user that owns this listener
		/// Port the exposed host binds to for forwarded downstream
		/// connections. Bind address is always the wildcard (0.0.0.0
		/// + [::] dual-stack): KTCPServer has no per-interface option.
		uint16_t  iListenPort  { 0 };
		KString   sTargetHost;           ///< default target, used if client does not pass one
		uint16_t  iTargetPort  { 0 };
		bool      bEnabled     { true };
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
		KString   sKind;         ///< "login_ok" | "login_fail" | "handshake_fail" | "tunnel_start" | "tunnel_stop" | "tunnel_disconnect" | "tunnel_error" | "config_change" | "bootstrap" | ...
		KString   sUsername;     ///< optional
		KString   sTunnelName;   ///< optional
		KString   sRemoteIP;     ///< optional
		KString   sDetail;       ///< free-form
	};

	void               LogEvent              (const Event& ev);
	std::vector<Event> GetRecentEvents       (std::size_t iLimit = 100);

	/// Return events, optionally filtered by exact-match @p sKind and/or
	/// @p sUsername. Either filter can be empty to skip it. Results are
	/// ordered newest first and capped at @p iLimit rows.
	std::vector<Event> GetEvents             (KStringView sKind,
	                                          KStringView sUsername,
	                                          std::size_t iLimit);

	/// Return events that @p sUsername has a "legitimate interest" in,
	/// from the perspective of a non-admin peer seeing only its own
	/// activity on the dashboard: rows where `username = :u` OR where
	/// `tunnel_name` is one of the tunnels owned by that user. Ordered
	/// newest first, capped at @p iLimit.
	std::vector<Event> GetRecentEventsForOwner (KStringView sUsername,
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

	/// Open a fresh KSQLite::Database handle for one query. We do not hold
	/// a long-lived handle because that would require the caller to
	/// synchronize its prepared statements; opening on demand is trivial
	/// and, for admin-UI traffic, has no performance impact.
	KSQLite::Database OpenRW ();
	KSQLite::Database OpenRO ();

	void              SetError (KString sError);

	KString           m_sDatabase;
	mutable KString   m_sError;
	mutable std::mutex m_Mutex;

}; // KTunnelStore
