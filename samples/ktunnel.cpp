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
#include <dekaf2/core/init/dekaf2.h> // KInit() + Dekaf::getInstance().Signals()
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/system/os/kservice.h>
#include <dekaf2/system/os/ksignals.h> // SIGINT/SIGTERM chain in ExposedServer ctor
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
	#include <sys/stat.h>    // ::chmod for the persisted identity-key file
#endif

using namespace dekaf2;

// ==========================================================================
// AES handshake helpers (Ed25519 server identity, X25519 ECDH, HKDF-SHA256)
// ==========================================================================
//
// These live in an early anonymous namespace because both ExposedServer's
// ctor (loading the server identity) and ProtectedHost::Run (installing the
// trust callback) need them. The bootstrap helpers in the late anonymous
// namespace at the bottom of this file (RunPrintFingerprint etc.) also use
// them. C++ lookup permits the late helpers to call the early ones; not the
// other way round.

namespace {

#if DEKAF2_HAS_ED25519

//-----------------------------------------------------------------------------
/// Resolve the default path for the Ed25519 server-identity PEM file.
/// Empty result means "AdHoc without -persist — generate ephemerally and
/// keep in memory only".
///
/// Otherwise the path lives next to the SQLite admin DB so a single backup
/// of $HOME/.config/ktunnel/ (or /var/lib/ktunnel/) captures both. AdHoc
/// installs with -persist that have no DB fall back to the user config dir,
/// which is where the DB *would* be — keeps the layout symmetric.
KString DefaultIdentityKeyPath(KStringView sDatabasePath, bool bAdHoc, bool bPersist)
//-----------------------------------------------------------------------------
{
	if (bAdHoc && !bPersist)
	{
		return KString{};   // ephemeral identity
	}

	if (!sDatabasePath.empty())
	{
		KString sDir{kDirname(sDatabasePath)};
		return kFormat("{}{}ktunnel_ed25519.pem", sDir, kDirSep);
	}

	return kFormat("{}{}ktunnel_ed25519.pem", kGetConfigPath(true), kDirSep);

} // DefaultIdentityKeyPath

//-----------------------------------------------------------------------------
/// Load an Ed25519 private key from @p sPath, or generate a fresh one if
/// the file does not exist and @p bGenerateIfMissing is true. When a key is
/// freshly generated AND @p sPath is non-empty, write it back as PEM with
/// mode 0600 so it survives across restarts. Empty @p sPath plus
/// @p bGenerateIfMissing means "give me an ephemeral key".
///
/// Returns null on any failure (parse error, write error, generation error).
std::shared_ptr<KEd25519Key> LoadOrGenerateIdentity(KStringViewZ sPath, bool bGenerateIfMissing)
//-----------------------------------------------------------------------------
{
	auto Key = std::make_shared<KEd25519Key>();

	if (!sPath.empty() && kFileExists(sPath))
	{
		KString sPEM;
		if (!kReadAll(sPath, sPEM))
		{
			KErr.FormatLine(">> ktunnel: cannot read identity key '{}'", sPath);
			return nullptr;
		}
		if (!Key->CreateFromPEM(sPEM))
		{
			KErr.FormatLine(">> ktunnel: cannot parse identity key '{}': {}",
			                sPath, Key->Error());
			return nullptr;
		}
		if (!Key->IsPrivateKey())
		{
			KErr.FormatLine(">> ktunnel: identity key at '{}' is a public key only "
			                "(need a private key to sign handshake frames)", sPath);
			return nullptr;
		}
		return Key;
	}

	if (!bGenerateIfMissing)
	{
		// Caller asked us to load only — missing file is not an error here,
		// just signal it by returning null. The caller (e.g. -fingerprint)
		// prints its own context-specific message.
		return nullptr;
	}

	if (!Key->Generate())
	{
		KErr.FormatLine(">> ktunnel: cannot generate Ed25519 identity: {}", Key->Error());
		return nullptr;
	}

	if (sPath.empty())
	{
		return Key;   // ephemeral, do not persist
	}

	KString sDir{kDirname(sPath)};
	if (!sDir.empty() && !kDirExists(sDir))
	{
		kCreateDir(sDir, DEKAF2_MODE_CREATE_CONFIG_DIR, true);
	}

	auto sPEM = Key->GetPEM(true);
	if (sPEM.empty() || !kWriteFile(sPath, sPEM))
	{
		KErr.FormatLine(">> ktunnel: cannot write identity key to '{}'", sPath);
		return nullptr;
	}

#if !defined(DEKAF2_IS_WINDOWS)
	// Lock down to user-only. kCreateDir already gave the parent dir 0700,
	// but kWriteFile uses the umask for the file itself — force 0600 here
	// so the private key cannot leak via a wide umask.
	::chmod(sPath.c_str(), 0600);
#endif

	return Key;

} // LoadOrGenerateIdentity

//-----------------------------------------------------------------------------
/// Trust-store backing the protected host's `-trust-on-first-use` flow.
/// Tiny line-oriented file at $HOME/.config/ktunnel/known_servers, format
///   `<host>:<port> ed25519 <base64-of-32-byte-pubkey>`
/// per line. Comments (`#`) and blank lines are tolerated. Lookup keys are
/// the host:port string of the exposed peer (matching
/// KTCPEndPoint::Serialize() output).
class KnownServers
{
public:

	explicit KnownServers(KString sPath) : m_sPath(std::move(sPath))
	{
		Load();
	}

	/// Returns the raw 32-byte public key associated with sHostPort, or
	/// an empty KString if no entry is known.
	DEKAF2_NODISCARD
	KString Lookup(KStringView sHostPort) const
	{
		auto it = m_Entries.find(sHostPort);
		return (it != m_Entries.end()) ? it->second : KString{};
	}

	/// Append (host, raw-pubkey) to the file. Creates the parent dir if
	/// missing. Returns false on any IO error.
	bool Add(KStringView sHostPort, KStringView sRawPubKey)
	{
		if (m_sPath.empty()) return false;

		KString sDir{kDirname(m_sPath)};
		if (!sDir.empty() && !kDirExists(sDir))
		{
			kCreateDir(sDir, DEKAF2_MODE_CREATE_CONFIG_DIR, true);
		}

		KString sContent;
		kReadAll(m_sPath, sContent);   // ok if file is missing
		if (!sContent.empty() && sContent.back() != '\n')
		{
			sContent.push_back('\n');
		}
		sContent += kFormat("{} ed25519 {}\n",
		                    sHostPort, KEncode::Base64(sRawPubKey));

		if (!kWriteFile(m_sPath, sContent)) return false;

		m_Entries[sHostPort] = sRawPubKey;
		return true;
	}

private:

	void Load()
	{
		if (m_sPath.empty() || !kFileExists(m_sPath)) return;

		KString sContent;
		if (!kReadAll(m_sPath, sContent)) return;

		for (auto sLine : sContent.Split('\n'))
		{
			while (!sLine.empty() && (sLine.front() == ' ' || sLine.front() == '\t'))
			{
				sLine.remove_prefix(1);
			}

			if (sLine.empty() || sLine.front() == '#') continue;

			auto vTokens = sLine.Split(' ');

			if (vTokens.size() < 3) continue;
			if (vTokens[1] != "ed25519") continue;

			auto sRaw = KDecode::Base64(vTokens[2]);
			if (sRaw.size() != 32) continue;

			m_Entries[vTokens[0]] = std::move(sRaw);
		}
	}

	KString                                m_sPath;
	KUnorderedMap<KString, KString>        m_Entries;

}; // KnownServers

//-----------------------------------------------------------------------------
/// Read a yes/no answer from stdin, with prompt. Defaults to false on any
/// unparseable input. Refuses to prompt at all if stdin is not a TTY (so a
/// systemd / launchd / cron context cannot silently accept a forged identity
/// just because the operator forgot -trust-fingerprint).
bool PromptYesNo(KStringView sPrompt)
//-----------------------------------------------------------------------------
{
#if !defined(DEKAF2_IS_WINDOWS)
	if (!::isatty(STDIN_FILENO)) return false;
#endif
	KOut.Write(sPrompt).Flush();
	KString sLine;
	kReadLine(KIn, sLine);
	sLine.Trim();
	sLine.MakeLower();
	return (sLine == "y" || sLine == "yes");

} // PromptYesNo

//-----------------------------------------------------------------------------
/// Build the client-side TrustCallback for KTunnel::Config. Captures the
/// trust policy by value so it remains valid for the lifetime of every
/// per-connection KTunnel::Config copy made in ProtectedHost::Run.
///
/// Trust decision precedence (matches the table in the design doc):
///   1. Explicit -trust-fingerprint:  match → accept; mismatch → reject
///   2. known_servers entry:          match → accept; mismatch → reject loudly
///   3. -trust-on-first-use:          interactive prompt; on "yes" persist
///   4. otherwise:                    reject with a Hilfe-text on stderr
KTunnel::TrustChecker BuildClientTrustCallback(const ExtendedConfig& Config)
//-----------------------------------------------------------------------------
{
	KString sExplicitFingerprint = Config.sTrustFingerprint;
	bool    bTOFU                = Config.bTrustOnFirstUse;
	KString sKnownServersPath    = Config.sKnownServersPath.empty()
	                             ? kFormat("{}{}known_servers",
	                                       kGetConfigPath(true), kDirSep)
	                             : Config.sKnownServersPath;

	return [sExplicitFingerprint, bTOFU, sKnownServersPath]
	       (KStringView sHostPort,
	        KStringView sRawPubKey,
	        KStringView sFingerprint) -> bool
	{
		if (!sExplicitFingerprint.empty())
		{
			if (sExplicitFingerprint == sFingerprint) return true;
			KErr.FormatLine(">> ktunnel: server fingerprint MISMATCH for {}", sHostPort);
			KErr.FormatLine(">>   expected: {}", sExplicitFingerprint);
			KErr.FormatLine(">>   received: {}", sFingerprint);
			return false;
		}

		KnownServers Store(sKnownServersPath);
		auto sStored = Store.Lookup(sHostPort);

		if (!sStored.empty())
		{
			if (sStored == sRawPubKey) return true;

			KErr.FormatLine(">> ktunnel: server identity for {} HAS CHANGED", sHostPort);
			KErr.FormatLine(">>   stored fingerprint:   {}", KTunnel::FormatFingerprint(sStored));
			KErr.FormatLine(">>   received fingerprint: {}", sFingerprint);
			KErr.FormatLine(">>   refusing to connect — possible man-in-the-middle.");
			KErr.FormatLine(">>   if the server identity was rotated intentionally,");
			KErr.FormatLine(">>   edit '{}' to remove the", sKnownServersPath);
			KErr.FormatLine(">>   stale line and reconnect with -trust-on-first-use.");
			return false;
		}

		if (!bTOFU)
		{
			KErr.FormatLine(">> ktunnel: server {} is not in known_servers", sHostPort);
			KErr.FormatLine(">>   fingerprint: {}", sFingerprint);
			KErr.FormatLine(">>   to accept this server, run again with one of:");
			KErr.FormatLine(">>     -trust-fingerprint '{}'", sFingerprint);
			KErr.FormatLine(">>     -trust-on-first-use   (writes to {})", sKnownServersPath);
			return false;
		}

		KOut.FormatLine("");
		KOut.FormatLine("The authenticity of '{}' cannot be established.", sHostPort);
		KOut.FormatLine("Ed25519 fingerprint is {}.", sFingerprint);

		if (!PromptYesNo("Are you sure you want to continue connecting (yes/no)? "))
		{
			KErr.FormatLine(">> ktunnel: connection refused by user");
			return false;
		}

		if (!Store.Add(sHostPort, sRawPubKey))
		{
			KErr.FormatLine(">> ktunnel: warning, could not write '{}' — will prompt again",
			                sKnownServersPath);
		}
		else
		{
			KOut.FormatLine("Permanently added '{}' to {}", sHostPort, sKnownServersPath);
		}
		return true;
	};

} // BuildClientTrustCallback

#endif // DEKAF2_HAS_ED25519

} // anonymous namespace (v2 handshake helpers)

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
bool ExposedServer::VerifyNodeLogin (KStringView sNode, KStringView sSecret,
                                     const KTCPEndPoint& RemoteAddr)
//-----------------------------------------------------------------------------
{
	// AdHoc mode has no persistent `nodes` table, no bcrypt. The
	// operator supplies one or more shared secrets on the CLI via
	// `-s <comma-separated>`; any of them grants access. sNode is
	// accepted but not verified — it is only used for log lines.
	if (!m_Store)
	{
		if (m_Config.Secrets.empty())
		{
			kDebug(1, "ad-hoc: tunnel login from {} rejected: no secrets configured", RemoteAddr);
			return false;
		}
		const bool bOk = m_Config.Secrets.contains(sSecret);
		kDebug(1, "ad-hoc: tunnel login from {} node '{}' {}",
		       RemoteAddr, sNode, bOk ? "accepted" : "rejected");
		return bOk;
	}

	// Rejection-path events are logged even for unknown nodes, so that an
	// operator looking at the events table can spot brute-force attempts.
	auto LogFail = [&](KStringView sDetail)
	{
		KTunnelStore::Event ev;
		ev.sKind     = "node_login_fail";
		ev.sNode     = sNode;
		ev.sRemoteIP = RemoteAddr.Serialize();
		ev.sDetail   = sDetail;
		m_Store->LogEvent(ev);
	};

	if (sNode.empty())
	{
		LogFail("empty node");
		return false;
	}

	auto oNode = m_Store->GetNode(sNode);

	// Do a constant-time dummy compare against a fixed, well-formed bcrypt
	// hash so a missing row cannot be distinguished from a wrong password
	// by response time. The hash below is bcrypt("<never-used>", cost 4).
	static constexpr KStringViewZ s_sDummyHash =
		"$2a$04$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG";

	KString sPassword(sSecret);

	if (!oNode)
	{
		(void) m_BCrypt->ValidatePassword(sPassword, s_sDummyHash);
		LogFail("unknown node");
		return false;
	}

	if (!oNode->bEnabled)
	{
		(void) m_BCrypt->ValidatePassword(sPassword, s_sDummyHash);
		LogFail("node disabled");
		return false;
	}

	if (!m_BCrypt->ValidatePassword(sPassword, oNode->sPasswordHash))
	{
		LogFail("bad password");
		return false;
	}

	m_Store->SetNodeLastLogin(sNode, KUnixTime::now());

	KTunnelStore::Event ev;
	ev.sKind     = "node_login_ok";
	ev.sNode     = sNode;
	ev.sRemoteIP = RemoteAddr.Serialize();
	ev.sDetail   = "tunnel";
	m_Store->LogEvent(ev);

	return true;

} // VerifyNodeLogin

//-----------------------------------------------------------------------------
void ExposedServer::RegisterActiveTunnel (KStringView sNode,
                                          const KTCPEndPoint& RemoteAddr,
                                          std::shared_ptr<KTunnel> Tunnel)
//-----------------------------------------------------------------------------
{
	std::shared_ptr<KTunnel> Prev; // destroy outside the lock

	{
		auto Tunnels = m_ActiveTunnels.unique();

		auto it = Tunnels->find(sNode);
		if (it != Tunnels->end())
		{
			// Same node logged in twice: evict the old tunnel. Dropping
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
			at.sNode        = sNode;
			at.EndpointAddr = RemoteAddr;
			at.tConnected   = KUnixTime::now();
			at.Tunnel       = std::move(Tunnel);
			Tunnels->emplace(at.sNode, std::move(at));
		}
	}

	if (Prev)
	{
		kDebug(1, "evicting previous tunnel for node '{}'", sNode);
		Prev->Stop();
	}

} // RegisterActiveTunnel

//-----------------------------------------------------------------------------
void ExposedServer::UnregisterActiveTunnel (KStringView sNode,
                                            const std::shared_ptr<KTunnel>& Tunnel)
//-----------------------------------------------------------------------------
{
	auto Tunnels = m_ActiveTunnels.unique();

	auto it = Tunnels->find(sNode);
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
	// Stable "first by node-name" — used for the wildcard-owner path
	// (CLI-synthesised "cli" listener). DB-configured tunnels always
	// supply a concrete owner and take the GetTunnelForNode() branch
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
std::shared_ptr<KTunnel> ExposedServer::GetTunnelForNode (KStringView sNode) const
//-----------------------------------------------------------------------------
{
	auto Tunnels = m_ActiveTunnels.shared();
	auto it = Tunnels->find(sNode);
	return (it == Tunnels->end()) ? std::shared_ptr<KTunnel>{} : it->second.Tunnel;

} // GetTunnelForNode

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
	// Run() thread. To get the verified node name back onto this
	// (ControlStream) thread so we can register the tunnel in the
	// per-node map, we hand the outcome through a shared promise. An
	// atomic guard keeps set_value() from being called twice if the
	// tunnel ever retries the auth step.
	auto LoginPromise = std::make_shared<std::promise<KString>>();
	auto LoginFuture  = LoginPromise->get_future();
	auto LoginFired   = std::make_shared<std::atomic<bool>>(false);

#if DEKAF2_HAS_ED25519
	// Hand the loaded server identity to KTunnel so its WaitForLogin
	// can sign the v2 hello-ack with it. Null-pointer is fine when
	// bAESPayload is false — the core code only dereferences it inside
	// the v2 handshake path.
	TunnelCfg.ServerIdentity = m_ServerIdentity;
#endif

	TunnelCfg.AuthCallback = [this, LoginPromise, LoginFired, EndpointAddress]
	                         (KStringView sNode, KStringView sSecret) -> bool
	{
		const bool bOK = VerifyNodeLogin(sNode, sSecret, EndpointAddress);

		if (!LoginFired->exchange(true))
		{
			if (bOK) LoginPromise->set_value(sNode);
			else     LoginPromise->set_value({});
		}

		return bOK;
	};

	auto Tunnel = std::make_shared<KTunnel>(TunnelCfg, std::move(Stream));

	// Register the tunnel in m_ControlTunnels *before* we spawn the Run()
	// thread, so a SIGINT landing between std::make_shared and the spawn
	// still reaches this peer via Shutdown(). Removal on the way out goes
	// through a scope guard and also purges any stale expired weak_ptrs
	// that accumulated since the last ControlStream finished — cheap
	// opportunistic cleanup so the vector does not grow unbounded.
	{
		m_ControlTunnels.unique()->emplace_back(Tunnel);
	}
	KAtScopeEnd(
		auto Tunnels = m_ControlTunnels.unique();

		for (auto it = Tunnels->begin(); it != Tunnels->end(); )
		{
			auto Locked = it->lock();

			if (!Locked || Locked == Tunnel)
			{
				it = Tunnels->erase(it);
			}
			else
			{
				++it;
			}
		}
	);

	m_Config.Message("[control]: opened control stream from {}", EndpointAddress);

	// Run the tunnel on a dedicated thread so this method can observe the
	// login outcome and keep the per-user registry in sync.
	std::thread TunnelThread = kMakeThread([this, Tunnel, EndpointAddress]()
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

			// In Stateful mode also persist a "handshake_fail" event for
			// every v2-handshake exception, so the operator can see the
			// actual reason on the admin UI / via direct DB inspection
			// instead of having to crank up kDebug. We key off the
			// "v2 handshake:" prefix that KTunnel::SetupEncryption
			// consistently uses; "login rejected ..." is already logged
			// from VerifyNodeLogin as a "node_login_fail" event so it does
			// NOT need a duplicate entry here.
			if (m_Store)
			{
				KStringView sWhat = ex.what();
				if (sWhat.starts_with("v2 handshake:"))
				{
					KTunnelStore::Event ev;
					ev.sKind     = "handshake_fail";
					ev.sNode     = Tunnel->GetLoginNode();   // empty if hello frame never parsed
					ev.sRemoteIP = EndpointAddress.Serialize();
					ev.sDetail   = sWhat;
					m_Store->LogEvent(ev);
				}
			}
		}
	});

	// Wait up to ConnectTimeout for the auth callback to fire. If the
	// login never arrives (peer gone, bad frame, etc.) the tunnel's
	// Run() will have thrown by then and we fall through to join().
	KString sNode;
	auto tWait = std::chrono::milliseconds(m_Config.ConnectTimeout.milliseconds().count());

	if (LoginFuture.wait_for(tWait) == std::future_status::ready)
	{
		sNode = LoginFuture.get();
	}

	bool bRegistered = false;

	if (!sNode.empty())
	{
		RegisterActiveTunnel(sNode, EndpointAddress, Tunnel);
		bRegistered = true;
		m_Config.Message("[control]: tunnel '{}' active from {}", sNode, EndpointAddress);
	}
	else
	{
		m_Config.Message("[control]: login failed or timed out from {}", EndpointAddress);
	}

	TunnelThread.join();

	if (bRegistered)
	{
		UnregisterActiveTunnel(sNode, Tunnel);

		if (m_Store)
		{
			KTunnelStore::Event ev;
			ev.sKind     = "tunnel_disconnect";
			ev.sNode     = sNode;
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

	auto Tunnel = bWildcard ? PickDefaultTunnel() : GetTunnelForNode(sOwner);

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
			ev.sNode     = sOwner;
			ev.sRemoteIP = Downstream.GetEndPointAddress().Serialize();
			ev.sDetail   = bWildcard
				? kFormat("no tunnel currently connected — dropped raw conn from {}",
				          Downstream.GetEndPointAddress())
				: kFormat("owner node '{}' not currently connected — "
				          "dropped raw conn from {}",
				          sOwner, Downstream.GetEndPointAddress());
			m_Store->LogEvent(ev);
		}
		throw KError(bWildcard
			? KString("no tunnel established")
			: kFormat("no active tunnel for owner node '{}'", sOwner));
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
	KUnorderedSet<KString> LiveNodes;
	{
		auto Tunnels = m_ActiveTunnels.shared();
		LiveNodes.reserve(Tunnels->size());
		for (const auto& kv : *Tunnels) LiveNodes.insert(kv.first);
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
		else if (!LiveNodes.count(kv.second.Key.sOwnerNode))
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
			d.Key.sOwnerNode  = t.sNode;
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
		d.Key.sOwnerNode  = "*";
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
				ev.sAdmin      = sActor;
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
					t.Key.sOwnerNode,
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
			ev.sAdmin      = sActor;
			ev.sTunnelName = t.sName;
			ev.sDetail     = entry.sError.empty()
				? kFormat("listening on [::]:{} -> {}:{} (owner node {})",
				          t.Key.iListenPort,
				          t.Key.sTargetHost, t.Key.iTargetPort, t.Key.sOwnerNode)
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
void ExposedServer::Shutdown()
//-----------------------------------------------------------------------------
{
	// Shared lock is enough — we only read the vector and dereference
	// weak_ptrs; the erase pass in ControlStream's scope guard will run
	// once each session unwinds, which cannot happen before KTunnel::Stop()
	// returns.
	auto Tunnels = m_ControlTunnels.shared();

	for (const auto& Weak : *Tunnels)
	{
		if (auto Live = Weak.lock())
		{
			Live->Stop();
		}
	}

} // Shutdown

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
		// (VerifyNodeLogin) and the admin UI login. We prefer one
		// common instance so the asynchronously-computed workload is
		// calibrated once per process.
		m_BCrypt = std::make_unique<KBCrypt>(std::chrono::milliseconds(100), true);

		// Friendly hint when the operator forgot to seed an admin
		// user: the runtime keeps going so existing tunnels still
		// pass traffic, but the admin UI is unreachable until either
		// `ktunnel -set-admin` is run or a row is inserted out-of-
		// band.
		if (m_Store->CountAdmins() == 0)
		{
			m_Config.Message("warning: no admins in {} — run `ktunnel -set-admin` "
			                 "to create the admin login (admin UI is unreachable "
			                 "until then)",
			                 m_Store->GetDatabasePath());
		}
	}
	else
	{
		// AdHoc: no SQLite file, no bcrypt, no admin UI. Peer
		// authentication falls back to `-s <secret>` in
		// VerifyNodeLogin. The synthetic "cli" tunnel produced by
		// GatherDesiredTunnels() is the only listener.
		m_Config.Message("ad-hoc mode: no persistent store, no admin UI, "
		                 "peer authentication via -s secret(s)");
	}

#if DEKAF2_HAS_ED25519
	// Load (or generate, in AdHoc with -persist) the Ed25519 server
	// identity used by the v2 AES handshake. We do this unconditionally
	// so a Stateful service can be later switched to -aes by editing the
	// unit file without having to first run `ktunnel -fingerprint` to
	// bootstrap the key. The cost is one PEM read at start-up.
	//
	// AdHoc without -persist intentionally leaves sIdentityKeyPath
	// empty: LoadOrGenerateIdentity() generates an ephemeral key in
	// memory only and we print its fingerprint to stdout so the
	// operator can hand it to the protected peer.
	if (m_Config.bAESPayload)
	{
		m_ServerIdentity = LoadOrGenerateIdentity(m_Config.sIdentityKeyPath, /*bGenerateIfMissing=*/true);

		if (!m_ServerIdentity)
		{
			throw KError(kFormat("cannot load or generate Ed25519 identity (path '{}')",
			                     m_Config.sIdentityKeyPath));
		}

		auto sFingerprint = KTunnel::FormatFingerprint(m_ServerIdentity->GetPublicKeyRaw());

		if (m_Config.sIdentityKeyPath.empty())
		{
			m_Config.Message("server identity: ephemeral, fingerprint {}", sFingerprint);
		}
		else
		{
			m_Config.Message("server identity: {}, fingerprint {}",
			                 m_Config.sIdentityKeyPath, sFingerprint);
		}

		if (m_Config.bNoTLS)
		{
			m_Config.Message("warning: -aes -notls — the v2 handshake itself is "
			                 "visible on the wire (DH-pubkeys, signature). The "
			                 "AES-protected payload is still confidential, but "
			                 "the operator should treat this as debug-only.");
		}
	}
#endif

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

	KRESTRoutes Routes;

	Routes.AddRoute("/Version").Get([](KRESTServer& HTTP)
	{
		html::Page Page("Version", "en_US");
		Page.Body().AddText("ktunnel v1.0");
		HTTP.SetRawOutput(Page.Serialize());
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE , KMIME::HTML_UTF8);

	}).Parse(KRESTRoute::ParserType::NOREAD);

	// Instantiate the admin UI (cookie-session auth, HTML via
	// KWebObjects) and register its routes under /Configure/.. — see
	// ktunnel_admin.cpp. We pass a pointer to this ExposedServer so
	// the UI can look up live tunnels, persisted admins/nodes/tunnels,
	// and share the bcrypt verifier. In AdHoc mode there is no
	// persistent store to drive the UI off of, so we simply do not
	// register it.
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

	// Chain our own SIGINT/SIGTERM handler *in front of* KREST's, so the
	// first Ctrl+C (or `systemctl stop`) unblocks the live /Tunnel
	// WebSocket threads before KREST tries to drain its worker pool. Without
	// this chain Http.Wait() below hangs: KREST stops the acceptor, but its
	// per-session thread is still join()ing a TunnelThread that is itself
	// stuck in KTunnel::Run() → poll(). Our handler calls Shutdown() which
	// fans out KTunnel::Stop() to every live control-stream tunnel; the
	// captured previous handler (KREST's) then runs and tears down the
	// REST acceptor. Result: Http.Wait() returns within milliseconds and
	// main() unwinds cleanly on the first signal.
	//
	// We deliberately do NOT take the KService::SetShutdownHandler route
	// that ProtectedHost uses: that one only hooks SIGTERM, but the user-
	// visible bug is interactive Ctrl+C (SIGINT).
	auto Signals = Dekaf::getInstance().Signals();

	if (Signals)
	{
		auto PrevSIGINT  = Signals->GetSignalHandler(SIGINT);
		auto PrevSIGTERM = Signals->GetSignalHandler(SIGTERM);

		Signals->SetSignalHandler(SIGINT,  [this, PrevSIGINT ](int iSig)
		{
			Shutdown();
			if (PrevSIGINT)
			{
				PrevSIGINT (iSig);
			}
		});

		Signals->SetSignalHandler(SIGTERM, [this, PrevSIGTERM](int iSig)
		{
			Shutdown();
			if (PrevSIGTERM)
			{
				PrevSIGTERM(iSig);
			}
		});
	}
	else
	{
		kWarning("ExposedServer: no central signal-handler thread (KInit(true) "
		         "missing?) — /Tunnel sessions will not be stopped on signal, "
		         "Ctrl+C may need to be pressed twice");
	}

	// Restore the default handler before this stack frame unwinds, so a
	// late signal that arrives after Http.Wait() returns cannot dereference
	// `this` through the chained lambda.
	KAtScopeEnd(
		if (auto S = Dekaf::getInstance().Signals())
		{
			S->SetDefaultHandler(SIGINT);
			S->SetDefaultHandler(SIGTERM);
		}
	);

	// Block the main thread until shutdown. KREST owns the downstream
	// signal handler for SIGINT/SIGTERM (via bBlocking=false +
	// RegisterSignalsForShutdown default), and Wait() returns once that
	// handler fires. Our chained handler above runs first and stops every
	// live control-stream tunnel, so Http.Wait() does not get stuck in a
	// worker-pool drain. The TunnelListener instances are torn down by the
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
						"peer node    : {}\n"
						"streams      : {}\n"
						"bytes rx     : {}\n"
						"bytes tx     : {}\n",
						m_pCurrentTunnel->GetEndPointAddress(),
						m_Config.sPeerNode,
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
			// Peer login identity: the `-n <name>` CLI flag. The default
			// ("node") almost certainly needs to be overridden — the admin
			// must have seeded a matching row in the exposed host's
			// `nodes` table via `ktunnel -add-node` or the admin UI.
			KString sNodeName = m_Config.sPeerNode;
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
			// ExtendedConfig) so we can inject the REPL callback and
			// trust callback without mutating the shared m_Config.
			KTunnel::Config TunnelCfg = m_Config;

			TunnelCfg.OpenReplCallback =
				[this](std::shared_ptr<KTunnel::Connection> conn)
				{
					RunRepl(std::move(conn));
				};

#if DEKAF2_HAS_ED25519
			// In AES mode the v2 handshake fans the trust decision out
			// to the embedder so it can consult known_servers, the
			// -trust-fingerprint flag, or prompt interactively. We
			// install the callback unconditionally — it is harmless
			// when bAESPayload is false because the core code only
			// invokes it during the v2 handshake.
			TunnelCfg.TrustCallback = BuildClientTrustCallback(m_Config);
#endif

			// we are the "client" side of the tunnel and have to login with node/secret
			KTunnel Tunnel(TunnelCfg, std::move(WebSocket.GetStream()), sNodeName, sSecret);

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
		"  -set-admin                  : create or rotate the admin login in the\n"
		"                                config DB, then exit. Use this before\n"
		"                                first interactive launch, or any time\n"
		"                                you need to rotate the password.\n"
		"  -admin-user <name>          : username for -set-admin (target row to\n"
		"                                create / rotate; default: admin) and for\n"
		"                                -install on a fresh DB (skips the\n"
		"                                interactive name prompt). When -install\n"
		"                                runs against a DB that already has an\n"
		"                                admin row this flag is ignored — use\n"
		"                                -set-admin to add or rotate admins.\n"
		"                                Not persisted into the generated unit /\n"
		"                                plist / SCM record.\n"
		"  -add-node                   : create or rotate a tunnel-endpoint\n"
		"                                (`nodes` table) login in the config DB,\n"
		"                                then exit. Combine with -node-name\n"
		"                                <name> and -pass-file <path> to seed a\n"
		"                                node non-interactively.\n"
		"  -node-name <name>           : node name for -add-node. Required when\n"
		"                                -add-node is given. Not persisted into\n"
		"                                the generated unit / plist / SCM record.\n"
		"  -pass-file <path>           : read the admin (or node) password from\n"
		"                                <path> instead of prompting on the TTY.\n"
		"                                Used for non-interactive provisioning.\n"
		"                                The file is read once and not\n"
		"                                referenced by the running service.\n"
		"  -fingerprint                : print the SHA-256 fingerprint of the\n"
		"                                exposed-host Ed25519 identity (the\n"
		"                                public key the server signs the v2\n"
		"                                AES handshake with). Use this to read\n"
		"                                back what to put on the protected\n"
		"                                side as -trust-fingerprint. Reads the\n"
		"                                identity from -identity-key <path> or\n"
		"                                from the default location next to the\n"
		"                                admin DB. Then exits.\n",
		KService::GetHelp()));

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort         = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort      = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets   = Options("s,secret     : on the protected host: REQUIRED, the password used to log in to the exposed host (must match the bcrypt-hashed password of the matching row in the exposed host's `nodes` table). On the exposed host: only used in ad-hoc mode (with -f / -t), where it is the comma-separated list of pre-shared secrets for peer authentication; in stateful mode it is ignored — manage the admin password via `ktunnel -set-admin` and node accounts via the admin UI.", "").String();
	m_Config.DefaultTarget = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunneledConnections
	                       = Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.sCertFile     = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused a self-signed cert is created", "");
	m_Config.sKeyFile      = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused a new key is created", "");
	m_Config.sTLSPassword  = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_Config.bNoTLS        = Options("notls        : do not use TLS, but unencrypted HTTP", false);
	m_Config.bAESPayload   = Options("aes          : encrypt payload with AES on top of the websocket. Engages the v2 X25519+Ed25519+HKDF handshake — the server signs every handshake with its long-term identity key, and the client checks the fingerprint against -trust-fingerprint or known_servers (or, with -trust-on-first-use, prompts interactively). Provides forward secrecy and protects against an active TLS-intercepting middlebox even if the corporate CA is in the local trust store.", false);
	m_Config.Timeout       = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_Config.bPersistCert  = Options("persist      : should a self-signed cert (and, in ad-hoc mode, the Ed25519 server identity used for -aes) be persisted to disk and reused at next start? Without this flag the AdHoc identity is regenerated at every start, forcing protected-side peers to re-confirm the new fingerprint each time.", false);
	m_Config.sCipherSuites = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_Config.bQuiet        = Options("q,quiet      : do not output status to stdout", false);
	m_Config.sDatabasePath = Options("db <path>    : if exposed host (stateful mode), path to the SQLite admin/config DB. Defaults to /var/lib/ktunnel/ktunnel.db when running as root (or as a system service), $HOME/.config/ktunnel/ktunnel.db otherwise. Not used in ad-hoc mode (with -f / -t).", "");
	m_Config.sPeerNode     = Options("n,node <name> : if protected host, node name to log in with (must exist as an enabled row in the exposed host's `nodes` table) - defaults to 'node'", "node");
	m_Config.sIdentityKeyPath
	                       = Options("identity-key <path> : if exposed host with -aes, path to the Ed25519 server-identity PEM file. Defaults to ktunnel_ed25519.pem next to the admin DB (Stateful) or to $HOME/.config/ktunnel/ktunnel_ed25519.pem (AdHoc with -persist). In AdHoc mode without -persist this is ignored and an ephemeral key is generated at start-up. Auto-created by `-install`.", "");
	m_Config.sTrustFingerprint
	                       = Options("trust-fingerprint <hex> : if protected host with -aes, accept exactly this server fingerprint (lowercase hex with colons, as printed by `ktunnel -fingerprint`). One-shot, not persisted; takes precedence over known_servers.", "");
	m_Config.bTrustOnFirstUse
	                       = Options("trust-on-first-use : if protected host with -aes and the server's identity is not yet in known_servers, prompt the operator interactively (TTY required) to accept the presented fingerprint and persist it for next time. Without this flag an unknown server is rejected outright.", false);
	m_Config.sKnownServersPath
	                       = Options("known-servers <path> : if protected host with -aes, override the path to the trust store (default: $HOME/.config/ktunnel/known_servers).", "");

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
				         "the `nodes` table in the admin DB is "
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

#if DEKAF2_HAS_ED25519
		// Resolve the default identity-key path the same way (Stateful
		// puts it next to the DB; AdHoc with -persist puts it in the
		// user config dir; AdHoc without -persist leaves it empty so
		// ExposedServer's ctor generates an ephemeral key in RAM).
		// An explicit `-identity-key <path>` always wins.
		if (m_Config.bAESPayload && m_Config.sIdentityKeyPath.empty())
		{
			m_Config.sIdentityKeyPath = DefaultIdentityKeyPath(
				m_Config.sDatabasePath,
				m_Config.eMode == ExtendedConfig::Mode::AdHoc,
				m_Config.bPersistCert);
		}
#endif

		// In interactive Stateful mode, -s no longer does anything (the
		// `nodes` table is the only authority for tunnel logins, the
		// `admins` table for the UI). We accept it without error but log
		// a warning so an operator who pasted the old CLI sees what
		// changed and how to migrate.
		if (m_Config.eMode == ExtendedConfig::Mode::Stateful
		    && !m_Config.Secrets.empty())
		{
			m_Config.Message("note: -s is ignored in stateful mode; "
			                 "use `ktunnel -set-admin` to set the admin "
			                 "password and `ktunnel -add-node` (or the "
			                 "admin UI) for tunnel-endpoint accounts");
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
// helpers — admin-bootstrap subcommands (`-install`, `-set-admin`, `-add-node`)
// =============================================================================
//
// These run BEFORE KService::Run() in main(): they take a quick look at argv,
// branch on whichever of our own subcommands is present, and either short-
// circuit (`-set-admin` / `-add-node`: do the DB upsert and exit) or do their
// pre-work and then fall through to KService::Run (`-install`: prompt for
// password and seed the DB before letting KService register the service unit).
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
/// in Tunnel::Main (UID 0 → system path, else user path).
///
/// @p bForceSystem is an escape hatch for callers that know a priori
/// the admin DB must live under the machine-wide path (e.g. a future
/// OS-integration flow that runs under root after an explicit elevation
/// step). No current caller sets it; both -set-admin and -install leave
/// the UID-0 check do the work, which keeps non-root `ktunnel -install`
/// working as a user-scope install (DB under $HOME/.config/ktunnel/).
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

	auto oExisting = Store.GetAdmin(sUser);

	if (oExisting)
	{
		// Rotate the password.
		if (!Store.UpdateAdminPasswordHash(sUser, sHash))
		{
			KErr.FormatLine(">> ktunnel: cannot update admin '{}': {}",
			                sUser, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("admin '{}' updated in {}",
		                sUser, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind   = "admin_password_change";
		ev.sAdmin  = sUser;
		ev.sDetail = "password rotated via -set-admin / -install";
		Store.LogEvent(ev);
	}
	else
	{
		KTunnelStore::Admin a;
		a.sUsername     = sUser;
		a.sPasswordHash = sHash;
		if (!Store.AddAdmin(a))
		{
			KErr.FormatLine(">> ktunnel: cannot create admin '{}': {}",
			                sUser, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("admin '{}' created in {}",
		                sUser, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind   = "bootstrap";
		ev.sAdmin  = a.sUsername;
		ev.sDetail = "admin seeded via -set-admin / -install";
		Store.LogEvent(ev);
	}

	return true;

} // SeedAdminUser

//-----------------------------------------------------------------------------
/// Open the store at @p sPath, hash @p sPassword, upsert (insert or update)
/// the node row at @p sName. Returns true on success and emits success /
/// failure messages to stdout / stderr. New nodes are created with the
/// `enabled` flag set; existing rows keep their `enabled` state untouched.
bool SeedNode (KStringView  sPath,
               KStringView  sName,
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
		KErr.FormatLine(">> ktunnel: failed to hash node password");
		return false;
	}

	auto oExisting = Store.GetNode(sName);

	if (oExisting)
	{
		if (!Store.UpdateNodePasswordHash(sName, sHash))
		{
			KErr.FormatLine(">> ktunnel: cannot update node '{}': {}",
			                sName, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("node '{}' updated in {}",
		                sName, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind   = "node_password_change";
		ev.sNode   = sName;
		ev.sDetail = "password rotated via -add-node";
		Store.LogEvent(ev);
	}
	else
	{
		KTunnelStore::Node n;
		n.sName         = sName;
		n.sPasswordHash = sHash;
		n.bEnabled      = true;
		if (!Store.AddNode(n))
		{
			KErr.FormatLine(">> ktunnel: cannot create node '{}': {}",
			                sName, Store.GetLastError());
			return false;
		}
		KOut.FormatLine("node '{}' created in {}",
		                sName, Store.GetDatabasePath());

		KTunnelStore::Event ev;
		ev.sKind   = "node_add";
		ev.sNode   = n.sName;
		ev.sDetail = "node seeded via -add-node";
		Store.LogEvent(ev);
	}

	return true;

} // SeedNode

//-----------------------------------------------------------------------------
/// Interactive admin-name capture for first-install bootstrap. Echoes a
/// `[default]` hint and accepts an empty Enter as "use the default". The
/// returned name has been Trim()ed; an empty return means stdin is not a
/// TTY (non-interactive callers must supply `-admin-user <name>` instead
/// — `BootstrapAdminForInstall` checks for that flag first and only calls
/// here when no flag was given).
KString PromptForAdminName (KStringView sDefault = "admin")
//-----------------------------------------------------------------------------
{
#if !defined(DEKAF2_IS_WINDOWS)
	if (!::isatty(STDIN_FILENO))
	{
		KErr.FormatLine(">> ktunnel: stdin is not a TTY; supply -admin-user <name> "
		                "for non-interactive installs");
		return {};
	}
#endif

	KOut.Write(kFormat("Admin user name [{}]: ", sDefault)).Flush();

	KString sLine;
	kReadLine(KIn, sLine);
	sLine.Trim();

	return sLine.empty() ? KString(sDefault) : sLine;

} // PromptForAdminName

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
		auto sFirst  = kPromptForPassword("Set password:    ");
		if (sFirst.empty())
		{
			KErr.FormatLine(">> ktunnel: empty password not allowed");
			continue;
		}
		auto sSecond = kPromptForPassword("Confirm password: ");
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
	// We deliberately do NOT use `-n` here — that flag belongs to the
	// runtime tunnel (`Tunnel::Main`: peer node name on the protected
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
/// Handle `-add-node`: standalone subcommand that creates or rotates a
/// tunnel-endpoint (node) login in the configuration DB and exits. The
/// node name is taken from `-node-name <name>` (required). Intended for
/// non-interactive provisioning of new tunnel endpoints — the admin UI
/// covers the interactive case.
int RunAddNode (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto sName = GetBootstrapFlagValue(argc, argv, "node-name");
	if (sName.empty())
	{
		KErr.FormatLine(">> ktunnel: -add-node requires -node-name <name>");
		return 1;
	}

	const auto sDBPath = ResolveBootstrapDBPath(argc, argv, /*bForceSystem=*/false);

	KString sPassword;

	if (!CapturePassword(argc, argv, sPassword))
	{
		return 1;
	}

	return SeedNode(sDBPath, sName, sPassword) ? 0 : 1;

} // RunAddNode

//-----------------------------------------------------------------------------
/// Handle the bootstrap step that runs immediately before the actual
/// service registration. Same rules as RunSetAdmin, except on success we
/// DO NOT exit; main() falls through to KService::Run which then writes
/// the unit / plist / SCM record.
///
/// Scope of the admin DB follows ResolveBootstrapDBPath's normal rule:
///   * sudo ktunnel -install (UID 0) → /var/lib/ktunnel/ktunnel.db,
///     paired with the system-level service that KService::Install
///     registers under the same UID.
///   * ktunnel -install as a normal user → $HOME/.config/ktunnel/ktunnel.db,
///     paired with a user-scope service (macOS LaunchAgent under
///     ~/Library/LaunchAgents, user-systemd unit, etc.) that
///     KService::Install writes in the same scope.
///   * explicit -db <path> always wins, regardless of UID.
///
/// We used to hard-force the system path here so that a sudo-install
/// would not accidentally land the DB in /root/.config/, but that
/// case is already covered by the UID-0 branch inside
/// ResolveBootstrapDBPath and forcing system-scope broke the common
/// non-root install path (EACCES on /var/lib/ktunnel/).
bool BootstrapAdminForInstall (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	const auto sDBPath = ResolveBootstrapDBPath(argc, argv, /*bForceSystem=*/false);

	KOut.FormatLine("ktunnel: pre-install admin bootstrap → {}", sDBPath);

	// Idempotency: only seed the very first admin row. If the admins
	// table is already populated, leave it alone — `-install` is a
	// service-lifecycle command (write the unit, register with the
	// service manager) and re-running it should not silently rotate
	// the admin password. Rotation / additional admins go through the
	// explicit `-set-admin` subcommand or the admin UI.
	std::size_t iAdminCount = 0;
	{
		KTunnelStore Store(sDBPath);
		if (Store.HasError())
		{
			KErr.FormatLine(">> ktunnel: cannot open store '{}': {}",
			                Store.GetDatabasePath(), Store.GetLastError());
			return false;
		}
		iAdminCount = Store.CountAdmins();
	}

	if (iAdminCount == 0)
	{
		// First install on this DB → prompt for the admin name and
		// password, then seed the row. The CLI flag wins over the
		// prompt so non-interactive provisioners (cloud-init, Ansible
		// with `-pass-file`) can name the admin without a TTY.
		KString sUser = GetBootstrapFlagValue(argc, argv, "admin-user");
		if (sUser.empty())
		{
			sUser = PromptForAdminName(/*sDefault=*/"admin");
			if (sUser.empty()) return false;   // not a TTY; PromptForAdminName already explained
		}

		KString sPassword;
		if (!CapturePassword(argc, argv, sPassword))
		{
			return false;
		}

		if (!SeedAdminUser(sDBPath, sUser, sPassword))
		{
			return false;
		}
	}
	else
	{
		KOut.FormatLine("admins table already has {} entr{} — leaving it untouched.",
		                iAdminCount, iAdminCount == 1 ? "y" : "ies");
		KOut.FormatLine("(use `ktunnel -set-admin [-admin-user <name>]` to rotate or add an admin)");

		// Surface a hint when the operator passed flags whose only
		// purpose is admin seeding — without it, the silent skip would
		// look like a bug ("I gave -pass-file and nothing happened").
		if (   HasBootstrapFlag(argc, argv, "admin-user")
		    || HasBootstrapFlag(argc, argv, "pass-file"))
		{
			KOut.FormatLine("note: -admin-user / -pass-file ignored on -install when the admins table is non-empty.");
		}
	}

#if DEKAF2_HAS_ED25519
	// Bootstrap the Ed25519 server identity at the same time so the operator
	// has the fingerprint in hand (to paste into the protected hosts'
	// `-trust-fingerprint`) before the service even starts. If an explicit
	// `-identity-key <path>` was given on the command line, honor it;
	// otherwise pair the identity with the admin DB by deriving the path
	// from the DB path's directory.
	KString sIdentityPath = GetBootstrapFlagValue(argc, argv, "identity-key");

	if (sIdentityPath.empty())
	{
		sIdentityPath = DefaultIdentityKeyPath(sDBPath, /*bAdHoc=*/false, /*bPersist=*/true);
	}

	auto Identity = LoadOrGenerateIdentity(sIdentityPath, /*bGenerateIfMissing=*/true);

	if (!Identity)
	{
		KErr.FormatLine(">> ktunnel: cannot prepare server identity — install aborted");
		return false;
	}

	auto sFingerprint = KTunnel::FormatFingerprint(Identity->GetPublicKeyRaw());

	KOut.FormatLine("");
	KOut.FormatLine("server identity: {}", sIdentityPath);
	KOut.FormatLine("fingerprint:     {}", sFingerprint);
	KOut.FormatLine("");
	KOut.FormatLine("On every protected host, run with one of:");
	KOut.FormatLine("  ktunnel ... -aes -trust-fingerprint '{}'", sFingerprint);
	KOut.FormatLine("  ktunnel ... -aes -trust-on-first-use");
	KOut.FormatLine("");
#endif

	return true;

} // BootstrapAdminForInstall

#if DEKAF2_HAS_ED25519
//-----------------------------------------------------------------------------
/// Handle `-fingerprint`: read the Ed25519 server-identity PEM (resolved
/// the same way as the admin DB — explicit `-identity-key`, else next to
/// the resolved DB path), print its SHA-256 fingerprint to stdout, exit.
/// Does NOT generate a fresh key when the file is missing — use `-install`
/// for that. This keeps `-fingerprint` a side-effect-free read.
int RunPrintFingerprint (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	KString sIdentityPath = GetBootstrapFlagValue(argc, argv, "identity-key");

	if (sIdentityPath.empty())
	{
		const auto sDBPath = ResolveBootstrapDBPath(argc, argv, /*bForceSystem=*/false);
		sIdentityPath = DefaultIdentityKeyPath(sDBPath, /*bAdHoc=*/false, /*bPersist=*/true);
	}

	auto Identity = LoadOrGenerateIdentity(sIdentityPath, /*bGenerateIfMissing=*/false);

	if (!Identity)
	{
		KErr.FormatLine(">> ktunnel: no identity at '{}'", sIdentityPath);
		KErr.FormatLine(">>   run `ktunnel -install` (or copy a PEM there) to create one");
		return 1;
	}

	auto sFingerprint = KTunnel::FormatFingerprint(Identity->GetPublicKeyRaw());

	KOut.FormatLine("{}", sFingerprint);
	KErr.FormatLine("# server identity: {}", sIdentityPath);

	return 0;

} // RunPrintFingerprint
#endif

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
		KStringView("node-name"),
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

	if (HasBootstrapFlag(argc, argv, "add-node"))
	{
		return RunAddNode(argc, argv);
	}

#if DEKAF2_HAS_ED25519
	if (HasBootstrapFlag(argc, argv, "fingerprint"))
	{
		return RunPrintFingerprint(argc, argv);
	}
#endif

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
