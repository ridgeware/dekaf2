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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 //
 */

#pragma once

/// @file kopenidclient.h
/// Backend-for-Frontend OpenID Connect client (relying party). The backend
/// owns the entire authorization-code + PKCE flow and stores the resulting
/// tokens server-side in a KSession, so the browser only ever holds an opaque,
/// HttpOnly session cookie. This is the complement to KOpenID (kopenid.h),
/// which is the token-validation / resource-server side: KOpenIDClient obtains
/// and refreshes tokens, KOpenID/KJWT validates them.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/clock/ktime.h>      // KUnixTime
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/kopenid.h>   // KOpenIDProviderList, KJWT
#include <dekaf2/web/url/kurl.h>          // KURL (returned by BuildLoginURL)
#include <cstddef>
#include <memory>
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

class KRESTServer;

/// @addtogroup crypto_auth
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Backend-for-Frontend OpenID Connect client (relying party).
///
/// The backend drives the full authorization-code + PKCE flow and keeps the
/// access/refresh/id tokens server-side (stashed in the KSession "Extra" slot);
/// the browser receives only an opaque, HttpOnly session cookie plus a
/// non-sensitive "_hint" cookie that lets client-side JS tell whether a session
/// exists. This BFF pattern is robust against browser restrictions on
/// third-party cookies / silent-renewal iframes, because the token lifecycle is
/// handled server-side rather than in the browser.
///
/// Typical wiring in a KRESTServer route table:
/// @code
/// KOpenIDClient::Config cfg;
/// cfg.sAuthority     = "https://sso.example.com";
/// cfg.sClientID      = "my-client";
/// cfg.sClientSecret  = "...";
/// cfg.sCallbackURI   = "https://host/auth/callback";
/// cfg.sPostLogoutURI = "https://host/";
/// cfg.sScope         = "openid profile offline_access";
///
/// KOpenIDClient OIDC(cfg, "/var/lib/myapp/sessions.sqlite");
///
/// // GET /auth/login     -> OIDC.Redirect to the IdP
/// // GET /auth/callback  -> OIDC.HandleCallback(HTTP)
/// // any protected route -> if (OIDC.Authorize(HTTP).empty()) redirect to login
/// // GET /auth/userinfo  -> OIDC.GetUserInfo(HTTP)
/// // GET /auth/logout    -> OIDC.Logout(HTTP)
/// @endcode
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOpenIDClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Config
	{
		KString   sAuthority;                       ///< issuer base URL, e.g. https://sso.example.com
		KString   sClientID;
		KString   sClientSecret;
		KString   sCallbackURI;                     ///< full redirect URL, e.g. https://host/auth/callback
		KString   sPostLogoutURI;                   ///< full URL the IdP redirects to after logout
		KString   sScope;                           ///< e.g. "openid profile offline_access"
		KString   sSessionCookieName { "session" };
		KDuration IdleTimeout        { chrono::minutes(30) };
		KDuration AbsoluteTimeout    { chrono::hours(8)    };
		KDuration DiscoveryRefresh   { chrono::hours(24)   }; ///< how long discovery + JWKS are cached
		KDuration ExpiryLeeway       { chrono::minutes(5)  }; ///< treat the access token as expired this long before its real expiry
		/// Assumed access-token lifetime when the token response omits "expires_in"
		/// (RFC 6749 marks it OPTIONAL). Without a fallback a missing value would
		/// collapse the expiry to "now", forcing a silent refresh — or, with no
		/// refresh_token, a re-login — on every subsequent request.
		KDuration DefaultAccessTTL   { chrono::hours(1)    };
		/// Verify the id_token signature against the provider's JWKS (via KJWT).
		/// On by default. In the authorization-code back-channel the id_token is
		/// received directly from the token endpoint over TLS, so signature
		/// verification is defense-in-depth (the OIDC spec marks it SHOULD).
		/// Note that KOpenIDProvider requires the issuer URL to use HTTPS, so
		/// verification cannot succeed against a plain-HTTP provider; disable it
		/// only for a trusted back-channel IdP or in tests. The nonce is always
		/// checked regardless of this setting.
		bool      bVerifyIDTokenSignature { true };
	};

	/// Standalone form with an in-memory session store (KSessionMemoryStore). The
	/// store is volatile and process-local — sessions are lost on restart and are
	/// not shared across processes — so this suits development/testing or single-
	/// process deployments. The client owns a private KSession dedicated to OIDC
	/// logins. Always available, independent of the SQL build/link options.
	explicit KOpenIDClient(Config config);

#if DEKAF2_HAS_SQLITE3
	/// Standalone form with a SQLite-backed session store (KSessionSQLiteStore) at
	/// the given filesystem path. The path must be a writable file location (the
	/// tokens of every logged-in user are kept there server-side). The client owns
	/// a private KSession. Only available when dekaf2 is built with SQLite support.
	KOpenIDClient(Config config, KStringViewZ sSessionDBPath);
#endif

	/// Standalone form with a KSQL-backed session store (KSessionKSQLStore) on the
	/// given, already-open KSQL connection. The caller owns the connection and must
	/// keep it alive for the lifetime of this client. The client owns a private
	/// KSession dedicated to OIDC logins.
	KOpenIDClient(Config config, KSQL& db);

	/// Construct sharing an externally-owned KSession. Use this when the
	/// application maintains a single session store for *both* local
	/// password logins and OIDC logins — e.g. a login page that offers both a
	/// username/password form and a "Sign in with SSO" button, backed by one
	/// cookie and one session table.
	///
	/// The application owns the KSession and configures it (cookie attributes,
	/// store backend, timeouts, and its own password authenticator). This
	/// client adopts the session's cookie name so the cookie it sets on an
	/// OIDC login matches what the shared session validates, and it creates
	/// OIDC sessions via KSession::CreateTrustedSession() — which bypasses the
	/// authenticator, so the app's local-password authenticator is never
	/// consulted for (and never rejects) a federated login.
	///
	/// @warning In this shared model the username is the shared authorization
	/// principal: an OIDC session and a local-password session for the same
	/// username are interchangeable (see KSession::CreateTrustedSession).
	/// Ensure the IdP's usernames and your local accounts share one reconciled
	/// namespace so that neither identity source can impersonate the other.
	/// @param config   the OIDC configuration; sSessionCookieName is overridden
	///                 by the shared session's cookie name for consistency
	/// @param pSession the shared session store (must be non-null)
	KOpenIDClient(Config config, std::shared_ptr<KSession> pSession);

	/// Build the OIDC authorization URL and set the PKCE/state cookie. Returns
	/// the URL the browser should be redirected to as a KURL (call Serialize()
	/// for the string form, e.g. when setting the Location header). Returns an
	/// empty KURL (empty() == true) on discovery failure.
	KURL    BuildLoginURL  (KRESTServer& HTTP, KStringView sReturnTo);

	/// Handle the IdP callback: validate state, exchange the code for tokens,
	/// validate the id_token (signature + nonce), create a server-side session,
	/// set the session cookie, and 302-redirect to the original page (throws
	/// KHTTPError for the redirect).
	void    HandleCallback (KRESTServer& HTTP);

	/// Validate the session cookie. If the stored access token is within
	/// Config::ExpiryLeeway of expiry, silently renew it using the stored
	/// refresh_token and persist the rotated tokens back into the session
	/// (preserving its idle/absolute timeouts). If renewal is not possible the
	/// session is cleared so the frontend re-initiates login. On success sets
	/// the Authorization header and returns the username; returns an empty
	/// string on failure.
	KString Authorize      (KRESTServer& HTTP);

	/// Write a JSON object with safe user info ({logged_in, username,
	/// expires_at}) into HTTP.json.tx without exposing any tokens.
	void    GetUserInfo    (KRESTServer& HTTP);

	/// Clear the session, expire the cookie, and 302-redirect the browser to the
	/// IdP's end-session endpoint (falling back to the post-logout URI). Throws
	/// KHTTPError for the redirect.
	void    Logout         (KRESTServer& HTTP);

//----------
private:
//----------

	/// The transient login-flow state that is round-tripped through the state
	/// cookie between BuildLoginURL() and HandleCallback(): the CSRF state, the
	/// PKCE code_verifier, the OIDC nonce, and the post-login return target.
	struct LoginState
	{
		KString sState;
		KString sVerifier;
		KString sNonce;
		KString sReturnTo;

		/// a state read back from the cookie is usable only if it carries at
		/// least a state and a verifier
		bool IsValid() const { return !sState.empty() && !sVerifier.empty(); }
	};

	DEKAF2_PRIVATE KString    GenerateVerifier   ();
	DEKAF2_PRIVATE KString    ComputeChallenge   (KStringView sVerifier);
	DEKAF2_PRIVATE KString    GenerateRandom     (std::size_t iBytes = 16);
	DEKAF2_PRIVATE bool       EnsureDiscovery    ();
	DEKAF2_PRIVATE KJSON      TokenRequest       (KJSON jParams);
	/// absolute access-token expiry from a token response's "expires_in"
	/// (seconds); falls back to Config::DefaultAccessTTL when it is absent or
	/// non-positive, so a missing expires_in does not collapse to "expires now".
	DEKAF2_PRIVATE KUnixTime  AccessTokenExpiry  (const KJSON& jTokenResponse) const;
	/// Validate an id_token's signature (via KJWT against the provider's JWKS)
	/// and its nonce, then return the resolved username (empty on failure).
	DEKAF2_PRIVATE KString    ValidateIDToken    (KStringView sIDToken, KStringView sExpectedNonce);
	DEKAF2_PRIVATE KString    CreateSession      (KStringView sUsername, KStringView sClientIP,
	                                              KStringView sUserAgent, const KJSON& jTokens);

	DEKAF2_PRIVATE void       SetSessionCookie   (KRESTServer& HTTP, KStringView sToken);
	DEKAF2_PRIVATE void       ExpireSessionCookie(KRESTServer& HTTP);
	DEKAF2_PRIVATE void       SetStateCookie     (KRESTServer& HTTP, const LoginState& Login);
	/// Read and decode the state cookie. Check the returned LoginState::IsValid().
	DEKAF2_PRIVATE LoginState ReadStateCookie    (KRESTServer& HTTP);
	DEKAF2_PRIVATE void       ExpireStateCookie  (KRESTServer& HTTP);
	DEKAF2_PRIVATE void       Redirect           (KRESTServer& HTTP, KStringView sURL);
	DEKAF2_PRIVATE KString    StateCookieName    () const;
	/// Build the KSession::Config (cookie name, security flags, timeouts) from
	/// m_Config — shared by the standalone constructors.
	DEKAF2_PRIVATE KSession::Config MakeSessionConfig() const;

	Config                    m_Config;
	std::shared_ptr<KSession> m_pSession;              ///< owned (standalone ctor) or shared (shared ctor)
	KOpenIDProviderList       m_Providers;             ///< for id_token validation via KJWT (lazily filled, once)
	std::once_flag            m_ProvidersOnce;         ///< guards the one-time build of m_Providers
	std::mutex                m_RefreshMutex;          ///< serialises KOpenIDProvider::Refresh() to a single writer
	KString                   m_sTokenEndpoint;
	KString                   m_sAuthEndpoint;
	KString                   m_sEndSessionEndpoint;
	KUnixTime                 m_DiscoveryExpiry {};   ///< when the cached discovery/endpoints expire

}; // KOpenIDClient

/// @}

DEKAF2_NAMESPACE_END
