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
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <csignal> // SIGINT, SIGTERM
#include <future>
#include <chrono>
#include <unordered_set>

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
	// Stable "first by user-name" — good enough for the legacy raw-port
	// forwarder. A per-listener owner is wired in by step 5.
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
	std::thread TunnelThread([Tunnel]()
	{
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

		KTunnelStore::Event ev;
		ev.sKind     = "tunnel_disconnect";
		ev.sUsername = sUser;
		ev.sRemoteIP = EndpointAddress.Serialize();
		ev.sDetail   = kFormat("rx={} tx={}", Tunnel->GetBytesRx(), Tunnel->GetBytesTx());
		m_Store->LogEvent(ev);
	}

	m_Config.Message("[control]: closed control stream from {}", EndpointAddress);

} // ControlStream

//-----------------------------------------------------------------------------
void ExposedServer::ForwardStream(KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	// this is the stream between the client and the first ktunnel instance which
	// will be forwarded through the tunnel

	kDebug(1, "incoming connection from {} for target {}", Downstream.GetEndPointAddress(), Endpoint);

	if (Endpoint.empty())
	{
		throw KError("missing target endpoint definition with domain:port");
	}

	// Legacy raw-port forwarder: there is currently one listener for the
	// whole server (`-f <port>`), so we pick whichever tunnel happens to
	// be connected. Step 5 adds per-listener owner routing.
	auto Tunnel = PickDefaultTunnel();

	if (!Tunnel)
	{
		throw KError("no tunnel established");
	}

	// is already set from KTCPServer ?
	Downstream.SetTimeout(m_Config.Timeout);

	// connect through the tunnel
	// will throw or return after connection is closed
	Tunnel->Connect(&Downstream, Endpoint);

} // ForwardStream

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

	auto Tunnel = GetTunnelForUser(sOwner);
	if (!Tunnel)
	{
		// The peer for this owner is not connected right now. Log it
		// once per rejected connection and close cleanly — we prefer
		// that over letting the caller's TCP half-open the socket.
		if (m_Store)
		{
			KTunnelStore::Event ev;
			ev.sKind     = "auth_reject";
			ev.sUsername = KString(sOwner);
			ev.sRemoteIP = Downstream.GetEndPointAddress().Serialize();
			ev.sDetail   = kFormat("owner '{}' not currently connected — "
			                       "dropped raw conn from {}",
			                       sOwner, Downstream.GetEndPointAddress());
			m_Store->LogEvent(ev);
		}
		throw KError(kFormat("no active tunnel for owner '{}'", sOwner));
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
	std::unordered_set<KString> LiveOwners;
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
void ExposedServer::ReconcileListeners (KStringView sActor)
//-----------------------------------------------------------------------------
{
	if (!m_Store) return;

	// Pull the *desired* state from the store under its own lock,
	// then apply the diff under the listener-map's unique lock so
	// the registry stays consistent across concurrent admin-UI
	// requests.
	auto Desired = m_Store->GetEnabledTunnels();

	auto Listeners = m_Listeners.unique();

	// Build a quick lookup of what we want after reconciling.
	std::unordered_map<KString, ListenerKey> DesiredByName;
	DesiredByName.reserve(Desired.size());
	for (const auto& t : Desired)
	{
		ListenerKey k;
		k.iListenPort = t.iListenPort;
		k.sTargetHost = t.sTargetHost;
		k.iTargetPort = t.iTargetPort;
		k.sOwnerUser  = t.sOwnerUser;
		DesiredByName.emplace(t.sName, std::move(k));
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

			KTunnelStore::Event ev;
			ev.sKind       = "tunnel_stop";
			ev.sUsername   = KString(sActor);
			ev.sTunnelName = it->first;
			ev.sDetail     = bGone ? "removed from registry"
			                       : "restarting due to config change";
			m_Store->LogEvent(ev);

			it = Listeners->erase(it);
		}
		else
		{
			++it;
		}
	}

	// 2. Start listeners for all desired rows that are not yet in the
	//    registry. Bind failures are captured as sError but do NOT
	//    throw so a single bad row can't take the admin UI down.
	for (const auto& t : Desired)
	{
		if (Listeners->count(t.sName)) continue;   // already up (unchanged)

		ListenerEntry entry;
		entry.Key.iListenPort = t.iListenPort;
		entry.Key.sTargetHost = t.sTargetHost;
		entry.Key.iTargetPort = t.iTargetPort;
		entry.Key.sOwnerUser  = t.sOwnerUser;

		KTCPEndPoint Target(t.sTargetHost, t.iTargetPort);

		try
		{
			entry.Listener = std::make_unique<TunnelListener>
			(
				this,
				t.sName,
				t.sOwnerUser,
				Target,
				t.iListenPort,            // KTCPServer ctor: port
				/* bSSL        = */ false,
				/* iMaxConns   = */ m_Config.iMaxTunneledConnections
			);

			// Non-blocking: Start() returns after the accept thread is
			// up and running.
			if (!entry.Listener->Start(m_Config.Timeout, /*bBlock=*/false))
			{
				entry.sError = kFormat("Start() failed on port {}", t.iListenPort);
			}
		}
		catch (const std::exception& ex)
		{
			entry.sError   = ex.what();
			entry.Listener.reset();
		}

		KTunnelStore::Event ev;
		ev.sKind       = entry.sError.empty() ? "tunnel_start" : "tunnel_error";
		ev.sUsername   = KString(sActor);
		ev.sTunnelName = t.sName;
		ev.sDetail     = entry.sError.empty()
			? kFormat("listening on [::]:{} -> {}:{} (owner {})",
			          t.iListenPort,
			          t.sTargetHost, t.iTargetPort, t.sOwnerUser)
			: kFormat("port {} bind failed: {}", t.iListenPort, entry.sError);
		m_Store->LogEvent(ev);

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

//-----------------------------------------------------------------------------
ExposedServer::ExposedServer (const Config& config)
//-----------------------------------------------------------------------------
: m_Config(config)
{
	// Persistent store: sqlite file under the config dir (or wherever -db
	// put it). The ctor creates the parent directory (mode 0700) and runs
	// the schema migration. HasError() surfaces migration issues.
	m_Store = std::make_unique<KTunnelStore>(m_Config.sDatabasePath);

	if (m_Store->HasError())
	{
		throw KError(kFormat("cannot open store '{}': {}",
		                     m_Store->GetDatabasePath(),
		                     m_Store->GetLastError()));
	}

	m_Config.Message("admin store: {}", m_Store->GetDatabasePath());

	// Shared bcrypt verifier used for both tunnel-login (VerifyTunnelLogin)
	// and the admin UI login. We prefer one common instance so the
	// asynchronously-computed workload is calibrated once per process.
	m_BCrypt = std::make_unique<KBCrypt>(std::chrono::milliseconds(100), true);

	// Bootstrap-admin seeding: if the users table is empty, insert a row
	// with the hash derived from the first -secret on the CLI. After
	// that, the store is authoritative — changing -secret does *not*
	// update the admin password (the user has to do that through the
	// admin UI or by editing the DB directly).
	if (m_Store->CountUsers() == 0 && !m_Config.sAdminPassHash.empty())
	{
		KTunnelStore::User u;
		u.sUsername     = m_Config.sAdminUser;
		u.sPasswordHash = m_Config.sAdminPassHash;
		u.bIsAdmin      = true;

		if (m_Store->AddUser(u))
		{
			m_Config.Message("seeded bootstrap admin user '{}' from first -secret", u.sUsername);

			KTunnelStore::Event ev;
			ev.sKind     = "bootstrap";
			ev.sUsername = u.sUsername;
			ev.sDetail   = "admin user seeded from first -secret";
			m_Store->LogEvent(ev);
		}
		else
		{
			m_Config.Message("cannot seed admin user '{}': {}",
			                 u.sUsername, m_Store->GetLastError());
		}
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

	// instantiate the admin UI (cookie-session auth, HTML via KWebObjects)
	// and register its routes under /Configure/.. — see ktunnel_admin.cpp.
	// We pass a pointer to this ExposedServer so the UI can look up live
	// tunnels, persisted users/config, and share the bcrypt verifier.
	m_AdminUI = std::make_unique<AdminUI>(*this);
	m_AdminUI->RegisterRoutes(Routes);

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

	// set a default route
	Routes.SetDefaultRoute([](KRESTServer& HTTP)
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
		else
		{
			HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/Configure/");
		}
	});

	// create the REST server instance
	KREST Http;

	// and run it
	m_Config.Message("starting {} server on port {}",
	                 m_Config.bNoTLS ? "TCP" : "TLS REST",
	                 Settings.iPort);

	if (!Http.Execute(Settings, Routes)) throw KError(Http.Error());

	// Legacy raw-forward server — only when the operator explicitly
	// asked for it via -f/-t. It binds a single port and pipes every
	// incoming connection to m_Config.DefaultTarget through the active
	// peer tunnel. Per-row listeners from the tunnels-table (see
	// ReconcileListeners below) run alongside it but must not collide
	// on ports.
	if (m_Config.iRawPort)
	{
		m_ExposedRawServer = std::make_unique<ExposedRawServer>
		(
			this,
			m_Config.DefaultTarget,
			m_Config.iRawPort,
			false,
			m_Config.iMaxTunneledConnections
		);

		m_Config.Message("listening on port {} for forward data connections (builtin, target {})",
		                 m_Config.iRawPort, m_Config.DefaultTarget);

		// register shutdown signals for the raw server as well - otherwise only
		// the KREST server would be stopped on SIGINT/SIGTERM and the blocking
		// Start() below would hang until a second signal is received. Because
		// KTCPServer::RegisterShutdownWithSignals now chains previously installed
		// user handlers, a single signal stops both servers cleanly.
		m_ExposedRawServer->RegisterShutdownWithSignals({ SIGINT, SIGTERM });
	}

	// Start per-tunnel listeners configured in the store. Each enabled
	// tunnels-row becomes its own non-blocking KTCPServer. Binding
	// failures (e.g. port in use) are captured as per-row errors in the
	// registry and surfaced on the Tunnels admin page — they do not
	// prevent the process from coming up.
	ReconcileListeners("bootstrap");

	// Block the main thread until shutdown. Two cases:
	//   - legacy raw-forward is up -> block on its Start(bBlock=true);
	//     SIGINT/SIGTERM tears it down and we fall through. The KREST
	//     server shuts down on its own via its signal handler, too.
	//   - no raw-forward -> wait on the KREST server instead (it owns
	//     the signal handler for SIGINT/SIGTERM via bBlocking=false +
	//     RegisterSignalsForShutdown default, and Wait() returns once
	//     that handler fires).
	if (m_ExposedRawServer)
	{
		m_ExposedRawServer->Start(m_Config.Timeout, /* bBlock= */ true);
	}
	else
	{
		Http.Wait();
	}

} // ExposedServer::ExposedServer


//-----------------------------------------------------------------------------
// handles one incoming connection
// - a raw tcp stream to forward via TLS to the protected host
void ExposedRawServer::Session (std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(*Stream, m_Target);
}

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
	// add help for service options
	Options.SetAdditionalHelp(KService::GetHelp());

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort         = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort      = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets   = Options("s,secret     : if exposed host, takes comma separated list of secrets for login by protected hosts, or one single secret if this is the protected host. Optional on the exposed host (then only users from the admin UI can authenticate peers); required on the protected host.", "").String();
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
	m_Config.sDatabasePath = Options("db <path>    : if exposed host, path to the SQLite admin/config DB - defaults to $HOME/.config/ktunnel/ktunnel.db", "");
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
		// -t / -f / -s are no longer required: a fresh install can be
		// configured entirely through the admin UI, and an installation
		// that wants to skip the UI entirely can omit all three and
		// drive tunnels through the tunnels-table only. We still
		// validate consistency where we do see CLI values.
		if (!m_Config.DefaultTarget.empty()
		    && m_Config.DefaultTarget.Port.get() == 0)
		{
			SetError("-t / -target needs an explicit port number");
		}

		// seed the admin-UI password from the first -secret value — this
		// bootstraps the admin password on the first launch, as described
		// in the setup docs. bcrypt is slow by design so we do this once
		// at startup instead of on every login attempt. Without any
		// -secret the store stays unseeded and the admin UI is
		// effectively unusable (no login) until a user is added out of
		// band; that is the intended "headless" mode.
		if (!m_Config.Secrets.empty())
		{
			KBCrypt BCrypt(std::chrono::milliseconds(100), /*bComputeAtFirstUse=*/false);
			m_Config.sAdminPassHash = BCrypt.GenerateHash(KStringViewZ(*m_Config.Secrets.begin()));

			if (m_Config.sAdminPassHash.empty())
			{
				SetError("failed to hash admin password");
			}
		}

		m_Config.Message("starting as exposed host with{} TLS", m_Config.bNoTLS ? "out" : "");
		m_Config.Message("admin UI available at {}://<host>:{}/Configure/",
		                 m_Config.bNoTLS ? "http" : "https", m_Config.iPort);

		ExposedServer Exposed(m_Config);
	}
	else
	{
		m_Config.Message("starting as protected host, connecting exposed host with{} TLS at {}",
		                 m_Config.bNoTLS ? "out" : "",
		                 m_Config.ExposedHost);

		ProtectedHost Protected(m_Config);

		// On POSIX we deliberately do NOT install SIGINT / SIGTERM handlers
		// here: the dekaf2 KSignals default handler (installed by
		// KInit(true)) is std::exit(EXIT_SUCCESS), which terminates the
		// process immediately — exactly what an interactive Ctrl+C is
		// expected to do, and significantly faster than routing through
		// ProtectedHost::Shutdown() + KTunnel::Stop(), because closing the
		// tunnel socket from another thread does not reliably unblock the
		// poll() inside KTunnel::ReadMessage() (observed both on Linux and
		// macOS), so the read-loop would otherwise sit out its full
		// ControlPing + ConnectTimeout before noticing the shutdown.
		//
		// On Windows the Service Control Manager does not deliver SIGINT /
		// SIGTERM; it invokes the handler registered via
		// KService::SetShutdownHandler. There we DO need the graceful path.
#ifdef DEKAF2_IS_WINDOWS
		KService::SetShutdownHandler([&Protected]()
		{
			Protected.Shutdown();
		});
#endif

		Protected.Run();

#ifdef DEKAF2_IS_WINDOWS
		// Detach the handler before Protected goes out of scope so a late
		// SCM stop event cannot dereference a dangling reference.
		KService::SetShutdownHandler({});
#endif
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// KService::Run() handles service-management flags (-install / -uninstall /
	// -start / -stop / -status / --help) before fnMain runs, and engages the
	// SCM / systemd / launchd integration when the process was launched by a
	// service manager. Interactive launches fall through to the lambda.
	return KService::Run("ktunnel", argc, argv,
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
