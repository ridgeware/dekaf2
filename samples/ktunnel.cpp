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

// KTunnel can be started either as an exposed host (that can listen on ports
// reachable by one or more clients) or as a protected host (so called because
// it typically will be protected behind a firewall or network configuration
// (like mobile IP networks) that do not allow incoming connections).
//
// The protected KTunnel connects via HTTPS (upgrading to websockets) to the
// exposed KTunnel, and opens more TLS data streams inside the websocket
// connection as requested by the exposed KTunnel, typically on request of
// incoming TCP connections to the latter, being either raw or TLS or whatever
// encoded - the connection is fully transparent.
//
// The protected KTunnel connects to the final target host, as requested by
// the exposed KTunnel (setup per forwarded connection).
//
// The exposed KTunnel opens one TLS communications port for internal
// communication and general REST services, and one or more TCP forward
// ports for different targets.
//
//         e.g. port 1234         e.g. port 443
//                                              |
// client >>--TCP/TLS-->> KTunnel <<----TLS websocket----<< KTunnel >>--TCP/TLS-->> target(s)
//            data in    (exposed)              |         (protected)   data in
//          any protocol                        |                     any protocol
//                                     firewall |
//                       or e.g. mobile network |
//
// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over a TLS websocket connection, initiated from upstream.
//
// The tunnel connection can transport up to 16 million tunneled streams.

#include "ktunnel.h"
#include "ktunnel_admin.h"
#include "ktunnel_store.h"
#include <dekaf2/core/init/dekaf2.h> // KInit()
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/system/os/kservice.h>
#include <dekaf2/system/filesystem/kfilesystem.h> // kReadAll for -pass-file
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/util/cli/kxterm.h>  // kPromptForPassword for -set-admin / -install
#include <dekaf2/containers/associative/kassociative.h>
#include <csignal> // SIGINT, SIGTERM
#include <future>

#if !defined(DEKAF2_IS_WINDOWS)
	#include <unistd.h>      // ::isatty check in CapturePassword
#endif

using namespace dekaf2;

//-----------------------------------------------------------------------------
void ExtendedConfig::PrintMessage(KStringView sMessage) const
//-----------------------------------------------------------------------------
{
	if (!bQuiet)
	{
		kWriteLine(sMessage);
	}
	else
	{
		kDebug(1, sMessage);
	}

} // ExtendedConfig::PrintMessage

//-----------------------------------------------------------------------------
bool ExposedServer::VerifyTunnelLogin (KStringView sUser, KStringView sSecret,
                                       const KTCPEndPoint& RemoteAddr)
//-----------------------------------------------------------------------------
{
	// AdHoc mode has no persistent `users` table, no bcrypt. The
	// operator supplies one or more shared secrets on the CLI via
	// `-s <comma-separated>`; any of them grants access. sUser is
	// accepted but not verified — it is only used for log lines.
	if (!m_Store)
	{
		if (m_Config.Secrets.empty())
		{
			kDebug(1, "ad-hoc: tunnel login from {} rejected: no secrets configured", RemoteAddr);
			return false;
		}
		const bool bOk = m_Config.Secrets.contains(KString(sSecret));
		kDebug(1, "ad-hoc: tunnel login from {} user '{}' {}",
		       RemoteAddr, sUser, bOk ? "accepted" : "rejected");
		return bOk;
	}

	// Rejection-path events are logged even for unknown users, so that an
	// operator looking at the events table can spot brute-force attempts.
	auto LogFail = [&](KStringView sDetail)
	{
		KTunnelStore::Event ev;
		ev.sKind     = "login_fail";
		ev.sUsername = KString(sUser);
		ev.sRemoteIP = RemoteAddr.Serialize();
		ev.sDetail   = KString(sDetail);
		m_Store->LogEvent(ev);
	};

	if (sUser.empty())
	{
		LogFail("empty user");
		return false;
	}

	auto oUser = m_Store->GetUser(sUser);

	// Do a constant-time dummy compare against a fixed, well-formed bcrypt
	// hash so a missing row cannot be distinguished from a wrong password
	// by response time. The hash below is bcrypt("<never-used>", cost 4).
	static constexpr KStringViewZ s_sDummyHash =
		"$2a$04$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG";

	KString sPassword(sSecret);

	if (!oUser)
	{
		(void) m_BCrypt->ValidatePassword(sPassword, s_sDummyHash);
		LogFail("unknown user");
		return false;
	}

	if (!m_BCrypt->ValidatePassword(sPassword, oUser->sPasswordHash))
	{
		LogFail("bad password");
		return false;
	}

	m_Store->SetLastLogin(sUser, KUnixTime::now());

	KTunnelStore::Event ev;
	ev.sKind     = "login_ok";
	ev.sUsername = KString(sUser);
	ev.sRemoteIP = RemoteAddr.Serialize();
	ev.sDetail   = "tunnel";
	m_Store->LogEvent(ev);

	return true;

} // VerifyTunnelLogin

//-----------------------------------------------------------------------------
void ExposedServer::RegisterActiveTunnel (KStringView sUser,
                                          const KTCPEndPoint& RemoteAddr,
                                          std::shared_ptr<KTunnel> Tunnel)
//-----------------------------------------------------------------------------
{
	std::shared_ptr<KTunnel> Prev; // destroy outside the lock

	{
		auto Tunnels = m_ActiveTunnels.unique();

		auto it = Tunnels->find(KString(sUser));
		if (it != Tunnels->end())
		{
			// Same user logged in twice: evict the old tunnel. Dropping
			// it outside the lock keeps us from holding the mutex while
			// the previous Run()-thread tears things down.
			Prev = std::move(it->second.Tunnel);
			it->second.Tunnel       = std::move(Tunnel);
			it->second.EndpointAddr = RemoteAddr;
			it->second.tConnected   = KUnixTime::now();
		}
		else
		{
			ActiveTunnel at;
			at.sUser        = KString(sUser);
			at.EndpointAddr = RemoteAddr;
			at.tConnected   = KUnixTime::now();
			at.Tunnel       = std::move(Tunnel);
			Tunnels->emplace(at.sUser, std::move(at));
		}
	}

	if (Prev)
	{
		kDebug(1, "evicting previous tunnel for user '{}'", sUser);
		Prev->Stop();
	}

} // RegisterActiveTunnel

//-----------------------------------------------------------------------------
void ExposedServer::UnregisterActiveTunnel (KStringView sUser,
                                            const std::shared_ptr<KTunnel>& Tunnel)
//-----------------------------------------------------------------------------
{
	auto Tunnels = m_ActiveTunnels.unique();

	auto it = Tunnels->find(KString(sUser));
	if (it != Tunnels->end() && it->second.Tunnel == Tunnel)
	{
		Tunnels->erase(it);
	}

} // UnregisterActiveTunnel

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel> ExposedServer::PickDefaultTunnel () const
//-----------------------------------------------------------------------------
{
	auto Tunnels = m_ActiveTunnels.shared();
	if (Tunnels->empty())
	{
		return {};
	}
	// Stable "first by user-name" — used for the wildcard-owner path
	// (CLI-synthesised "cli" listener). DB-configured tunnels always
	// supply a concrete owner and take the GetTunnelForUser() branch
	// in ForwardStreamForOwner instead.
	return Tunnels->begin()->second.Tunnel;

} // PickDefaultTunnel

//-----------------------------------------------------------------------------
std::vector<ExposedServer::ActiveTunnel> ExposedServer::SnapshotActiveTunnels () const
//-----------------------------------------------------------------------------
{
	std::vector<ActiveTunnel> out;

	auto Tunnels = m_ActiveTunnels.shared();
	out.reserve(Tunnels->size());
	for (const auto& kv : *Tunnels)
	{
		out.push_back(kv.second);
	}
	return out;

} // SnapshotActiveTunnels

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel> ExposedServer::GetTunnelForUser (KStringView sUser) const
//-----------------------------------------------------------------------------
{
	auto Tunnels = m_ActiveTunnels.shared();
	auto it = Tunnels->find(KString(sUser));
	return (it == Tunnels->end()) ? std::shared_ptr<KTunnel>{} : it->second.Tunnel;

} // GetTunnelForUser

//-----------------------------------------------------------------------------
void ExposedServer::ControlStream(std::unique_ptr<KIOStreamSocket> Stream)
//-----------------------------------------------------------------------------
{
	// Capture the peer address before we hand the stream to KTunnel (the
	// ctor moves the unique_ptr in, so we cannot read GetEndPointAddress()
	// off it afterwards without going through the tunnel).
	const auto EndpointAddress = Stream->GetEndPointAddress();

	// We copy the server-wide KTunnel::Config (slicing ExtendedConfig down
	// to its KTunnel::Config base) so that per-connection AuthCallback
	// captures do not race across parallel clients.
	KTunnel::Config TunnelCfg = m_Config;

	// The auth callback runs inside KTunnel::WaitForLogin on the tunnel's
	// Run() thread. To get the verified user name back onto this
	// (ControlStream) thread so we can register the tunnel in the
	// per-user map, we hand the outcome through a shared promise. An
	// atomic guard keeps set_value() from being called twice if the
	// tunnel ever retries the auth step.
	auto LoginPromise = std::make_shared<std::promise<KString>>();
	auto LoginFuture  = LoginPromise->get_future();
	auto LoginFired   = std::make_shared<std::atomic<bool>>(false);

	TunnelCfg.AuthCallback = [this, LoginPromise, LoginFired, EndpointAddress]
	                         (KStringView sUser, KStringView sSecret) -> bool
	{
		const bool bOK = VerifyTunnelLogin(sUser, sSecret, EndpointAddress);

		if (!LoginFired->exchange(true))
		{
			LoginPromise->set_value(bOK ? KString(sUser) : KString{});
		}

		return bOK;
	};

	auto Tunnel = std::make_shared<KTunnel>(TunnelCfg, std::move(Stream));

	m_Config.Message("[control]: opened control stream from {}", EndpointAddress);

	// Run the tunnel on a dedicated thread so this method can observe the
	// login outcome and keep the per-user registry in sync.
	std::thread TunnelThread = kMakeThread([Tunnel]()
	{
		// tunnel disconnects are expected business-as-usual (peer gone,
		// bad frame, etc.) and not exceptions - log at debug severity and
		// let the thread exit normally. Anything that escapes this catch
		// will still be caught by kMakeThread()'s outer handler.
		try
		{
			Tunnel->Run();
		}
		catch (const std::exception& ex)
		{
			kDebug(1, "tunnel run: {}", ex.what());
		}
	});

	// Wait up to ConnectTimeout for the auth callback to fire. If the
	// login never arrives (peer gone, bad frame, etc.) the tunnel's
	// Run() will have thrown by then and we fall through to join().
	KString sUser;
	auto tWait = std::chrono::milliseconds(m_Config.ConnectTimeout.milliseconds().count());
	if (LoginFuture.wait_for(tWait) == std::future_status::ready)
	{
		sUser = LoginFuture.get();
	}

	bool bRegistered = false;
	if (!sUser.empty())
	{
		RegisterActiveTunnel(sUser, EndpointAddress, Tunnel);
		bRegistered = true;
		m_Config.Message("[control]: tunnel '{}' active from {}", sUser, EndpointAddress);
	}
	else
	{
		m_Config.Message("[control]: login failed or timed out from {}", EndpointAddress);
	}

	TunnelThread.join();

	if (bRegistered)
	{
		UnregisterActiveTunnel(sUser, Tunnel);

		if (m_Store)
		{
			KTunnelStore::Event ev;
			ev.sKind     = "tunnel_disconnect";
			ev.sUsername = sUser;
			ev.sRemoteIP = EndpointAddress.Serialize();
			ev.sDetail   = kFormat("rx={} tx={}", Tunnel->GetBytesRx(), Tunnel->GetBytesTx());
			m_Store->LogEvent(ev);
		}
	}

	m_Config.Message("[control]: closed control stream from {}", EndpointAddress);

} // ControlStream

//-----------------------------------------------------------------------------
void ExposedServer::ForwardStreamForOwner (KIOStreamSocket&    Downstream,
                                           const KTCPEndPoint& Endpoint,
                                           KStringView         sOwner)
//-----------------------------------------------------------------------------
{
	kDebug(1, "per-owner forward: from={} owner={} target={}",
	       Downstream.GetEndPointAddress(), sOwner, Endpoint);

	if (Endpoint.empty())
	{
		throw KError("missing target endpoint definition with domain:port");
	}

	// Wildcard owner ("*" or empty) → first-come-first-served across
	// all currently-connected tunnels. This is the path used by the
	// synthetic "cli" listener that mirrors the legacy
	// `-f <port> -t <target>` CLI configuration.
	const bool bWildcard = sOwner.empty() || sOwner == "*";

	auto Tunnel = bWildcard ? PickDefaultTunnel() : GetTunnelForUser(sOwner);

	if (!Tunnel)
	{
		// The peer for this owner is not connected right now (or no
		// tunnel at all, for the wildcard case). Log it once per
		// rejected connection and close cleanly — we prefer that over
		// letting the caller's TCP half-open the socket.
		if (m_Store)
		{
			KTunnelStore::Event ev;
			ev.sKind     = "auth_reject";
			ev.sUsername = KString(sOwner);
			ev.sRemoteIP = Downstream.GetEndPointAddress().Serialize();
			ev.sDetail   = bWildcard
				? kFormat("no tunnel currently connected — dropped raw conn from {}",
				          Downstream.GetEndPointAddress())
				: kFormat("owner '{}' not currently connected — "
				          "dropped raw conn from {}",
				          sOwner, Downstream.GetEndPointAddress());
			m_Store->LogEvent(ev);
		}
		throw KError(bWildcard
			? KString("no tunnel established")
			: kFormat("no active tunnel for owner '{}'", sOwner));
	}

	Downstream.SetTimeout(m_Config.Timeout);
	Tunnel->Connect(&Downstream, Endpoint);

} // ForwardStreamForOwner

//-----------------------------------------------------------------------------
void TunnelListener::Session (std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	// KTCPServer calls this on its accept thread for every connection —
	// delegate to the ExposedServer so the tunnel lookup happens under
	// the right mutex. Exceptions bubble back into KTCPServer where they
	// close the stream and log a warning.
	m_ExposedServer.ForwardStreamForOwner(*Stream, m_Target, m_sOwner);

} // TunnelListener::Session

//-----------------------------------------------------------------------------
KUnorderedMap<KString, ExposedServer::ListenerInfo>
ExposedServer::SnapshotListenerStates () const
//-----------------------------------------------------------------------------
{
	KUnorderedMap<KString, ListenerInfo> out;

	// One snapshot for the listener registry, one for the active
	// tunnels — taken separately to keep the critical sections short.
	KUnorderedSet<KString> LiveOwners;
	{
		auto Tunnels = m_ActiveTunnels.shared();
		LiveOwners.reserve(Tunnels->size());
		for (const auto& kv : *Tunnels) LiveOwners.insert(kv.first);
	}

	auto Listeners = m_Listeners.shared();
	out.reserve(Listeners->size());
	for (const auto& kv : *Listeners)
	{
		ListenerInfo info;
		info.sName  = kv.first;
		info.sError = kv.second.sError;

		if      (!kv.second.sError.empty())          info.eState = ListenerState::PortError;
		else if (!kv.second.Listener)                info.eState = ListenerState::Stopped;
		else if (!kv.second.Listener->IsRunning())   info.eState = ListenerState::Stopped;
		else if (!LiveOwners.count(kv.second.Key.sOwnerUser))
		                                             info.eState = ListenerState::OwnerOffline;
		else                                         info.eState = ListenerState::Listening;

		out.emplace(kv.first, std::move(info));
	}
	return out;

} // SnapshotListenerStates

//-----------------------------------------------------------------------------
std::vector<ExposedServer::DesiredTunnel> ExposedServer::GatherDesiredTunnels () const
//-----------------------------------------------------------------------------
{
	std::vector<DesiredTunnel> out;

	// Enabled rows from the persistent store (admin UI / API configured).
	if (m_Store)
	{
		auto DBTunnels = m_Store->GetEnabledTunnels();
		out.reserve(DBTunnels.size() + 1);
		for (const auto& t : DBTunnels)
		{
			DesiredTunnel d;
			d.sName           = t.sName;
			d.Key.iListenPort = t.iListenPort;
			d.Key.sTargetHost = t.sTargetHost;
			d.Key.iTargetPort = t.iTargetPort;
			d.Key.sOwnerUser  = t.sOwnerUser;
			out.push_back(std::move(d));
		}
	}

	// Synthetic CLI-driven tunnel: the legacy `-f <port> -t <target>`
	// CLI configuration becomes a single extra row with wildcard owner
	// ("*"), routed via PickDefaultTunnel() in ForwardStreamForOwner.
	// The name "cli" is reserved so the admin UI cannot create a DB
	// row that collides with it (see AddTunnel validation).
	if (m_Config.iRawPort && !m_Config.DefaultTarget.empty())
	{
		DesiredTunnel d;
		d.sName           = "cli";
		d.Key.iListenPort = m_Config.iRawPort;
		d.Key.sTargetHost = m_Config.DefaultTarget.Domain.get();
		d.Key.iTargetPort = m_Config.DefaultTarget.Port.get();
		d.Key.sOwnerUser  = "*";
		out.push_back(std::move(d));
	}

	return out;

} // GatherDesiredTunnels

//-----------------------------------------------------------------------------
void ExposedServer::ReconcileListeners (KStringView sActor)
//-----------------------------------------------------------------------------
{
	// Unified desired state — covers both DB rows and the optional
	// CLI-synthesised tunnel.
	auto Desired = GatherDesiredTunnels();

	// Pre-flight port-conflict detection: catch colliding iListenPort
	// values before ever calling bind(), so the operator sees a clear
	// error message (with both tunnel names) instead of an EADDRINUSE
	// from the OS.
	//
	//   - sError "conflicts with admin HTTPS port N" for any desired
	//     tunnel that wants to bind the same port as the KREST server.
	//   - sError "conflicts with tunnel '<first>' on port N" for the
	//     second (and later) tunnel that wants a port already claimed
	//     by another desired tunnel. The first-seen tunnel wins.
	KUnorderedMap<KString, KString> PreflightError;   // tunnel name → error detail
	KUnorderedMap<uint16_t, KString> PortOwner;       // port → first tunnel that wants it

	for (const auto& t : Desired)
	{
		if (m_Config.iPort != 0 && t.Key.iListenPort == m_Config.iPort)
		{
			PreflightError.emplace(t.sName,
				kFormat("port {} conflicts with admin HTTPS port",
				        t.Key.iListenPort));
			continue;
		}

		auto ins = PortOwner.emplace(t.Key.iListenPort, t.sName);
		if (!ins.second)
		{
			PreflightError.emplace(t.sName,
				kFormat("port {} already claimed by tunnel '{}'",
				        t.Key.iListenPort, ins.first->second));
		}
	}

	auto Listeners = m_Listeners.unique();

	// Build a quick lookup of what we want after reconciling.
	KUnorderedMap<KString, ListenerKey> DesiredByName;
	DesiredByName.reserve(Desired.size());
	for (const auto& t : Desired)
	{
		DesiredByName.emplace(t.sName, t.Key);
	}

	// 1. Stop + drop registry entries that are no longer desired or
	//    whose key changed. Key-change restarts so the new target /
	//    owner / port takes effect immediately.
	for (auto it = Listeners->begin(); it != Listeners->end(); )
	{
		auto di = DesiredByName.find(it->first);
		const bool bGone    = (di == DesiredByName.end());
		const bool bChanged = !bGone && !(di->second == it->second.Key);

		if (bGone || bChanged)
		{
			if (it->second.Listener && it->second.Listener->IsRunning())
			{
				it->second.Listener->Stop();
			}

			if (m_Store)
			{
				KTunnelStore::Event ev;
				ev.sKind       = "tunnel_stop";
				ev.sUsername   = KString(sActor);
				ev.sTunnelName = it->first;
				ev.sDetail     = bGone ? "removed from registry"
				                       : "restarting due to config change";
				m_Store->LogEvent(ev);
			}

			it = Listeners->erase(it);
		}
		else
		{
			++it;
		}
	}

	// 2. Start listeners for all desired rows that are not yet in the
	//    registry. Bind failures (or pre-flight conflicts) are captured
	//    as sError but do NOT throw so a single bad row can't take the
	//    admin UI down.
	for (const auto& t : Desired)
	{
		if (Listeners->count(t.sName)) continue;   // already up (unchanged)

		ListenerEntry entry;
		entry.Key = t.Key;

		// Check pre-flight conflicts first — if we know this one is
		// going to fail we do not even attempt to bind.
		auto pfe = PreflightError.find(t.sName);
		if (pfe != PreflightError.end())
		{
			entry.sError = pfe->second;
		}
		else
		{
			KTCPEndPoint Target(t.Key.sTargetHost, t.Key.iTargetPort);

			try
			{
				entry.Listener = std::make_unique<TunnelListener>
				(
					this,
					t.sName,
					t.Key.sOwnerUser,
					Target,
					t.Key.iListenPort,        // KTCPServer ctor: port
					/* bSSL        = */ false,
					/* iMaxConns   = */ m_Config.iMaxTunneledConnections
				);

				// Non-blocking: Start() returns after the accept
				// thread is up and running.
				if (!entry.Listener->Start(m_Config.Timeout, /*bBlock=*/false))
				{
					entry.sError = kFormat("Start() failed on port {}",
					                       t.Key.iListenPort);
				}
			}
			catch (const std::exception& ex)
			{
				entry.sError   = ex.what();
				entry.Listener.reset();
			}
		}

		if (m_Store)
		{
			KTunnelStore::Event ev;
			ev.sKind       = entry.sError.empty() ? "tunnel_start" : "tunnel_error";
			ev.sUsername   = KString(sActor);
			ev.sTunnelName = t.sName;
			ev.sDetail     = entry.sError.empty()
				? kFormat("listening on [::]:{} -> {}:{} (owner {})",
				          t.Key.iListenPort,
				          t.Key.sTargetHost, t.Key.iTargetPort, t.Key.sOwnerUser)
				: kFormat("port {} bind failed: {}",
				          t.Key.iListenPort, entry.sError);
			m_Store->LogEvent(ev);
		}

		if (!entry.sError.empty())
		{
			m_Config.Message("tunnel '{}': {}", t.sName, entry.sError);
		}

		Listeners->emplace(t.sName, std::move(entry));
	}

} // ReconcileListeners

//-----------------------------------------------------------------------------
ExposedServer::~ExposedServer()
//-----------------------------------------------------------------------------
{
	// Stop per-tunnel listeners up front — their accept threads hold
	// references to *this via TunnelListener::m_ExposedServer, so they
	// must die before the ExposedServer members are torn down.
	auto Listeners = m_Listeners.unique();

	for (auto& kv : *Listeners)
	{
		if (kv.second.Listener && kv.second.Listener->IsRunning())
		{
			kv.second.Listener->Stop();
		}
	}
	Listeners->clear();
}

//-----------------------------------------------------------------------------
ExposedServer::ExposedServer (const Config& config)
//-----------------------------------------------------------------------------
: m_Config(config)
{
	const bool bStateful = (m_Config.eMode == ExtendedConfig::Mode::Stateful);

	if (bStateful)
	{
		// Persistent store: sqlite file under the config dir (or wherever
		// -db put it). The ctor creates the parent directory (mode 0700)
		// and runs the schema migration. HasError() surfaces migration
		// issues.
		m_Store = std::make_unique<KTunnelStore>(m_Config.sDatabasePath);

		if (m_Store->HasError())
		{
			throw KError(kFormat("cannot open store '{}': {}",
			                     m_Store->GetDatabasePath(),
			                     m_Store->GetLastError()));
		}

		m_Config.Message("admin store: {}", m_Store->GetDatabasePath());

		// Shared bcrypt verifier used for both tunnel-login
		// (VerifyTunnelLogin) and the admin UI login. We prefer one
		// common instance so the asynchronously-computed workload is
		// calibrated once per process.
		m_BCrypt = std::make_unique<KBCrypt>(std::chrono::milliseconds(100), true);

		// Friendly hint when the operator forgot to seed an admin
		// user: the runtime keeps going so existing tunnels still
		// pass traffic, but the admin UI is unreachable until either
		// `ktunnel -set-admin` is run or a row is inserted out-of-
		// band.
		if (m_Store->CountUsers() == 0)
		{
			m_Config.Message("warning: no users in {} — run `ktunnel -set-admin` "
			                 "to create the admin login (admin UI is unreachable "
			                 "until then)",
			                 m_Store->GetDatabasePath());
		}
	}
	else
	{
		// AdHoc: no SQLite file, no bcrypt, no admin UI. Peer
		// authentication falls back to `-s <secret>` in
		// VerifyTunnelLogin. The synthetic "cli" tunnel produced by
		// GatherDesiredTunnels() is the only listener.
		m_Config.Message("ad-hoc mode: no persistent store, no admin UI, "
		                 "peer authentication via -s secret(s)");
	}

	KREST::Options Settings;

	Settings.Type                     = KREST::HTTP;
	Settings.iPort                    = m_Config.iPort;
	Settings.iMaxConnections          = m_Config.iMaxTunneledConnections * 2 + 20;
	Settings.iTimeout                 = m_Config.Timeout.seconds().count();
	Settings.KLogHeader               = "";
	Settings.bBlocking                = false;
	Settings.bMicrosecondTimerHeader  = true;
	Settings.TimerHeader              = "x-microseconds";

	if (m_Config.bNoTLS)
	{
		Settings.bCreateEphemeralCert = false;
	}
	else
	{
		Settings.sCert                = m_Config.sCertFile;
		Settings.sKey                 = m_Config.sKeyFile;
		Settings.sTLSPassword         = m_Config.sTLSPassword;
		Settings.sAllowedCipherSuites = m_Config.sCipherSuites;
		Settings.bCreateEphemeralCert = true;
		Settings.bStoreEphemeralCert  = m_Config.bPersistCert;
	}

	Settings.AddHeader(KHTTPHeader::SERVER, "ktunnel");
	Settings.AddHeader("x-server", "ktunnel");

	KRESTRoutes Routes;

	Routes.AddRoute("/Version").Get([](KRESTServer& HTTP)
	{
		html::Page Page("Version", "en_US");
		Page.Body() += html::Text("ktunnel v1.0");
		HTTP.SetRawOutput(Page.Serialize());
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE , KMIME::HTML_UTF8);

	}).Parse(KRESTRoute::ParserType::NOREAD);

	// Instantiate the admin UI (cookie-session auth, HTML via
	// KWebObjects) and register its routes under /Configure/.. — see
	// ktunnel_admin.cpp. We pass a pointer to this ExposedServer so
	// the UI can look up live tunnels, persisted users/config, and
	// share the bcrypt verifier. In AdHoc mode there is no persistent
	// store to drive the UI off of, so we simply do not register it.
	if (bStateful)
	{
		m_AdminUI = std::make_unique<AdminUI>(*this);
		m_AdminUI->RegisterRoutes(Routes);
	}

	Routes.AddRoute("/Tunnel").Get([this](KRESTServer& HTTP)
	{
		// check user/secret - NOT!
		// we do this now with the first frame coming through the tunnel,
		// as this allows us to encrypt the login even on non-TLS connections
#if 0
		auto Creds = HTTP.Request.GetBasicAuthParms();

		if (m_Config.Secrets.empty() || !m_Config.Secrets.contains(Creds.sPassword))
		{
			throw KError(kFormat("invalid secret from {}: {}", HTTP.GetRemoteIP(), Creds.sPassword));
		}
#endif

		// set the handler for websockets in the REST server instance
		HTTP.SetWebSocketHandler([this](KWebSocket& WebSocket)
		{
			ControlStream(WebSocket.MoveStream());
		});

		// ask for per-thread execution in the REST server instance
		HTTP.SetKeepWebSocketInRunningThread();

	}).Parse(KRESTRoute::ParserType::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	// set a default route — in Stateful mode we redirect unknown
	// paths to the admin UI, in AdHoc mode there is no admin UI so
	// we just 404 (with the robots.txt and favicon shortcuts intact).
	Routes.SetDefaultRoute([bStateful](KRESTServer& HTTP)
	{
		if (HTTP.RequestPath.sRoute == "/favicon.ico" ||
			HTTP.RequestPath.sRoute.starts_with("/apple-touch-icon"))
		{
			HTTP.Response.SetStatus(KHTTPError::H4xx_NOTFOUND);
		}
		else if (HTTP.RequestPath.sRoute == "/robots.txt")
		{
			HTTP.SetRawOutput("User-agent: *\nDisallow: /\n");
			HTTP.Response.SetStatus(KHTTPError::H2xx_OK);
			HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		}
		else if (bStateful)
		{
			HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/Configure/");
		}
		else
		{
			HTTP.Response.SetStatus(KHTTPError::H4xx_NOTFOUND);
		}
	});

	// create the REST server instance
	KREST Http;

	// and run it
	m_Config.Message("starting {} server on port {}",
	                 m_Config.bNoTLS ? "TCP" : "TLS REST",
	                 Settings.iPort);

	if (!Http.Execute(Settings, Routes)) throw KError(Http.Error());

	// Start per-tunnel listeners — this is the single path that handles
	// both DB-configured tunnels (enabled rows in `tunnels`) and the
	// optional CLI-driven tunnel (`-f <port>` + `-t <target>`). See
	// GatherDesiredTunnels() for the source-of-truth merge. Each
	// desired entry becomes a non-blocking KTCPServer. Bind failures
	// (e.g. port in use) are captured as per-row errors in the registry
	// and surfaced on the Tunnels admin page — they do not prevent the
	// process from coming up. A pre-flight port-conflict check inside
	// ReconcileListeners makes sure a collision with the admin HTTPS
	// port, or between two desired tunnels, is reported with clear
	// tunnel names instead of letting the OS return EADDRINUSE.
	ReconcileListeners("bootstrap");

	// Block the main thread until shutdown. KREST owns the signal
	// handler for SIGINT/SIGTERM (via bBlocking=false +
	// RegisterSignalsForShutdown default), and Wait() returns once that
	// handler fires. The TunnelListener instances are torn down by the
	// ExposedServer destructor after Wait() returns.
	Http.Wait();

} // ExposedServer::ExposedServer


//-----------------------------------------------------------------------------
ProtectedHost::ProtectedHost(const ExtendedConfig& Config)
//-----------------------------------------------------------------------------
: m_Config(Config)
{
} // ProtectedHost::ctor

//-----------------------------------------------------------------------------
void ProtectedHost::Shutdown()
//-----------------------------------------------------------------------------
{
	// latch the quit flag first so a concurrent Run() that has just finished
	// Tunnel.Run() and is about to re-check the flag will see it
	m_bQuit.store(true, std::memory_order_release);

	// tear down any currently active tunnel so its blocking Run() returns
	// promptly; also wake up the inter-retry sleep (the wait_for below
	// keys off m_bQuit via the predicate)
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		if (m_pCurrentTunnel)
		{
			m_pCurrentTunnel->Stop();
		}
	}

	m_CV.notify_all();

} // ProtectedHost::Shutdown

//-----------------------------------------------------------------------------
void ProtectedHost::RunRepl(std::shared_ptr<KTunnel::Connection> Connection)
//-----------------------------------------------------------------------------
{
	if (!Connection) return;

	// Banner — identical newline convention as the rest of the handler
	// so the Browser-side REPL view can just append to a <pre>.
	Connection->WriteData("ktunnel peer REPL\ntype 'help' for commands\n\n> ");

	for (;;)
	{
		KString sLineBuf;
		if (!Connection->ReadData(sLineBuf)) break;   // channel closed by remote or tunnel teardown

		// Frames may arrive in chunks or with or without trailing
		// newlines (depending on how the initiator batches). Split on
		// newlines and process each non-empty line individually.
		KStringView sFrame = sLineBuf;
		while (!sFrame.empty())
		{
			auto iNL = sFrame.find('\n');
			KStringView sLine = (iNL == KStringView::npos) ? sFrame : sFrame.substr(0, iNL);
			sFrame = (iNL == KStringView::npos) ? KStringView{} : sFrame.substr(iNL + 1);

			KString sCmd(sLine);
			sCmd.TrimLeft(" \t\r");
			sCmd.TrimRight(" \t\r");

			if (sCmd.empty())
			{
				Connection->WriteData("> ");
				continue;
			}

			KString sReply;
			bool    bBye = false;

			if (sCmd == "help" || sCmd == "?")
			{
				sReply =
					"Commands:\n"
					"  help     - this help\n"
					"  status   - tunnel status and traffic counters\n"
					"  version  - build version\n"
					"  exit     - close this REPL session\n";
			}
			else if (sCmd == "status")
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				if (!m_pCurrentTunnel)
				{
					sReply = "no active tunnel\n";
				}
				else
				{
					sReply = kFormat(
						"connected to : {}\n"
						"peer user    : {}\n"
						"streams      : {}\n"
						"bytes rx     : {}\n"
						"bytes tx     : {}\n",
						m_pCurrentTunnel->GetEndPointAddress(),
						m_Config.sPeerUser,
						m_pCurrentTunnel->GetConnectionCount(),
						m_pCurrentTunnel->GetBytesRx(),
						m_pCurrentTunnel->GetBytesTx());
				}
			}
			else if (sCmd == "version")
			{
				sReply = "ktunnel peer (dekaf2)\n";
			}
			else if (sCmd == "exit" || sCmd == "quit")
			{
				Connection->WriteData("bye\n");
				bBye = true;
			}
			else
			{
				sReply = kFormat("unknown command: '{}' (try 'help')\n", sCmd);
			}

			if (bBye)
			{
				return;
			}

			Connection->WriteData(std::move(sReply));
			Connection->WriteData("> ");
		}
	}

} // ProtectedHost::RunRepl

//-----------------------------------------------------------------------------
void ProtectedHost::Run()
//-----------------------------------------------------------------------------
{
	KHTTPStreamOptions StreamOptions;
	// connect timeout to exposed host, also wait timeout between two tries
	StreamOptions.SetTimeout(m_Config.ConnectTimeout);
	// only do HTTP/1.1, we haven't implemented websockets for any later protocol
	// (but as we are not a browser that doesn't mean any disadvantage)
	StreamOptions.Unset(KStreamOptions::RequestHTTP2);

	while (!m_bQuit.load(std::memory_order_acquire))
	{
		try
		{
			// Peer login identity: the `-u <user>` CLI flag. Defaults to
			// "admin" so a fresh install works without extra setup — the
			// admin user is seeded from the first `-secret` during the
			// exposed host's bootstrap.
			KString sUsername = m_Config.sPeerUser;
			KString sSecret   = *m_Config.Secrets.begin();

			if (sSecret.empty())
			{
				kPrintLine("need a secret to login at {}", m_Config.ExposedHost);
				return;
			}

			KURL ExposedHost;
			ExposedHost.Protocol = m_Config.bNoTLS ? url::KProtocol::HTTP : url::KProtocol::HTTPS;
			ExposedHost.Domain   = m_Config.ExposedHost.Domain;
			ExposedHost.Port     = m_Config.ExposedHost.Port;
			ExposedHost.Path     = "/Tunnel";

			KWebSocketClient WebSocket(ExposedHost, StreamOptions);

			WebSocket.SetTimeout(m_Config.ConnectTimeout);
			WebSocket.SetBinary();

			m_Config.Message("connecting {}..", ExposedHost);

			if (!WebSocket.Connect())
			{
				throw KError("cannot establish tunnel connection");
			}

			// Per-connection KTunnel::Config copy (sliced from
			// ExtendedConfig) so we can inject the REPL callback
			// without mutating the shared m_Config.
			KTunnel::Config TunnelCfg = m_Config;
			TunnelCfg.OpenReplCallback =
				[this](std::shared_ptr<KTunnel::Connection> conn)
				{
					RunRepl(std::move(conn));
				};

			// we are the "client" side of the tunnel and have to login with user/pass
			KTunnel Tunnel(TunnelCfg, std::move(WebSocket.GetStream()), sUsername, sSecret);

			// publish the tunnel pointer so Shutdown() can stop it while
			// Run() is blocked in Tunnel.Run(). If Shutdown() was called
			// in the tiny window between the quit-check above and here,
			// honor it immediately.
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);

				if (m_bQuit.load(std::memory_order_acquire))
				{
					break;
				}

				m_pCurrentTunnel = &Tunnel;
			}

			// ensure the tunnel pointer is cleared no matter how Run()
			// returns (normal, exception, or Shutdown-induced disconnect)
			auto ClearTunnel = [this]() noexcept
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_pCurrentTunnel = nullptr;
			};
			auto Guard = KScopeGuard<decltype(ClearTunnel)>(ClearTunnel);

			m_Config.Message("control stream opened - now waiting for data streams");

			Tunnel.Run();

			if (m_bQuit.load(std::memory_order_acquire))
			{
				break;
			}

			m_Config.Message("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}
		catch(const std::exception& ex)
		{
			if (m_bQuit.load(std::memory_order_acquire))
			{
				break;
			}

			m_Config.Message("{}", ex.what());
			m_Config.Message("could not connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}

		// interruptible sleep until the next connect try — wait_for returns
		// early when Shutdown() sets m_bQuit and notifies the CV
		{
			std::unique_lock<std::mutex> Lock(m_Mutex);
			m_CV.wait_for(Lock, m_Config.ConnectTimeout.duration(), [this]()
			{
				return m_bQuit.load(std::memory_order_acquire);
			});
		}
	}

	m_Config.Message("protected host shut down");

} // ProtectedHost::Run

//-----------------------------------------------------------------------------
int Tunnel::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// initialize with signal handler thread
	KInit(true).SetName("KTUNL").KeepCLIMode();

	// we will throw when setting errors
	SetThrowOnError(true);

	// setup CLI option parsing
	KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

	// Add the service-management help block from KService, plus our
	// own admin-bootstrap subcommand documentation. Both are evaluated
	// in main() before this KOptions parser ever runs (see
	// FilterBootstrapArgs); we include the help here purely so they
	// show up in `ktunnel --help`.
	Options.SetAdditionalHelp(kFormat(
		"{}\n"
		"admin bootstrap (one-shot, evaluated before service registration):\n"
		"  -set-admin                  : create or rotate the admin user in the\n"
		"                                config DB, then exit. Use this before\n"
		"                                first interactive launch, or any time\n"
		"                                you need to rotate the password.\n"
		"  -admin-user <name>          : username for -set-admin / -install\n"
		"                                (default: admin). Stored once into the\n"
		"                                `users` table; not persisted into the\n"
		"                                generated unit / plist / SCM record.\n"
		"  -pass-file <path>           : read the admin password from <path>\n"
		"                                instead of prompting on the TTY. Used\n"
		"                                for non-interactive provisioning. The\n"
		"                                file is read once and not referenced\n"
		"                                by the running service.\n",
		KService::GetHelp()));

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort         = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort      = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets   = Options("s,secret     : on the protected host: REQUIRED, the password used to log in to the exposed host (must match a row in the exposed host's `users` table). On the exposed host: only used in ad-hoc mode (with -f / -t), where it is the comma-separated list of pre-shared secrets for peer authentication; in stateful mode it is ignored — manage the admin password via `ktunnel -set-admin` and peer users via the admin UI.", "").String();
	m_Config.DefaultTarget = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunneledConnections
	                       = Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.sCertFile     = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused a self-signed cert is created", "");
	m_Config.sKeyFile      = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused a new key is created", "");
	m_Config.sTLSPassword  = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_Config.bNoTLS        = Options("notls        : do not use TLS, but unencrypted HTTP", false);
	m_Config.bAESPayload   = Options("aes          : encrypt payload with AES (additional to a TLS connection)", false);
	m_Config.Timeout       = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_Config.bPersistCert  = Options("persist      : should a self-signed cert be persisted to disk and reused at next start?", false);
	m_Config.sCipherSuites = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_Config.bQuiet        = Options("q,quiet      : do not output status to stdout", false);
	m_Config.sDatabasePath = Options("db <path>    : if exposed host (stateful mode), path to the SQLite admin/config DB. Defaults to /var/lib/ktunnel/ktunnel.db when running as root (or as a system service), $HOME/.config/ktunnel/ktunnel.db otherwise. Not used in ad-hoc mode (with -f / -t).", "");
	m_Config.sPeerUser     = Options("u,user <name> : if protected host, username to log in with (must exist in exposed host's users table) - defaults to 'admin'", "admin");

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	if (!m_Config.iPort)
	{
		SetError("need valid port number");
	}

	if (m_Config.ExposedHost.Port.empty())
	{
		m_Config.ExposedHost.Port.get() = m_Config.iPort;
	}

	if (!IsExposed() && (!m_Config.sCertFile.empty() || !m_Config.sKeyFile.empty()))
	{
		SetError("the protected host may not have a cert or key option");
	}

	// split and store list of secrets
	for (auto& sSecret : sSecrets.Split())
	{
		m_Config.Secrets.insert(sSecret);
	}

	if (!m_Config.iMaxTunneledConnections) SetError("maxtunnels should be at least 1");

	// On the protected host a secret is required to authenticate against
	// the exposed peer. On the exposed host it is optional: without -s,
	// the bootstrap admin user is not seeded and peer logins must be
	// authenticated entirely against the users table in the store.
	if (!IsExposed() && m_Config.Secrets.empty())
	{
		SetError("protected host needs a (shared) secret");
	}

	if (IsExposed())
	{
		if (!m_Config.DefaultTarget.empty()
		    && m_Config.DefaultTarget.Port.get() == 0)
		{
			SetError("-t / -target needs an explicit port number");
		}

		// --------------------------------------------------------------
		// Mode detection — see ExtendedConfig::Mode.
		//
		// Rules:
		//   * Launched by a service manager (systemd / launchd / SCM)
		//     → Stateful. -f / -t / -s are forbidden: they would
		//     either land in the generated unit file (security risk:
		//     secrets on disk, world-readable) or silently compete
		//     with DB rows. Configure tunnels via the admin UI and
		//     seed the admin password via `-install` (Phase 3) or
		//     `-set-admin` (Phase 3).
		//   * Interactive launch with -f and -t on the CLI
		//     → AdHoc. No SQLite file is opened, no admin UI, peer
		//     auth uses -s only. Intended for recovery / bootstrap
		//     on an SSH session when the admin UI is unreachable.
		//   * Interactive launch without -f / -t
		//     → Stateful. Everything comes from the DB; the operator
		//     uses the admin UI at /Configure/ to add tunnels. -s
		//     optionally bootstraps the admin password on first run
		//     (legacy behaviour, to be removed in Phase 3).
		// --------------------------------------------------------------
		const bool bIsService     = KService::IsRunningAsService();
		const bool bHasCliTunnel  = (m_Config.iRawPort != 0)
		                         || !m_Config.DefaultTarget.empty();
		const bool bHasCliSecret  = !m_Config.Secrets.empty();
		const bool bHasExplicitDB = !m_Config.sDatabasePath.empty();

		if (bIsService)
		{
			m_Config.eMode = ExtendedConfig::Mode::Stateful;

			if (bHasCliTunnel)
			{
				SetError("in service mode, -f / -t are not allowed; "
				         "configure tunnels via the admin UI");
			}
			if (bHasCliSecret)
			{
				SetError("in service mode, -s is not allowed; "
				         "the `users` table in the admin DB is "
				         "authoritative for peer authentication");
			}
		}
		else if (bHasCliTunnel)
		{
			m_Config.eMode = ExtendedConfig::Mode::AdHoc;

			if (!bHasCliSecret)
			{
				SetError("ad-hoc mode (-f / -t on the CLI) requires "
				         "-s <secret> for tunnel authentication");
			}
			if (bHasExplicitDB)
			{
				SetError("-db is not compatible with -f / -t ad-hoc "
				         "mode; either drop -db (ad-hoc) or drop "
				         "-f / -t (stateful via admin UI)");
			}
			if (m_Config.iRawPort == 0 || m_Config.DefaultTarget.empty())
			{
				SetError("-f and -t must both be given to define the "
				         "ad-hoc tunnel");
			}
		}
		else
		{
			m_Config.eMode = ExtendedConfig::Mode::Stateful;
		}

		// Resolve the DB path for Stateful mode. AdHoc leaves
		// sDatabasePath empty so ExposedServer skips opening a store.
		if (m_Config.eMode == ExtendedConfig::Mode::Stateful
		    && !bHasExplicitDB)
		{
			// Prefer the system-wide path only when running as a
			// root-equivalent service (UID 0 on POSIX, LocalSystem
			// assumption on Windows: kGetUid() returns 0 there
			// regardless). Interactive root launches also get the
			// system path, which matches `sudo ktunnel ...` intent.
			const bool bSystemScope = (kGetUid() == 0);
			m_Config.sDatabasePath = KTunnelStore::DefaultDatabasePath(bSystemScope);
		}

		// In interactive Stateful mode, -s no longer does anything (the
		// `users` table is the only authority for admin / peer logins).
		// We accept it without error but log a warning so an operator
		// who pasted the old CLI sees what changed and how to migrate.
		if (m_Config.eMode == ExtendedConfig::Mode::Stateful
		    && !m_Config.Secrets.empty())
		{
			m_Config.Message("note: -s is ignored in stateful mode; "
			                 "use `ktunnel -set-admin` to set the admin "
			                 "password and the admin UI for peer users");
			m_Config.Secrets.clear();   // make sure no code path picks
			                            // it up by accident
		}

		m_Config.Message("starting as exposed host with{} TLS in {} mode",
		                 m_Config.bNoTLS ? "out" : "",
		                 m_Config.eMode == ExtendedConfig::Mode::AdHoc ? "ad-hoc" : "stateful");

		if (m_Config.eMode == ExtendedConfig::Mode::Stateful)
		{
			m_Config.Message("admin UI available at {}://<host>:{}/Configure/",
			                 m_Config.bNoTLS ? "http" : "https", m_Config.iPort);
		}

		ExposedServer Exposed(m_Config);
	}
	else
	{
		m_Config.Message("starting as protected host, connecting exposed host with{} TLS at {}",
		                 m_Config.bNoTLS ? "out" : "",
		                 m_Config.ExposedHost);

		ProtectedHost Protected(m_Config);

		// Service-manager shutdown is now routed through
		// ProtectedHost::Shutdown() on every platform:
		//
		//   * Windows SCM stop: KService::SetShutdownHandler is the
		//     ONLY path (the SCM does not deliver SIGINT / SIGTERM).
		//   * POSIX systemctl stop / launchctl stop / launchctl unload:
		//     KService::SetShutdownHandler hooks SIGTERM specifically;
		//     SIGINT (interactive Ctrl+C) is deliberately left at the
		//     KSignals default (std::exit(EXIT_SUCCESS)) for an instant
		//     exit, since that matches user expectation for Ctrl+C.
		//
		// Why this is now safe (and was not before): KTunnel::Stop()
		// closes the tunnel stream which calls SignalDisconnecting() on
		// KIOStreamSocket. That sets the disconnecting flag AND wakes
		// the KPollInterruptor (eventfd on Linux, pipe on other POSIX,
		// no-op on Windows because WSAPoll auto-unblocks on socket
		// close), so a blocked poll() inside KTunnel::ReadMessage()
		// returns promptly instead of sitting out a full ControlPing
		// timeout. This means a SIGTERM from the service manager
		// produces a clean disconnect within milliseconds — no risk
		// of systemd's TimeoutStopSec firing SIGKILL.
		KService::SetShutdownHandler([&Protected]()
		{
			Protected.Shutdown();
		});

		Protected.Run();

		// Detach the handler before Protected goes out of scope so a
		// late stop event from the service manager (or, on POSIX, a
		// belated SIGTERM from a parent shell) cannot dereference a
		// dangling reference.
		KService::SetShutdownHandler({});
	}

	return 0;

} // Main

// =============================================================================
// helpers — admin-bootstrap subcommands (`-install`, `-set-admin`)
// =============================================================================
//
// These run BEFORE KService::Run() in main(): they take a quick look at argv,
// branch on whichever of our own subcommands is present, and either short-
// circuit (`-set-admin`: do the DB upsert and exit) or do their pre-work and
// then fall through to KService::Run (`-install`: prompt for password and
// seed the DB before letting KService register the service unit).
//
// We do NOT use KOptions for this preliminary parse: KOptions expects to
// describe the *full* argument set and would reject the service-management
// flags. A small bespoke argv-walker is good enough for the handful of flags
// we care about here, and it leaves the eventual KOptions parse in
// Tunnel::Main untouched.

namespace {

//-----------------------------------------------------------------------------
/// Match a CLI flag with single- or double-dash prefix. Both `-install` and
/// `--install` are accepted (matches the convention used by KService::HandleCLI).
bool MatchesBootstrapFlag (KStringView sArg, KStringView sName)
//-----------------------------------------------------------------------------
{
	if      (sArg.starts_with("--")) sArg.remove_prefix(2);
	else if (sArg.starts_with("-"))  sArg.remove_prefix(1);
	else                             return false;
	return sArg == sName;

} // MatchesBootstrapFlag

//-----------------------------------------------------------------------------
/// Returns true if `sName` (without dash prefix) is anywhere in argv.
bool HasBootstrapFlag (int argc, char** argv, KStringView sName)
//-----------------------------------------------------------------------------
{
	for (int i = 1; i < argc; ++i)
	{
		if (MatchesBootstrapFlag(argv[i], sName)) return true;
	}
	return false;

} // HasBootstrapFlag

//-----------------------------------------------------------------------------
/// Returns the value following `-sName` / `--sName` in argv (i.e. argv[i+1]),
/// or an empty view if the flag is missing or has no following token.
KStringViewZ GetBootstrapFlagValue (int argc, char** argv, KStringView sName)
//-----------------------------------------------------------------------------
{
	for (int i = 1; i + 1 < argc; ++i)
	{
		if (MatchesBootstrapFlag(argv[i], sName)) return argv[i + 1];
	}
	return {};

} // GetBootstrapFlagValue

//-----------------------------------------------------------------------------
/// Read a password from `-pass-file <path>` if the operator supplied one,
/// stripping a single trailing newline (Unix or DOS). Returns true if the
/// flag was given AND the file was read successfully; on any failure we
/// emit a stderr message and return false so the caller can abort.
bool TryReadPassFile (int argc, char** argv, KString& sPasswordOut)
//-----------------------------------------------------------------------------
{
	auto sPath = GetBootstrapFlagValue(argc, argv, "pass-file");
	if (sPath.empty()) return false;

	if (!kReadAll(sPath, sPasswordOut))
	{
		KErr.FormatLine(">> ktunnel: cannot read password file '{}'", sPath);
		sPasswordOut.clear();
		return false;
	}

	// Strip exactly one trailing newline if present — many editors add
	// one automatically and we do NOT want it to become part of the
	// hashed password.
	while (!sPasswordOut.empty()
	       && (sPasswordOut.back() == '\n' || sPasswordOut.back() == '\r'))
	{
		sPasswordOut.pop_back();
	}

	if (sPasswordOut.empty())
	{
		KErr.FormatLine(">> ktunnel: password file '{}' is empty", sPath);
		return false;
	}
	return true;

} // TryReadPassFile

//-----------------------------------------------------------------------------
/// Pick the DB path the bootstrap subcommands should write to:
///   1. explicit `-db <path>` always wins
///   2. otherwise: KTunnelStore::DefaultDatabasePath(bWantSystem)
/// where `bWantSystem` follows the same rule as the runtime resolution
/// in Tunnel::Main (UID 0 → system path, else user path). For -install
/// we additionally pass `bForceSystem` so a non-root operator using
/// `sudo ktunnel -install` ends up writing to /var/lib/... rather than
/// /root/.config/...
KString ResolveBootstrapDBPath (int argc, char** argv, bool bForceSystem)
//-----------------------------------------------------------------------------
{
	auto sExplicit = GetBootstrapFlagValue(argc, argv, "db");
	if (!sExplicit.empty()) return sExplicit;

	const bool bSystem = bForceSystem || (kGetUid() == 0);
	return KTunnelStore::DefaultDatabasePath(bSystem);

} // ResolveBootstrapDBPath

//-----------------------------------------------------------------------------
/// Open the store at @p sPath, hash @p sPassword, upsert (insert or
/// update) the admin row at @p sUser. Returns true on success and emits
/// success / failure messages to stdout / stderr.
bool SeedAdminUser (KStringView  sPath,
                    KStringView  sUser,
                    KStringViewZ sPassword)
//-----------------------------------------------------------------------------
{
	KTunnelStore Store(sPath);

	if (Store.HasError())
	{
		KErr.FormatLine(">> ktunnel: cannot open store '{}': {}",
		                Store.GetDatabasePath(),
		                Store.GetLastError());
		return false;
	}

	KBCrypt BCrypt(std::chrono::milliseconds(100), /*bComputeAtFirstUse=*/false);

	auto sHash = BCrypt.GenerateHash(sPassword);

	if (sHash.empty())
	{
		KErr.FormatLine(">> ktunnel: failed to hash admin password");
		return false;
	}

	auto oExisting = Store.GetUser(sUser);
	
	if (oExisting)
	{
		// Rotate the password and make sure the admin bit is set.
		if (!Store.UpdateUserPasswordHash(sUser, sHash)
		 || !Store.UpdateUserAdminFlag(sUser, true))
		{
			KErr.FormatLine(">> ktunnel: cannot update admin user '{}': {}",
			                sUser, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("admin user '{}' updated in {}",
		                sUser, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind     = "admin_password_change";
		ev.sUsername = KString(sUser);
		ev.sDetail   = "password rotated via -set-admin / -install";
		Store.LogEvent(ev);
	}
	else
	{
		KTunnelStore::User u;
		u.sUsername     = sUser;
		u.sPasswordHash = sHash;
		u.bIsAdmin      = true;
		if (!Store.AddUser(u))
		{
			KErr.FormatLine(">> ktunnel: cannot create admin user '{}': {}",
			                sUser, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("admin user '{}' created in {}",
		                sUser, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind     = "bootstrap";
		ev.sUsername = u.sUsername;
		ev.sDetail   = "admin user seeded via -set-admin / -install";
		Store.LogEvent(ev);
	}

	return true;

} // SeedAdminUser

//-----------------------------------------------------------------------------
/// Interactive password capture: prompt twice (echo off), require both
/// entries to match and to be non-empty. Up to @p iMaxAttempts attempts.
/// Falls back to `-pass-file` if that flag was given, in which case no
/// prompt is shown at all (TTY is not even consulted).
bool CapturePassword (int argc, char** argv, KString& sPasswordOut,
                      std::size_t iMaxAttempts = 3)
//-----------------------------------------------------------------------------
{
	if (HasBootstrapFlag(argc, argv, "pass-file"))
	{
		return TryReadPassFile(argc, argv, sPasswordOut);
	}

#if !defined(DEKAF2_IS_WINDOWS)
	if (!::isatty(STDIN_FILENO))
	{
		KErr.FormatLine(">> ktunnel: stdin is not a TTY; supply -pass-file <path> for non-interactive installs");
		return false;
	}
#endif

	for (std::size_t iAttempt = 0; iAttempt < iMaxAttempts; ++iAttempt)
	{
		auto sFirst  = kPromptForPassword("Set admin password:    ");
		if (sFirst.empty())
		{
			KErr.FormatLine(">> ktunnel: empty password not allowed");
			continue;
		}
		auto sSecond = kPromptForPassword("Confirm admin password: ");
		if (sFirst == sSecond)
		{
			sPasswordOut = std::move(sFirst);
			return true;
		}
		KErr.FormatLine(">> ktunnel: passwords do not match, please try again");
	}

	KErr.FormatLine(">> ktunnel: too many failed attempts, giving up");
	return false;

} // CapturePassword

//-----------------------------------------------------------------------------
/// Handle `-set-admin`: standalone subcommand that creates or rotates the
/// admin user in the configuration DB and exits. Intended for
///   - first-time setup on an interactive box that was never `-install`ed
///   - password rotation after installation
///   - automated provisioning via `-pass-file` (e.g. cloud-init, Ansible)
int RunSetAdmin (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// We deliberately do NOT use `-u` here — that flag belongs to the
	// runtime tunnel (`Tunnel::Main`: peer username on the protected
	// host) and re-using it would create a footgun where the same arg
	// means different things in different contexts.
	auto sUser = GetBootstrapFlagValue(argc, argv, "admin-user");
	if (sUser.empty()) sUser = "admin";

	const auto sDBPath = ResolveBootstrapDBPath(argc, argv, /*bForceSystem=*/false);

	KString sPassword;

	if (!CapturePassword(argc, argv, sPassword))
	{
		return 1;
	}

	return SeedAdminUser(sDBPath, sUser, sPassword) ? 0 : 1;

} // RunSetAdmin

//-----------------------------------------------------------------------------
/// Handle the bootstrap step that runs immediately before the actual
/// service registration. Same rules as RunSetAdmin, except:
///   - the system-wide DB path is preferred (sudo ktunnel -install)
///   - on success we DO NOT exit; main() falls through to KService::Run
///     which then writes the unit / plist / SCM record.
bool BootstrapAdminForInstall (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto sUser = GetBootstrapFlagValue(argc, argv, "admin-user");
	if (sUser.empty()) sUser = "admin";

	const auto sDBPath = ResolveBootstrapDBPath(argc, argv, /*bForceSystem=*/true);

	KOut.FormatLine("ktunnel: pre-install admin bootstrap → {}", sDBPath);

	KString sPassword;
	
	if (!CapturePassword(argc, argv, sPassword))
	{
		return false;
	}

	return SeedAdminUser(sDBPath, sUser, sPassword);

} // BootstrapAdminForInstall

//-----------------------------------------------------------------------------
/// Build a filtered copy of argv that omits the bootstrap-only flags.
/// KService::Install() persists every argv element it sees into the
/// generated systemd unit / launchd plist / SCM record, so we MUST
/// strip the one-shot bootstrap flags here — otherwise:
///   * `-pass-file <path>` would end up in the unit file and the
///     bootstrap secret would re-execute (or fail) on every service
///     start;
///   * `-admin-user` is meaningless to the runtime and would cause a
///     KOptions parse error in Tunnel::Main on every service launch.
/// Flags that ARE meaningful at runtime (`-db`, `-p`, `-cert`, etc.)
/// pass through unmodified.
///
/// Returns a vector of pointers into the original argv array — the
/// caller keeps that vector alive for as long as the filtered argv
/// is in use. The vector is null-terminated to match argv conventions.
std::vector<char*> FilterBootstrapArgs (int& argc_inout, char** argv)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView SkipFlags[] = {
		KStringView("admin-user"),
		KStringView("pass-file"),
	};

	std::vector<char*> out;
	out.reserve(static_cast<std::size_t>(argc_inout) + 1);

	for (int i = 0; i < argc_inout; ++i)
	{
		bool bSkipped = false;
		for (auto sSkip : SkipFlags)
		{
			if (MatchesBootstrapFlag(argv[i], sSkip))
			{
				// Skip the flag itself and consume its value (if any).
				if (i + 1 < argc_inout) ++i;
				bSkipped = true;
				break;
			}
		}
		if (!bSkipped) out.push_back(argv[i]);
	}

	argc_inout = static_cast<int>(out.size());
	out.push_back(nullptr);   // argv is conventionally null-terminated
	return out;

} // FilterBootstrapArgs

} // anonymous namespace

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// admin-bootstrap subcommands. We intercept these BEFORE
	// KService::Run() because:
	//   * `-set-admin` is purely our concern; KService doesn't know it.
	//   * `-install` is KService's, but we need to seed the admin user
	//     into the DB before KService writes the systemd unit / plist /
	//     SCM record. If the bootstrap fails (e.g. password mismatch,
	//     DB unwritable) we abort immediately so no half-installed
	//     service is left behind.
	if (HasBootstrapFlag(argc, argv, "set-admin"))
	{
		return RunSetAdmin(argc, argv);
	}

	if (HasBootstrapFlag(argc, argv, "install"))
	{
		if (!BootstrapAdminForInstall(argc, argv)) return 1;
		// Fall through: KService::Run sees -install and registers the
		// service. The runtime service launch will then read the seeded
		// admin password from the same DB.
	}

	// Strip bootstrap-only flags (-admin-user, -pass-file) so they are
	// not persisted into the unit / plist / SCM record. Done unconditionally
	// because KService::Run also dispatches things like -uninstall / -start
	// / -stop where these flags would simply be noise. The filtered argv
	// is owned by `FilteredArgv` for the rest of main().
	auto FilteredArgv = FilterBootstrapArgs(argc, argv);
	char** const filtered_argv = FilteredArgv.data();

	// KService::Run() handles service-management flags (-install / -uninstall /
	// -start / -stop / -status / --help) before fnMain runs, and engages the
	// SCM / systemd / launchd integration when the process was launched by a
	// service manager. Interactive launches fall through to the lambda.
	return KService::Run("ktunnel", argc, filtered_argv,
		[](int ac, char** av)
		{
			try
			{
				return Tunnel().Main(ac, av);
			}
			catch (const std::exception& ex)
			{
				KErr.FormatLine(">> {}: {}", "ktunnel", ex.what());
			}
			return 1;
		},
		true,
		"KTunnel",
		"Secure reverse tunnel");
}
