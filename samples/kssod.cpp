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

// kssod.cpp
//
// kssod - a minimal, persistent OpenID Connect *provider* (SSO server) built on
// dekaf2's KOpenIDServer. It issues id/access/refresh tokens via the
// authorization-code + PKCE flow and signs in any standards-compliant relying
// party (e.g. an app written with KOpenIDClient, see koidc_bff.cpp).
//
// Persistence (all in one SQLite file, --db):
//   * users    : KSSOdUserStore   (table kssod_users)
//   * clients  : KSSOdClientStore (table kssod_clients)
//   * sessions : KSessionSQLiteStore behind a KSessionCachingStore
//                (the OP login sessions that make SSO work)
//   * signing  : the RS256 private key is persisted as PEM (--key), so tokens
//                stay verifiable across restarts.
// (Authorization codes / refresh tokens are kept in memory - they are
//  short-lived by design and a restart simply invalidates outstanding ones.)
//
// This file is the OpenID Connect / routing / authentication half of the sample.
// The HTML rendering lives in kssod_ui.{h,cpp} (built with the html:: KWebObjects
// and html::ui:: KWebUI C++ builders) - routes here call Render*() from there.
// Keeping the two apart lets each read as its own lesson: OIDC vs. KWebObjects.
//
// Try it:
//   kssod --http 8080
//   open http://localhost:8080/   then sign in as  admin / admin123  (admin)
//                                                or alice / alice123
//   discovery: http://localhost:8080/.well-known/openid-configuration
//
// Pre-seeded relying party:  demo-app / demo-secret
//   -> http://localhost:3000/auth/callback   (post-logout http://localhost:3000/)

#include "kssod_store.h"
#include "kssod_ui.h"  // the HTML rendering layer (this file stays OIDC-focused)

#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/crypto/auth/kopenidserver.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionsqlitestore.h>
#include <dekaf2/crypto/auth/bits/ksessioncachingstore.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/crypto/auth/ktotp.h>           // TOTP / HOTP / backup + email-OTP helpers
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>  // KSHA256 (pending-2FA token)
#include <dekaf2/crypto/random/krandom.h>       // kGetRandom (pending-2FA token)
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/web/url/kuseragent.h>      // KHTTPUserAgent: browser/OS/device for the session list
#include <dekaf2/util/mail/kmail.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kwords.h>        // KSimpleSpacedWords: split free-text fields at whitespace
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/logging/klog.h>
#include <map>
#include <memory>
#include <mutex>

using namespace dekaf2;

namespace {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//  in-memory grant store (codes + refresh tokens; transient by design)
class MemoryGrantStore : public KOpenIDServer::GrantStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	bool SaveCode(const Code& Rec) override
	{ std::lock_guard<std::mutex> L(m_Mutex); m_Codes[Rec.sCode] = Rec; return true; }

	bool TakeCode(KStringView sCode, Code& Out) override
	{
		std::lock_guard<std::mutex> L(m_Mutex);
		auto it = m_Codes.find(sCode);
		if (it == m_Codes.end()) return false;
		Out = it->second; m_Codes.erase(it); return true;
	}
	bool SaveRefresh(const Refresh& Rec) override
	{ std::lock_guard<std::mutex> L(m_Mutex); m_Refresh[Rec.sToken] = Rec; return true; }

	bool TakeRefresh(KStringView sToken, Refresh& Out) override
	{
		std::lock_guard<std::mutex> L(m_Mutex);
		auto it = m_Refresh.find(sToken);
		if (it == m_Refresh.end()) return false;
		Out = it->second; m_Refresh.erase(it); return true;
	}
	void PurgeExpired(KUnixTime tNow) override
	{
		std::lock_guard<std::mutex> L(m_Mutex);
		for (auto it = m_Codes.begin();   it != m_Codes.end();   ) { it = (tNow >= it->second.tExpiry) ? m_Codes.erase(it)   : std::next(it); }
		for (auto it = m_Refresh.begin(); it != m_Refresh.end(); ) { it = (tNow >= it->second.tExpiry) ? m_Refresh.erase(it) : std::next(it); }
	}
private:
	std::mutex                 m_Mutex;
	std::map<KString, Code>    m_Codes;
	std::map<KString, Refresh> m_Refresh;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// In-memory store of half-finished logins: a user has passed the password step
/// but still owes a second factor. Keyed by an opaque token carried in the 2FA
/// form; entries expire so an abandoned prompt cannot be resumed indefinitely.
class PendingTwoFactorStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	/// what we learn back about a pending login (bValid=false if unknown/expired)
	struct Pending
	{
		KString sUsername;
		KString sMethod;        ///< "totp" or "email"
		KString sEmailCodeHash; ///< for "email": the hash of the code we mailed
		bool    bValid { false };
	};

	/// second factor is the authenticator app (verified against the stored secret)
	KString BeginTotp (KStringView sUsername)                       { return Insert(sUsername, "totp",  {}); }
	/// second factor is a one-time code we emailed; we keep only its hash
	KString BeginEmail(KStringView sUsername, KStringView sCodeHash) { return Insert(sUsername, "email", sCodeHash); }

	/// look up a still-valid token without consuming it (so a wrong code can be
	/// retried within the TTL)
	Pending Peek(KStringView sToken)
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		auto it = m_Pending.find(KString(sToken));
		if (it == m_Pending.end())                   return {};
		if (it->second.tExpires < KUnixTime::now()) { m_Pending.erase(it); return {}; }
		return Pending{ it->second.sUsername, it->second.sMethod, it->second.sEmailCodeHash, true };
	}

	/// replace the emailed code (used when the user asks to resend it)
	bool SetEmailCode(KStringView sToken, KStringView sCodeHash)
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		auto it = m_Pending.find(KString(sToken));
		if (it == m_Pending.end()) return false;
		it->second.sEmailCodeHash = KString(sCodeHash);
		return true;
	}

	void Consume(KStringView sToken)
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_Pending.erase(KString(sToken));
	}

private:
	struct Entry { KString sUsername; KString sMethod; KString sEmailCodeHash; KUnixTime tExpires; };

	KString Insert(KStringView sUsername, KStringView sMethod, KStringView sCodeHash)
	{
		KString sToken = KSHA256(kGetRandom(32)).HexDigest();
		std::lock_guard<std::mutex> Lock(m_Mutex);
		Sweep();
		m_Pending[sToken] = Entry{ KString(sUsername), KString(sMethod), KString(sCodeHash), KUnixTime::now() + m_TTL };
		return sToken;
	}

	void Sweep()
	{
		auto tNow = KUnixTime::now();
		for (auto it = m_Pending.begin(); it != m_Pending.end(); )
		{
			it = (it->second.tExpires < tNow) ? m_Pending.erase(it) : std::next(it);
		}
	}

	std::mutex                 m_Mutex;
	std::map<KString, Entry>   m_Pending;
	KDuration                  m_TTL { std::chrono::minutes(5) };
};

//-----------------------------------------------------------------------------
/// send a plaintext email via the configured relay; fills sError on failure.
/// The whole message is also logged at debug level so a demo install without a
/// real relay can still read the link/code from the server log.
bool SendMail(const KSSOdSettingsStore::Smtp& Smtp, KStringView sTo,
              KStringView sSubject, KStringView sBody, KString& sError)
//-----------------------------------------------------------------------------
{
	kDebug(1, "outgoing mail to <{}>: {}\n{}", sTo, sSubject, sBody);

	if (!Smtp.IsConfigured())
	{
		sError = "email is not configured";
		return false;
	}

	KMail Mail;
	Mail.From(Smtp.sFrom, Smtp.sFromName);
	Mail.To(KString(sTo));
	Mail.Subject(KString(sSubject));
	Mail.Message(KString(sBody));

	if (!Mail.Send(KURL(Smtp.sURL), Smtp.sUser, Smtp.sPass))
	{
		sError = Mail.Error();
		return false;
	}
	return true;

} // SendMail

//-----------------------------------------------------------------------------
/// the user behind the current OP login session, or empty if not signed in
KString CurrentUser(KRESTServer& HTTP, KSession& Session)
//-----------------------------------------------------------------------------
{
	KStringView sToken = HTTP.GetCookie(Session.GetCookieName());
	if (sToken.empty()) return {};
	KSession::Record Record;
	if (!Session.Validate(sToken, &Record)) return {};
	return Record.sUsername;
}

//-----------------------------------------------------------------------------
/// human label for a parsed device type (the yaml-free fast classifier)
KStringView DeviceTypeName(KHTTPUserAgent::DeviceType Type)
//-----------------------------------------------------------------------------
{
	switch (Type)
	{
		case KHTTPUserAgent::DeviceType::Desktop: return "Desktop";
		case KHTTPUserAgent::DeviceType::Mobile:  return "Mobile";
		case KHTTPUserAgent::DeviceType::Tablet:  return "Tablet";
		case KHTTPUserAgent::DeviceType::Unknown: break;
	}
	return {};
}

//-----------------------------------------------------------------------------
/// build the renderable session list for a user, marking the request's own
/// session and deriving a stable, non-secret revoke id from each token
std::vector<SessionView> SessionViewsFor(KRESTServer& HTTP, KSession& Session, KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::vector<SessionView> Out;
	KStringView sCurrentToken = HTTP.GetCookie(Session.GetCookieName());

	for (const auto& Rec : Session.ListSessionsFor(sUser))
	{
		SessionView V;
		// the revoke id is a hash of the token: stable, opaque, safe in HTML -
		// the raw token (the live credential) never leaves the server
		V.sRevokeID = KSHA256(Rec.sToken).HexDigest();

		if (!Rec.sUserAgent.empty())
		{
			KHTTPUserAgent UA(Rec.sUserAgent);
			V.sBrowser = UA.GetBrowser().GetString();
			V.sOS      = UA.GetOS().GetString();
			V.sDevice  = DeviceTypeName(UA.GetDeviceType());
		}
		V.sClientIP   = Rec.sClientIP;
		V.sSignedIn   = Rec.tCreated.to_string();
		V.sLastActive = Rec.tLastSeen.to_string();
		V.bCurrent    = (Rec.sToken == sCurrentToken);
		Out.push_back(std::move(V));
	}
	return Out;
}

//-----------------------------------------------------------------------------
/// gate an admin-only route. On success returns true and fills sUserOut.
/// Not signed in -> 302 to /login (throws). Signed in but not admin -> 403 page.
bool GateAdmin(KRESTServer& HTTP, KSession& Session, KSSOdUserStore& Users, KString& sUserOut)
//-----------------------------------------------------------------------------
{
	sUserOut = CurrentUser(HTTP, Session);
	if (sUserOut.empty())
	{
		HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/login");
		throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
	}
	if (!Users.IsAdmin(sUserOut))
	{
		RenderForbidden(HTTP, sUserOut); // the 403 page lives in the UI layer
		return false;
	}
	return true;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow=*/true);
		Options.SetBriefDescription("kssod - an OpenID Connect provider (SSO server)");

		KREST::Options Settings;

		KString sConfigDir = kGetConfigPath();

		Settings.iPort                = Options("http <port>           : TCP port to bind to (default 8080)", 8080);
		bool     bNoTLS               = Options("notls                 : serve plain HTTP instead of TLS — also disables Secure cookies; for local testing only", false);
		KString  sIssuer              = Options("issuer <url>          : public issuer base URL (default http(s)://localhost:<port>)",
		                                        kFormat("{}://localhost:{}", !bNoTLS ? "https" : "http", Settings.iPort));
		KString  sDB                  = Options("db <path>             : SQLite database file", kFormat("{}/kssod.sqlite", sConfigDir));
		KString  sKeyFile             = Options("signkey <path>        : RS256 token signing key (PEM); generated if missing", kFormat("{}/kssod.key", sConfigDir));
		Settings.iMaxConnections      = Options("n <max>               : max parallel connections (default 25)", 25);
//		Settings.iMaxKeepaliveRounds  = Options("keepalive <maxrounds> : max keepalive rounds (default 10, 0 == off)", 10);
		Settings.iTimeout             = Options("timeout <seconds>     : server timeout (default 5)", 5);
		double dRateLimit             = Options("ratelimit <req/s>     : per-IP request rate limit, e.g. 10 or 0.5 (default 0 == off)", 0.0);
		uint16_t iRateBurst           = Options("rateburst <count>     : rate limit burst size (default 10)", 10);
		uint16_t iConnLimit           = Options("connlimit <max>       : per-IP max concurrent connections (default 0 == off)", 0);
		KStringViewZ sMaxBody         = Options("maxbody <size>        : max request body size, supports k/M/G suffixes (default 1M, 0 == unlimited)", "1M");
		Settings.sCert                = Options("cert <file>           : TLS certificate filepath (.pem), defaults to self-signed ephemeral cert", "");
		Settings.sKey                 = Options("key <file>            : TLS private key filepath (.pem), defaults to ephemeral key", "");
		Settings.bStoreEphemeralCert  = Options("persist               : should a self-signed cert be persisted to disk and reused at next start?", false);
		Settings.sTLSPassword         = Options("tlspass <pass>        : TLS certificate password, if any", "");
		Settings.sAllowedCipherSuites = Options("ciphers <suites>      : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
		uint32_t iSessionIdleMin      = Options("session-idle <minutes>: sign-in session idle timeout in minutes (default 30)", 30);
		uint32_t iSessionMaxHours     = Options("session-max <hours>   : sign-in session absolute lifetime in hours (default 8)", 8);

		if (Options.Terminate())
		{
			return 0;
		}

		// TLS is the default, like every other dekaf2 HTTP server: an ephemeral
		// self-signed cert is created when none is given; --cert/--key supply a
		// real one; --notls opts out into plain HTTP. bCreateEphemeralCert is the
		// "make one up if no real cert was loaded" fallback (KREST still loads
		// --cert/--key when present). bTLS is the effective TLS state, used to
		// gate Secure cookies and the issuer scheme: the server serves TLS unless
		// --notls was given without a real cert.
		Settings.bCreateEphemeralCert = !bNoTLS;
		const bool bTLS = !bNoTLS || !Settings.sCert.empty() || !Settings.sKey.empty();

		if (dRateLimit > 0) Settings.SetRateLimit(dRateLimit, iRateBurst);
		if (iConnLimit > 0) Settings.SetConnectionLimit(iConnLimit);

		Settings.iMaxRequestBodySize = kFromBinarySize(sMaxBody);

		// --- schema ----------------------------------------------------------
		KString sError;
		if (!KSSOdInitDatabase(sDB, sError))
		{
			KErr.FormatLine("kssod: {}", sError);
			return 1;
		}

		// --- persistent RS256 signing key ------------------------------------
		KRSAKey SigningKey;
		if (!SigningKey.Load(sKeyFile))
		{
			SigningKey = KRSAKey(2048);
			if (SigningKey.empty())
			{
				KErr.FormatLine("kssod: could not generate an RSA signing key");
				return 1;
			}
			if (!SigningKey.Save(sKeyFile, /*bPrivateKey=*/true))
			{
				KErr.FormatLine("kssod: warning: could not persist signing key to {}", sKeyFile);
			}
			KOut.FormatLine(":: generated a new RS256 signing key -> {}", sKeyFile);
		}
		else
		{
			KOut.FormatLine(":: loaded RS256 signing key from {}", sKeyFile);
		}

		// --- stores ----------------------------------------------------------
		auto pUsers    = std::make_shared<KSSOdUserStore>(sDB);
		auto pClients  = std::make_shared<KSSOdClientStore>(sDB);
		auto pGrants   = std::make_shared<MemoryGrantStore>();
		auto pSettings = std::make_shared<KSSOdSettingsStore>(sDB);

		// seed an empty database with demo data
		if (pUsers->Count() == 0)
		{
			pUsers->AddUser("admin", "admin123", "Site Admin",    "admin@example.com", /*bAdmin=*/true);
			pUsers->AddUser("alice", "alice123", "Alice Example", "alice@example.com", /*bAdmin=*/false);
			KOut.FormatLine(":: seeded users:   admin/admin123 (admin)   alice/alice123");
		}
		if (pClients->Count() == 0)
		{
			KOpenIDServer::ClientStore::Client Demo;
			Demo.sClientID              = "demo-app";
			Demo.sClientSecretHash      = KBCrypt().GenerateHash("demo-secret");
			Demo.RedirectURIs           = { "http://localhost:3000/auth/callback" };
			Demo.PostLogoutRedirectURIs = { "http://localhost:3000/" };
			Demo.Scopes                 = { "openid", "profile", "email" };
			Demo.bPublic                = false;
			pClients->AddClient(Demo, /*bRequireAssignment=*/false); // open: any user may sign in
			// demonstrate the role catalog + per-client role grants (the client is
			// open, so these are purely RBAC hints in the tokens, not an access gate)
			pUsers->AddRole("demo-app", "admin");
			pUsers->AddRole("demo-app", "editor");
			pUsers->AddRole("demo-app", "user");
			pUsers->GrantRoles("admin", "demo-app", { "admin" });
			pUsers->GrantRoles("alice", "demo-app", { "user" });
			KOut.FormatLine(":: seeded client:  demo-app/demo-secret -> http://localhost:3000/auth/callback");
			KOut.FormatLine(":: seeded roles:   catalog [admin editor user]  admin->[admin] alice->[user]");
		}

		// --- the OP login session: SQLite store behind the caching layer -----
		KSession::Config SessionConfig;
		SessionConfig.sCookieName     = "kssod_session";
		SessionConfig.sSameSite       = "Lax";   // sent on the cross-site /authorize navigation
		SessionConfig.bSecure         = bTLS;
		// Bound how long a sign-in stays usable for silent SSO. The cookie itself
		// stays non-persistent (bPersistentCookie defaults to false), i.e. a session
		// cookie the browser drops when it closes — so an abandoned login does not
		// outlive the browser, and these timeouts cap it even if it does not.
		SessionConfig.IdleTimeout     = std::chrono::minutes(iSessionIdleMin);
		SessionConfig.AbsoluteTimeout = std::chrono::hours(iSessionMaxHours);
		auto pSession = std::make_shared<KSession>(
			std::make_unique<KSessionCachingStore>(std::make_unique<KSessionSQLiteStore>(sDB)),
			SessionConfig);

		// --- the provider ----------------------------------------------------
		KOpenIDServer::Config Config;
		Config.sIssuer        = sIssuer;
		Config.sKid           = "kssod-key-1";
		Config.bSecureCookies = bTLS;
		KOpenIDServer Server(std::move(Config), pSession, pUsers, pClients, pGrants, std::move(SigningKey));

		auto Redirect = [](KRESTServer& HTTP, KStringView sURL)
		{
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sURL);
			throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
		};

		// half-finished logins (password ok, second factor still owed)
		PendingTwoFactorStore Pending2FA;

		// gather the self-service state the account page renders
		auto AccountStateFor = [&pUsers, &pSettings, &pSession](KRESTServer& HTTP, KStringView sUser)
		{
			AccountState St;
			St.bHasTotp       = pUsers->HasTotp(sUser);
			St.iBackupCodes   = pUsers->CountBackupCodes(sUser);
			St.bEmailVerified = pUsers->IsEmailVerified(sUser);
			St.bEmailOtp      = pUsers->HasEmailOtp(sUser);
			St.bSmtp          = pSettings->SmtpConfigured();
			St.Sessions       = SessionViewsFor(HTTP, *pSession, sUser);
			return St;
		};

		KRESTRoutes Routes;

		// ===================== OIDC protocol endpoints =======================
		Routes.AddRoute({ KHTTPMethod::GET,  false, "/.well-known/openid-configuration",
			[&Server](KRESTServer& HTTP) { Server.ServeDiscovery(HTTP); } });
		Routes.AddRoute({ KHTTPMethod::GET,  false, "/jwks",
			[&Server](KRESTServer& HTTP) { Server.ServeJWKS(HTTP); } });
		Routes.AddRoute({ KHTTPMethod::GET,  false, "/authorize",
			[&Server](KRESTServer& HTTP) { Server.HandleAuthorize(HTTP); } });
		Routes.AddRoute({ KHTTPMethod::POST, false, "/token",
			[&Server](KRESTServer& HTTP) { Server.HandleToken(HTTP); }, KRESTRoute::WWWFORM });
		Routes.AddRoute({ KHTTPMethod::GET,  false, "/userinfo",
			[&Server](KRESTServer& HTTP) { Server.HandleUserInfo(HTTP); } });
		Routes.AddRoute({ KHTTPMethod::GET,  false, "/logout",
			[&Server](KRESTServer& HTTP) { Server.HandleLogout(HTTP); } });

		// ======================== end-user web UI ============================

		// landing / home router (root is normalized to "" by the router). Signed-in
		// users are sent straight to their home - admins to the admin dashboard,
		// everyone else to their account page; anonymous visitors get the sign-in
		// landing. (The nav bar carries the same links, so no menu page is needed.)
		Routes.AddRoute({ KHTTPMethod::GET, false, "",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (!sUser.empty())
			{
				Redirect(HTTP, pUsers->IsAdmin(sUser) ? "/admin" : "/account");
			}
			RenderLanding(HTTP);
		} });

		Routes.AddRoute({ KHTTPMethod::GET, false, "/login",
			[&pSession, &pSettings, &Redirect](KRESTServer& HTTP)
		{
			if (!CurrentUser(HTTP, *pSession).empty()) Redirect(HTTP, "/");
			RenderLogin(HTTP, "", pSettings->SmtpConfigured());
		} });

		// kicks off the email-OTP step: mint a code, mail it, remember its hash.
		// shared by POST /login and the "resend" route.
		auto BeginEmailOtp = [&pUsers, &pSettings, &Pending2FA](KStringView sUser) -> KString
		{
			KString sCode  = KTOTP::GenerateNumericCode(6);
			KString sToken = Pending2FA.BeginEmail(sUser, KTOTP::HashCode(sCode));
			KString sErr;
			SendMail(pSettings->LoadSmtp(), pUsers->GetEmail(sUser), "Your kssod sign-in code",
			         kFormat("Your sign-in code is {}\r\n\r\nIt expires in 5 minutes. "
			                 "If you did not try to sign in, you can ignore this email.", sCode), sErr);
			return sToken;
		};

		Routes.AddRoute({ KHTTPMethod::POST, false, "/login",
			[&Server, &pUsers, &pSettings, &Pending2FA, &BeginEmailOtp](KRESTServer& HTTP)
		{
			const auto& Q = HTTP.GetQueryParms();
			KString sUser = Q["username"];
			if (!Server.VerifyPassword(sUser, Q["password"]))
			{
				RenderLogin(HTTP, "Invalid username or password.", pSettings->SmtpConfigured(),
				            KHTTPError::H4xx_NOTAUTH);
				return;
			}
			// password ok - demand the second factor (if any) before the session
			// cookie is issued (CompleteLogin runs only after step two). An
			// authenticator app takes precedence over email codes.
			if (pUsers->HasTotp(sUser))
			{
				RenderTwoFactor(HTTP, Pending2FA.BeginTotp(sUser), "totp", "", false);
				return;
			}
			if (pUsers->HasEmailOtp(sUser) && pSettings->SmtpConfigured())
			{
				RenderTwoFactor(HTTP, BeginEmailOtp(sUser), "email", "", false);
				return;
			}
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, Server.CompleteLogin(HTTP, sUser));
			throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/login/2fa",
			[&Server, &pUsers, &pSettings, &Pending2FA](KRESTServer& HTTP)
		{
			const auto& Q        = HTTP.GetQueryParms();
			KStringView sPending = Q["pending"];
			auto        P        = Pending2FA.Peek(sPending);
			if (!P.bValid)
			{
				// the pending token expired or is unknown - send them back to step one
				RenderLogin(HTTP, "Your sign-in timed out. Please sign in again.",
				            pSettings->SmtpConfigured(), KHTTPError::H4xx_NOTAUTH);
				return;
			}

			KStringView sCode = Q["code"];
			bool bOK = (P.sMethod == "email")
			         ? (KTOTP::HashCode(sCode) == P.sEmailCodeHash)
			         : (KTOTP(pUsers->GetTotpSecret(P.sUsername)).Verify(sCode)
			            || pUsers->ConsumeBackupCode(P.sUsername, KTOTP::HashCode(sCode)));
			if (!bOK)
			{
				RenderTwoFactor(HTTP, sPending, P.sMethod,
				                (P.sMethod == "email") ? "That code is not valid. Check your email and try again."
				                                       : "That code is not valid. Try again or use a backup code.",
				                true, KHTTPError::H4xx_NOTAUTH);
				return;
			}

			Pending2FA.Consume(sPending);
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, Server.CompleteLogin(HTTP, P.sUsername));
			throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
		}, KRESTRoute::WWWFORM });

		// resend the email sign-in code (email method only)
		Routes.AddRoute({ KHTTPMethod::POST, false, "/login/2fa/resend",
			[&pUsers, &pSettings, &Pending2FA](KRESTServer& HTTP)
		{
			const auto& Q        = HTTP.GetQueryParms();
			KStringView sPending = Q["pending"];
			auto        P        = Pending2FA.Peek(sPending);
			if (!P.bValid || P.sMethod != "email")
			{
				RenderLogin(HTTP, "Your sign-in timed out. Please sign in again.",
				            pSettings->SmtpConfigured(), KHTTPError::H4xx_NOTAUTH);
				return;
			}
			// mint a new code on the EXISTING pending token (so the same form works on)
			KString sCode = KTOTP::GenerateNumericCode(6);
			Pending2FA.SetEmailCode(sPending, KTOTP::HashCode(sCode));
			KString sErr;
			SendMail(pSettings->LoadSmtp(), pUsers->GetEmail(P.sUsername), "Your kssod sign-in code",
			         kFormat("Your sign-in code is {}\r\n\r\nIt expires in 5 minutes.", sCode), sErr);
			RenderTwoFactor(HTTP, sPending, "email", "A new code is on its way.", false);
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::GET, false, "/account",
			[&pSession, &pUsers, &AccountStateFor, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			KJSON Claims;
			pUsers->GetClaims(sUser, Claims);
			RenderAccount(HTTP, sUser, pUsers->IsAdmin(sUser), Claims, {}, false, AccountStateFor(HTTP, sUser));
		} });

		// sign out of every browser: end all of this user's OP login sessions. Apps
		// the user is already signed in to stay open until they next contact us, but
		// no further silent SSO can happen anywhere — and this browser is logged out
		// at once. (Existing app sessions ending only on their next sign-in is the
		// inherent limit of provider-side logout; see the help text on the page.)
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/sessions/close-all",
			[&pSession, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			pSession->LogoutAllFor(sUser);
			HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE, pSession->SerializeExpiryCookie());
			Redirect(HTTP, "/login");
		}, KRESTRoute::WWWFORM });

		// end a single session, identified by the opaque revoke id (a hash of the
		// token) shown in the list - we never accept a raw token from the client
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/sessions/close",
			[&pSession, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) { Redirect(HTTP, "/login"); return; }

			KStringView sId = HTTP.GetQueryParms()["id"];
			if (!sId.empty())
			{
				KStringView sCurrentToken = HTTP.GetCookie(pSession->GetCookieName());
				for (const auto& Rec : pSession->ListSessionsFor(sUser))
				{
					if (KSHA256(Rec.sToken).HexDigest() == sId)
					{
						pSession->Logout(Rec.sToken);
						// if the user closed their own current session, also drop the cookie
						if (Rec.sToken == sCurrentToken)
						{
							HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE, pSession->SerializeExpiryCookie());
							Redirect(HTTP, "/login");
							return;
						}
						break;
					}
				}
			}
			Redirect(HTTP, "/account");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/password",
			[&pSession, &pUsers, &AccountStateFor, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");

			const auto& Q       = HTTP.GetQueryParms();
			KStringView sCurrent = Q["current"];
			KStringView sNew     = Q["new"];
			KStringView sConfirm = Q["confirm"];

			KJSON Claims;
			pUsers->GetClaims(sUser, Claims);
			bool bAdmin = pUsers->IsAdmin(sUser);

			KStringView sError;
			if      (!pUsers->VerifyPassword(sUser, sCurrent)) sError = "Current password is incorrect.";
			else if (sNew.size() < 8)                          sError = "New password must be at least 8 characters.";
			else if (sNew != sConfirm)                         sError = "New passwords do not match.";

			if (!sError.empty())
			{
				RenderAccount(HTTP, sUser, bAdmin, Claims, sError, true, AccountStateFor(HTTP, sUser),
				              KHTTPError::H4xx_BADREQUEST);
				return;
			}
			pUsers->ChangePassword(sUser, sNew);
			RenderAccount(HTTP, sUser, bAdmin, Claims, "Password changed.", false, AccountStateFor(HTTP, sUser));
		}, KRESTRoute::WWWFORM });

		// ----- email: verification + email-OTP-as-2FA toggle -----

		// send (or resend) a verification link to the signed-in user's address
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/email/verify-send",
			[&pSession, &pUsers, &pSettings, &sIssuer, &AccountStateFor, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");

			KJSON Claims;
			pUsers->GetClaims(sUser, Claims);
			bool bAdmin = pUsers->IsAdmin(sUser);

			KString sEmail = pUsers->GetEmail(sUser);
			if (sEmail.empty() || !pSettings->SmtpConfigured())
			{
				RenderAccount(HTTP, sUser, bAdmin, Claims, "Email is not available right now.", true,
				              AccountStateFor(HTTP, sUser), KHTTPError::H4xx_BADREQUEST);
				return;
			}

			KString sToken = pUsers->CreateEmailToken(sUser, "verify");
			KString sErr;
			SendMail(pSettings->LoadSmtp(), sEmail, "Confirm your kssod email",
			         kFormat("Hello {},\r\n\r\nplease confirm this address by opening:\r\n{}/verify-email?token={}"
			                 "\r\n\r\nThe link is valid for 24 hours.", sUser, sIssuer, sToken), sErr);

			RenderAccount(HTTP, sUser, bAdmin, Claims,
			              kFormat("We sent a verification link to {}.", sEmail), false, AccountStateFor(HTTP, sUser));
		}, KRESTRoute::WWWFORM });

		// public: follow the verification link from the email
		Routes.AddRoute({ KHTTPMethod::GET, false, "/verify-email",
			[&pUsers](KRESTServer& HTTP)
		{
			KString sUser = pUsers->ConsumeEmailToken(HTTP.GetQueryParms()["token"], "verify");
			if (sUser.empty())
			{
				// the link itself was well-formed; the token is just stale - a 200
				// outcome page reads better here than a 4xx for an emailed link
				RenderInfo(HTTP, "Link expired",
				           "This verification link is invalid or has expired. You can request a new one from your account.",
				           "/account", "Go to your account");
				return;
			}
			pUsers->SetEmailVerified(sUser, true);
			RenderInfo(HTTP, "Email confirmed", "Thanks - your email address is now verified.",
			           "/account", "Go to your account");
		} });

		// turn email codes on/off as the second factor (verified address required)
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/email/on",
			[&pSession, &pUsers, &pSettings, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			// only meaningful with a verified address and a working relay
			if (pSettings->SmtpConfigured() && pUsers->IsEmailVerified(sUser))
			{
				pUsers->SetEmailOtp(sUser, true);
			}
			Redirect(HTTP, "/account");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/email/off",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			pUsers->SetEmailOtp(sUser, false);
			Redirect(HTTP, "/account");
		}, KRESTRoute::WWWFORM });

		// ----- password recovery (public) -----

		Routes.AddRoute({ KHTTPMethod::GET, false, "/forgot",
			[&pSession, &pSettings, &Redirect](KRESTServer& HTTP)
		{
			if (!CurrentUser(HTTP, *pSession).empty()) Redirect(HTTP, "/account");
			if (!pSettings->SmtpConfigured())          Redirect(HTTP, "/login"); // feature off
			RenderForgot(HTTP, {}, false);
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/forgot",
			[&pUsers, &pSettings, &sIssuer](KRESTServer& HTTP)
		{
			KStringView sEmail = HTTP.GetQueryParms()["email"];

			// look the user up, but never reveal whether the address exists: the
			// response is identical whether or not we actually sent anything
			if (pSettings->SmtpConfigured())
			{
				KString sUser = pUsers->FindByEmail(sEmail);
				if (!sUser.empty() && pUsers->IsEmailVerified(sUser))
				{
					KString sToken = pUsers->CreateEmailToken(sUser, "recovery");
					KString sErr;
					SendMail(pSettings->LoadSmtp(), sEmail, "Reset your kssod password",
					         kFormat("Hello {},\r\n\r\nto choose a new password, open:\r\n{}/reset?token={}"
					                 "\r\n\r\nThe link is valid for one hour. If you did not request this, "
					                 "you can ignore this email.", sUser, sIssuer, sToken), sErr);
				}
			}
			RenderInfo(HTTP, "Check your email",
			           "If that address belongs to an account, we just sent a password reset link.",
			           "/login", "Back to sign in");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::GET, false, "/reset",
			[](KRESTServer& HTTP)
		{
			// the token is validated on submit; here we just carry it into the form
			RenderReset(HTTP, HTTP.GetQueryParms()["token"], {});
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/reset",
			[&pUsers](KRESTServer& HTTP)
		{
			const auto& Q       = HTTP.GetQueryParms();
			KStringView sToken  = Q["token"];
			KStringView sNew     = Q["new"];
			KStringView sConfirm = Q["confirm"];

			if (sNew.size() < 8)
			{
				RenderReset(HTTP, sToken, "New password must be at least 8 characters.", KHTTPError::H4xx_BADREQUEST);
				return;
			}
			if (sNew != sConfirm)
			{
				RenderReset(HTTP, sToken, "New passwords do not match.", KHTTPError::H4xx_BADREQUEST);
				return;
			}
			// consume the token only now that the input is valid (so a typo does not
			// burn the one-shot link)
			KString sUser = pUsers->ConsumeEmailToken(sToken, "recovery");
			if (sUser.empty())
			{
				RenderInfo(HTTP, "Link expired",
				           "This reset link is invalid or has expired. Please request a new one.",
				           "/forgot", "Request a new link");
				return;
			}
			pUsers->ChangePassword(sUser, sNew);
			RenderInfo(HTTP, "Password updated", "Your password has been changed. You can sign in now.",
			           "/login", "Sign in");
		}, KRESTRoute::WWWFORM });

		// ----- two-step verification (TOTP) enrolment -----

		// step 1: mint a fresh secret and show it for the user to add to their app
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/setup",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			Render2FASetup(HTTP, sUser, pUsers->IsAdmin(sUser), KTOTP::Generate().Secret(), "");
		}, KRESTRoute::WWWFORM });

		// step 2: the user confirmed a code -> store the secret, hand out backup codes
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/enable",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");

			const auto& Q       = HTTP.GetQueryParms();
			KStringView sSecret = Q["secret"];
			if (!KTOTP(KString(sSecret)).Verify(Q["code"]))
			{
				// wrong code: re-show the SAME secret so the user can retry
				Render2FASetup(HTTP, sUser, pUsers->IsAdmin(sUser), sSecret,
				               "That code did not match. Check the time on your device and try again.",
				               KHTTPError::H4xx_BADREQUEST);
				return;
			}
			pUsers->SetTotpSecret(sUser, sSecret);

			// issue a fresh set of single-use backup codes (stored hashed)
			auto Codes = KTOTP::GenerateBackupCodes();
			std::vector<KString> Hashes;
			Hashes.reserve(Codes.size());
			for (const auto& sCode : Codes) Hashes.push_back(KTOTP::HashCode(sCode));
			pUsers->SetBackupCodes(sUser, Hashes);

			RenderBackupCodes(HTTP, sUser, pUsers->IsAdmin(sUser), Codes);
		}, KRESTRoute::WWWFORM });

		// regenerate backup codes (invalidates the previous set)
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/backup",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty())        Redirect(HTTP, "/login");
			if (!pUsers->HasTotp(sUser)) Redirect(HTTP, "/account"); // nothing to back up

			auto Codes = KTOTP::GenerateBackupCodes();
			std::vector<KString> Hashes;
			Hashes.reserve(Codes.size());
			for (const auto& sCode : Codes) Hashes.push_back(KTOTP::HashCode(sCode));
			pUsers->SetBackupCodes(sUser, Hashes);

			RenderBackupCodes(HTTP, sUser, pUsers->IsAdmin(sUser), Codes);
		}, KRESTRoute::WWWFORM });

		// disable 2FA (drops the secret and any remaining backup codes)
		Routes.AddRoute({ KHTTPMethod::POST, false, "/account/2fa/disable",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser = CurrentUser(HTTP, *pSession);
			if (sUser.empty()) Redirect(HTTP, "/login");
			pUsers->ClearTotp(sUser);
			Redirect(HTTP, "/account");
		}, KRESTRoute::WWWFORM });

		// ========================== admin web UI =============================

		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin",
			[&pSession, &pUsers](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			RenderAdminHome(HTTP, sUser);
		} });

		// ----- email (SMTP) settings -----
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/settings",
			[&pSession, &pUsers, &pSettings](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			RenderSettings(HTTP, sUser, pSettings->LoadSmtp(), {}, false);
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/settings",
			[&pSession, &pUsers, &pSettings](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KSSOdSettingsStore::Smtp Smtp;
			Smtp.sURL      = Q["smtp_url"];
			Smtp.sUser     = Q["smtp_user"];
			Smtp.sPass     = Q["smtp_pass"];
			Smtp.sFrom     = Q["smtp_from"];
			Smtp.sFromName = Q["smtp_fromname"];
			pSettings->SaveSmtp(Smtp);

			RenderSettings(HTTP, sUser, Smtp,
			               Smtp.IsConfigured() ? "Saved. Email features are now available."
			                                   : "Saved. Set a relay URL and From address to enable email features.",
			               false);
		}, KRESTRoute::WWWFORM });

		// fire a one-off test message to confirm the relay actually works
		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/settings/test",
			[&pSession, &pUsers, &pSettings](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			auto    Smtp = pSettings->LoadSmtp();
			KString sTo  = HTTP.GetQueryParms()["to"];
			KString sErr;
			bool    bOK  = SendMail(Smtp, sTo, "kssod test email",
			                        "This is a test message from kssod. If you can read it, outgoing mail works.", sErr);

			RenderSettings(HTTP, sUser, Smtp,
			               bOK ? kFormat("Test email sent to {}.", sTo)
			                   : kFormat("Could not send: {}", sErr),
			               !bOK, bOK ? 200 : KHTTPError::H4xx_BADREQUEST);
		}, KRESTRoute::WWWFORM });

		// ----- users -----
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/users",
			[&pSession, &pUsers](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			RenderUsers(HTTP, sUser, *pUsers);
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/users/add",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KStringView sName     = Q["username"];
			KStringView sPassword = Q["password"];
			bool        bAdmin    = !Q["is_admin"].empty();

			// echo the submitted values back on error (never the password)
			KJSON Prefill = {
				{ "username", sName         },
				{ "name",     Q["name"]     },
				{ "email",    Q["email"]    },
				{ "is_admin", Q["is_admin"] }
			};

			if (sName.empty() || sPassword.empty())
			{
				RenderUsers(HTTP, sUser, *pUsers, "Username and password are required.", true, KHTTPError::H4xx_BADREQUEST, Prefill);
				return;
			}
			if (pUsers->Exists(sName))
			{
				RenderUsers(HTTP, sUser, *pUsers, kFormat("User '{}' already exists.", sName), true, KHTTPError::H4xx_BADREQUEST, Prefill);
				return;
			}
			if (!pUsers->AddUser(sName, sPassword, Q["name"], Q["email"], bAdmin))
			{
				RenderUsers(HTTP, sUser, *pUsers, "Could not create the user.", true, KHTTPError::H5xx_ERROR, Prefill);
				return;
			}
			Redirect(HTTP, "/admin/users");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/users/delete",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q       = HTTP.GetQueryParms();
			KStringView sTarget = Q["username"];
			// never delete yourself, and the target must exist
			if (sTarget.empty() || sTarget == sUser || !pUsers->Exists(sTarget)) { Redirect(HTTP, "/admin/users"); }

			if (Q["confirmed"].empty())
			{
				// first click: show the blunt confirmation, do NOT delete yet
				RenderUserDeleteConfirm(HTTP, sUser, sTarget, *pUsers);
				return;
			}
			pUsers->DeleteUser(sTarget); // cascades the user's assignments
			Redirect(HTTP, "/admin/users");
		}, KRESTRoute::WWWFORM });

		// ----- clients -----
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/clients",
			[&pSession, &pUsers, &pClients](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			RenderClients(HTTP, sUser, *pClients);
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/add",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KStringView sClientID = Q["client_id"];
			KStringView sSecret   = Q["secret"];
			bool        bPublic   = Q["confidential"].empty(); // checked = confidential, so unchecked = public
			bool        bRequire  = !Q["require_assignment"].empty();
			// re-authentication policy (minutes in the UI, stored as a KDuration)
			int64_t     iMaxAgeMin = Q["max_auth_age"].Int64();
			if (iMaxAgeMin < 0) iMaxAgeMin = 0;

			KOpenIDServer::ClientStore::Client Client;
			Client.sClientID              = sClientID;
			Client.bPublic                = bPublic;
			Client.bForceLogin            = !Q["force_login"].empty();
			Client.MaxAuthAge             = KDuration(std::chrono::minutes(iMaxAgeMin));
			// the URI/scope fields are free text (one per line or space separated);
			// KSimpleSpacedWords breaks them at any whitespace, skipping empties
			KSimpleSpacedWords Redirects  (Q["redirect_uris"]);
			KSimpleSpacedWords PostLogouts(Q["postlogout_uris"]);
			KSimpleSpacedWords Scopes     (Q["scopes"]);
			Client.RedirectURIs          .assign(Redirects  ->begin(), Redirects  ->end());
			Client.PostLogoutRedirectURIs.assign(PostLogouts->begin(), PostLogouts->end());
			Client.Scopes                .assign(Scopes     ->begin(), Scopes     ->end());
			if (Client.Scopes.empty()) Client.Scopes = { "openid" };

			// echo the submitted values back on error so nothing is retyped
			KJSON Prefill = {
				{ "client_id",          sClientID               },
				{ "secret",             sSecret                 },
				{ "redirect_uris",      Q["redirect_uris"]      },
				{ "postlogout_uris",    Q["postlogout_uris"]    },
				{ "scopes",             Q["scopes"]             },
				{ "supported_roles",    Q["supported_roles"]    },
				{ "confidential",       Q["confidential"]       },
				{ "require_assignment", Q["require_assignment"] },
				{ "force_login",        Q["force_login"]        },
				{ "max_auth_age",       Q["max_auth_age"]       }
			};

			KStringView sError;
			if      (sClientID.empty())              sError = "Client ID is required.";
			else if (pClients->Exists(sClientID))    sError = "A client with that ID already exists.";
			else if (Client.RedirectURIs.empty())    sError = "At least one redirect URI is required.";
			else if (!bPublic && sSecret.empty())    sError = "A confidential client needs a secret (or mark it public).";

			if (!sError.empty())
			{
				RenderClients(HTTP, sUser, *pClients, sError, true, KHTTPError::H4xx_BADREQUEST, Prefill);
				return;
			}
			if (!bPublic)
			{
				Client.sClientSecretHash = KBCrypt().GenerateHash(KString(sSecret));
			}
			if (!pClients->AddClient(Client, bRequire))
			{
				RenderClients(HTTP, sUser, *pClients, "Could not register the client.", true, KHTTPError::H5xx_ERROR, Prefill);
				return;
			}
			// seed the client's declared role catalog (Azure appRoles style)
			KSimpleSpacedWords SupportedRoles(Q["supported_roles"]);
			for (KStringView sRole : *SupportedRoles)
			{
				pUsers->AddRole(sClientID, sRole);
			}
			Redirect(HTTP, "/admin/clients");
		}, KRESTRoute::WWWFORM });

		// show the edit form for an existing app, prefilled with its current settings
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/clients/edit",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			KStringView sClientID = HTTP.GetQueryParms()["client_id"];
			KSSOdClientStore::ClientInfo Info;
			if (sClientID.empty() || !pClients->LookupInfo(sClientID, Info)) { Redirect(HTTP, "/admin/clients"); }

			// repopulate the textareas one URI per line, scopes space separated
			KString sRedirects;  sRedirects .Join(Info.Client.RedirectURIs,           "\n");
			KString sPostLogout; sPostLogout.Join(Info.Client.PostLogoutRedirectURIs, "\n");
			KString sScopes;     sScopes    .Join(Info.Client.Scopes,                 " " );
			KJSON Values = {
				{ "confidential",       Info.Client.bPublic ? "" : "1"     },
				{ "redirect_uris",      sRedirects                         },
				{ "postlogout_uris",    sPostLogout                        },
				{ "scopes",             sScopes                            },
				{ "require_assignment", Info.bRequireAssignment ? "1" : "" },
				{ "force_login",        Info.Client.bForceLogin ? "1" : "" },
				{ "max_auth_age",       Info.Client.MaxAuthAge.IsZero()
				                        ? KString{} : kFormat("{}", Info.Client.MaxAuthAge.minutes().count()) },
			};
			RenderClientEdit(HTTP, sUser, sClientID, Values);
		} });

		// save edits to an existing app
		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/edit",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q        = HTTP.GetQueryParms();
			KString     sClientID = Q["client_id"];

			KSSOdClientStore::ClientInfo Existing;
			if (sClientID.empty() || !pClients->LookupInfo(sClientID, Existing)) { Redirect(HTTP, "/admin/clients"); }

			KStringView sSecret  = Q["secret"];
			bool        bPublic  = Q["confidential"].empty(); // checked = confidential
			bool        bRequire = !Q["require_assignment"].empty();
			int64_t     iMaxAgeMin = Q["max_auth_age"].Int64();
			if (iMaxAgeMin < 0) iMaxAgeMin = 0;

			KOpenIDServer::ClientStore::Client Client;
			Client.sClientID              = sClientID;
			Client.bPublic                = bPublic;
			Client.bForceLogin            = !Q["force_login"].empty();
			Client.MaxAuthAge             = KDuration(std::chrono::minutes(iMaxAgeMin));
			KSimpleSpacedWords Redirects  (Q["redirect_uris"]);
			KSimpleSpacedWords PostLogouts(Q["postlogout_uris"]);
			KSimpleSpacedWords Scopes     (Q["scopes"]);
			Client.RedirectURIs          .assign(Redirects  ->begin(), Redirects  ->end());
			Client.PostLogoutRedirectURIs.assign(PostLogouts->begin(), PostLogouts->end());
			Client.Scopes                .assign(Scopes     ->begin(), Scopes     ->end());
			if (Client.Scopes.empty()) Client.Scopes = { "openid" };

			// the values to re-show on a validation error
			KJSON Values = {
				{ "confidential",       Q["confidential"]       },
				{ "redirect_uris",      Q["redirect_uris"]      },
				{ "postlogout_uris",    Q["postlogout_uris"]    },
				{ "scopes",             Q["scopes"]             },
				{ "require_assignment", Q["require_assignment"] },
				{ "force_login",        Q["force_login"]        },
				{ "max_auth_age",       Q["max_auth_age"]       },
			};

			// a confidential client must end up with a secret: either a new one was
			// typed, or it already had one
			bool bHadSecret = !Existing.Client.sClientSecretHash.empty();

			KStringView sError;
			if (Client.RedirectURIs.empty())                       sError = "At least one redirect URI is required.";
			else if (!bPublic && sSecret.empty() && !bHadSecret)   sError = "A confidential client needs a secret (or mark it public).";

			if (!sError.empty())
			{
				RenderClientEdit(HTTP, sUser, sClientID, Values, sError, true, KHTTPError::H4xx_BADREQUEST);
				return;
			}

			// secret handling: public clears it; a typed secret rotates it; otherwise keep
			bool bUpdateSecret = false;
			if (bPublic)               { bUpdateSecret = true; Client.sClientSecretHash.clear(); }
			else if (!sSecret.empty()) { bUpdateSecret = true; Client.sClientSecretHash = KBCrypt().GenerateHash(KString(sSecret)); }

			if (!pClients->UpdateClient(Client, bRequire, bUpdateSecret))
			{
				RenderClientEdit(HTTP, sUser, sClientID, Values, "Could not save the changes.", true, KHTTPError::H5xx_ERROR);
				return;
			}
			Redirect(HTTP, "/admin/clients");
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/delete",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q       = HTTP.GetQueryParms();
			KStringView sTarget = Q["client_id"];
			if (sTarget.empty() || !pClients->Exists(sTarget)) { Redirect(HTTP, "/admin/clients"); }

			if (Q["confirmed"].empty())
			{
				// first click: show the blunt confirmation, do NOT delete yet
				RenderClientDeleteConfirm(HTTP, sUser, sTarget, *pUsers);
				return;
			}
			pClients->DeleteClient(sTarget); // cascades roles + assignments
			Redirect(HTTP, "/admin/clients");
		}, KRESTRoute::WWWFORM });

		// ----- per-client access / role assignment -----
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/clients/access",
			[&pSession, &pUsers](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			KStringView sClientID = HTTP.GetQueryParms()["client_id"];
			RenderClientAccess(HTTP, sUser, sClientID, *pUsers);
		} });

		// (per-client "assign" was removed: assignment is done in the per-user
		//  grid at /admin/users/access — see GET /admin/users/access/save)

		// ----- role catalog (per client) -----
		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/roles/add",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KStringView sClientID = Q["client_id"];
			KStringView sRole     = Q["role"];

			if (sClientID.empty()) { Redirect(HTTP, "/admin/clients"); }

			KStringView sError;
			if      (sRole.empty())            sError = "Role name is required.";
			else if (sRole.contains(' '))      sError = "Role names must not contain spaces.";

			if (!sError.empty())
			{
				KJSON Prefill = { { "role", sRole } };
				RenderClientAccess(HTTP, sUser, sClientID, *pUsers, sError, true, KHTTPError::H4xx_BADREQUEST, Prefill);
				return;
			}
			pUsers->AddRole(sClientID, sRole);
			Redirect(HTTP, kFormat("/admin/clients/access?client_id={}", sClientID));
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/roles/delete",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KStringView sClientID = Q["client_id"];
			KStringView sRole     = Q["role"];
			if (!sClientID.empty() && !sRole.empty()) pUsers->DeleteRole(sClientID, sRole);
			Redirect(HTTP, kFormat("/admin/clients/access?client_id={}", sClientID));
		}, KRESTRoute::WWWFORM });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/clients/roles/default",
			[&pSession, &pUsers, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KStringView sClientID = Q["client_id"];
			if (!sClientID.empty()) pUsers->SetDefaultRole(sClientID, Q["role"]); // empty role clears it
			Redirect(HTTP, kFormat("/admin/clients/access?client_id={}", sClientID));
		}, KRESTRoute::WWWFORM });

		// ----- global access overview + per-user grid -----
		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/access",
			[&pSession, &pUsers, &pClients](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			RenderAccessOverview(HTTP, sUser, *pUsers, *pClients);
		} });

		Routes.AddRoute({ KHTTPMethod::GET, false, "/admin/users/access",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;
			KStringView sTarget = HTTP.GetQueryParms()["user"];
			if (sTarget.empty() || !pUsers->Exists(sTarget)) { Redirect(HTTP, "/admin/access"); }
			RenderUserAccess(HTTP, sUser, sTarget, *pUsers, *pClients);
		} });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/admin/users/access/save",
			[&pSession, &pUsers, &pClients, &Redirect](KRESTServer& HTTP)
		{
			KString sUser;
			if (!GateAdmin(HTTP, *pSession, *pUsers, sUser)) return;

			const auto& Q = HTTP.GetQueryParms();
			KString sTarget = Q["user"];
			if (sTarget.empty() || !pUsers->Exists(sTarget)) { Redirect(HTTP, "/admin/access"); }

			// the grid enumerates clients (and each client's roles) in the same
			// deterministic order as the render, so positional field names align
			auto ClientList = pClients->List();
			for (std::size_t i = 0; i < ClientList.size(); ++i)
			{
				const KString& sClientID = ClientList[i].Client.sClientID;
				bool bAccess = !Q[kFormat("access_{}", i)].empty();
				// always read the role checkboxes: when access is unchecked the row
				// is suspended (access=0) but its roles are preserved, not deleted
				std::vector<KString> Selected;
				auto Defined = pUsers->ListRoles(sClientID);
				for (std::size_t j = 0; j < Defined.size(); ++j)
				{
					if (!Q[kFormat("role_{}_{}", i, j)].empty()) Selected.push_back(Defined[j]);
				}
				pUsers->Upsert(sTarget, sClientID, bAccess, Selected);
			}
			Redirect(HTTP, "/admin/access"); // back to the overview after saving
		}, KRESTRoute::WWWFORM });

		// --- run -------------------------------------------------------------
		Settings.Type                 = KREST::HTTP;
		Settings.bBlocking            = true;

		KOut.FormatLine(":: kssod listening on {}://localhost:{}  (issuer: {}, db: {})",
		                bTLS ? "https" : "http", Settings.iPort, sIssuer, sDB);
		if (!bTLS)
		{
			KOut.FormatLine(":: plain HTTP (--notls): cookies are non-Secure — do not use in production");
		}
		KOut.Flush(); // the server blocks below; flush the banner now (stdout is
		              // fully buffered when redirected to a file/pipe)

		KREST Rest;
		if (!Rest.Execute(Settings, Routes))
		{
			KErr.FormatLine("kssod: server error: {}", Rest.Error());
			return 1;
		}

		return 0;
	}
	catch (const KException& ex)
	{
		KErr.FormatLine("kssod: {}", ex.what());
		return 1;
	}

} // main
