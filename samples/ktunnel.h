/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/http/websocket/kwebsocketclient.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>

class KTunnelStore; // defined in ktunnel_store.h

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExtendedConfig : public KTunnel::Config
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Operating mode of the exposed host. Selected by Tunnel::Main()
	/// based on the CLI arguments and on whether the process was
	/// launched by a service manager.
	///
	/// `Stateful`
	///   DB-backed operation. Tunnels are read from the `tunnels`
	///   table, the admin UI is registered at `/Configure/`, peer
	///   authentication uses bcrypt against the `users` table. This
	///   is the default for interactive launches without `-f`/`-t`
	///   and for all service-manager launches (systemd, launchd,
	///   SCM — see KService::IsRunningAsService()).
	///
	/// `AdHoc`
	///   CLI-only operation. No SQLite file is opened, no admin UI is
	///   registered, no `users` table. Exactly one forward tunnel is
	///   configured via `-f <port> -t <target>` on the command line,
	///   and peer authentication falls back to `-s <secret>` (the
	///   legacy pre-shared-secret model). Intended for ad-hoc
	///   recovery / bootstrap use over SSH, when the admin UI is
	///   unreachable.
	enum class Mode : std::uint8_t
	{
		Stateful,
		AdHoc,
	};

	template<class... Args>
	void Message (KFormatString<Args...> sFormat, Args&&... args) const
	{
		PrintMessage(kFormat(sFormat, std::forward<Args>(args)...));
	}

	KTCPEndPoint           DefaultTarget;
	KTCPEndPoint           ExposedHost;
	/// Absolute path to the SQLite configuration database. Resolved by
	/// Tunnel::Main() to the appropriate default for the current mode
	/// (system path for root-services, user path otherwise) unless the
	/// operator supplied an explicit `-db <path>` override. Left empty
	/// when running in AdHoc mode (no DB is opened at all).
	KString                sDatabasePath;
	/// Node name sent by the *protected* host when logging in to the
	/// exposed host. Must match a row in the exposed host's `nodes`
	/// table (its password is the bcrypt-hashed value managed by an
	/// admin via `ktunnel -add-node` or the admin UI). Ignored on the
	/// exposed host. Set via `-n <name>` on the CLI; defaults to "node",
	/// a placeholder that must usually be overridden — the bootstrap
	/// flows seed admin accounts (`-set-admin`), not node accounts.
	KString                sPeerNode      { "node" };

	/// Protected-side trust configuration for the v2 AES handshake.
	/// All four are unused on the exposed side.
	///
	/// `sTrustFingerprint` (CLI: `-trust-fingerprint <hex>`):
	///   one-shot trust override. If non-empty and matches the server's
	///   presented fingerprint, the connection is accepted regardless of
	///   any known_servers entry; on mismatch the connection is rejected
	///   even if known_servers would have accepted it. Not persisted.
	///
	/// `bTrustOnFirstUse` (CLI: `-trust-on-first-use`):
	///   if true, an unknown server (no known_servers entry, no
	///   `sTrustFingerprint`) triggers an interactive yes/no prompt that
	///   shows the fingerprint; on "yes" we record it in known_servers
	///   and continue. Without this flag an unknown server is hard-
	///   rejected so a non-interactive launch cannot silently accept a
	///   forged identity.
	///
	/// `sKnownServersPath` (CLI: `-known-servers <path>`):
	///   override for the per-user trust store. Defaults to
	///   `$HOME/.config/ktunnel/known_servers` (resolved at runtime).
	KString                sTrustFingerprint;
	bool                   bTrustOnFirstUse { false };
	KString                sKnownServersPath;

	/// Operating mode of the exposed host (see Mode). Populated by
	/// Tunnel::Main() before ExposedServer is constructed. On the
	/// protected host side this field is unused.
	Mode                   eMode          { Mode::Stateful };
	bool                   bNoTLS         { false };
	bool                   bQuiet         { false };

//----------
private:
//----------

	void PrintMessage (KStringView sMessage) const;

}; // ExtendedConfig

class TunnelListener;
class AdminUI;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExposedServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Config : public ExtendedConfig
	{
		KString  sCertFile;
		KString  sKeyFile;
		KString  sTLSPassword;
		KString  sCipherSuites;
		/// Path to the long-term Ed25519 server-identity PEM file used
		/// by the v2 AES handshake to sign hello-ack frames. Resolved by
		/// Tunnel::Main() based on -persist (AdHoc) or the DB directory
		/// (Stateful) unless the operator supplied an explicit
		/// `-identity-key <path>` override. In AdHoc mode without
		/// `-persist` this is left empty and an ephemeral key is
		/// generated at start-up.
		KString  sIdentityKeyPath;
		uint16_t iPort    { 0 };
		uint16_t iRawPort { 0 };
		bool     bPersistCert { false };
	};

	ExposedServer (const Config& config);
	~ExposedServer ();

	/// Request a graceful shutdown of every currently-open control
	/// stream so their KTunnel::Run() unblocks promptly. Thread-safe
	/// and safe to call from a signal-handler thread (KSignals): it
	/// takes a shared lock on m_ControlTunnels, locks each weak_ptr,
	/// and calls KTunnel::Stop() — which signals the underlying
	/// stream and wakes the blocked poll() inside ReadMessage().
	///
	/// Without this hook, a SIGINT during an active /Tunnel session
	/// causes Http.Wait() to hang: KREST's own signal handler tears
	/// down the REST acceptor, but the per-session worker thread is
	/// stuck joining a TunnelThread that will itself only return when
	/// the tunnel's poll() times out (many seconds later). Shutdown()
	/// short-circuits that chain so Http.Wait() returns within
	/// milliseconds and main() can unwind cleanly on the first
	/// Ctrl+C. Installed as a chained SIGINT/SIGTERM handler in the
	/// ExposedServer constructor, just before Http.Wait().
	void Shutdown ();

	/// Forward a raw downstream TCP connection through the tunnel
	/// belonging to @p sOwner. Closes the stream and logs an event
	/// if the owner is not currently connected. Used by per-tunnel
	/// TunnelListener instances that are managed by ReconcileListeners.
	///
	/// A wildcard owner ("*" or empty string) means "any tunnel that
	/// is currently connected" — the first-connected tunnel is used,
	/// without owner verification. This is the route used by the
	/// synthetic "cli" listener that represents the legacy
	/// `-f <port> -t <target>` CLI configuration.
	void ForwardStreamForOwner (KIOStreamSocket& Downstream,
	                            const KTCPEndPoint& Endpoint,
	                            KStringView sOwner);

	/// Per-listener runtime state reported to the admin UI.
	/// @li `Stopped` — configured as disabled, or not in the registry
	/// @li `Listening` — registry entry is up and the TCP acceptor runs
	/// @li `PortError` — tried to bind but the OS refused (port in use)
	/// @li `OwnerOffline` — listener is up but the owner node has no
	///     active tunnel; new connections get rejected at accept time
	enum class ListenerState : std::uint8_t { Stopped, Listening, PortError, OwnerOffline };

	/// Snapshot of one entry in the listener registry; used by the admin UI.
	struct ListenerInfo
	{
		KString       sName;         ///< tunnels.name
		ListenerState eState { ListenerState::Stopped };
		KString       sError;        ///< populated for PortError
	};

	/// Thread-safe map from tunnel name → current listener state. Names
	/// without an entry are reported as `Stopped`.
	KUnorderedMap<KString, ListenerInfo> SnapshotListenerStates () const;

	/// Reconcile the per-tunnel listener registry with the persistent
	/// store. Starts/stops/restarts KTCPServer instances as needed.
	/// Called at startup and after every successful admin-UI mutation
	/// of the tunnels table (Add/Toggle/Delete). Thread-safe.
	/// @p sActor is used as the `events.username` field when logging
	/// lifecycle events ("tunnel_start", "tunnel_stop", "config_change").
	void ReconcileListeners (KStringView sActor = {});

	/// Snapshot of a single currently-connected tunnel, used by the admin
	/// UI dashboard. The shared_ptr keeps the tunnel alive for the
	/// snapshot's lifetime even if the peer disconnects in between.
	struct ActiveTunnel
	{
		KString                  sNode;        ///< login node on the tunnel
		KTCPEndPoint             EndpointAddr; ///< remote address of the peer
		KUnixTime                tConnected;   ///< wall-clock time of login
		std::shared_ptr<KTunnel> Tunnel;       ///< non-null
	};

	/// Thread-safe snapshot of all tunnels currently logged in at this
	/// server. Returns an empty vector if nobody is connected.
	std::vector<ActiveTunnel> SnapshotActiveTunnels () const;

	/// Returns the (one) currently-active tunnel for a given node, or a
	/// null shared_ptr if that node is not connected.
	std::shared_ptr<KTunnel>  GetTunnelForNode       (KStringView sNode) const;

	/// Access to the persistent configuration / audit store backing the
	/// admin UI. Never null after successful construction.
	KTunnelStore&             GetStore               ()       { return *m_Store; }
	const KTunnelStore&       GetStore               () const { return *m_Store; }

	/// Access to the shared bcrypt verifier. Never null after
	/// successful construction.
	KBCrypt&                  GetBCrypt              ()       { return *m_BCrypt; }

	/// Access to the ExposedServer configuration — the admin UI reads
	/// things like bNoTLS to decide on cookie flags.
	const Config&             GetConfig              () const { return m_Config; }

//----------
protected:
//----------

	void ControlStream (std::unique_ptr<KIOStreamSocket> Stream);

//----------
private:
//----------

	/// Verify a tunnel-login (node, secret) pair against the persistent
	/// store. Called from the KTunnel::Config::AuthCallback installed
	/// per ControlStream(). Logs a node_login_ok / node_login_fail event
	/// as a side effect.
	bool                      VerifyNodeLogin        (KStringView sNode, KStringView sSecret,
	                                                  const KTCPEndPoint& RemoteAddr);

	void                      RegisterActiveTunnel   (KStringView sNode,
	                                                  const KTCPEndPoint& RemoteAddr,
	                                                  std::shared_ptr<KTunnel> Tunnel);
	void                      UnregisterActiveTunnel (KStringView sNode,
	                                                  const std::shared_ptr<KTunnel>& Tunnel);

	/// "First-come-first-served" pick used by ForwardStreamForOwner
	/// when a wildcard owner is given (i.e. the CLI-synthesised "cli"
	/// listener, which has no dedicated peer). Returns null if no
	/// tunnel is currently connected.
	std::shared_ptr<KTunnel>  PickDefaultTunnel      () const;

	/// node-name -> active tunnel. We accept only one tunnel per node for
	/// now; a second login from the same node replaces the first (and the
	/// previous tunnel is Stop()ed so it drops its resources).
	/// KThreadSafe couples the map with a shared_mutex: readers
	/// (SnapshotActiveTunnels, GetTunnelForNode, PickDefaultTunnel,
	/// SnapshotListenerStates) take a shared lock and can run in
	/// parallel; Register/Unregister take a unique lock.
	KThreadSafe<KUnorderedMap<KString, ActiveTunnel>> m_ActiveTunnels;

	/// Weak references to every KTunnel currently live inside
	/// ControlStream(), regardless of authentication state. Populated
	/// at the start of each ControlStream invocation and removed via a
	/// scope guard at the end, so the shared_ptr lifetime stays owned
	/// by ControlStream's stack frame. Shutdown() iterates this list
	/// and calls KTunnel::Stop() on each live entry, which unblocks
	/// the tunnel's poll() and lets the KREST worker thread drain.
	///
	/// Why not m_ActiveTunnels: that map only holds *authenticated*
	/// peers keyed by node-name, so a peer stuck in the login exchange
	/// would be invisible to it. Using a weak_ptr registry here covers
	/// both the pre- and post-auth windows without changing the
	/// semantics of the by-node lookup map.
	KThreadSafe<std::vector<std::weak_ptr<KTunnel>>>  m_ControlTunnels;

	/// Snapshot key for the listener registry: the per-row fields that
	/// influence socket ownership. Two rows with identical keys share
	/// the same running KTCPServer; a change triggers a restart in
	/// ReconcileListeners.
	struct ListenerKey
	{
		uint16_t iListenPort  { 0 };
		KString  sTargetHost;
		uint16_t iTargetPort  { 0 };
		KString  sOwnerNode;
		bool operator== (const ListenerKey& o) const noexcept
		{
			return iListenPort == o.iListenPort
			    && iTargetPort == o.iTargetPort
			    && sTargetHost == o.sTargetHost
			    && sOwnerNode  == o.sOwnerNode;
		}
	};

	/// One registry entry: the listener itself + its identifying key +
	/// any bind error observed at start time.
	struct ListenerEntry
	{
		std::unique_ptr<TunnelListener> Listener;
		ListenerKey Key;
		KString     sError;
	};

	/// The "desired" state for one listener. Produced by
	/// GatherDesiredTunnels() which unifies DB rows and (optionally)
	/// a CLI-driven synthetic row into a single source of truth.
	struct DesiredTunnel
	{
		KString     sName;   ///< unique tunnel name (e.g. "cli")
		ListenerKey Key;     ///< listen port + target + owner
	};

	/// Build the list of tunnels that should currently be listening.
	/// Combines enabled rows from the persistent store with a
	/// synthetic "cli" entry when `-f <port>` and `-t <target>`
	/// were given on the CLI. The CLI entry uses owner "*" to route
	/// incoming connections via PickDefaultTunnel.
	std::vector<DesiredTunnel> GatherDesiredTunnels () const;

	std::unique_ptr<KTunnelStore>     m_Store;
	std::unique_ptr<KBCrypt>          m_BCrypt;
	std::unique_ptr<AdminUI>          m_AdminUI;
	const Config&                     m_Config;

#if DEKAF2_HAS_ED25519
	/// Long-term Ed25519 server identity used by the v2 AES handshake to
	/// sign hello-ack frames. Loaded (or generated, in AdHoc with
	/// -persist; ephemeral in AdHoc without) by the ExposedServer ctor
	/// when m_Config.bAESPayload is set, otherwise null. Stored here
	/// rather than on m_Config because m_Config is held by const-ref;
	/// ControlStream() copies the pointer into the per-connection
	/// KTunnel::Config so the tunnel's WaitForLogin can sign with it.
	std::shared_ptr<KEd25519Key>      m_ServerIdentity;
#endif

	/// Per-tunnel listeners keyed by tunnel name. KThreadSafe couples
	/// the map with a shared_mutex: SnapshotListenerStates takes a
	/// shared lock (admin-UI dashboard, parallel-read friendly),
	/// ReconcileListeners and the destructor take a unique lock.
	KThreadSafe<KUnorderedMap<KString, ListenerEntry>> m_Listeners;

}; // ExposedServer


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Per-tunnel raw-TCP listener managed by ExposedServer::ReconcileListeners.
/// One instance is spawned for each enabled `tunnels` row in the store,
/// plus (optionally) one synthetic row representing the legacy CLI
/// configuration `-f <port> -t <target>`.
///
/// Incoming connections are pushed through the control tunnel that belongs
/// to the configured @p sOwner. If @p sOwner is the wildcard ("*" or
/// empty), the first currently-connected tunnel is used (legacy CLI
/// behaviour). Otherwise, if the owner is not currently logged in,
/// the connection is closed and an "auth_reject" event is logged.
class TunnelListener : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	template<typename... Args>
	TunnelListener (ExposedServer*      Exposed,
	                KString             sName,
	                KString             sOwner,
	                const KTCPEndPoint& Target,
	                Args&&...           args)
	: KTCPServer (std::forward<Args>(args)...)
	, m_ExposedServer (*Exposed)
	, m_sName         (std::move(sName))
	, m_sOwner        (std::move(sOwner))
	, m_Target        (Target)
	{
	}

	KStringView    GetName   () const { return m_sName; }
	KStringView    GetOwner  () const { return m_sOwner; }

//----------
protected:
//----------

	void Session (std::unique_ptr<KIOStreamSocket>& Stream) override final;

//----------
private:
//----------

	ExposedServer& m_ExposedServer;
	KString        m_sName;
	KString        m_sOwner;
	KTCPEndPoint   m_Target;

}; // TunnelListener

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ProtectedHost
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ProtectedHost (const ExtendedConfig& Config);

	/// Run the reconnect loop (blocking). Returns cleanly when Shutdown() is
	/// called from a signal handler or service-control handler.
	void Run      ();

	/// Request a graceful shutdown of the reconnect loop. Thread-safe and
	/// safe to call from a signal / service-control handler: sets the quit
	/// flag, tears down the currently active KTunnel if any (so its
	/// blocking Run() returns), and wakes the inter-retry sleep.
	void Shutdown ();

//----------
private:
//----------

	/// REPL handler wired into KTunnel::Config::OpenReplCallback when
	/// this peer connects to the exposed host. Runs on a tunnel worker
	/// thread; the handed-in Connection is this peer's side of a
	/// duplex text channel. Returns (and the tunnel closes the channel)
	/// when the remote end disconnects or the user types 'exit'.
	void RunRepl (std::shared_ptr<KTunnel::Connection> Connection);

	const ExtendedConfig&           m_Config;
	std::atomic<bool>               m_bQuit            { false };
	std::mutex                      m_Mutex;
	std::condition_variable         m_CV;
	KTunnel*                        m_pCurrentTunnel   { nullptr };

}; // ProtectedHost

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Tunnel : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	int  Main               (int argc, char** argv);

	/// is this the exposed host?
	bool IsExposed          () const
	{
		return m_Config.ExposedHost.empty();
	}

//----------
private:
//----------

	ExposedServer::Config m_Config;

}; // KTunnel
