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

#include <dekaf2/crypto/auth/kopenidserver.h>

#include <dekaf2/crypto/auth/kopenid.h>          // KOpenIDKeys (verify our own access token)
#include <dekaf2/crypto/auth/kbcrypt.h>          // KBCrypt (client secret)
#include <dekaf2/crypto/rsa/krsasign.h>          // KRSASign (RS256)
#include <dekaf2/crypto/hash/kmessagedigest.h>   // KSHA256 (PKCE)
#include <dekaf2/crypto/encoding/kbase64.h>      // KBase64Url
#include <dekaf2/crypto/random/krandom.h>        // kGetRandom
#include <dekaf2/web/url/kurl.h>                 // KURL
#include <dekaf2/http/protocol/khttp_header.h>   // KHTTPHeader, BasicAuthParms
#include <dekaf2/http/server/khttperror.h>       // KHTTPError
#include <dekaf2/rest/framework/krestserver.h>   // KRESTServer
#include <dekaf2/core/format/kformat.h>          // kFormat
#include <dekaf2/core/logging/klog.h>            // kDebug

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KOpenIDServer::KOpenIDServer(Config config,
                             std::shared_ptr<KSession>    LoginSession,
                             std::shared_ptr<UserStore>   Users,
                             std::shared_ptr<ClientStore> Clients,
                             std::shared_ptr<GrantStore>  Grants,
                             KRSAKey                      SigningKey)
//-----------------------------------------------------------------------------
: m_Config       (std::move(config))
, m_LoginSession (std::move(LoginSession))
, m_Users        (std::move(Users))
, m_Clients      (std::move(Clients))
, m_Grants       (std::move(Grants))
, m_SigningKey   (std::move(SigningKey))
, m_PublicJWK    (m_SigningKey.GetPublicJWK(m_Config.sKid, "RS256"))
{
} // ctor

//-----------------------------------------------------------------------------
void KOpenIDServer::Redirect(KRESTServer& HTTP, KStringView sURL)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sURL);
	throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");

} // Redirect

//-----------------------------------------------------------------------------
void KOpenIDServer::ServeDiscovery(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KJSON Scopes = KJSON::array();
	for (const auto& sScope : m_Config.Scopes)
	{
		Scopes.push_back(sScope);
	}

	HTTP.json.tx = {
		{ "issuer",                                 m_Config.sIssuer                           },
		{ "authorization_endpoint",                 m_Config.sIssuer + m_Config.sAuthorizePath },
		{ "token_endpoint",                         m_Config.sIssuer + m_Config.sTokenPath     },
		{ "userinfo_endpoint",                      m_Config.sIssuer + m_Config.sUserInfoPath  },
		{ "jwks_uri",                               m_Config.sIssuer + m_Config.sJWKSPath      },
		{ "end_session_endpoint",                   m_Config.sIssuer + m_Config.sLogoutPath    },
		{ "response_types_supported",               KJSON::array({ "code"                                }) },
		{ "grant_types_supported",                  KJSON::array({ "authorization_code", "refresh_token" }) },
		{ "subject_types_supported",                KJSON::array({ "public"                              }) },
		{ "id_token_signing_alg_values_supported",  KJSON::array({ "RS256"                               }) },
		{ "token_endpoint_auth_methods_supported",  KJSON::array({ "client_secret_post", "client_secret_basic", "none" }) },
		{ "code_challenge_methods_supported",       KJSON::array({ "S256"                                }) },
		{ "scopes_supported",                       std::move(Scopes)                                       }
	};

} // ServeDiscovery

//-----------------------------------------------------------------------------
void KOpenIDServer::ServeJWKS(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KJSON JWKS;
	JWKS["keys"] = KJSON::array();
	JWKS["keys"].push_back(m_PublicJWK);
	HTTP.json.tx = std::move(JWKS);

} // ServeJWKS

//-----------------------------------------------------------------------------
bool KOpenIDServer::CurrentSession(KRESTServer& HTTP, KSession::Record& Out)
//-----------------------------------------------------------------------------
{
	KStringView sToken = HTTP.GetCookie(m_LoginSession->GetCookieName());
	if (sToken.empty())
	{
		return false;
	}

	return m_LoginSession->Validate(sToken, &Out);

} // CurrentSession

//-----------------------------------------------------------------------------
KString KOpenIDServer::LoggedInUser(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KSession::Record Record;
	if (!CurrentSession(HTTP, Record))
	{
		return {};
	}

	return Record.sUsername;

} // LoggedInUser

//-----------------------------------------------------------------------------
bool KOpenIDServer::ValidateClientRequest(AuthRequest& Req, KString& sError)
//-----------------------------------------------------------------------------
{
	if (Req.sClientID.empty())
	{
		sError = "missing client_id";
		return false;
	}

	ClientStore::Client Client;
	if (!m_Clients->Lookup(Req.sClientID, Client))
	{
		sError = "unknown client";
		return false;
	}

	// exact redirect_uri match against the registered allow-list (no open redirect)
	bool bRedirectOK = false;
	for (const auto& sURI : Client.RedirectURIs)
	{
		if (sURI == Req.sRedirectURI)
		{
			bRedirectOK = true;
			break;
		}
	}
	if (!bRedirectOK)
	{
		sError = "redirect_uri not registered";
		return false;
	}

	// a PKCE challenge must be present (the S256 method is checked at /authorize)
	if (Req.sCodeChallenge.empty())
	{
		sError = "PKCE with code_challenge_method=S256 is required";
		return false;
	}

	// the requested scope must contain 'openid' and be a subset of the client's allowed scopes
	bool bHasOpenID = false;
	for (auto sReq : Req.sScope.Split(' '))
	{
		if (sReq == "openid")
		{
			bHasOpenID = true;
		}

		bool bAllowed = false;
		for (const auto& sAllowed : Client.Scopes)
		{
			if (sAllowed == sReq)
			{
				bAllowed = true;
				break;
			}
		}

		if (!bAllowed)
		{
			sError = kFormat("scope not allowed for this client: {}", sReq);
			return false;
		}
	}
	if (!bHasOpenID)
	{
		sError = "scope must include 'openid'";
		return false;
	}

	// snapshot the client's re-auth policy (used by HandleAuthorize)
	Req.bClientForceLogin = Client.bForceLogin;
	Req.ClientMaxAuthAge  = Client.MaxAuthAge;
	return true;

} // ValidateClientRequest

//-----------------------------------------------------------------------------
KOpenIDServer::AuthRequest KOpenIDServer::ParseAuthRequest(KRESTServer& HTTP, KString& sError)
//-----------------------------------------------------------------------------
{
	AuthRequest Req;

	const auto& Q = HTTP.GetQueryParms();
	KStringView sResponseType = Q["response_type"];
	KStringView sMethod       = Q["code_challenge_method"];
	Req.sClientID      = Q["client_id"];
	Req.sRedirectURI   = Q["redirect_uri"];
	Req.sScope         = Q["scope"];
	Req.sState         = Q["state"];
	Req.sNonce         = Q["nonce"];
	Req.sCodeChallenge = Q["code_challenge"];

	if (sResponseType != "code")
	{
		sError = "unsupported response_type (only 'code')";
		return Req;
	}

	// the PKCE *method* can only be checked here on the live query (it is not part
	// of the parked request); the challenge presence + client/redirect/scope checks
	// live in ValidateClientRequest, which runs again on the login-resume path.
	if (sMethod != "S256")
	{
		sError = "PKCE with code_challenge_method=S256 is required";
		return Req;
	}

	if (!ValidateClientRequest(Req, sError))
	{
		return Req;
	}

	// authentication-freshness inputs from the live query (direct /authorize only)
	Req.sPrompt = Q["prompt"];
	KStringView sMaxAge = Q["max_age"];
	if (!sMaxAge.empty())
	{
		Req.iMaxAgeSeconds = sMaxAge.Int64();
		if (Req.iMaxAgeSeconds < 0) Req.iMaxAgeSeconds = 0; // junk / negative -> "must have authenticated now"
	}

	Req.bValid = true;
	return Req;

} // ParseAuthRequest

//-----------------------------------------------------------------------------
KURL KOpenIDServer::IssueCodeAndRedirectURL(const AuthRequest& Req, KStringView sSubject)
//-----------------------------------------------------------------------------
{
	// per-client access control: the application decides whether this user may
	// use this client. A denial still redirects back to the client (per OIDC),
	// but with error=access_denied instead of a code.
	KJSON jClientClaims;
	if (!m_Users->AuthorizeClientAccess(sSubject, Req.sClientID, jClientClaims))
	{
		kDebug(1, "user '{}' is not authorized for client '{}'", sSubject, Req.sClientID);
		KURL URL(Req.sRedirectURI);
		URL.Query->Set("error", "access_denied");
		if (!Req.sState.empty())
		{
			URL.Query->Set("state", Req.sState);
		}
		return URL;
	}

	GrantStore::Code Code;
	Code.sCode          = KBase64Url::Encode(kGetRandom(32));
	Code.sClientID      = Req.sClientID;
	Code.sRedirectURI   = Req.sRedirectURI;
	Code.sCodeChallenge = Req.sCodeChallenge;
	Code.sNonce         = Req.sNonce;
	Code.sScope         = Req.sScope;
	Code.sSubject       = sSubject;
	Code.tAuthTime      = KUnixTime::now();
	Code.tExpiry        = KUnixTime::now() + m_Config.AuthCodeTTL;

	if (!m_Grants->SaveCode(Code))
	{
		return {}; // empty KURL signals failure
	}

	// the redirect_uri is echoed back exactly as registered, with the freshly
	// issued code (and the client's state) appended as query parameters - KURL
	// handles correct encoding and a redirect_uri that already carries a query.
	// Serialization to a string happens at the call site (the Location header).
	KURL URL(Req.sRedirectURI);
	URL.Query->Set("code", Code.sCode);
	if (!Req.sState.empty())
	{
		URL.Query->Set("state", Req.sState);
	}
	return URL;

} // IssueCodeAndRedirectURL

//-----------------------------------------------------------------------------
void KOpenIDServer::SetPendingCookie(KRESTServer& HTTP, const AuthRequest& Req)
//-----------------------------------------------------------------------------
{
	// Park the already-validated request SERVER-SIDE and hand the browser only an
	// unguessable random handle. The browser therefore carries no client-supplied
	// request data, so a planted/tossed cookie can neither forge nor tamper with a
	// pending authorize (the worst it can do is name a handle that does not exist).
	KString sHandle = KBase64Url::Encode(kGetRandom(32));

	PendingAuth P;
	P.sClientID      = Req.sClientID;
	P.sRedirectURI   = Req.sRedirectURI;
	P.sScope         = Req.sScope;
	P.sState         = Req.sState;
	P.sNonce         = Req.sNonce;
	P.sCodeChallenge = Req.sCodeChallenge;
	P.tExpiry        = KUnixTime::now() + chrono::seconds(300);

	{
		auto Store = m_PendingAuth.unique();
		// opportunistically drop expired entries so the map cannot grow unbounded
		auto tNow = KUnixTime::now();
		for (auto it = Store->begin(); it != Store->end(); )
		{
			if (it->second.tExpiry < tNow) it = Store->erase(it);
			else                           ++it;
		}
		(*Store)[sHandle] = std::move(P);
	}

	KStringView sSecure = m_Config.bSecureCookies ? " Secure;" : "";
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}={}; HttpOnly;{} SameSite=Lax; Path=/; Max-Age=300",
		        PendingCookieName(), sHandle, sSecure));

} // SetPendingCookie

//-----------------------------------------------------------------------------
KOpenIDServer::AuthRequest KOpenIDServer::ReadPendingCookie(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// non-destructive lookup: PendingClientID() reads it to render the access-denied
	// interstitial without consuming it; ExpirePendingCookie() is the consume point.
	AuthRequest Req;

	KStringView sHandle = HTTP.GetCookie(PendingCookieName());
	if (sHandle.empty())
	{
		return Req;
	}

	auto Store = m_PendingAuth.unique();
	auto it = Store->find(KString(sHandle));
	if (it == Store->end())
	{
		return Req;
	}
	if (it->second.tExpiry < KUnixTime::now())
	{
		Store->erase(it);
		return Req;
	}

	const auto& P      = it->second;
	Req.sClientID      = P.sClientID;
	Req.sRedirectURI   = P.sRedirectURI;
	Req.sScope         = P.sScope;
	Req.sState         = P.sState;
	Req.sNonce         = P.sNonce;
	Req.sCodeChallenge = P.sCodeChallenge;
	// the stored request was validated at /authorize; CompleteLogin re-validates it
	// before issuing a code (so a client de-registered meanwhile is still caught)
	Req.bValid         = true;
	return Req;

} // ReadPendingCookie

//-----------------------------------------------------------------------------
void KOpenIDServer::ExpirePendingCookie(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// consume the server-side entry, then tell the browser to drop the handle cookie
	KStringView sHandle = HTTP.GetCookie(PendingCookieName());
	if (!sHandle.empty())
	{
		auto Store = m_PendingAuth.unique();
		Store->erase(KString(sHandle));
	}

	KStringView sSecure = m_Config.bSecureCookies ? " Secure;" : "";
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}=; HttpOnly;{} SameSite=Lax; Path=/; Max-Age=0", PendingCookieName(), sSecure));

} // ExpirePendingCookie

//-----------------------------------------------------------------------------
KURL KOpenIDServer::ErrorRedirectURL(const AuthRequest& Req, KStringView sError)
//-----------------------------------------------------------------------------
{
	KURL URL(Req.sRedirectURI);
	URL.Query->Set("error", sError);
	if (!Req.sState.empty())
	{
		URL.Query->Set("state", Req.sState);
	}
	return URL;

} // ErrorRedirectURL

//-----------------------------------------------------------------------------
void KOpenIDServer::HandleAuthorize(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KString sError;
	AuthRequest Req = ParseAuthRequest(HTTP, sError);

	if (!Req.bValid)
	{
		kDebug(1, "authorize rejected: {}", sError);
		throw KHTTPError(KHTTPError::H4xx_BADREQUEST, kFormat("invalid authorization request: {}", sError));
	}

	// OIDC 'prompt': we honor 'none' (never show UI) and 'login' (force re-auth).
	bool bPromptNone  = false;
	bool bPromptLogin = false;
	for (auto sToken : Req.sPrompt.Split(' '))
	{
		if      (sToken == "none")  bPromptNone  = true;
		else if (sToken == "login") bPromptLogin = true;
	}

	KSession::Record Session;
	bool bHaveSession = CurrentSession(HTTP, Session);

	// decide whether an interactive (re-)authentication is required. The session's
	// tCreated is its authentication time; the four triggers are the RP's
	// prompt=login and max_age, and the client's bForceLogin and MaxAuthAge policy.
	bool bMustReauth = false;
	if (bHaveSession)
	{
		const KDuration Age = KUnixTime::now() - Session.tCreated;

		if      (bPromptLogin)                                                              bMustReauth = true;
		else if (Req.bClientForceLogin)                                                     bMustReauth = true;
		else if (Req.iMaxAgeSeconds >= 0 && Age > chrono::seconds(Req.iMaxAgeSeconds))      bMustReauth = true;
		else if (!Req.ClientMaxAuthAge.IsZero() && Age > Req.ClientMaxAuthAge)              bMustReauth = true;
	}

	if (!bHaveSession || bMustReauth)
	{
		if (bPromptNone)
		{
			// the RP forbade any interaction, but we would have to prompt: per OIDC,
			// hand the error back to the client instead of showing the login screen.
			kDebug(1, "authorize: prompt=none but {} for client '{}'",
			       bHaveSession ? "re-authentication required" : "no active session", Req.sClientID);
			Redirect(HTTP, ErrorRedirectURL(Req, "login_required").Serialize());
			return;
		}

		// stash the validated request and send the browser to the login screen.
		// (A forced re-auth over a live session lands here too: the fresh login
		// in CompleteLogin mints a new session, advancing the authentication time.)
		SetPendingCookie(HTTP, Req);
		Redirect(HTTP, m_Config.sLoginPath);
		return;
	}

	// authenticated and fresh enough — but is this user authorized for this client?
	// A *silently-resolved* SSO session that is denied must NOT dead-end with
	// access_denied (the user never got a chance to pick the right account): stash
	// the request and send the browser to the access-denied screen, where the app
	// offers to sign in as a different (authorized) account or to return to the
	// client. A denial AFTER an interactive login happens in CompleteLogin and
	// still returns access_denied, so this cannot loop. prompt=none forbids any
	// interaction, so there the spec error goes straight back to the client.
	{
		KJSON jClaims;
		if (!m_Users->AuthorizeClientAccess(Session.sUsername, Req.sClientID, jClaims))
		{
			if (bPromptNone)
			{
				Redirect(HTTP, ErrorRedirectURL(Req, "access_denied").Serialize());
				return;
			}
			kDebug(1, "authorize: user '{}' not authorized for client '{}' on a live session -> access-denied screen",
			       Session.sUsername, Req.sClientID);
			SetPendingCookie(HTTP, Req);
			Redirect(HTTP, m_Config.sAccessDeniedPath);
			return;
		}
	}

	// authorized: issue the code and bounce back to the client
	KURL RedirectURL = IssueCodeAndRedirectURL(Req, Session.sUsername);
	if (RedirectURL.empty())
	{
		throw KHTTPError(KHTTPError::H5xx_ERROR, "could not issue authorization code");
	}
	Redirect(HTTP, RedirectURL.Serialize());

} // HandleAuthorize

//-----------------------------------------------------------------------------
bool KOpenIDServer::VerifyPassword(KStringView sUsername, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	return m_Users->VerifyPassword(sUsername, sPassword);

} // VerifyPassword

//-----------------------------------------------------------------------------
KString KOpenIDServer::CompleteLogin(KRESTServer& HTTP, KStringView sUsername)
//-----------------------------------------------------------------------------
{
	// establish the OP login session for the (already authenticated) user
	KString sToken = m_LoginSession->CreateTrustedSession(
		sUsername, HTTP.GetRemoteIP(), HTTP.Request.Headers[KHTTPHeader::USER_AGENT]);

	if (sToken.empty())
	{
		return {};
	}

	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE, m_LoginSession->SerializeSetCookie(sToken));

	// resume a pending authorize, if any
	AuthRequest Req = ReadPendingCookie(HTTP);
	ExpirePendingCookie(HTTP);

	// Re-validate before issuing a code, even though the parked request was already
	// validated at /authorize: this enforces the redirect_uri allow-list / PKCE /
	// scope on the resume path too (defense in depth, and it catches a client that
	// was de-registered or reconfigured between /authorize and the login).
	KString sError;
	if (Req.bValid && ValidateClientRequest(Req, sError))
	{
		KURL RedirectURL = IssueCodeAndRedirectURL(Req, sUsername);
		if (!RedirectURL.empty())
		{
			return RedirectURL.Serialize();
		}
	}
	else if (Req.bValid)
	{
		kDebug(1, "pending authorize failed re-validation on login resume: {}", sError);
	}

	return m_Config.sPostLoginRedirect;

} // CompleteLogin

//-----------------------------------------------------------------------------
KString KOpenIDServer::PendingClientID(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	return ReadPendingCookie(HTTP).sClientID;

} // PendingClientID

//-----------------------------------------------------------------------------
KString KOpenIDServer::DeclineAccess(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	AuthRequest Req = ReadPendingCookie(HTTP);
	ExpirePendingCookie(HTTP);

	if (!Req.bValid)
	{
		return {};
	}

	return ErrorRedirectURL(Req, "access_denied").Serialize();

} // DeclineAccess

//-----------------------------------------------------------------------------
KString KOpenIDServer::SignJWT(const KJSON& Payload) const
//-----------------------------------------------------------------------------
{
	KJSON Header = { { "alg", "RS256" }, { "kid", m_Config.sKid }, { "typ", "JWT" } };

	KString sInput = KBase64Url::Encode(Header.dump());
	sInput += '.';
	sInput += KBase64Url::Encode(Payload.dump());

	KRSASign Signer(KRSASign::SHA256, sInput);
	KString sSig = Signer.Sign(m_SigningKey);

	KString sJWT = sInput;
	sJWT += '.';
	sJWT += KBase64Url::Encode(sSig);
	return sJWT;

} // SignJWT

//-----------------------------------------------------------------------------
KJSON KOpenIDServer::IssueTokens(KStringView sSubject, KStringView sClientID,
                                 KStringView sScope,   KStringView sNonce,
                                 KUnixTime tAuthTime,  const KJSON& jClientClaims)
//-----------------------------------------------------------------------------
{
	KUnixTime tNow = KUnixTime::now();

	// --- id_token: start from the user's profile claims, then set the core claims ---
	KJSON idClaims;
	KJSON UserClaims;
	if (m_Users->GetClaims(sSubject, UserClaims) && UserClaims.is_object())
	{
		idClaims = std::move(UserClaims);
	}
	// merge the per-client claims (e.g. "roles") supplied by AuthorizeClientAccess
	if (jClientClaims.is_object())
	{
		for (auto it = jClientClaims.begin(); it != jClientClaims.end(); ++it)
		{
			idClaims[it.key()] = it.value();
		}
	}
	idClaims["iss"]       = m_Config.sIssuer;
	idClaims["sub"]       = sSubject;
	idClaims["aud"]       = sClientID;
	idClaims["token_use"] = "id";   // lets /userinfo tell id_tokens from access_tokens
	idClaims["iat"]       = tNow.to_time_t();
	idClaims["exp"]       = (tNow + m_Config.IdTokenTTL).to_time_t();
	idClaims["auth_time"] = tAuthTime.to_time_t();
	if (!sNonce.empty())
	{
		idClaims["nonce"] = sNonce;
	}

	KString sIdToken = SignJWT(idClaims);

	// --- access_token: a self-validating JWT. "aud" now identifies the client the
	// token was minted for, so a resource server can bind tokens to itself via
	// KRESTServer::Options::sAuthAudience (defends against cross-client/cross-RS
	// replay). Because the id_token now also carries aud=client_id, the token type
	// is distinguished by an explicit "token_use" claim, which /userinfo checks. ---
	KJSON acClaims = {
		{ "iss"      , m_Config.sIssuer   },
		{ "sub"      , sSubject           },
		{ "aud"      , sClientID          },
		{ "client_id", sClientID          },
		{ "token_use", "access"           },
		{ "iat"      , tNow.to_time_t()   },
		{ "exp"      , (tNow + m_Config.AccessTTL).to_time_t() },
		{ "scope"    , sScope             }
	};
	// expose the same client-scoped claims (roles) to resource servers, without
	// letting them shadow the standard access-token claims
	if (jClientClaims.is_object())
	{
		for (auto it = jClientClaims.begin(); it != jClientClaims.end(); ++it)
		{
			if (!kjson::Exists(acClaims, it.key())) acClaims[it.key()] = it.value();
		}
	}

	KString sAccessToken = SignJWT(acClaims);

	// --- refresh_token: opaque, stored server-side, rotated on use ---
	KString sRefresh = KBase64Url::Encode(kGetRandom(32));
	GrantStore::Refresh R;
	R.sToken    = sRefresh;
	R.sClientID = sClientID;
	R.sScope    = sScope;
	R.sSubject  = sSubject;
	R.tAuthTime = tAuthTime;
	R.tExpiry   = tNow + m_Config.RefreshTTL;
	m_Grants->SaveRefresh(R);

	return KJSON {
		{ "access_token" , std::move(sAccessToken) },
		{ "token_type"   , "Bearer"                },
		{ "expires_in"   , m_Config.AccessTTL.seconds().count() },
		{ "id_token"     , std::move(sIdToken)     },
		{ "refresh_token", std::move(sRefresh)     },
		{ "scope"        , sScope                  }
	};

} // IssueTokens

//-----------------------------------------------------------------------------
bool KOpenIDServer::AuthenticateClient(KRESTServer& HTTP, const ClientStore::Client& Client,
                                       KStringView sFormClientSecret)
//-----------------------------------------------------------------------------
{
	if (Client.bPublic)
	{
		// public client: no secret, PKCE is the proof
		return true;
	}

	// client_secret_post (form field) or client_secret_basic (Authorization: Basic)
	KString sSecret(sFormClientSecret);
	if (sSecret.empty())
	{
		auto Basic = HTTP.Request.GetBasicAuthParms();
		sSecret = Basic.sPassword;
	}
	if (sSecret.empty())
	{
		return false;
	}

	return KBCrypt().ValidatePassword(sSecret, Client.sClientSecretHash);

} // AuthenticateClient

//-----------------------------------------------------------------------------
void KOpenIDServer::TokenError(KRESTServer& HTTP, KStringView sError, KStringView sDescription)
//-----------------------------------------------------------------------------
{
	kDebug(1, "token error: {} ({})", sError, sDescription);
	HTTP.SetStatus(sError == "invalid_client" ? 401 : 400);
	HTTP.json.tx = {
		{ "error"            , sError       },
		{ "error_description", sDescription }
	};

} // TokenError

//-----------------------------------------------------------------------------
void KOpenIDServer::HandleToken(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// NOTE: the /token route must be registered with the WWWFORM parser, so the
	// form-encoded body is available through GetQueryParms().
	const auto& Q = HTTP.GetQueryParms();
	KStringView sGrantType    = Q["grant_type"];
	KStringView sClientSecret = Q["client_secret"];

	if (sGrantType == "authorization_code")
	{
		KStringView sCode        = Q["code"];
		KStringView sRedirectURI = Q["redirect_uri"];
		KStringView sVerifier    = Q["code_verifier"];

		if (sCode.empty())
		{
			return TokenError(HTTP, "invalid_request", "missing code");
		}

		GrantStore::Code Code;
		if (!m_Grants->TakeCode(sCode, Code)) // single-use: fetch+delete
		{
			return TokenError(HTTP, "invalid_grant", "unknown or already-used code");
		}
		if (KUnixTime::now() >= Code.tExpiry)
		{
			return TokenError(HTTP, "invalid_grant", "authorization code expired");
		}
		if (sRedirectURI != Code.sRedirectURI)
		{
			return TokenError(HTTP, "invalid_grant", "redirect_uri mismatch");
		}

		ClientStore::Client Client;
		if (!m_Clients->Lookup(Code.sClientID, Client))
		{
			return TokenError(HTTP, "invalid_client", "unknown client");
		}
		if (!AuthenticateClient(HTTP, Client, sClientSecret))
		{
			return TokenError(HTTP, "invalid_client", "client authentication failed");
		}

		// PKCE: SHA256(verifier) must equal the stored challenge
		if (sVerifier.empty())
		{
			return TokenError(HTTP, "invalid_grant", "missing code_verifier");
		}
		if (KBase64Url::Encode(KSHA256(sVerifier).Digest()) != Code.sCodeChallenge)
		{
			return TokenError(HTTP, "invalid_grant", "PKCE verification failed");
		}

		// re-check per-client access (it may have been revoked since /authorize)
		// and fetch the current client-scoped claims (roles) to embed
		KJSON jClientClaims;
		if (!m_Users->AuthorizeClientAccess(Code.sSubject, Code.sClientID, jClientClaims))
		{
			return TokenError(HTTP, "access_denied", "user is not authorized for this client");
		}

		HTTP.json.tx = IssueTokens(Code.sSubject, Code.sClientID, Code.sScope, Code.sNonce, Code.tAuthTime, jClientClaims);
		return;
	}

	if (sGrantType == "refresh_token")
	{
		KStringView sRefresh = Q["refresh_token"];
		if (sRefresh.empty())
		{
			return TokenError(HTTP, "invalid_request", "missing refresh_token");
		}

		GrantStore::Refresh R;
		if (!m_Grants->TakeRefresh(sRefresh, R)) // rotation: fetch+delete
		{
			return TokenError(HTTP, "invalid_grant", "unknown refresh_token");
		}
		if (KUnixTime::now() >= R.tExpiry)
		{
			return TokenError(HTTP, "invalid_grant", "refresh_token expired");
		}

		ClientStore::Client Client;
		if (!m_Clients->Lookup(R.sClientID, Client))
		{
			return TokenError(HTTP, "invalid_client", "unknown client");
		}
		if (!AuthenticateClient(HTTP, Client, sClientSecret))
		{
			return TokenError(HTTP, "invalid_client", "client authentication failed");
		}

		// re-check access on refresh too (access may have been revoked since the
		// grant was issued — the refresh token can live for weeks) and refresh the
		// client-scoped claims (roles) so rotated tokens carry the current set
		KJSON jClientClaims;
		if (!m_Users->AuthorizeClientAccess(R.sSubject, R.sClientID, jClientClaims))
		{
			return TokenError(HTTP, "access_denied", "user is not authorized for this client");
		}

		HTTP.json.tx = IssueTokens(R.sSubject, R.sClientID, R.sScope, KStringView{}, R.tAuthTime, jClientClaims);
		return;
	}

	TokenError(HTTP, "unsupported_grant_type", sGrantType);

} // HandleToken

//-----------------------------------------------------------------------------
bool KOpenIDServer::VerifyOwnJWT(KStringView sJWT, KJSON& Payload) const
//-----------------------------------------------------------------------------
{
	auto Parts = sJWT.Split('.');
	if (Parts.size() != 3)
	{
		return false;
	}

	// verify the signature against our own public key
	KJSON JWKS;
	JWKS["keys"] = KJSON::array();
	JWKS["keys"].push_back(m_PublicJWK);
	KOpenIDKeys Keys(JWKS);

	KStringView sSignedData(Parts[0].data(),
		static_cast<std::size_t>(Parts[1].data() + Parts[1].size() - Parts[0].data()));

	if (!Keys.VerifySignature(m_Config.sKid, "RS256", "", sSignedData, KBase64Url::Decode(Parts[2])))
	{
		return false;
	}

	Payload = kjson::Parse(KBase64Url::Decode(Parts[1]));
	return Payload.is_object() && kjson::GetStringRef(Payload, "iss") == m_Config.sIssuer;

} // VerifyOwnJWT

//-----------------------------------------------------------------------------
void KOpenIDServer::HandleUserInfo(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KString     sBearer(HTTP.Request.Headers[KHTTPHeader::AUTHORIZATION]);
	KStringView sToken(sBearer);
	sToken.TrimLeft();
	sToken.remove_prefix("Bearer ") || sToken.remove_prefix("bearer ");

	KJSON Payload;
	// require a verified, unexpired ACCESS token. The token_use check rejects an
	// id_token presented as a bearer (token-type confusion): both token kinds now
	// carry aud=client_id, so the type marker - not the audience - distinguishes them.
	if (!VerifyOwnJWT(sToken, Payload)
	    || KUnixTime::now() >= KUnixTime(kjson::GetInt(Payload, "exp"))
	    || kjson::GetStringRef(Payload, "token_use") != "access")
	{
		HTTP.SetStatus(401);
		HTTP.json.tx = { { "error", "invalid_token" } };
		return;
	}

	const KString& sSub = kjson::GetStringRef(Payload, "sub");
	KJSON Claims;
	if (!m_Users->GetClaims(sSub, Claims) || !Claims.is_object())
	{
		Claims = KJSON::object();
	}
	Claims["sub"] = sSub;
	HTTP.json.tx  = std::move(Claims);

} // HandleUserInfo

//-----------------------------------------------------------------------------
void KOpenIDServer::HandleLogout(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// 1) terminate the OP session unconditionally (server-side erase + clear the
	//    browser cookie). Logout must succeed regardless of the redirect outcome.
	KStringView sToken = HTTP.GetCookie(m_LoginSession->GetCookieName());
	if (!sToken.empty())
	{
		m_LoginSession->Logout(sToken);
	}
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE, m_LoginSession->SerializeExpiryCookie());

	const auto& Q           = HTTP.GetQueryParms();
	KStringView sPostLogout = Q["post_logout_redirect_uri"];

	// 2) nothing requested -> go to the OP's own default landing page
	if (sPostLogout.empty())
	{
		Redirect(HTTP, m_Config.sPostLoginRedirect);
		return;
	}

	// 3) a redirect target was requested. It is attacker-supplied, so it MUST be
	//    validated against the requesting client's registered post-logout URIs
	//    (exact match) - otherwise this is an open redirect. Identify the client
	//    by the explicit client_id, else by the "aud" of a verified id_token_hint
	//    (the latter is what KOpenIDClient sends).
	KString sClientID = Q["client_id"];
	if (sClientID.empty())
	{
		KStringView sHint = Q["id_token_hint"];
		KJSON       Hint;
		if (!sHint.empty() && VerifyOwnJWT(sHint, Hint))
		{
			sClientID = kjson::GetStringRef(Hint, "aud");
		}
	}

	ClientStore::Client Client;
	bool bAllowed     = false;
	bool bClientFound = false;
	if (!sClientID.empty() && m_Clients->Lookup(sClientID, Client))
	{
		bClientFound = true;
		for (const auto& sURI : Client.PostLogoutRedirectURIs)
		{
			if (sURI == sPostLogout) { bAllowed = true; break; }
		}
	}

	if (!bAllowed)
	{
		// The client only ever gets a generic error. The registered set is logged
		// server-side (admin-only, debug level >= 1) so a typo - the match is exact
		// and case-sensitive, e.g. "/login/" vs "/Login/" - is visible in the log
		// without leaking the app's URLs in the HTTP response.
		if (!bClientFound)
		{
			kDebug(1, "logout: post_logout_redirect_uri rejected - client '{}' not found (requested '{}')",
			       sClientID, sPostLogout);
		}
		else
		{
			KString sRegistered;
			sRegistered.Join(Client.PostLogoutRedirectURIs, " | ");
			kDebug(1, "logout: post_logout_redirect_uri not registered for client '{}': requested '{}', registered: [{}]",
			       sClientID, sPostLogout, sRegistered);
		}
		throw KHTTPError(KHTTPError::H4xx_BADREQUEST, "post_logout_redirect_uri is not registered");
	}

	// 4) honor the validated target, carrying the client's state back if present
	KURL        URL(sPostLogout);
	KStringView sState = Q["state"];
	if (!sState.empty())
	{
		URL.Query->Set("state", sState);
	}
	Redirect(HTTP, URL.Serialize());

} // HandleLogout

DEKAF2_NAMESPACE_END
