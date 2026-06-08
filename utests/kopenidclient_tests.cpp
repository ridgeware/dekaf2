#include "catch.hpp"

#include <dekaf2/crypto/auth/kopenidclient.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kcaseless.h>   // kCaselessEqual
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <map>
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
};

//-----------------------------------------------------------------------------
// build the application-side routes that exercise a KOpenIDClient
KRESTRoutes BuildAppRoutes(KOpenIDClient& OIDC)
//-----------------------------------------------------------------------------
{
	KRESTRoutes Routes;

	Routes.AddRoute({ KHTTPMethod::GET, false, "/auth/login", [&OIDC](KRESTServer& http)
	{
		http.SetRawOutput(OIDC.BuildLoginURL(http, "/").Serialize());
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
		[&Discovery](KRESTServer& http) { http.json.tx = Discovery; }});
	MockRoutes.AddRoute({ KHTTPMethod::GET, false, "/jwks",
		[&JWKS](KRESTServer& http) { http.json.tx = JWKS; }});
	MockRoutes.AddRoute({ KHTTPMethod::POST, false, "/token",
		[&Mock](KRESTServer& http)
	{
		bool bRefresh = http.GetRequestBody().contains("grant_type=refresh_token");
		// (route parsed as WWWFORM below, so GetRequestBody() holds the raw form)
		http.json.tx = {
			{ "access_token",  bRefresh ? "access-2"  : "access-1"  },
			{ "refresh_token", bRefresh ? "refresh-2" : "refresh-1" },
			{ "id_token",      MakeES256JWT(Mock.Key, Mock.sBase, "alice", Mock.sNonce, 3600, Mock.sAud, Mock.sAzp) },
			{ "token_type",    "Bearer" }
		};
		if (!Mock.bOmitExpiresIn) http.json.tx["expires_in"] = 3600;
	}, KRESTRoute::WWWFORM });

	KREST::Options MockOpt;
	MockOpt.Type                 = KREST::HTTP;
	MockOpt.iPort                = iPort;
	MockOpt.bBlocking            = false;
	MockOpt.bCreateEphemeralCert = false;
	MockOpt.iTimeout             = 5;

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

	SECTION("missing expires_in defaults to Config::DefaultAccessTTL (no spurious refresh)")
	{
		// A provider that omits the OPTIONAL "expires_in" must not make the
		// session look expired-on-arrival. With the default leeway (5 min) and
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
