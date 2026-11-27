#include "catch.hpp"

#include <dekaf2/crypto/auth/kopenidclient.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kcaseless.h>   // kCaselessEqual
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>       // kSleep
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
// return all values of a given response header (e.g. every "Set-Cookie")
std::vector<KString> HeaderValues(KStringView sResponse, KStringView sHeader)
//-----------------------------------------------------------------------------
{
	std::vector<KString> out;

	auto iEnd = sResponse.find("\r\n\r\n");
	KStringView sHead = (iEnd == KStringView::npos) ? sResponse : sResponse.substr(0, iEnd);

	std::size_t i = 0;
	while (i < sHead.size())
	{
		auto iEol = sHead.find("\r\n", i);
		if (iEol == KStringView::npos) iEol = sHead.size();
		KStringView sLine = sHead.substr(i, iEol - i);
		i = iEol + 2;

		if (sLine.size() > sHeader.size() && sLine[sHeader.size()] == ':'
		    && kCaselessEqual(sLine.substr(0, sHeader.size()), sHeader))
		{
			KStringView sVal = sLine.substr(sHeader.size() + 1);
			while (!sVal.empty() && (sVal.front() == ' ' || sVal.front() == '\t')) sVal.remove_prefix(1);
			out.push_back(KString(sVal));
		}
	}
	return out;
}

//-----------------------------------------------------------------------------
// extract the body (everything after the first blank line)
KString ResponseBody(KStringView sResponse)
//-----------------------------------------------------------------------------
{
	auto iEnd = sResponse.find("\r\n\r\n");
	return (iEnd == KStringView::npos) ? KString{} : KString(sResponse.substr(iEnd + 4));
}

//-----------------------------------------------------------------------------
// pull a single query parameter value out of a URL ("...&name=value&...")
KString URLParam(KStringView sURL, KStringView sName)
//-----------------------------------------------------------------------------
{
	KString sKey = kFormat("{}=", sName);
	auto iPos = sURL.find(sKey);
	if (iPos == KStringView::npos) return {};
	iPos += sKey.size();
	auto iEnd = sURL.find('&', iPos);
	if (iEnd == KStringView::npos) iEnd = sURL.size();
	return KString(sURL.substr(iPos, iEnd - iPos));
}

//-----------------------------------------------------------------------------
// forge an ES256-signed JWT (id_token) using the given EC key
KString MakeES256JWT(const KECKey& Key, KStringView sIssuer, KStringView sSub,
                     KStringView sNonce, int64_t iTTL, KStringView sAudience = "test-client",
                     KStringView sAzp = KStringView{})
//-----------------------------------------------------------------------------
{
	KJSON Header  = { { "alg", "ES256" }, { "kid", "test-ec" }, { "typ", "JWT" } };
	int64_t iNow  = KUnixTime::now().to_time_t();
	KJSON Payload = {
		{ "iss",   KString(sIssuer)   },
		{ "sub",   KString(sSub)      },
		{ "aud",   KString(sAudience) },
		{ "exp",   iNow + iTTL      },
		{ "iat",   iNow             },
		{ "nonce", KString(sNonce)  },
		{ "preferred_username", KString(sSub) }
	};
	if (!sAzp.empty()) Payload["azp"] = KString(sAzp);

	KString sInput = KBase64Url::Encode(Header.dump());
	sInput += '.';
	sInput += KBase64Url::Encode(Payload.dump());

	KECSign Signer;
	KString sSig = Signer.Sign(Key, sInput);

	KString sJWT = sInput;
	sJWT += '.';
	sJWT += KBase64Url::Encode(sSig);
	return sJWT;
}

// shared, mutable state for the mock identity provider
struct MockIdP
{
	KECKey  Key { true };   ///< freshly generated P-256 signing key
	KString sBase;          ///< http://localhost:<port>
	KString sNonce;         ///< nonce to embed into the next issued id_token
	KString sAud { "test-client" }; ///< audience to mint into the next id_token (default = our client_id)
	KString sAzp;           ///< authorized party (azp) to mint; empty = omit the claim
	bool    bOmitExpiresIn { false }; ///< drop "expires_in" from the token response
	int     iExpiresIn        { 3600 };  ///< "expires_in" of the code exchange
	int     iRefreshExpiresIn { 3600 };  ///< "expires_in" of a renewal
	bool    bRotateRefresh    { false }; ///< issue new tokens on every renewal, and accept a refresh_token once
	KDuration RefreshDelay;              ///< time the token endpoint takes for a renewal
	int     iRefreshStatus    { 0 };     ///< answer renewals with this HTTP status instead: 503 as a provider in trouble, 400 with invalid_grant
	std::atomic<int> iRefreshes { 0 };   ///< count of renewals
	int     iExchangeStatus   { 0 };     ///< answer code exchanges with this HTTP status instead
	int     iDiscoveryStatus  { 0 };     ///< answer discovery requests with this HTTP status instead
	std::atomic<int> iDiscoveries { 0 }; ///< count of discovery requests
	std::mutex       UsedMutex;          ///< guards UsedRefreshTokens
	std::set<KString> UsedRefreshTokens; ///< refresh tokens presented so far, with bRotateRefresh
};

//-----------------------------------------------------------------------------
// build the application-side routes that exercise a KOpenIDClient
KRESTRoutes BuildAppRoutes(KOpenIDClient& OIDC)
//-----------------------------------------------------------------------------
{
	KRESTRoutes Routes;

	Routes.AddRoute({ KHTTPMethod::GET, false, "/auth/login", [&OIDC](KRESTServer& http)
	{
		// the return target as an application takes it from its login link
		http.SetRawOutput(OIDC.BuildLoginURL(http, http.GetQueryParmSafe("return", "/")).Serialize());
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/auth/callback", [&OIDC](KRESTServer& http)
	{
		OIDC.HandleCallback(http);
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/protected", [&OIDC](KRESTServer& http)
	{
		KString sUser = OIDC.Authorize(http);
		if (sUser.empty())
		{
			http.SetRawOutput("ANON");
		}
		else
		{
			http.SetRawOutput(kFormat("{}|{}", sUser,
				http.Request.Headers[KHTTPHeader::AUTHORIZATION]));
		}
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/auth/userinfo", [&OIDC](KRESTServer& http)
	{
		OIDC.GetUserInfo(http);
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/auth/logout", [&OIDC](KRESTServer& http)
	{
		OIDC.Logout(http);
	}});

	return Routes;
}

//-----------------------------------------------------------------------------
// drive one HTTP request through the app routes via an in-memory stream
KString Drive(const KRESTRoutes& Routes, KStringView sMethod, KStringView sPathQuery,
              KStringView sCookie)
//-----------------------------------------------------------------------------
{
	KString sRequest = kFormat("{} {} HTTP/1.1\r\nHost: localhost\r\nUser-Agent: utest\r\n",
	                           sMethod, sPathQuery);
	if (!sCookie.empty()) sRequest += kFormat("Cookie: {}\r\n", sCookie);
	sRequest += "\r\n";

	KString sResponse;
	KInStringStream  iss(sRequest);
	KOutStringStream oss(sResponse);
	KStream stream(iss, oss);

	KRESTServer::Options Options;
	// the port here is only metadata: this KRESTServer is driven from the
	// in-memory stream above and never binds a socket
	KRESTServer Server(stream, "127.0.0.1:47853", url::KProtocol::HTTP, 47853, Routes, Options);
	Server.Execute();

	return sResponse;
}

// a tiny cookie jar that mirrors what a browser would keep
struct CookieJar
{
	std::map<KString, KString> Jar;

	void Apply(KStringView sResponse)
	{
		for (const auto& sSetCookie : HeaderValues(sResponse, "Set-Cookie"))
		{
			auto iSemi   = sSetCookie.find(';');
			KStringView sNV = (iSemi == KString::npos) ? KStringView(sSetCookie)
			                                           : KStringView(sSetCookie).substr(0, iSemi);
			auto iEq = sNV.find('=');
			if (iEq == KStringView::npos) continue;

			KString sName (sNV.substr(0, iEq));
			KString sValue(sNV.substr(iEq + 1));

			bool bExpire = sValue.empty() || sSetCookie.find("Max-Age=0") != KString::npos;
			if (bExpire) Jar.erase(sName);
			else         Jar[sName] = sValue;
		}
	}

	KString Header() const
	{
		KString s;
		for (const auto& kv : Jar)
		{
			if (!s.empty()) s += "; ";
			s += kv.first;
			s += '=';
			s += kv.second;
		}
		return s;
	}

	bool Has(KStringView sName) const { return Jar.find(KString(sName)) != Jar.end(); }
};

//-----------------------------------------------------------------------------
// one login through the app routes and the mock provider, up to the response
// of the callback - sReturn is the return target of the login link
KString LoginCallback(const KRESTRoutes& Routes, MockIdP& Mock, KStringView sReturn = "/")
//-----------------------------------------------------------------------------
{
	CookieJar Cookies;

	KString sLogin = Drive(Routes, "GET", kFormat("/auth/login?return={}", KEncode::URL(sReturn)), Cookies.Header());
	Cookies.Apply(sLogin);
	Mock.sNonce    = URLParam(ResponseBody(sLogin), "nonce");
	KString sState = URLParam(ResponseBody(sLogin), "state");

	return Drive(Routes, "GET", kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
}

//-----------------------------------------------------------------------------
// the Set-Cookie value of the cookie sName in a response, empty if none
KString SetCookieOf(KStringView sResponse, KStringView sName)
//-----------------------------------------------------------------------------
{
	for (const auto& sSetCookie : HeaderValues(sResponse, "Set-Cookie"))
	{
		if (sSetCookie.starts_with(kFormat("{}=", sName))) return sSetCookie;
	}
	return {};
}

} // anonymous namespace

//=============================================================================
TEST_CASE("KSession UpdateExtra")
//=============================================================================
{
	SECTION("memory store round-trip")
	{
		KSession::Config cfg;
		cfg.sCookieName = "t";
		cfg.bSecure     = false;

		KSession Session(std::make_unique<KSessionMemoryStore>(), cfg);
		Session.SetAuthenticator([](KStringView, KStringView) { return true; });

		KString sToken = Session.Login("bob", "pw", "1.2.3.4", "ua", R"({"a":1})");
		REQUIRE_FALSE ( sToken.empty() );

		KSession::Record Rec;
		REQUIRE ( Session.Validate(sToken, &Rec) );
		CHECK   ( Rec.sExtra == R"({"a":1})" );

		// the actual update
		CHECK   ( Session.UpdateExtra(sToken, R"({"a":2})") );

		KSession::Record Rec2;
		REQUIRE ( Session.Validate(sToken, &Rec2) );
		CHECK   ( Rec2.sExtra == R"({"a":2})" );

		// other fields are untouched
		CHECK   ( Rec2.sUsername == "bob" );

		// failure modes
		CHECK_FALSE ( Session.UpdateExtra("does-not-exist", "{}") );
		CHECK_FALSE ( Session.UpdateExtra("", "{}") );
	}
}

#if DEKAF2_HAS_SQLITE3

//=============================================================================
TEST_CASE("KOpenIDClient")
//=============================================================================
{
	// ---- spin up a mock OpenID Connect provider ----------------------------
	// a "krummer" high port in the registered range, to minimise the chance of
	// colliding with something already listening on the test machine
	const uint16_t iPort = 47853;

	MockIdP Mock;
	Mock.sBase = kFormat("http://localhost:{}", iPort);

	// publish the EC public key as a JWK
	KString sPubRaw = Mock.Key.GetPublicKeyRaw();
	REQUIRE ( sPubRaw.size() == 65 );
	KJSON JWK = {
		{ "kty", "EC" },
		{ "crv", "P-256" },
		{ "x",   KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32)) },
		{ "y",   KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32)) },
		{ "kid", "test-ec" },
		{ "alg", "ES256" },
		{ "use", "sig" }
	};
	KJSON JWKS; JWKS["keys"] = KJSON::array(); JWKS["keys"].push_back(JWK);

	KJSON Discovery = {
		{ "issuer",                 Mock.sBase             },
		{ "authorization_endpoint", Mock.sBase + "/authorize" },
		{ "token_endpoint",         Mock.sBase + "/token"  },
		{ "end_session_endpoint",   Mock.sBase + "/logout" },
		{ "jwks_uri",               Mock.sBase + "/jwks"   }
	};

	KRESTRoutes MockRoutes;
	MockRoutes.AddRoute({ KHTTPMethod::GET, false, "/.well-known/openid-configuration",
		[&Discovery, &Mock](KRESTServer& http)
	{
		++Mock.iDiscoveries;

		if (Mock.iDiscoveryStatus)
		{
			http.SetStatus(Mock.iDiscoveryStatus);
			http.json.tx = { { "message", "upstream not available" } };
			return;
		}

		http.json.tx = Discovery;
	}});
	MockRoutes.AddRoute({ KHTTPMethod::GET, false, "/jwks",
		[&JWKS](KRESTServer& http) { http.json.tx = JWKS; }});
	MockRoutes.AddRoute({ KHTTPMethod::POST, false, "/token",
		[&Mock](KRESTServer& http)
	{
		bool bRefresh = http.GetRequestBody().contains("grant_type=refresh_token");
		// (route parsed as WWWFORM below, so GetRequestBody() holds the raw form)
		KString sAccess  = bRefresh ? "access-2"  : "access-1";
		KString sRefresh = bRefresh ? "refresh-2" : "refresh-1";

		if (bRefresh)
		{
			auto iRefresh = ++Mock.iRefreshes;

			if (Mock.iRefreshStatus >= 500)
			{
				// the provider restarts, a gateway answers for it
				http.SetStatus(Mock.iRefreshStatus);
				http.json.tx = { { "message", "upstream not available" } };
				return;
			}

			if (Mock.iRefreshStatus >= 400)
			{
				// RFC 6749 5.2: the refresh_token is refused
				http.SetStatus(Mock.iRefreshStatus);
				http.json.tx = { { "error", "invalid_grant" }, { "error_description", "unknown refresh_token" } };
				return;
			}

			if (Mock.bRotateRefresh)
			{
				// a refresh_token is valid once, as with a provider that rotates them
				{
					std::lock_guard<std::mutex> Lock(Mock.UsedMutex);

					if (!Mock.UsedRefreshTokens.insert(http.GetQueryParm("refresh_token")).second)
					{
						throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid_grant" };
					}
				}

				sAccess  = kFormat("access-{}" , iRefresh + 1);
				sRefresh = kFormat("refresh-{}", iRefresh + 1);
			}

			if (Mock.RefreshDelay > KDuration::zero())
			{
				kSleep(Mock.RefreshDelay);
			}
		}
		else if (Mock.iExchangeStatus)
		{
			http.SetStatus(Mock.iExchangeStatus);
			http.json.tx = { { "message", "upstream not available" } };
			return;
		}

		http.json.tx = {
			{ "access_token",  sAccess  },
			{ "refresh_token", sRefresh },
			{ "id_token",      MakeES256JWT(Mock.Key, Mock.sBase, "alice", Mock.sNonce, 3600, Mock.sAud, Mock.sAzp) },
			{ "token_type",    "Bearer" }
		};
		if (!Mock.bOmitExpiresIn) http.json.tx["expires_in"] = bRefresh ? Mock.iRefreshExpiresIn : Mock.iExpiresIn;
	}, KRESTRoute::WWWFORM });

	KREST::Options MockOpt;
	MockOpt.Type                 = KREST::HTTP;
	MockOpt.iPort                = iPort;
	MockOpt.bBlocking            = false;
	MockOpt.bCreateEphemeralCert = false;
	MockOpt.Timeout              = chrono::seconds(5);

	KREST MockServer;
	REQUIRE ( MockServer.Execute(MockOpt, MockRoutes) );

	// ---- a KOpenIDClient pointed at the mock provider ----------------------
	KOpenIDClient::Config cfg;
	cfg.sAuthority         = Mock.sBase;
	cfg.sClientID          = "test-client";
	cfg.sClientSecret      = "test-secret";
	cfg.sCallbackURI       = "http://localhost/auth/callback";
	cfg.sPostLogoutURI     = "http://localhost/";
	cfg.sScope             = "openid profile offline_access";
	cfg.sSessionCookieName = "session";
	// the mock provider speaks plain HTTP; KOpenIDProvider (and therefore KJWT)
	// require HTTPS, so we disable signature verification for the flow tests and
	// cover the secure default separately (see the "fails closed" section).
	cfg.bVerifyIDTokenSignature = false;

	KString sDB = kGenerateTempPath(".sqlite");
	KOpenIDClient OIDC(cfg, sDB);
	KRESTRoutes AppRoutes = BuildAppRoutes(OIDC);

	SECTION("full authorization-code + PKCE login flow")
	{
		CookieJar Cookies;

		// 1) login: build the auth URL + set the state cookie
		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);

		KString sState = URLParam(ResponseBody(sLogin), "state");
		KString sNonce = URLParam(ResponseBody(sLogin), "nonce");
		REQUIRE_FALSE ( sState.empty() );
		REQUIRE_FALSE ( sNonce.empty() );
		REQUIRE       ( Cookies.Has("session_state") );

		// the provider will mint an id_token carrying exactly this nonce
		Mock.sNonce = sNonce;

		// 2) callback: exchange code -> tokens, validate id_token, set session
		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);

		CHECK ( sCallback.contains("302") );
		CHECK ( Cookies.Has("session") );
		CHECK_FALSE ( Cookies.Has("session_state") ); // consumed + expired

		// 3) a protected request authorizes via the session cookie
		KString sProtected = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("alice|access-1") ); // no refresh yet

		// 4) userinfo exposes safe data, no tokens
		KString sUserInfo = Drive(AppRoutes, "GET", "/auth/userinfo", Cookies.Header());
		KJSON jUser = kjson::Parse(ResponseBody(sUserInfo));
		CHECK ( kjson::GetBool(jUser, "logged_in") == true );
		CHECK ( kjson::GetString(jUser, "username") == "alice" );
		CHECK_FALSE ( sUserInfo.contains("access-1") ); // tokens never leak

		// 5) logout clears the session
		KString sLogout = Drive(AppRoutes, "GET", "/auth/logout", Cookies.Header());
		Cookies.Apply(sLogout);
		CHECK ( sLogout.contains("302") );
		CHECK_FALSE ( Cookies.Has("session") );

		KString sAfter = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sAfter.contains("ANON") );
	}

	SECTION("the return target is a path on this server only")
	{
		CHECK       ( KOpenIDClient::IsLocalReturnTarget("/")                          );
		CHECK       ( KOpenIDClient::IsLocalReturnTarget("/xy/?ab=Some%20Thing#here")  );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("")                           );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("xy/")                        );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("https://evil.example/")      );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("//evil.example/")            );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("/\\evil.example/")           );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("/a\\b")                      );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("/a\r\nSet-Cookie: x=1")      );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget(KStringView("/a\0b", 4))      );
		CHECK_FALSE ( KOpenIDClient::IsLocalReturnTarget("/a\x7f")                     );

		// after the login, a local target is kept, anything else becomes "/"
		auto Location = [&](KStringView sReturn)
		{
			auto Values = HeaderValues(LoginCallback(AppRoutes, Mock, sReturn), "Location");
			return Values.empty() ? KString{} : Values.front();
		};

		CHECK ( Location("/xy/?ab=Calm")          == "/xy/?ab=Calm" );
		CHECK ( Location("https://evil.example/") == "/" );
		CHECK ( Location("//evil.example/")       == "/" );
		CHECK ( Location("")                      == "/" );
	}

	SECTION("the session cookie is a browser-session cookie by default, persistent on request")
	{
		KString sCallback = LoginCallback(AppRoutes, Mock);

		KString sCookie = SetCookieOf(sCallback, "session");
		KString sHint   = SetCookieOf(sCallback, "session_hint");
		REQUIRE_FALSE ( sCookie.empty() );
		REQUIRE_FALSE ( sHint.empty()   );
		CHECK_FALSE   ( sCookie.contains("Max-Age") );
		CHECK_FALSE   ( sHint.contains("Max-Age")   );

		KOpenIDClient::Config cfgKeep = cfg;
		cfgKeep.bPersistentCookie     = true;
		cfgKeep.AbsoluteTimeout       = chrono::hours(24 * 30);
		cfgKeep.sSessionCookieName    = "session_keep";

		KString sDBk = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDCKeep(cfgKeep, sDBk);
		KRESTRoutes AppRoutesKeep = BuildAppRoutes(OIDCKeep);

		sCallback = LoginCallback(AppRoutesKeep, Mock);

		sCookie = SetCookieOf(sCallback, "session_keep");
		sHint   = SetCookieOf(sCallback, "session_keep_hint");
		REQUIRE_FALSE ( sCookie.empty() );
		REQUIRE_FALSE ( sHint.empty()   );
		CHECK ( sCookie.contains("; Max-Age=2592000") );
		CHECK ( sHint.contains("; Max-Age=2592000")   );
		CHECK ( sCookie.contains("HttpOnly")          );

		// the logout still deletes it
		CookieJar Cookies;
		Cookies.Apply(sCallback);
		KString sLogout = Drive(AppRoutesKeep, "GET", "/auth/logout", Cookies.Header());
		CHECK ( SetCookieOf(sLogout, "session_keep").contains("Max-Age=0") );
	}

	SECTION("a persistent cookie without absolute timeout ends with the browser session")
	{
		KOpenIDClient::Config cfgOpen = cfg;
		cfgOpen.bPersistentCookie     = true;
		cfgOpen.AbsoluteTimeout       = KDuration::zero();
		cfgOpen.sSessionCookieName    = "session_open";

		KString sDBo = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDCOpen(cfgOpen, sDBo);
		KRESTRoutes AppRoutesOpen = BuildAppRoutes(OIDCOpen);

		KString sCallback = LoginCallback(AppRoutesOpen, Mock);

		KString sCookie = SetCookieOf(sCallback, "session_open");
		KString sHint   = SetCookieOf(sCallback, "session_open_hint");
		REQUIRE_FALSE ( sCookie.empty() );
		REQUIRE_FALSE ( sHint.empty()   );
		CHECK_FALSE   ( sCookie.contains("Max-Age") );
		CHECK_FALSE   ( sHint.contains("Max-Age")   );
	}

	SECTION("a shared KSession decides about a persistent cookie")
	{
		KSession::Config sc;
		sc.sCookieName       = "shared";
		sc.bSecure           = true;
		sc.bPersistentCookie = true;
		sc.AbsoluteTimeout   = chrono::hours(24 * 7);

		auto pSession = std::make_shared<KSession>(std::make_unique<KSessionMemoryStore>(), sc);

		// the client's own flag does not count - the app's session is the authority
		KOpenIDClient::Config cfgShared = cfg;
		cfgShared.bPersistentCookie     = false;

		KOpenIDClient OIDCShared(cfgShared, pSession);
		KRESTRoutes AppRoutesShared = BuildAppRoutes(OIDCShared);

		KString sCookie = SetCookieOf(LoginCallback(AppRoutesShared, Mock), "shared");
		REQUIRE_FALSE ( sCookie.empty() );
		CHECK ( sCookie.contains("; Max-Age=604800") );
	}

	SECTION("silent token refresh on near-expiry")
	{
		// a second client whose leeway is so large that any token counts as
		// near-expiry, forcing a refresh on the very first protected request
		KOpenIDClient::Config cfg2 = cfg;
		cfg2.ExpiryLeeway        = chrono::hours(2);
		cfg2.sSessionCookieName  = "session2";

		KString sDB2 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC2(cfg2, sDB2);
		KRESTRoutes AppRoutes2 = BuildAppRoutes(OIDC2);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes2, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes2, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session2") );

		// first protected request triggers a refresh_token exchange -> access-2
		KString sProtected = Drive(AppRoutes2, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("alice|access-2") );
	}

	SECTION("logout names the client, also without a session")
	{
		// after the session here is gone (the provider lost our refresh token, say)
		// there is no id_token_hint - the provider then knows the client, and with
		// it the registered post_logout_redirect_uri, by the client_id alone
		KString sLogout = Drive(AppRoutes, "GET", "/auth/logout", "");
		CHECK ( sLogout.contains("302") );
		// the Location header of the redirect
		CHECK ( sLogout.contains("/logout?client_id=test-client&") );
		CHECK ( sLogout.contains("post_logout_redirect_uri=") );
		CHECK_FALSE ( sLogout.contains("id_token_hint=") );
	}

	SECTION("a provider that does not answer a renewal keeps the session")
	{
		KOpenIDClient::Config cfg3 = cfg;
		cfg3.ExpiryLeeway        = chrono::hours(2);   // every request is due for renewal
		cfg3.RenewalRetry        = KDuration::zero();
		cfg3.sSessionCookieName  = "session3";

		KString sDB3 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC3(cfg3, sDB3);
		KRESTRoutes AppRoutes3 = BuildAppRoutes(OIDC3);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes3, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes3, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session3") );

		// the provider restarts: this request is not authorized, but the session stays
		Mock.iRefreshStatus = 503;
		KString sDown = Drive(AppRoutes3, "GET", "/protected", Cookies.Header());
		Cookies.Apply(sDown);
		CHECK ( sDown.contains("ANON") );
		CHECK ( Cookies.Has("session3") );

		// it is back: the same session renews
		Mock.iRefreshStatus = 0;
		KString sUp = Drive(AppRoutes3, "GET", "/protected", Cookies.Header());
		CHECK ( sUp.contains("alice|access-2") );

		kRemoveFile(sDB3);
	}

	SECTION("after a provider did not answer, the next renewal waits")
	{
		KOpenIDClient::Config cfg4 = cfg;
		cfg4.ExpiryLeeway        = chrono::hours(2);
		cfg4.RenewalRetry        = chrono::hours(1);
		cfg4.sSessionCookieName  = "session4";

		KString sDB4 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC4(cfg4, sDB4);
		KRESTRoutes AppRoutes4 = BuildAppRoutes(OIDC4);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes4, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes4, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session4") );

		Mock.iRefreshStatus = 503;
		int iBefore = Mock.iRefreshes;
		CHECK ( Drive(AppRoutes4, "GET", "/protected", Cookies.Header()).contains("ANON") );
		CHECK ( Mock.iRefreshes == iBefore + 1 );

		// not every request of every session asks a provider that is down
		CHECK ( Drive(AppRoutes4, "GET", "/protected", Cookies.Header()).contains("ANON") );
		CHECK ( Mock.iRefreshes == iBefore + 1 );

		Mock.iRefreshStatus = 0;
		kRemoveFile(sDB4);
	}

	SECTION("a provider that does not answer the discovery is not asked again at once")
	{
		KOpenIDClient::Config cfg6 = cfg;
		cfg6.RenewalRetry        = chrono::hours(1);
		cfg6.sSessionCookieName  = "session6";

		KString sDB6 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC6(cfg6, sDB6);
		KRESTRoutes AppRoutes6 = BuildAppRoutes(OIDC6);

		Mock.iDiscoveryStatus = 503;
		int iBefore = Mock.iDiscoveries;

		CHECK ( OIDC6.GetCircuitBreaker().GetState() == KCircuitBreaker::State::Closed );

		// no login URL without the endpoints of the provider
		CHECK ( ResponseBody(Drive(AppRoutes6, "GET", "/auth/login", "")).empty() );
		CHECK ( Mock.iDiscoveries == iBefore + 1 );

		// not every login asks a provider that is down
		CHECK ( ResponseBody(Drive(AppRoutes6, "GET", "/auth/login", "")).empty() );
		CHECK ( Mock.iDiscoveries == iBefore + 1 );

		// a logout neither: it ends at the post logout URI of the application
		KString sLogout = Drive(AppRoutes6, "GET", "/auth/logout", "");
		CHECK ( sLogout.contains("302") );
		CHECK_FALSE ( sLogout.contains("client_id=") );
		CHECK ( Mock.iDiscoveries == iBefore + 1 );

		// the application can read the breaker for the provider
		CHECK ( OIDC6.GetCircuitBreaker().GetState()         == KCircuitBreaker::State::Open );
		CHECK ( OIDC6.GetCircuitBreaker().GetOpenings()      == 1 );
		CHECK ( OIDC6.GetCircuitBreaker().GetRefused()       == 2 );
		CHECK ( OIDC6.GetCircuitBreaker().GetOptions().sName == cfg6.sAuthority );

		Mock.iDiscoveryStatus = 0;
		kRemoveFile(sDB6);
	}

	SECTION("the discovery is asked again after Config::RenewalRetry")
	{
		KOpenIDClient::Config cfg7 = cfg;
		cfg7.RenewalRetry        = KDuration::zero();
		cfg7.sSessionCookieName  = "session7";

		KString sDB7 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC7(cfg7, sDB7);
		KRESTRoutes AppRoutes7 = BuildAppRoutes(OIDC7);

		Mock.iDiscoveryStatus = 503;
		CHECK ( ResponseBody(Drive(AppRoutes7, "GET", "/auth/login", "")).empty() );

		// the provider is back
		Mock.iDiscoveryStatus = 0;
		CHECK_FALSE ( URLParam(ResponseBody(Drive(AppRoutes7, "GET", "/auth/login", "")), "state").empty() );

		kRemoveFile(sDB7);
	}

	SECTION("a code exchange is sent also after the provider did not answer")
	{
		KOpenIDClient::Config cfg8 = cfg;
		cfg8.ExpiryLeeway        = chrono::hours(2);
		cfg8.RenewalRetry        = chrono::hours(1);
		cfg8.sSessionCookieName  = "session8";

		KString sDB8 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC8(cfg8, sDB8);
		KRESTRoutes AppRoutes8 = BuildAppRoutes(OIDC8);

		CookieJar Cookies;
		Cookies.Apply(LoginCallback(AppRoutes8, Mock));
		REQUIRE ( Cookies.Has("session8") );

		// the renewal gets no answer, from now on renewals wait
		Mock.iRefreshStatus = 503;
		CHECK ( Drive(AppRoutes8, "GET", "/protected", Cookies.Header()).contains("ANON") );
		Mock.iRefreshStatus = 0;

		// a browser that comes back from the provider with a code has reached it,
		// and the code is good only once: this login is not refused
		CHECK_FALSE ( SetCookieOf(LoginCallback(AppRoutes8, Mock), "session8").empty() );

		kRemoveFile(sDB8);
	}

	SECTION("a code exchange without an answer makes the renewals wait")
	{
		KOpenIDClient::Config cfg9 = cfg;
		cfg9.ExpiryLeeway        = chrono::hours(2);
		cfg9.RenewalRetry        = chrono::hours(1);
		cfg9.sSessionCookieName  = "session9";

		KString sDB9 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC9(cfg9, sDB9);
		KRESTRoutes AppRoutes9 = BuildAppRoutes(OIDC9);

		CookieJar Cookies;
		Cookies.Apply(LoginCallback(AppRoutes9, Mock));
		REQUIRE ( Cookies.Has("session9") );

		// another login fails at the provider
		Mock.iExchangeStatus = 503;
		CHECK ( SetCookieOf(LoginCallback(AppRoutes9, Mock), "session9").empty() );
		Mock.iExchangeStatus = 0;

		// the renewal of the first session does not ask the provider, and keeps the session
		int iBefore = Mock.iRefreshes;
		KString sProtected = Drive(AppRoutes9, "GET", "/protected", Cookies.Header());
		Cookies.Apply(sProtected);
		CHECK ( sProtected.contains("ANON") );
		CHECK ( Mock.iRefreshes == iBefore );
		CHECK ( Cookies.Has("session9") );

		kRemoveFile(sDB9);
	}

	SECTION("a refused refresh_token ends the session")
	{
		KOpenIDClient::Config cfg5 = cfg;
		cfg5.ExpiryLeeway        = chrono::hours(2);
		cfg5.sSessionCookieName  = "session5";

		KString sDB5 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC5(cfg5, sDB5);
		KRESTRoutes AppRoutes5 = BuildAppRoutes(OIDC5);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes5, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes5, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session5") );

		// the provider does not know the refresh_token any more: sign in again
		Mock.iRefreshStatus = 400;
		KString sRefused = Drive(AppRoutes5, "GET", "/protected", Cookies.Header());
		Cookies.Apply(sRefused);
		CHECK ( sRefused.contains("ANON") );
		CHECK_FALSE ( Cookies.Has("session5") );

		Mock.iRefreshStatus = 0;
		kRemoveFile(sDB5);
	}

	SECTION("missing expires_in defaults to Config::DefaultAccessTTL (no spurious refresh)")
	{
		// A provider that omits the OPTIONAL "expires_in" must not make the
		// session look expired-on-arrival. With the default leeway (10 s) and
		// DefaultAccessTTL (1 h) the token is comfortably valid, so the protected
		// request serves the original access-1 without a refresh_token exchange.
		// (Before the fix the expiry collapsed to "now" and this returned access-2.)
		Mock.bOmitExpiresIn = true;

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session") );

		KString sProtected = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("alice|access-1") );
	}

	SECTION("default leeway: a 5 minute access token is used without renewal")
	{
		// KOpenIDServer issues access tokens for 5 minutes
		Mock.iExpiresIn = 300;

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session") );

		CHECK ( Drive(AppRoutes, "GET", "/protected", Cookies.Header()).contains("alice|access-1") );
		CHECK ( Drive(AppRoutes, "GET", "/protected", Cookies.Header()).contains("alice|access-1") );
		CHECK ( Mock.iRefreshes == 0 );
	}

	SECTION("concurrent requests of a session share one renewal")
	{
		// the access token expires within the leeway, the renewed one does not. The
		// provider accepts a refresh_token once, and takes its time for the renewal,
		// so that the requests below overlap with it
		Mock.iExpiresIn        = 5;
		Mock.iRefreshExpiresIn = 3600;
		Mock.bRotateRefresh    = true;
		Mock.RefreshDelay      = chrono::milliseconds(300);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);
		REQUIRE ( Cookies.Has("session") );

		KString sCookie = Cookies.Header();
		std::vector<KString> Responses(8);
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < Responses.size(); ++i)
		{
			Threads.emplace_back([&, i]()
			{
				Responses[i] = Drive(AppRoutes, "GET", "/protected", sCookie);
			});
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		for (const auto& sResponse : Responses)
		{
			CHECK ( sResponse.contains("alice|access-2") );
		}

		CHECK ( Mock.iRefreshes == 1 );

		// the session keeps the renewed tokens
		CHECK ( Drive(AppRoutes, "GET", "/protected", sCookie).contains("alice|access-2") );
		CHECK ( Mock.iRefreshes == 1 );
	}

	SECTION("concurrent requests while the endpoints of the provider are loaded")
	{
		// endpoints that expire at once: every request loads them again, while the
		// other requests use them
		KOpenIDClient::Config cfg10 = cfg;
		cfg10.DiscoveryRefresh    = KDuration::zero();
		cfg10.sSessionCookieName  = "session10";

		KString sDB10 = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDC10(cfg10, sDB10);
		KRESTRoutes AppRoutes10 = BuildAppRoutes(OIDC10);

		std::vector<std::vector<KString>> Responses(8);
		std::vector<std::thread> Threads;

		for (std::size_t i = 0; i < Responses.size(); ++i)
		{
			Threads.emplace_back([&, i]()
			{
				for (int iRound = 0; iRound < 4; ++iRound)
				{
					Responses[i].push_back(Drive(AppRoutes10, "GET", "/auth/login",  ""));
					Responses[i].push_back(Drive(AppRoutes10, "GET", "/auth/logout", ""));
				}
			});
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		for (const auto& ThreadResponses : Responses)
		{
			for (const auto& sResponse : ThreadResponses)
			{
				// the login URL, or the redirect of the logout to the provider
				bool bLogin  = ResponseBody(sResponse).starts_with(kFormat("{}/authorize?", Mock.sBase));
				bool bLogout = sResponse.contains(kFormat("{}/logout?client_id=test-client&", Mock.sBase));
				CHECK ( (bLogin || bLogout) );
			}
		}

		kRemoveFile(sDB10);
	}

	SECTION("callback is rejected on id_token nonce mismatch")
	{
		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		KString sState = URLParam(ResponseBody(sLogin), "state");

		// provider mints an id_token with the WRONG nonce
		Mock.sNonce = "this-is-not-the-expected-nonce";

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);

		// no session must have been established
		CHECK_FALSE ( Cookies.Has("session") );

		KString sProtected = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("ANON") );
	}

	SECTION("callback is rejected on id_token audience mismatch (cross-client replay)")
	{
		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		KString sState = URLParam(ResponseBody(sLogin), "state");
		// keep the nonce CORRECT so that audience is the only reason to reject
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");

		// the provider mints an id_token for a DIFFERENT client (aud != our client_id)
		Mock.sAud = "some-other-client";

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);

		// the audience check must reject it: no session may be established
		CHECK_FALSE ( Cookies.Has("session") );

		KString sProtected = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("ANON") );
	}

	SECTION("callback is rejected on id_token azp mismatch")
	{
		CookieJar Cookies;

		KString sLogin = Drive(AppRoutes, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		KString sState = URLParam(ResponseBody(sLogin), "state");
		Mock.sNonce = URLParam(ResponseBody(sLogin), "nonce");

		// aud is correct (our client_id), but azp names a DIFFERENT authorized party,
		// so azp is the only reason to reject -> proves the azp check is enforced
		Mock.sAud = "test-client";
		Mock.sAzp = "some-other-client";

		KString sCallback = Drive(AppRoutes, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);

		CHECK_FALSE ( Cookies.Has("session") );

		KString sProtected = Drive(AppRoutes, "GET", "/protected", Cookies.Header());
		CHECK ( sProtected.contains("ANON") );
	}

	SECTION("secure default rejects an unverifiable id_token (fails closed)")
	{
		// a client with the production default: signature verification ON.
		// Against a plain-HTTP provider KJWT/KOpenIDProvider must refuse the
		// id_token, so no session may be established.
		KOpenIDClient::Config cfgSecure   = cfg;
		cfgSecure.bVerifyIDTokenSignature = true;          // production default
		cfgSecure.sSessionCookieName      = "session_sec";

		KString sDBs = kGenerateTempPath(".sqlite");
		KOpenIDClient OIDCSecure(cfgSecure, sDBs);
		KRESTRoutes AppRoutesSecure = BuildAppRoutes(OIDCSecure);

		CookieJar Cookies;

		KString sLogin = Drive(AppRoutesSecure, "GET", "/auth/login", Cookies.Header());
		Cookies.Apply(sLogin);
		Mock.sNonce    = URLParam(ResponseBody(sLogin), "nonce");
		KString sState = URLParam(ResponseBody(sLogin), "state");

		KString sCallback = Drive(AppRoutesSecure, "GET",
			kFormat("/auth/callback?code=any-code&state={}", sState), Cookies.Header());
		Cookies.Apply(sCallback);

		CHECK_FALSE ( Cookies.Has("session_sec") );
	}
}

#endif // DEKAF2_HAS_SQLITE3
