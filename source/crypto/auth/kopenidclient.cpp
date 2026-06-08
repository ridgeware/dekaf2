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

#include <dekaf2/crypto/auth/kopenidclient.h>

#include <dekaf2/crypto/hash/kmessagedigest.h>   // KSHA256
#include <dekaf2/crypto/encoding/kbase64.h>      // KBase64Url
#include <dekaf2/crypto/random/krandom.h>        // kGetRandom
#include <dekaf2/http/client/kwebclient.h>       // KWebClient
#include <dekaf2/http/protocol/khttp_method.h>   // KHTTPMethod
#include <dekaf2/http/protocol/khttp_header.h>   // KHTTPHeader
#include <dekaf2/http/server/khttperror.h>       // KHTTPError
#include <dekaf2/rest/framework/krestserver.h>   // KRESTServer
#include <dekaf2/web/url/kmime.h>                // KMIME
#include <dekaf2/web/url/kurlencode.h>           // kUrlEncode, URIPart
#include <dekaf2/web/url/kurl.h>                 // KURL
#include <dekaf2/core/format/kformat.h>          // kFormat
#include <dekaf2/core/logging/klog.h>            // kDebug
#include <mutex>                                 // std::call_once, std::unique_lock

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KOpenIDClient::KOpenIDClient(Config config, KStringViewZ sSessionDBPath)
: m_Config(std::move(config))
//-----------------------------------------------------------------------------
{
	KSession::Config sc;
	sc.sCookieName     = m_Config.sSessionCookieName;
	sc.sSameSite       = "Lax";
	sc.bSecure         = true;
	sc.bHttpOnly       = true;
	sc.IdleTimeout     = m_Config.IdleTimeout;
	sc.AbsoluteTimeout = m_Config.AbsoluteTimeout;

	m_pSession = std::make_shared<KSession>(sSessionDBPath, sc);

	// No authenticator is set: OIDC sessions are created by CreateSession()
	// via KSession::CreateTrustedSession(), which does not consult one (the
	// identity is vouched for by the validated id_token, not a password).

} // KOpenIDClient (standalone)

//-----------------------------------------------------------------------------
KOpenIDClient::KOpenIDClient(Config config, std::shared_ptr<KSession> pSession)
: m_Config(std::move(config))
, m_pSession(std::move(pSession))
//-----------------------------------------------------------------------------
{
	// The application owns and configures the shared KSession, including its
	// own (local-password) authenticator. We must NOT touch that authenticator
	// — OIDC sessions are created via KSession::CreateTrustedSession(), which
	// bypasses it. We only adopt the session's cookie name so the cookie this
	// client sets on an OIDC login is the same one the shared session reads
	// back on validation.
	if (m_pSession)
	{
		m_Config.sSessionCookieName = m_pSession->GetCookieName();
	}

} // KOpenIDClient (shared session)

//-----------------------------------------------------------------------------
KString KOpenIDClient::StateCookieName() const
//-----------------------------------------------------------------------------
{
	return kFormat("{}_state", m_Config.sSessionCookieName);

} // StateCookieName

//-----------------------------------------------------------------------------
KString KOpenIDClient::GenerateVerifier()
//-----------------------------------------------------------------------------
{
	return KBase64Url::Encode(kGetRandom(32));

} // GenerateVerifier

//-----------------------------------------------------------------------------
KString KOpenIDClient::ComputeChallenge(KStringView sVerifier)
//-----------------------------------------------------------------------------
{
	KSHA256 Hash(sVerifier);
	return KBase64Url::Encode(Hash.Digest());

} // ComputeChallenge

//-----------------------------------------------------------------------------
KString KOpenIDClient::GenerateRandom(std::size_t iBytes)
//-----------------------------------------------------------------------------
{
	return KBase64Url::Encode(kGetRandom(iBytes));

} // GenerateRandom

//-----------------------------------------------------------------------------
bool KOpenIDClient::EnsureDiscovery()
//-----------------------------------------------------------------------------
{
	if (!m_sTokenEndpoint.empty() && KUnixTime::now() < m_DiscoveryExpiry)
	{
		return true;
	}

	KString sDiscoveryURL = m_Config.sAuthority;
	// KString::back() returns '\0' on an empty string (no UB) and pop_back() is
	// a no-op when empty, so no explicit empty() guard is needed here
	while (sDiscoveryURL.back() == '/')
	{
		sDiscoveryURL.pop_back();
	}
	sDiscoveryURL += "/.well-known/openid-configuration";

	KWebClient Client;
	auto sResponse = Client.HttpRequest(sDiscoveryURL, KHTTPMethod::GET, KString{}).TrimLeft();

	if (sResponse.empty())
	{
		kDebug(1, "failed to load OIDC discovery from {}", sDiscoveryURL);
		return false;
	}

	KJSON jMeta = kjson::Parse(sResponse);
	if (!jMeta.is_object())
	{
		kDebug(1, "failed to parse OIDC discovery from {}", sDiscoveryURL);
		return false;
	}

	m_sTokenEndpoint      = kjson::GetStringRef(jMeta, "token_endpoint");
	m_sAuthEndpoint       = kjson::GetStringRef(jMeta, "authorization_endpoint");
	m_sEndSessionEndpoint = kjson::GetStringRef(jMeta, "end_session_endpoint");

	if (m_sTokenEndpoint.empty() || m_sAuthEndpoint.empty())
	{
		kDebug(1, "missing required endpoints in OIDC discovery document");
		return false;
	}

	m_DiscoveryExpiry = KUnixTime::now() + m_Config.DiscoveryRefresh;
	return true;

} // EnsureDiscovery

//-----------------------------------------------------------------------------
KJSON KOpenIDClient::TokenRequest(KJSON jParams)
//-----------------------------------------------------------------------------
{
	if (!EnsureDiscovery())
	{
		return {};
	}

	jParams["client_id"]     = m_Config.sClientID;
	jParams["client_secret"] = m_Config.sClientSecret;

	// build the application/x-www-form-urlencoded request body
	KString sBody;
	for (auto& it : jParams.items())
	{
		if (!sBody.empty())
		{
			sBody += '&';
		}
		kUrlEncode(it.key(), sBody, URIPart::Query);
		sBody += '=';
		// kjson::Print yields the textual form and works on both the wrapped
		// (KJSON2) and unwrapped (LJSON) json type, unlike the KJSON2-only
		// member .String()
		kUrlEncode(kjson::Print(it.value()), sBody, URIPart::Query);
	}

	KWebClient Client;
	auto sResponse = Client.HttpRequest(m_sTokenEndpoint, KHTTPMethod::POST, sBody,
	                                    KMIME(KMIME::WWW_FORM_URLENCODED)).TrimLeft();

	if (sResponse.empty())
	{
		kDebug(1, "token endpoint returned no data");
		return {};
	}

	KJSON jResponse = kjson::Parse(sResponse);
	if (!jResponse.is_object())
	{
		kDebug(1, "token response did not parse: {}", sResponse.substr(0, 200));
		return {};
	}

	if (kjson::Exists(jResponse, "error"))
	{
		kDebug(1, "token error={}, description={}",
			kjson::GetStringRef(jResponse, "error"),
			kjson::GetStringRef(jResponse, "error_description"));
		return {};
	}

	return jResponse;

} // TokenRequest

//-----------------------------------------------------------------------------
KUnixTime KOpenIDClient::AccessTokenExpiry(const KJSON& jTokenResponse) const
//-----------------------------------------------------------------------------
{
	int64_t iExpiresIn = kjson::GetInt(jTokenResponse, "expires_in");

	// "expires_in" is OPTIONAL (RFC 6749 §5.1). When the provider omits it (or
	// sends a non-positive value), fall back to a configured default instead of
	// letting the expiry collapse to "now" - which would force a silent refresh,
	// or a re-login when no refresh_token is held, on every subsequent request.
	KDuration TTL = (iExpiresIn > 0) ? KDuration(chrono::seconds(iExpiresIn))
	                                 : m_Config.DefaultAccessTTL;

	return KUnixTime::now() + TTL;

} // AccessTokenExpiry

//-----------------------------------------------------------------------------
KString KOpenIDClient::ValidateIDToken(KStringView sIDToken, KStringView sExpectedNonce)
//-----------------------------------------------------------------------------
{
	if (sIDToken.empty())
	{
		kDebug(1, "no id_token in token response");
		return {};
	}

	KJSON Payload;

	if (m_Config.bVerifyIDTokenSignature)
	{
		// Build the provider list (which fetches discovery + JWKS) exactly once,
		// thread-safely. KOpenIDProvider requires the issuer URL to use HTTPS.
		std::call_once(m_ProvidersOnce, [this]
		{
			m_Providers.emplace_back(KURL(m_Config.sAuthority), KStringView{},
			                         m_Config.DiscoveryRefresh, /*bMustSupportScope=*/false);
		});

		// Drive the periodic key refresh. Refresh() is interval-gated and
		// self-throttling (and retries every few minutes while it has no keys,
		// e.g. after a startup outage). We serialise it with a try-lock so it
		// stays a single writer (KOpenIDProvider does lockless reads plus a
		// single decaying stage) without ever blocking concurrent logins on the
		// network fetch - a contending thread just proceeds with the current keys.
		{
			std::unique_lock<std::mutex> RefreshLock(m_RefreshMutex, std::try_to_lock);
			if (RefreshLock.owns_lock())
			{
				m_Providers.front().Refresh();
			}
		}

		// KJWT validates the signature against the provider's JWKS, plus issuer
		// and expiry (id_tokens carry no "scope", so none is required here)
		KJWT JWT;
		if (!JWT.Check(sIDToken, m_Providers))
		{
			kDebug(1, "id_token validation failed: {}", JWT.GetLastError());
			return {};
		}
		Payload = std::move(JWT.Payload);
	}
	else
	{
		// signature verification disabled: trust the TLS back-channel and only
		// decode the payload segment (header.payload.signature)
		auto iDot1 = sIDToken.find('.');
		auto iDot2 = (iDot1 == KStringView::npos) ? KStringView::npos
		                                          : sIDToken.find('.', iDot1 + 1);
		if (iDot1 == KStringView::npos || iDot2 == KStringView::npos)
		{
			kDebug(1, "malformed id_token");
			return {};
		}
		Payload = kjson::Parse(KBase64Url::Decode(sIDToken.substr(iDot1 + 1, iDot2 - iDot1 - 1)));
		if (!Payload.is_object())
		{
			kDebug(1, "id_token payload is not a JSON object");
			return {};
		}
	}

	// OIDC core requirement: the id_token nonce must match the one we issued
	if (!sExpectedNonce.empty() && kjson::GetStringRef(Payload, "nonce") != sExpectedNonce)
	{
		kDebug(1, "id_token nonce mismatch");
		return {};
	}

	// OIDC core requirement: the id_token "aud" MUST identify this RP, otherwise
	// a token minted for a different client of the same OP would be accepted
	// (cross-client replay). Enforced in both the signature-verified and the
	// decode-only path. AudienceMatches accepts the single-string and the array
	// form (RFC 7519); an empty sClientID (no client configured) imposes no
	// constraint, so this never breaks an unconfigured RP.
	if (!KJWT::AudienceMatches(Payload, m_Config.sClientID))
	{
		kDebug(1, "id_token audience does not include our client_id '{}'", m_Config.sClientID);
		return {};
	}

	KString sUsername = kjson::GetStringRef(Payload, "username");
	if (sUsername.empty())
	{
		sUsername = kjson::GetStringRef(Payload, "preferred_username");
		if (sUsername.empty())
		{
			sUsername = kjson::GetStringRef(Payload, "sub");
			if (sUsername.empty())
			{
				kDebug(1, "no username found in JWT");
			}
		}
	}
	return sUsername;

} // ValidateIDToken

//-----------------------------------------------------------------------------
KString KOpenIDClient::CreateSession(KStringView sUsername, KStringView sClientIP,
                                     KStringView sUserAgent, const KJSON& jTokens)
//-----------------------------------------------------------------------------
{
	// trusted: the id_token was already validated (signature + nonce) before we
	// got here, so we bypass the KSession authenticator. This works identically
	// whether the session is private (standalone ctor) or shared with an
	// application that uses a real password authenticator.
	return m_pSession->CreateTrustedSession(sUsername, sClientIP, sUserAgent, jTokens.dump());

} // CreateSession

//-----------------------------------------------------------------------------
void KOpenIDClient::SetSessionCookie(KRESTServer& HTTP, KStringView sToken)
//-----------------------------------------------------------------------------
{
	// HttpOnly: the real opaque token, never readable by JS
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}={}; HttpOnly; Secure; SameSite=Lax; Path=/",
		        m_Config.sSessionCookieName, sToken));

	// Non-HttpOnly hint: lets JS detect whether a session exists without
	// exposing the token itself. Contains no sensitive data.
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}_hint=1; Secure; SameSite=Lax; Path=/",
		        m_Config.sSessionCookieName));

} // SetSessionCookie

//-----------------------------------------------------------------------------
void KOpenIDClient::ExpireSessionCookie(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}=; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=0",
		        m_Config.sSessionCookieName));
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}_hint=; Secure; SameSite=Lax; Path=/; Max-Age=0",
		        m_Config.sSessionCookieName));

} // ExpireSessionCookie

//-----------------------------------------------------------------------------
void KOpenIDClient::SetStateCookie(KRESTServer& HTTP, const LoginState& Login)
//-----------------------------------------------------------------------------
{
	// Encode the login state as JSON, then base64url the whole thing so that
	// arbitrary return_to URLs cannot break the field separators.
	KJSON j = {
		{ "s", Login.sState    },
		{ "v", Login.sVerifier },
		{ "n", Login.sNonce    },
		{ "r", Login.sReturnTo }
	};
	KString sEncoded = KBase64Url::Encode(j.dump());
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}={}; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=300",
		        StateCookieName(), sEncoded));

} // SetStateCookie

//-----------------------------------------------------------------------------
KOpenIDClient::LoginState KOpenIDClient::ReadStateCookie(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	LoginState Login;

	KStringView sEncoded = HTTP.GetCookie(StateCookieName());
	if (sEncoded.empty())
	{
		return Login;
	}

	KJSON j = kjson::Parse(KBase64Url::Decode(sEncoded));
	if (!j.is_object())
	{
		return Login;
	}

	Login.sState    = kjson::GetStringRef(j, "s");
	Login.sVerifier = kjson::GetStringRef(j, "v");
	Login.sNonce    = kjson::GetStringRef(j, "n");
	Login.sReturnTo = kjson::GetStringRef(j, "r");

	return Login;

} // ReadStateCookie

//-----------------------------------------------------------------------------
void KOpenIDClient::ExpireStateCookie(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
		kFormat("{}=; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=0", StateCookieName()));

} // ExpireStateCookie

//-----------------------------------------------------------------------------
void KOpenIDClient::Redirect(KRESTServer& HTTP, KStringView sURL)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sURL);
	throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");

} // Redirect

//-----------------------------------------------------------------------------
KURL KOpenIDClient::BuildLoginURL(KRESTServer& HTTP, KStringView sReturnTo)
//-----------------------------------------------------------------------------
{
	if (!EnsureDiscovery())
	{
		return {}; // empty KURL signals failure
	}

	LoginState Login;
	Login.sVerifier    = GenerateVerifier();
	Login.sState       = GenerateRandom(16);
	Login.sNonce       = GenerateRandom(16);
	Login.sReturnTo    = sReturnTo;
	KString sChallenge = ComputeChallenge(Login.sVerifier);

	SetStateCookie(HTTP, Login);

	// assemble the authorization URL via KURL, so every query value is
	// URL-encoded uniformly and the separators are handled for us
	KURL URL(m_sAuthEndpoint);
	URL.Query->Set("response_type",         "code");
	URL.Query->Set("client_id",             m_Config.sClientID);      // config members: copy
	URL.Query->Set("scope",                 m_Config.sScope);
	URL.Query->Set("redirect_uri",          m_Config.sCallbackURI);
	URL.Query->Set("state",                 std::move(Login.sState)); // Login done after SetStateCookie: move
	URL.Query->Set("nonce",                 std::move(Login.sNonce));
	URL.Query->Set("code_challenge",        std::move(sChallenge));
	URL.Query->Set("code_challenge_method", "S256");

	return URL;

} // BuildLoginURL

//-----------------------------------------------------------------------------
void KOpenIDClient::HandleCallback(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	LoginState Login = ReadStateCookie(HTTP);
	if (!Login.IsValid())
	{
		kDebug(1, "missing or invalid state cookie");
		ExpireStateCookie(HTTP);
		Redirect(HTTP, "/");
		return;
	}
	ExpireStateCookie(HTTP);

	const auto& QueryParms = HTTP.GetQueryParms();
	KStringView sReturnedState = QueryParms["state"];
	KStringView sCode          = QueryParms["code"];
	KStringView sError         = QueryParms["error"];

	if (!sError.empty())
	{
		kDebug(1, "SSO error: {}", sError);
		Redirect(HTTP, "/");
		return;
	}
	if (sReturnedState != Login.sState || sCode.empty())
	{
		kDebug(1, "state mismatch or empty code");
		Redirect(HTTP, "/");
		return;
	}

	KJSON jTokens = TokenRequest({
		{ "grant_type"   , "authorization_code"  },
		{ "code"         , sCode                 },
		{ "redirect_uri" , m_Config.sCallbackURI },
		{ "code_verifier", Login.sVerifier       }
	});

	KString   sAccessToken  = kjson::GetStringRef(jTokens, "access_token");
	KString   sRefreshToken = kjson::GetStringRef(jTokens, "refresh_token");
	KString   sIDToken      = kjson::GetStringRef(jTokens, "id_token");
	KUnixTime tExpiresAt    = AccessTokenExpiry(jTokens);

	if (sAccessToken.empty())
	{
		kDebug(1, "no access_token in token response");
		Redirect(HTTP, "/");
		return;
	}

	// validate the id_token signature + nonce and resolve the username
	KString sUsername = ValidateIDToken(sIDToken, Login.sNonce);
	if (sUsername.empty())
	{
		kDebug(1, "id_token validation failed; rejecting login");
		Redirect(HTTP, "/");
		return;
	}

	KJSON jSessionData = {
		{ "access_token" , sAccessToken           },
		{ "refresh_token", sRefreshToken          },
		{ "id_token"     , sIDToken               },
		{ "expires_at"   , tExpiresAt.to_time_t() },
		{ "username"     , sUsername              }
	};

	KString sSessionToken = CreateSession(
		sUsername,
		HTTP.GetRemoteIP(),
		HTTP.Request.Headers[KHTTPHeader::USER_AGENT],
		jSessionData);

	if (sSessionToken.empty())
	{
		kDebug(1, "failed to create session");
		Redirect(HTTP, "/");
		return;
	}

	SetSessionCookie(HTTP, sSessionToken);
	Redirect(HTTP, Login.sReturnTo.empty() ? "/" : Login.sReturnTo);

} // HandleCallback

//-----------------------------------------------------------------------------
KString KOpenIDClient::Authorize(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KStringView sToken = HTTP.GetCookie(m_Config.sSessionCookieName);
	if (sToken.empty())
	{
		return {};
	}

	KSession::Record Record;
	if (!m_pSession->Validate(sToken, &Record))
	{
		return {};
	}

	KJSON jTokens = kjson::Parse(Record.sExtra);
	if (!jTokens.is_object())
	{
		return {};
	}

	KString   sAccessToken = kjson::GetStringRef(jTokens, "access_token");
	KUnixTime tExpiresAt(kjson::GetInt(jTokens, "expires_at"));

	// If the access token is within the leeway window of expiry, silently renew
	// it using the stored refresh_token, then persist the rotated tokens back
	// into the session (via KSession::UpdateExtra, which preserves the idle and
	// absolute timeouts). If renewal is not possible, clear the session so the
	// frontend re-initiates a fresh code exchange.
	if (KUnixTime::now() + m_Config.ExpiryLeeway >= tExpiresAt)
	{
		KString sRefreshToken = kjson::GetStringRef(jTokens, "refresh_token");

		KJSON jNew = sRefreshToken.empty() ? KJSON{} : TokenRequest({
			{ "grant_type"   , "refresh_token" },
			{ "refresh_token", sRefreshToken   }
		});

		KString sNewAccess = kjson::GetStringRef(jNew, "access_token");
		if (sNewAccess.empty())
		{
			kDebug(1, "access token expired and refresh failed, clearing session for user:{}", Record.sUsername);
			m_pSession->Logout(Record.sToken);
			ExpireSessionCookie(HTTP);
			return {};
		}

		sAccessToken            = sNewAccess;
		jTokens["access_token"] = sNewAccess;
		jTokens["expires_at"]   = AccessTokenExpiry(jNew).to_time_t();

		// providers may rotate the refresh token and/or re-issue the id_token
		KString sNewRefresh = kjson::GetStringRef(jNew, "refresh_token");
		if (!sNewRefresh.empty())
		{
			jTokens["refresh_token"] = sNewRefresh;
		}
		KString sNewIDToken = kjson::GetStringRef(jNew, "id_token");
		if (!sNewIDToken.empty())
		{
			jTokens["id_token"] = sNewIDToken;
		}

		if (!m_pSession->UpdateExtra(Record.sToken, jTokens.dump()))
		{
			// The configured Store does not support in-place updates (a custom
			// backend that has not overridden UpdateExtra). The freshly renewed
			// token is still used for this request, but it cannot be persisted,
			// so the next request will renew again.
			kDebug(2, "refreshed access token could not be persisted (Store has no UpdateExtra)");
		}
	}

	// inject for VerifyAuthentication(), which strips the "Bearer " prefix
	HTTP.Request.Headers[KHTTPHeader::AUTHORIZATION] = sAccessToken;
	return Record.sUsername;

} // Authorize

//-----------------------------------------------------------------------------
void KOpenIDClient::GetUserInfo(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KStringView sToken = HTTP.GetCookie(m_Config.sSessionCookieName);
	if (sToken.empty())
	{
		HTTP.json.tx = { { "logged_in", false } };
		return;
	}

	KSession::Record Record;
	if (!m_pSession->Validate(sToken, &Record))
	{
		ExpireSessionCookie(HTTP);
		HTTP.json.tx = { { "logged_in", false } };
		return;
	}

	KJSON jTokens = kjson::Parse(Record.sExtra);

	HTTP.json.tx = {
		{ "logged_in" , true                                 },
		{ "username"  , Record.sUsername                     },
		{ "expires_at", kjson::GetInt(jTokens, "expires_at") }
	};

} // GetUserInfo

//-----------------------------------------------------------------------------
void KOpenIDClient::Logout(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KStringView sToken = HTTP.GetCookie(m_Config.sSessionCookieName);
	KString sIDToken;

	if (!sToken.empty())
	{
		KSession::Record Record;
		if (m_pSession->Validate(sToken, &Record))
		{
			KJSON jTokens = kjson::Parse(Record.sExtra);
			sIDToken = kjson::GetStringRef(jTokens, "id_token");
		}
		m_pSession->Logout(sToken);
	}
	ExpireSessionCookie(HTTP);

	if (!EnsureDiscovery() || m_sEndSessionEndpoint.empty())
	{
		Redirect(HTTP, m_Config.sPostLogoutURI.empty() ? "/" : m_Config.sPostLogoutURI);
		return;
	}

	KURL URL(m_sEndSessionEndpoint);
	URL.Query->Set("post_logout_redirect_uri", m_Config.sPostLogoutURI);
	if (!sIDToken.empty())
	{
		URL.Query->Set("id_token_hint", sIDToken);
	}

	Redirect(HTTP, URL.Serialize());

} // Logout

DEKAF2_NAMESPACE_END
