#include "catch.hpp"

#include <dekaf2/crypto/auth/kopenidserver.h>
#include <dekaf2/crypto/auth/kopenid.h>            // KOpenIDKeys (validate the issued id_token)
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>     // KSHA256 (PKCE)
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kcaseless.h>         // kCaselessEqual
#include <map>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
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
KString FirstHeader(KStringView sResponse, KStringView sHeader)
//-----------------------------------------------------------------------------
{
	auto v = HeaderValues(sResponse, sHeader);
	return v.empty() ? KString{} : v.front();
}

//-----------------------------------------------------------------------------
KString ResponseBody(KStringView sResponse)
//-----------------------------------------------------------------------------
{
	auto iEnd = sResponse.find("\r\n\r\n");
	return (iEnd == KStringView::npos) ? KString{} : KString(sResponse.substr(iEnd + 4));
}

//-----------------------------------------------------------------------------
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

// ---- in-test stores -------------------------------------------------------

struct TestUserStore : KOpenIDServer::UserStore
{
	KString sAliceHash;        // bcrypt of "secret"
	bool    bDenyAccess { false }; // make AuthorizeClientAccess deny
	KJSON   ClientClaims;      // client-scoped claims to return (e.g. {"roles":[...]})

	bool VerifyPassword(KStringView user, KStringView pass) override
	{
		return user == "alice" && KBCrypt().ValidatePassword(KString(pass), sAliceHash);
	}
	bool GetClaims(KStringView user, KJSON& Claims) override
	{
		if (user != "alice") return false;
		Claims = { { "name", "Alice Example" }, { "email", "alice@example.com" } };
		return true;
	}
	bool AuthorizeClientAccess(KStringView /*user*/, KStringView /*clientID*/, KJSON& jClientClaims) override
	{
		if (bDenyAccess) return false;
		if (ClientClaims.is_object()) jClientClaims = ClientClaims;
		return true;
	}
};

struct TestClientStore : KOpenIDServer::ClientStore
{
	KString   sSecretHash;                  // bcrypt of "test-secret"
	bool      bForceLogin { false };        // re-auth policy: always prompt
	KDuration MaxAuthAge  { KDuration::zero() }; // re-auth policy: max OP-session age

	bool Lookup(KStringView clientID, Client& Out) override
	{
		if (clientID != "test-client") return false;
		Out.sClientID              = "test-client";
		Out.sClientSecretHash      = sSecretHash;
		Out.RedirectURIs           = { "http://localhost/cb" };
		Out.PostLogoutRedirectURIs = { "http://localhost/after-logout" };
		Out.Scopes                 = { "openid", "profile", "email" };
		Out.bPublic                = false;
		Out.bForceLogin            = bForceLogin;
		Out.MaxAuthAge             = MaxAuthAge;
		return true;
	}
};

struct TestGrantStore : KOpenIDServer::GrantStore
{
	std::mutex                              Mutex;
	std::unordered_map<KString, Code>       Codes;
	std::unordered_map<KString, Refresh>    Refreshes;

	bool SaveCode(const Code& Rec) override
	{
		std::lock_guard<std::mutex> L(Mutex);
		Codes[Rec.sCode] = Rec;
		return true;
	}
	bool TakeCode(KStringView sCode, Code& Out) override
	{
		std::lock_guard<std::mutex> L(Mutex);
		auto it = Codes.find(KString(sCode));
		if (it == Codes.end()) return false;
		Out = it->second;
		Codes.erase(it);
		return true;
	}
	bool SaveRefresh(const Refresh& Rec) override
	{
		std::lock_guard<std::mutex> L(Mutex);
		Refreshes[Rec.sToken] = Rec;
		return true;
	}
	bool TakeRefresh(KStringView sToken, Refresh& Out) override
	{
		std::lock_guard<std::mutex> L(Mutex);
		auto it = Refreshes.find(KString(sToken));
		if (it == Refreshes.end()) return false;
		Out = it->second;
		Refreshes.erase(it);
		return true;
	}
	void PurgeExpired(KUnixTime) override {}
};

// a browser-ish cookie jar
struct CookieJar
{
	std::map<KString, KString> Jar;
	void Apply(KStringView sResponse)
	{
		for (const auto& sSet : HeaderValues(sResponse, "Set-Cookie"))
		{
			auto iSemi = sSet.find(';');
			KStringView sNV = (iSemi == KString::npos) ? KStringView(sSet) : KStringView(sSet).substr(0, iSemi);
			auto iEq = sNV.find('=');
			if (iEq == KStringView::npos) continue;
			KString sName(sNV.substr(0, iEq));
			KString sVal (sNV.substr(iEq + 1));
			if (sVal.empty() || sSet.find("Max-Age=0") != KString::npos) Jar.erase(sName);
			else Jar[sName] = sVal;
		}
	}
	KString Header() const
	{
		KString s;
		for (const auto& kv : Jar) { if (!s.empty()) s += "; "; s += kv.first; s += '='; s += kv.second; }
		return s;
	}
};

} // anonymous namespace

//=============================================================================
TEST_CASE("KOpenIDServer")
//=============================================================================
{
	const KString sIssuer = "http://localhost:48999";

	// --- signing key + stores ----------------------------------------------
	KRSAKey SigningKey(2048);
	REQUIRE_FALSE ( SigningKey.empty() );

	KBCrypt Bcrypt(static_cast<uint16_t>(6)); // low workload -> fast test

	auto pUsers   = std::make_shared<TestUserStore>();
	pUsers->sAliceHash = Bcrypt.GenerateHash("secret");
	auto pClients = std::make_shared<TestClientStore>();
	pClients->sSecretHash = Bcrypt.GenerateHash("test-secret");
	auto pGrants  = std::make_shared<TestGrantStore>();

	KSession::Config sc;
	sc.sCookieName = "op_session";
	sc.bSecure     = false;
	auto pSession  = std::make_shared<KSession>(std::make_unique<KSessionMemoryStore>(), sc);

	KOpenIDServer::Config cfg;
	cfg.sIssuer = sIssuer;
	cfg.sKid    = "key-1";

	KOpenIDServer Server(cfg, pSession, pUsers, pClients, pGrants, std::move(SigningKey));

	// --- OP routes ----------------------------------------------------------
	KRESTRoutes Routes;
	Routes.AddRoute({ KHTTPMethod::GET,  false, "/.well-known/openid-configuration",
		[&Server](KRESTServer& h){ Server.ServeDiscovery(h); }});
	Routes.AddRoute({ KHTTPMethod::GET,  false, "/jwks",      [&Server](KRESTServer& h){ Server.ServeJWKS(h); }});
	Routes.AddRoute({ KHTTPMethod::GET,  false, "/authorize", [&Server](KRESTServer& h){ Server.HandleAuthorize(h); }});
	Routes.AddRoute({ KHTTPMethod::GET,  false, "/userinfo",  [&Server](KRESTServer& h){ Server.HandleUserInfo(h); }});
	Routes.AddRoute({ KHTTPMethod::GET,  false, "/logout",    [&Server](KRESTServer& h){ Server.HandleLogout(h); }});
	Routes.AddRoute({ KHTTPMethod::POST, false, "/token",     [&Server](KRESTServer& h){ Server.HandleToken(h); }, KRESTRoute::WWWFORM });
	// the application owns the login UI; here a minimal POST handler that
	// verifies the password and resumes the pending authorize
	Routes.AddRoute({ KHTTPMethod::POST, false, "/login", [&Server](KRESTServer& h)
	{
		const auto& Q = h.GetQueryParms();
		if (!Server.VerifyPassword(Q["username"], Q["password"]))
		{
			h.SetStatus(401);
			h.SetRawOutput("login failed");
			return;
		}
		h.Response.Headers.Set(KHTTPHeader::LOCATION, Server.CompleteLogin(h, Q["username"]));
		throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
	}, KRESTRoute::WWWFORM });
	// the application owns the access-denied screen too; minimal handlers to
	// exercise the interstitial helpers (PendingClientID / DeclineAccess)
	Routes.AddRoute({ KHTTPMethod::GET, false, "/no-access", [&Server](KRESTServer& h)
	{
		h.SetRawOutput(Server.PendingClientID(h)); // body = the parked client_id (or empty)
	}});
	Routes.AddRoute({ KHTTPMethod::POST, false, "/no-access/decline", [&Server](KRESTServer& h)
	{
		h.Response.Headers.Set(KHTTPHeader::LOCATION, Server.DeclineAccess(h));
		throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
	}, KRESTRoute::WWWFORM });

	// --- a stream-driven request helper (no socket needed: the OP never calls out) ---
	auto Drive = [&Routes](KStringView sMethod, KStringView sPathQuery, KStringView sCookie,
	                       KStringView sFormBody = {}) -> KString
	{
		KString sReq = kFormat("{} {} HTTP/1.1\r\nHost: localhost\r\nUser-Agent: utest\r\n", sMethod, sPathQuery);
		if (!sCookie.empty()) sReq += kFormat("Cookie: {}\r\n", sCookie);
		if (!sFormBody.empty())
		{
			sReq += "Content-Type: application/x-www-form-urlencoded\r\n";
			sReq += kFormat("Content-Length: {}\r\n", sFormBody.size());
		}
		sReq += "\r\n";
		sReq += sFormBody;

		KString sResp;
		KInStringStream  iss(sReq);
		KOutStringStream oss(sResp);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		KRESTServer Srv(stream, "127.0.0.1:48999", url::KProtocol::HTTP, 48999, Routes, Options);
		Srv.Execute();
		return sResp;
	};

	// --- PKCE material (the relying party would generate this) --------------
	KString sVerifier  = KBase64Url::Encode(kGetRandom(32));
	KString sChallenge = KBase64Url::Encode(KSHA256(sVerifier).Digest());

	KString sAuthorizeQuery = kFormat(
		"/authorize?response_type=code&client_id=test-client"
		"&redirect_uri=http://localhost/cb&scope=openid%20profile"
		"&state=the-state&nonce=the-nonce&code_challenge={}&code_challenge_method=S256",
		sChallenge);

	SECTION("discovery + jwks")
	{
		KJSON jDisc = kjson::Parse(ResponseBody(Drive("GET", "/.well-known/openid-configuration", "")));
		CHECK ( kjson::GetStringRef(jDisc, "issuer")        == sIssuer );
		CHECK ( kjson::GetStringRef(jDisc, "token_endpoint")== sIssuer + "/token" );

		KJSON jJWKS = kjson::Parse(ResponseBody(Drive("GET", "/jwks", "")));
		REQUIRE ( jJWKS["keys"].is_array() );
		REQUIRE ( jJWKS["keys"].size() == 1 );
		CHECK ( kjson::GetStringRef(jJWKS["keys"][0], "kty") == "RSA" );
		CHECK ( kjson::GetStringRef(jJWKS["keys"][0], "alg") == "RS256" );
		CHECK_FALSE ( kjson::GetStringRef(jJWKS["keys"][0], "n").empty() );
	}

	SECTION("full authorization-code + PKCE flow, then validate the id_token")
	{
		CookieJar Cookies;

		// 1) /authorize without an OP session -> 302 to /login, pending cookie set
		KString r1 = Drive("GET", sAuthorizeQuery, Cookies.Header());
		Cookies.Apply(r1);
		CHECK ( r1.contains("302") );
		CHECK ( FirstHeader(r1, "Location") == "/login" );

		// 2) /login -> verifies password, resumes authorize -> 302 to redirect_uri?code=...
		KString r2 = Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret");
		Cookies.Apply(r2);
		CHECK ( r2.contains("302") );
		KString sLocation = FirstHeader(r2, "Location");
		CHECK ( sLocation.starts_with("http://localhost/cb?") );
		CHECK ( URLParam(sLocation, "state") == "the-state" );
		KString sCode = URLParam(sLocation, "code");
		REQUIRE_FALSE ( sCode.empty() );

		// 3) /token: exchange the code (with PKCE verifier + client secret)
		KString sTokenBody = kFormat(
			"grant_type=authorization_code&code={}&redirect_uri=http://localhost/cb"
			"&code_verifier={}&client_id=test-client&client_secret=test-secret",
			sCode, sVerifier);
		KString r3 = Drive("POST", "/token", "", sTokenBody);
		KJSON jTok = kjson::Parse(ResponseBody(r3));
		REQUIRE ( jTok.is_object() );
		CHECK ( kjson::GetStringRef(jTok, "token_type") == "Bearer" );
		KString sIdToken     = kjson::GetStringRef(jTok, "id_token");
		KString sAccessToken = kjson::GetStringRef(jTok, "access_token");
		KString sRefresh     = kjson::GetStringRef(jTok, "refresh_token");
		REQUIRE_FALSE ( sIdToken.empty() );
		REQUIRE_FALSE ( sAccessToken.empty() );
		REQUIRE_FALSE ( sRefresh.empty() );

		// the access_token now binds aud=client_id (so a resource server can pin it)
		// and is marked token_use=access (so /userinfo can tell it from an id_token)
		{
			auto AParts = KStringView(sAccessToken).Split('.');
			REQUIRE ( AParts.size() == 3 );
			KJSON jAccess = kjson::Parse(KBase64Url::Decode(AParts[1]));
			CHECK ( kjson::GetStringRef(jAccess, "aud")       == "test-client" );
			CHECK ( kjson::GetStringRef(jAccess, "client_id") == "test-client" );
			CHECK ( kjson::GetStringRef(jAccess, "token_use") == "access"      );
		}

		// 4) validate the id_token against the published JWKS
		KJSON jJWKS = kjson::Parse(ResponseBody(Drive("GET", "/jwks", "")));
		KOpenIDKeys Keys(jJWKS);
		REQUIRE ( Keys.IsValid() );

		auto Parts = KStringView(sIdToken).Split('.');
		REQUIRE ( Parts.size() == 3 );
		KStringView sSigned(Parts[0].data(),
			static_cast<std::size_t>(Parts[1].data() + Parts[1].size() - Parts[0].data()));
		CHECK ( Keys.VerifySignature("key-1", "RS256", "", sSigned, KBase64Url::Decode(Parts[2])) );

		KJSON jPayload = kjson::Parse(KBase64Url::Decode(Parts[1]));
		CHECK ( kjson::GetStringRef(jPayload, "iss")   == sIssuer );
		CHECK ( kjson::GetStringRef(jPayload, "aud")   == "test-client" );
		CHECK ( kjson::GetStringRef(jPayload, "sub")   == "alice" );
		CHECK ( kjson::GetStringRef(jPayload, "nonce") == "the-nonce" );
		CHECK ( kjson::GetStringRef(jPayload, "email") == "alice@example.com" );
		CHECK ( kjson::GetStringRef(jPayload, "token_use") == "id" );

		// 5) the code is single-use: replaying it must fail
		KString r5 = Drive("POST", "/token", "", sTokenBody);
		CHECK ( kjson::GetStringRef(kjson::Parse(ResponseBody(r5)), "error") == "invalid_grant" );

		// 6) userinfo with the access token
		KString r6 = Drive("GET", "/userinfo", "", "");
		// (no auth header on r6 above -> expect 401; now with the bearer:)
		KString sReqUI = kFormat("GET /userinfo HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer {}\r\n\r\n", sAccessToken);
		{
			KString sResp;
			KInStringStream iss(sReqUI);
			KOutStringStream oss(sResp);
			KStream stream(iss, oss);
			KRESTServer::Options o;
			KRESTServer Srv(stream, "127.0.0.1:48999", url::KProtocol::HTTP, 48999, Routes, o);
			Srv.Execute();
			KJSON jUI = kjson::Parse(ResponseBody(sResp));
			CHECK ( kjson::GetStringRef(jUI, "sub")   == "alice" );
			CHECK ( kjson::GetStringRef(jUI, "email") == "alice@example.com" );
		}

		// 6b) token-type confinement: presenting the id_token as a bearer at
		//     /userinfo must be rejected (both tokens carry aud=client_id now, so
		//     the token_use marker is what keeps them apart)
		{
			KString sReqId = kFormat("GET /userinfo HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer {}\r\n\r\n", sIdToken);
			KString sResp;
			KInStringStream iss(sReqId);
			KOutStringStream oss(sResp);
			KStream stream(iss, oss);
			KRESTServer::Options o;
			KRESTServer Srv(stream, "127.0.0.1:48999", url::KProtocol::HTTP, 48999, Routes, o);
			Srv.Execute();
			CHECK ( sResp.contains("401") );
			CHECK ( kjson::GetStringRef(kjson::Parse(ResponseBody(sResp)), "error") == "invalid_token" );
		}

		// 7) refresh_token grant -> fresh tokens
		KString sRefreshBody = kFormat(
			"grant_type=refresh_token&refresh_token={}&client_id=test-client&client_secret=test-secret",
			sRefresh);
		KJSON jRef = kjson::Parse(ResponseBody(Drive("POST", "/token", "", sRefreshBody)));
		CHECK_FALSE ( kjson::GetStringRef(jRef, "access_token").empty() );
		CHECK_FALSE ( kjson::GetStringRef(jRef, "id_token").empty() );

		// 8) SSO: a second authorize now finds the OP session -> immediate code, no /login
		KString sVerifier2  = KBase64Url::Encode(kGetRandom(32));
		KString sChallenge2 = KBase64Url::Encode(KSHA256(sVerifier2).Digest());
		KString sAuthorize2 = kFormat(
			"/authorize?response_type=code&client_id=test-client&redirect_uri=http://localhost/cb"
			"&scope=openid&state=s2&code_challenge={}&code_challenge_method=S256", sChallenge2);
		KString r8 = Drive("GET", sAuthorize2, Cookies.Header());
		CHECK ( r8.contains("302") );
		CHECK ( FirstHeader(r8, "Location").starts_with("http://localhost/cb?") ); // straight to the client, SSO

		// 9) RP-initiated logout with a *registered* post_logout_redirect_uri (the
		//    client is identified via the id_token_hint's aud) -> 302 there, state echoed
		KString sLogoutOK = kFormat(
			"/logout?id_token_hint={}&post_logout_redirect_uri=http://localhost/after-logout&state=bye",
			sIdToken);
		KString r9 = Drive("GET", sLogoutOK, Cookies.Header());
		Cookies.Apply(r9);
		CHECK ( r9.contains("302") );
		CHECK ( FirstHeader(r9, "Location").starts_with("http://localhost/after-logout") );
		CHECK ( URLParam(FirstHeader(r9, "Location"), "state") == "bye" );

		// 10) the OP session is gone now -> SSO no longer applies, authorize -> /login
		KString r10 = Drive("GET", sAuthorize2, Cookies.Header());
		CHECK ( r10.contains("302") );
		CHECK ( FirstHeader(r10, "Location") == "/login" );

		// 11) open-redirect attempt: an *unregistered* post_logout_redirect_uri must
		//     be rejected (400), even with a valid id_token_hint -> no redirect issued
		KString sLogoutEvil = kFormat(
			"/logout?id_token_hint={}&post_logout_redirect_uri=http://evil.example/", sIdToken);
		KString r11 = Drive("GET", sLogoutEvil, "");
		CHECK ( r11.contains("400") );
		CHECK ( FirstHeader(r11, "Location").empty() );

		// 12) no redirect requested -> the OP's own default landing page
		KString r12 = Drive("GET", "/logout", "");
		CHECK ( r12.contains("302") );
		CHECK ( FirstHeader(r12, "Location") == "/" );
	}

	SECTION("client-scoped roles from AuthorizeClientAccess are embedded in the tokens")
	{
		pUsers->ClientClaims = { { "roles", KJSON::array({ "admin", "editor" }) } };

		CookieJar Cookies;
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		KString r2 = Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret");
		Cookies.Apply(r2);
		KString sCode = URLParam(FirstHeader(r2, "Location"), "code");
		REQUIRE_FALSE ( sCode.empty() );

		KString sBody = kFormat(
			"grant_type=authorization_code&code={}&redirect_uri=http://localhost/cb"
			"&code_verifier={}&client_id=test-client&client_secret=test-secret", sCode, sVerifier);
		KJSON jTok = kjson::Parse(ResponseBody(Drive("POST", "/token", "", sBody)));

		auto IdParts = KStringView(kjson::GetStringRef(jTok, "id_token")).Split('.');
		REQUIRE ( IdParts.size() == 3 );
		KJSON jId = kjson::Parse(KBase64Url::Decode(IdParts[1]));
		REQUIRE ( jId["roles"].is_array() );
		CHECK ( jId["roles"].dump().contains("admin")  );
		CHECK ( jId["roles"].dump().contains("editor") );

		// roles are exposed to resource servers via the access token too
		auto AcParts = KStringView(kjson::GetStringRef(jTok, "access_token")).Split('.');
		REQUIRE ( AcParts.size() == 3 );
		KJSON jAc = kjson::Parse(KBase64Url::Decode(AcParts[1]));
		CHECK ( jAc["roles"].is_array() );
	}

	SECTION("access denied right after an interactive login returns access_denied (terminal)")
	{
		pUsers->bDenyAccess = true;

		CookieJar Cookies;
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		KString r2 = Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret");

		KString sLoc = FirstHeader(r2, "Location");
		CHECK ( r2.contains("302") );
		CHECK ( sLoc.starts_with("http://localhost/cb?") );
		CHECK ( sLoc.contains("error=access_denied") );
		CHECK ( URLParam(sLoc, "code").empty() );
	}

	SECTION("a denied *silent* SSO session goes to the access-denied screen, not access_denied")
	{
		CookieJar Cookies;
		// establish an OP session while access is still allowed
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		Cookies.Apply(Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret"));

		// revoke access, then do a *silent* authorize (the session is present)
		pUsers->bDenyAccess = true;
		KString sChal2 = KBase64Url::Encode(KSHA256(KBase64Url::Encode(kGetRandom(32))).Digest());
		KString sAuth2 = kFormat(
			"/authorize?response_type=code&client_id=test-client&redirect_uri=http://localhost/cb"
			"&scope=openid&state=s2&code_challenge={}&code_challenge_method=S256", sChal2);

		KString r = Drive("GET", sAuth2, Cookies.Header());
		Cookies.Apply(r);
		CHECK ( r.contains("302") );
		CHECK ( FirstHeader(r, "Location") == "/no-access" ); // interstitial, NOT the client

		// the parked request is recoverable so the screen can name the app...
		CHECK ( ResponseBody(Drive("GET", "/no-access", Cookies.Header())).contains("test-client") );

		// ...and declining hands error=access_denied (with state) back to the client
		KString sLoc = FirstHeader(Drive("POST", "/no-access/decline", Cookies.Header()), "Location");
		CHECK ( sLoc.starts_with("http://localhost/cb?") );
		CHECK ( sLoc.contains("error=access_denied") );
		CHECK ( URLParam(sLoc, "state") == "s2" );
	}

	SECTION("prompt=none over a denied session returns access_denied (no interstitial)")
	{
		CookieJar Cookies;
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		Cookies.Apply(Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret"));

		pUsers->bDenyAccess = true;
		KString sChal2 = KBase64Url::Encode(KSHA256(KBase64Url::Encode(kGetRandom(32))).Digest());
		KString sAuth2 = kFormat(
			"/authorize?response_type=code&client_id=test-client&redirect_uri=http://localhost/cb"
			"&scope=openid&state=s2&prompt=none&code_challenge={}&code_challenge_method=S256", sChal2);

		KString sLoc = FirstHeader(Drive("GET", sAuth2, Cookies.Header()), "Location");
		CHECK ( sLoc.starts_with("http://localhost/cb?") );
		CHECK ( sLoc.contains("error=access_denied") );
	}

	SECTION("access revoked between authorize and token is denied at the token endpoint")
	{
		CookieJar Cookies;
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		KString r2 = Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret");
		Cookies.Apply(r2);
		KString sCode = URLParam(FirstHeader(r2, "Location"), "code");
		REQUIRE_FALSE ( sCode.empty() ); // allowed at authorize

		pUsers->bDenyAccess = true;      // revoke before the code exchange

		KString sBody = kFormat(
			"grant_type=authorization_code&code={}&redirect_uri=http://localhost/cb"
			"&code_verifier={}&client_id=test-client&client_secret=test-secret", sCode, sVerifier);
		KJSON jErr = kjson::Parse(ResponseBody(Drive("POST", "/token", "", sBody)));
		CHECK ( kjson::GetStringRef(jErr, "error") == "access_denied" );
	}

	SECTION("PKCE mismatch is rejected at the token endpoint")
	{
		CookieJar Cookies;
		Drive("GET", sAuthorizeQuery, Cookies.Header()); // sets pending cookie via the response we apply next
		Cookies.Apply(Drive("GET", sAuthorizeQuery, Cookies.Header()));
		KString r2 = Drive("POST", "/login", Cookies.Header(), "username=alice&password=secret");
		Cookies.Apply(r2);
		KString sCode = URLParam(FirstHeader(r2, "Location"), "code");
		REQUIRE_FALSE ( sCode.empty() );

		// wrong code_verifier
		KString sBody = kFormat(
			"grant_type=authorization_code&code={}&redirect_uri=http://localhost/cb"
			"&code_verifier=this-is-the-wrong-verifier&client_id=test-client&client_secret=test-secret", sCode);
		KJSON jErr = kjson::Parse(ResponseBody(Drive("POST", "/token", "", sBody)));
		CHECK ( kjson::GetStringRef(jErr, "error") == "invalid_grant" );
	}

	SECTION("unregistered redirect_uri is rejected at authorize")
	{
		KString sBad = kFormat(
			"/authorize?response_type=code&client_id=test-client&redirect_uri=http://evil.example/cb"
			"&scope=openid&state=s&code_challenge={}&code_challenge_method=S256", sChallenge);
		KString r = Drive("GET", sBad, "");
		CHECK_FALSE ( r.contains("302") );      // must NOT redirect anywhere
		CHECK ( r.contains("400") );
	}

	SECTION("a forged pending-authorize cookie yields no code on login resume")
	{
		// The pending request lives server-side; the browser holds only an opaque
		// handle. An attacker who plants a handle the OP never issued (this also
		// covers replaying the old base64-JSON cookie shape, which is now read as a
		// handle) must NOT cause a code to be minted: a valid login still succeeds,
		// but resolves to the post-login default, not to any client redirect_uri.
		KString sLogin = Drive("POST", "/login",
			"__Host-oidc_authorize=an-attacker-fabricated-handle",
			"username=alice&password=secret");

		CHECK ( sLogin.contains("302") );                          // login itself succeeded
		KString sLocation = FirstHeader(sLogin, "Location");
		CHECK ( sLocation == "/" );                                // sPostLoginRedirect, NOT a client
		CHECK_FALSE ( sLocation.starts_with("http://localhost/cb") );
		CHECK_FALSE ( sLocation.contains("code=") );               // no authorization code issued
	}

	SECTION("re-authentication policy: prompt, max_age, and per-client policy")
	{
		// establish a fresh OP login session and return its cookie jar
		auto LogIn = [&]() -> CookieJar
		{
			CookieJar C;
			C.Apply(Drive("GET", sAuthorizeQuery, C.Header()));                            // -> /login, pending cookie
			C.Apply(Drive("POST", "/login", C.Header(), "username=alice&password=secret")); // -> code + OP session cookie
			return C;
		};

		// a second authorize (distinct PKCE/state) with optional extra query params
		KString sVer2  = KBase64Url::Encode(kGetRandom(32));
		KString sChal2 = KBase64Url::Encode(KSHA256(sVer2).Digest());
		auto AuthorizeQ = [&](KStringView sExtra) -> KString
		{
			return kFormat(
				"/authorize?response_type=code&client_id=test-client&redirect_uri=http://localhost/cb"
				"&scope=openid&state=s2&code_challenge={}&code_challenge_method=S256{}", sChal2, sExtra);
		};

		SECTION("prompt=login forces re-auth despite a live session")
		{
			auto C = LogIn();
			// sanity: a plain authorize is silent SSO straight back to the client
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ(""), C.Header()), "Location").starts_with("http://localhost/cb?") );
			// prompt=login overrides SSO -> back to the login screen
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ("&prompt=login"), C.Header()), "Location") == "/login" );
		}

		SECTION("prompt=none returns login_required instead of a login screen")
		{
			// no session: cannot prompt -> error back to the client (not /login)
			KString sLoc = FirstHeader(Drive("GET", AuthorizeQ("&prompt=none"), ""), "Location");
			CHECK ( sLoc.starts_with("http://localhost/cb?") );
			CHECK ( sLoc.contains("error=login_required") );
			CHECK ( URLParam(sLoc, "state") == "s2" );

			// a live session with no re-auth trigger -> a silent code, no error
			auto C = LogIn();
			KString sLoc2 = FirstHeader(Drive("GET", AuthorizeQ("&prompt=none"), C.Header()), "Location");
			CHECK ( sLoc2.starts_with("http://localhost/cb?") );
			CHECK_FALSE ( sLoc2.contains("error=") );
			CHECK_FALSE ( URLParam(sLoc2, "code").empty() );
		}

		SECTION("prompt=none plus a re-auth trigger -> login_required (never a silent prompt)")
		{
			auto C = LogIn();
			// max_age=0 demands a just-now authentication, but prompt=none forbids UI
			KString sLoc = FirstHeader(Drive("GET", AuthorizeQ("&prompt=none&max_age=0"), C.Header()), "Location");
			CHECK ( sLoc.contains("error=login_required") );
		}

		SECTION("request max_age forces re-auth when the session is older than it")
		{
			auto C = LogIn();
			// max_age=0: any non-zero session age exceeds it -> /login
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ("&max_age=0"), C.Header()), "Location") == "/login" );
			// a generous max_age leaves SSO intact
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ("&max_age=3600"), C.Header()), "Location").starts_with("http://localhost/cb?") );
		}

		SECTION("client bForceLogin always re-authenticates")
		{
			auto C = LogIn();
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ(""), C.Header()), "Location").starts_with("http://localhost/cb?") );
			pClients->bForceLogin = true;
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ(""), C.Header()), "Location") == "/login" );
		}

		SECTION("client MaxAuthAge forces re-auth once the session is older than it")
		{
			auto C = LogIn();
			// a generous policy leaves SSO intact
			pClients->MaxAuthAge = std::chrono::hours(1);
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ(""), C.Header()), "Location").starts_with("http://localhost/cb?") );
			// a tiny policy, after a short wait, forces a fresh login
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
			pClients->MaxAuthAge = std::chrono::milliseconds(5);
			CHECK ( FirstHeader(Drive("GET", AuthorizeQ(""), C.Header()), "Location") == "/login" );
		}
	}
}
