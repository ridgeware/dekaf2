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

/// @file kopenidserver.h
/// OpenID Connect Provider (the issuing "OP" side). Implements the
/// authorization-code + PKCE flow endpoints (discovery, JWKS, authorize, token,
/// userinfo, end-session) so that any standards-compliant relying party — in
/// particular KOpenIDClient (kopenidclient.h), but also vanilla OIDC clients —
/// can sign in against it. This is the issuing counterpart to KOpenIDClient
/// (consume-for-login) and KJWT/KOpenID (validate a bearer token).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/web/url/kurl.h>          // KURL (built by IssueCodeAndRedirectURL)
#include <dekaf2/threading/primitives/kthreadsafe.h> // m_PendingAuth
#include <memory>
#include <vector>
#include <unordered_map>

DEKAF2_NAMESPACE_BEGIN

class KRESTServer;

/// @addtogroup crypto_auth
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// OpenID Connect Provider (OP) — issues id/access/refresh tokens via the
/// authorization-code + PKCE flow.
///
/// KOpenIDServer implements the protocol and cryptography; the hosting
/// application supplies three pluggable stores and a login UI:
///   - UserStore   — authenticate a user and supply identity claims
///   - ClientStore — the relying-party registry (client_id, secret, redirect URIs)
///   - GrantStore  — short-lived authorization codes + rotating refresh tokens
///   - a KSession  — the OP's *own* browser login session, which is what makes
///                   SSO work: once a user is logged in here, a second app's
///                   /authorize issues a code without re-prompting.
///
/// Endpoint methods take a KRESTServer& and are meant to be wired into a route
/// table. The login screen stays in the application: HandleAuthorize() redirects
/// an unauthenticated user to Config::sLoginPath; the app's login handler
/// verifies the password (VerifyPassword) and calls CompleteLogin(), which
/// establishes the OP session and resumes the pending authorization.
///
/// @warning This is a reference/sample-grade implementation of an OpenID
/// Provider. The protocol paths are intended to be correct (single-use
/// PKCE-bound codes, exact redirect_uri matching, signed tokens, rotating
/// refresh tokens), but a production deployment needs an independent security
/// review.
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOpenIDServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Config
	{
		KString   sIssuer;                          ///< issuer URL (== the "iss" claim); must be HTTPS in production
		KString   sKid { "key-1" };                 ///< key id advertised in the JWKS and JWT headers
		KDuration AuthCodeTTL { chrono::seconds(60) }; ///< authorization code lifetime (issuance -> token exchange)
		KDuration AccessTTL   { chrono::minutes(5)  }; ///< access_token lifetime
		KDuration IdTokenTTL  { chrono::minutes(5)  }; ///< id_token lifetime
		KDuration RefreshTTL  { chrono::hours(720)  }; ///< refresh_token lifetime (default 30 days)
		KString   sLoginPath        { "/login" };   ///< app route that renders the login form (authorize redirects here)
		KString   sPostLoginRedirect{ "/"      };   ///< where CompleteLogin() sends the browser when no authorize is pending
		KString   sAuthorizePath    { "/authorize" };
		KString   sTokenPath        { "/token"     };
		KString   sUserInfoPath     { "/userinfo"  };
		KString   sJWKSPath         { "/jwks"      };
		KString   sLogoutPath       { "/logout"    };
		/// app route shown when a signed-in user is NOT authorized for the client
		/// they tried to enter. Instead of dead-ending a silently-resolved SSO
		/// session with access_denied, HandleAuthorize stashes the request and
		/// sends the browser here, where the app offers to sign in as a different
		/// (authorized) account or to return to the client.
		KString   sAccessDeniedPath { "/no-access" };
		std::vector<KString> Scopes { "openid", "profile", "email" }; ///< scopes the OP advertises/grants
		/// set the Secure attribute on the transient "authorize" cookie that
		/// carries a pending request across the login screen. Keep true in
		/// production (HTTPS); set false only for a plain-HTTP localhost dev
		/// server, where a Secure cookie would be dropped by the browser. (The
		/// OP login session cookie's Secure flag is governed by KSession::Config.)
		bool      bSecureCookies { true };
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Authenticates users and supplies their identity claims.
	struct DEKAF2_PUBLIC UserStore
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		virtual ~UserStore() = default;
		/// @returns true iff (sUsername, sPassword) is a valid, enabled credential pair
		virtual bool VerifyPassword(KStringView sUsername, KStringView sPassword) = 0;
		/// fill standard OIDC claims (sub, name, email, ...) for sUsername
		/// @returns false if the user is unknown
		virtual bool GetClaims(KStringView sUsername, KJSON& Claims) = 0;

		/// Per-client authorization hook. Decides whether sUsername may obtain
		/// tokens for sClientID and supplies client-scoped claims (e.g. roles) to
		/// merge into the issued tokens. Called at /authorize (an early denial
		/// redirects the browser back to the client with error=access_denied) and
		/// again at /token (a denial there fails the exchange, and the returned
		/// claims are embedded fresh into the id/access tokens).
		///
		/// The default allows every user and adds no extra claims — i.e. an open
		/// provider where any authenticated user may use any registered client.
		/// Override to implement assignment-based access control and RBAC; the
		/// access *policy* (open vs assignment-required, per client or global)
		/// lives entirely in the implementation.
		///
		/// @param sUsername     the authenticated subject
		/// @param sClientID     the relying party being accessed
		/// @param jClientClaims receives client-scoped claims to merge into the
		///                      tokens, e.g. { "roles": ["admin","editor"] }
		/// @returns true to allow, false to deny access to this client
		virtual bool AuthorizeClientAccess(KStringView sUsername, KStringView sClientID, KJSON& jClientClaims)
		{
			(void)sUsername; (void)sClientID; (void)jClientClaims;
			return true;
		}
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// The relying-party (client) registry.
	struct DEKAF2_PUBLIC ClientStore
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		struct Client
		{
			KString              sClientID;
			KString              sClientSecretHash;      ///< bcrypt hash; empty for a public client
			std::vector<KString> RedirectURIs;           ///< exact-match allow-list for the login callback
			std::vector<KString> PostLogoutRedirectURIs; ///< exact-match allow-list for RP-initiated logout
			std::vector<KString> Scopes;                 ///< scopes this client may request
			bool                 bPublic { false };      ///< public client: no secret, PKCE only
			/// Re-authentication policy (enforced at /authorize, independent of what
			/// the relying party requests): when the OP login session is older than
			/// this, force a fresh interactive login before issuing a code. Zero (the
			/// default) imposes no limit, leaving normal single sign-on in effect.
			/// This is the IdP-side equivalent of an OIDC max_age, applied to a third-
			/// party app that never sends one itself.
			KDuration            MaxAuthAge  { KDuration::zero() };
			/// Re-authentication policy: when true, always re-authenticate at
			/// /authorize even if a live OP session exists — as if every request
			/// carried prompt=login. Use for sensitive clients that must never be
			/// entered via silent SSO.
			bool                 bForceLogin { false };
		};
		virtual ~ClientStore() = default;
		/// @returns true and fills Out if sClientID is registered
		virtual bool Lookup(KStringView sClientID, Client& Out) = 0;
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Stores the two short-lived grant artifacts: authorization codes and
	/// refresh tokens. Both fetch operations are *consuming* (single-use code,
	/// rotated refresh token) and must be atomic against concurrent callers.
	struct DEKAF2_PUBLIC GrantStore
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		struct Code
		{
			KString   sCode;
			KString   sClientID;
			KString   sRedirectURI;
			KString   sCodeChallenge; ///< PKCE S256 challenge
			KString   sNonce;
			KString   sScope;
			KString   sSubject;       ///< the authenticated user
			KUnixTime tAuthTime;
			KUnixTime tExpiry;
		};
		struct Refresh
		{
			KString   sToken;
			KString   sClientID;
			KString   sScope;
			KString   sSubject;
			KUnixTime tAuthTime;
			KUnixTime tExpiry;
		};
		virtual ~GrantStore() = default;
		virtual bool SaveCode    (const Code& Rec)                  = 0;
		/// fetch AND remove a code (single-use); @returns false if unknown
		virtual bool TakeCode    (KStringView sCode, Code& Out)     = 0;
		virtual bool SaveRefresh (const Refresh& Rec)               = 0;
		/// fetch AND remove a refresh token (rotation); @returns false if unknown
		virtual bool TakeRefresh (KStringView sToken, Refresh& Out) = 0;
		/// drop expired codes/refresh tokens (call periodically)
		virtual void PurgeExpired(KUnixTime tNow)                   = 0;
	};

	/// @param config       OP configuration (issuer, TTLs, endpoint paths)
	/// @param LoginSession the OP's own browser session (drives SSO); non-null
	/// @param Users        user authentication + claims; non-null
	/// @param Clients      the relying-party registry; non-null
	/// @param Grants       code + refresh storage; non-null
	/// @param SigningKey   the RSA private key used to sign id/access tokens (RS256)
	KOpenIDServer(Config config,
	              std::shared_ptr<KSession>     LoginSession,
	              std::shared_ptr<UserStore>    Users,
	              std::shared_ptr<ClientStore>  Clients,
	              std::shared_ptr<GrantStore>   Grants,
	              KRSAKey                       SigningKey);

	/// GET /.well-known/openid-configuration — the discovery document
	void ServeDiscovery (KRESTServer& HTTP);
	/// GET jwks_uri — the public signing keys (JWKS)
	void ServeJWKS      (KRESTServer& HTTP);
	/// GET /authorize — validate the request; if the user has an OP session,
	/// issue a code and 302 to the client's redirect_uri; otherwise stash the
	/// request and 302 to Config::sLoginPath. Throws KHTTPError for redirects.
	void HandleAuthorize(KRESTServer& HTTP);
	/// POST /token — authorization_code or refresh_token grant; writes the
	/// token (or error) JSON response.
	void HandleToken    (KRESTServer& HTTP);
	/// GET /userinfo — return the subject's claims for a valid Bearer access token
	void HandleUserInfo (KRESTServer& HTTP);
	/// GET /logout — clear the OP session and 302 to post_logout_redirect_uri
	void HandleLogout   (KRESTServer& HTTP);

	/// Verify a username/password pair (thin pass-through to the UserStore), for
	/// the application's login handler.
	bool    VerifyPassword(KStringView sUsername, KStringView sPassword);

	/// Establish the OP login session for an already-authenticated user, consume
	/// any pending /authorize request, and return the URL the browser should be
	/// sent to next: the client's redirect_uri (with the freshly issued code) if
	/// an authorize was pending, otherwise Config::sPostLoginRedirect. Sets the
	/// session cookie on the response.
	/// @returns the redirect URL, or empty on error (e.g. code could not be stored)
	KString CompleteLogin (KRESTServer& HTTP, KStringView sUsername);

	/// The client_id of the authorize request currently parked across the login /
	/// access-denied screens (read from the transient pending cookie), or empty if
	/// none is pending. Lets the access-denied page name the application.
	KString PendingClientID(KRESTServer& HTTP);

	/// Decline the pending authorize request: clears the pending cookie and returns
	/// the client's redirect_uri carrying error=access_denied (and the original
	/// state). For the "back to the app" choice on the access-denied page.
	/// @returns the redirect URL, or empty if no request is pending.
	KString DeclineAccess (KRESTServer& HTTP);

//----------
private:
//----------

	/// the result of validating an /authorize request
	struct AuthRequest
	{
		KString   sClientID;
		KString   sRedirectURI;
		KString   sScope;
		KString   sState;
		KString   sNonce;
		KString   sCodeChallenge;
		KString   sPrompt;                              ///< OIDC 'prompt' (space-delimited set; we honor 'none' and 'login')
		int64_t   iMaxAgeSeconds    { -1 };             ///< OIDC 'max_age' in seconds; -1 == not supplied
		bool      bClientForceLogin { false };          ///< snapshot of the client's force-login policy
		KDuration ClientMaxAuthAge  { KDuration::zero() }; ///< snapshot of the client's max-auth-age policy
		bool      bValid { false };
	};

	/// a validated /authorize request parked server-side while the user logs in;
	/// the browser only holds an opaque random handle to it (see SetPendingCookie),
	/// so a pending request can neither be forged nor tampered with from the client
	struct PendingAuth
	{
		KString   sClientID;
		KString   sRedirectURI;
		KString   sScope;
		KString   sState;
		KString   sNonce;
		KString   sCodeChallenge;
		KUnixTime tExpiry;
	};

	DEKAF2_PRIVATE AuthRequest ParseAuthRequest (KRESTServer& HTTP, KString& sError);
	/// validate a client request: client exists, redirect_uri is on the registered
	/// allow-list (exact match), a PKCE challenge is present, and the scope is
	/// 'openid' plus a subset of the client's allowed scopes. Shared by /authorize
	/// and the login-resume path so both enforce the same rules. Also snapshots the
	/// client's force-login / max-auth-age policy. @returns false and sets sError.
	DEKAF2_PRIVATE bool        ValidateClientRequest(AuthRequest& Req, KString& sError);
	/// issue+store a single-use code and build the client's redirect_uri with the
	/// code (and state) appended; @returns the URL as a KURL, or an empty KURL
	/// (empty() == true) if the code could not be stored. Serialize at the boundary.
	DEKAF2_PRIVATE KURL        IssueCodeAndRedirectURL(const AuthRequest& Req, KStringView sSubject);
	DEKAF2_PRIVATE void        SetPendingCookie (KRESTServer& HTTP, const AuthRequest& Req);
	DEKAF2_PRIVATE AuthRequest ReadPendingCookie(KRESTServer& HTTP);
	DEKAF2_PRIVATE void        ExpirePendingCookie(KRESTServer& HTTP);
	/// name of the pending-authorize handle cookie; the "__Host-" prefix (which
	/// forces Secure + Path=/ + no Domain, defeating subdomain cookie injection) is
	/// only valid over HTTPS, so it is used exactly when cookies are marked Secure.
	DEKAF2_PRIVATE KStringView PendingCookieName() const
	{ return m_Config.bSecureCookies ? "__Host-oidc_authorize" : "oidc_authorize"; }
	DEKAF2_PRIVATE KString     LoggedInUser     (KRESTServer& HTTP); ///< sub from the OP session, or empty
	/// the OP login session behind the request's cookie; on success fills Out
	/// (username + tCreated, the authentication time used for the max_age / re-auth
	/// decision). @returns false if there is no cookie or the session is invalid/expired.
	DEKAF2_PRIVATE bool        CurrentSession   (KRESTServer& HTTP, KSession::Record& Out);
	/// build the client's redirect_uri carrying an OAuth error (e.g. login_required),
	/// preserving state — for the prompt=none path, which must not show a login screen.
	DEKAF2_PRIVATE KURL        ErrorRedirectURL (const AuthRequest& Req, KStringView sError);
	DEKAF2_PRIVATE KString     SignJWT          (const KJSON& Payload) const;
	/// verify a JWT THIS OP issued: checks the RS256 signature against our own
	/// public key and that "iss" matches. Does NOT check expiry — the caller
	/// decides (userinfo rejects expired tokens, logout tolerates them).
	/// @param sJWT     the compact JWS
	/// @param Payload  receives the decoded payload on success
	/// @returns true if the signature and issuer are valid
	DEKAF2_PRIVATE bool        VerifyOwnJWT     (KStringView sJWT, KJSON& Payload) const;
	DEKAF2_PRIVATE KJSON       IssueTokens      (KStringView sSubject, KStringView sClientID,
	                                             KStringView sScope, KStringView sNonce, KUnixTime tAuthTime,
	                                             const KJSON& jClientClaims = KJSON::object());
	DEKAF2_PRIVATE bool        AuthenticateClient(KRESTServer& HTTP, const ClientStore::Client& Client,
	                                             KStringView sFormClientSecret);
	DEKAF2_PRIVATE void        TokenError       (KRESTServer& HTTP, KStringView sError, KStringView sDescription);
	DEKAF2_PRIVATE void        Redirect         (KRESTServer& HTTP, KStringView sURL);

	Config                        m_Config;
	std::shared_ptr<KSession>     m_LoginSession;
	std::shared_ptr<UserStore>    m_Users;
	std::shared_ptr<ClientStore>  m_Clients;
	std::shared_ptr<GrantStore>   m_Grants;
	KRSAKey                       m_SigningKey;
	KJSON                         m_PublicJWK;   ///< precomputed public JWK for the JWKS endpoint
	/// server-side store of validated, pending /authorize requests, keyed by the
	/// random handle carried in the oidc_authorize cookie. Lets the login-resume
	/// path recover the request without trusting any client-supplied cookie data.
	KThreadSafe<std::unordered_map<KString, PendingAuth>> m_PendingAuth;

}; // KOpenIDServer

/// @}

DEKAF2_NAMESPACE_END
