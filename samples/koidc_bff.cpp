// koidc_bff.cpp
//
// Minimal "Backend-for-Frontend" (BFF) OpenID Connect server built on dekaf2's
// KOpenIDClient. The backend drives the entire authorization-code + PKCE flow
// and keeps all tokens server-side; the browser only ever holds an opaque,
// HttpOnly session cookie. This is the pattern that survives modern browser
// restrictions on third-party cookies / silent-renewal iframes.
//
// Example:
//   koidc_bff --http 8080 \
//             --authority     https://sso.example.com \
//             --client-id     my-spa \
//             --client-secret s3cr3t \
//             --callback      https://app.example.com/auth/callback \
//             --post-logout   https://app.example.com/ \
//             --sessions      /var/lib/koidc/sessions.sqlite
//
// Wire your SPA so that:
//   * a "Login" button points the browser at  GET /auth/login
//   * the IdP redirect URI is registered as   GET /auth/callback
//   * the SPA polls                           GET /auth/userinfo  (logged_in?)
//   * a "Logout" button points at             GET /auth/logout
//   * protected API calls go through a handler that calls OIDC.Authorize()

#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/crypto/auth/kopenidclient.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/logging/klog.h>
#include <memory>

using namespace dekaf2;

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow=*/true);
		Options.SetBriefDescription("a minimal Backend-for-Frontend OpenID Connect server");

		KOpenIDClient::Config Config;
		Config.sAuthority        = Options("authority <url>       : OIDC issuer base URL, e.g. https://sso.example.com");
		Config.sClientID         = Options("client-id <id>        : OAuth2 client id");
		Config.sClientSecret     = Options("client-secret <secret>: OAuth2 client secret");
		Config.sCallbackURI      = Options("callback <url>        : full redirect URL, e.g. https://host/auth/callback");
		Config.sPostLogoutURI    = Options("post-logout <url>     : full URL to return to after logout", "/");
		Config.sScope            = Options("scope <list>          : space separated scopes", "openid profile offline_access");
		Config.sSessionCookieName= Options("cookie <name>         : session cookie name", "session");

		KStringViewZ sSessionDB  = Options("sessions <path>       : SQLite path for the server-side session store", "sessions.sqlite");

		KREST::Options Settings;
		Settings.iPort           = Options("http <port>           : TCP port to bind to");
		Settings.iTimeout        = Options("timeout <seconds>     : server timeout (default 5)", 5);
		// use TLS with an ephemeral self-signed cert unless told otherwise
		Settings.bCreateEphemeralCert = !Options("notls           : do NOT switch to TLS (plain HTTP, dev only)", false);

		// stop here if the options framework already handled the request
		// (e.g. -help was given) instead of starting the server
		if (Options.Terminate())
		{
			return 0;
		}

		Settings.Type            = KREST::HTTP;
		Settings.bBlocking       = true;

		// one client instance, shared by all requests (its methods are designed
		// to be called per-request with the current KRESTServer)
		auto OIDC = std::make_unique<KOpenIDClient>(std::move(Config), sSessionDB);

		KRESTRoutes Routes;

		// --- start the login: build the auth URL, set the PKCE/state cookie ---
		Routes.AddRoute(KRESTRoute(KHTTPMethod::GET, "/auth/login",
			[&OIDC](KRESTServer& HTTP)
		{
			KURL URL = OIDC->BuildLoginURL(HTTP, HTTP.GetQueryParmSafe("return_to", "/"));
			if (URL.empty())
			{
				throw KHTTPError(KHTTPError::H5xx_ERROR, "OIDC discovery failed");
			}
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, URL.Serialize());
			throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "");
		}));

		// --- IdP redirect target: exchange code -> tokens, create session ----
		Routes.AddRoute(KRESTRoute(KHTTPMethod::GET, "/auth/callback",
			[&OIDC](KRESTServer& HTTP)
		{
			OIDC->HandleCallback(HTTP); // sets the session cookie and 302-redirects
		}));

		// --- safe, token-free status for the SPA ("am I logged in?") ---------
		Routes.AddRoute(KRESTRoute(KHTTPMethod::GET, "/auth/userinfo",
			[&OIDC](KRESTServer& HTTP)
		{
			OIDC->GetUserInfo(HTTP); // writes {logged_in, username, expires_at}
		}));

		// --- log out locally and at the IdP ----------------------------------
		Routes.AddRoute(KRESTRoute(KHTTPMethod::GET, "/auth/logout",
			[&OIDC](KRESTServer& HTTP)
		{
			OIDC->Logout(HTTP); // clears the session and 302-redirects
		}));

		// --- an example protected endpoint -----------------------------------
		// Authorize() validates the session cookie, silently refreshes the
		// access token when needed, and injects the bearer token into the
		// request's Authorization header for any downstream call.
		Routes.AddRoute(KRESTRoute(KHTTPMethod::GET, "/api/me",
			[&OIDC](KRESTServer& HTTP)
		{
			KString sUser = OIDC->Authorize(HTTP);
			if (sUser.empty())
			{
				// not (or no longer) logged in — send the browser to login
				HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/auth/login");
				throw KHTTPError(KHTTPError::H302_MOVED_TEMPORARILY, "not logged in");
			}
			HTTP.json.tx = { { "user", sUser } };
		}));

		KREST Server;
		if (!Server.Execute(Settings, Routes))
		{
			KErr.FormatLine("server error: {}", Server.Error());
			return 1;
		}

		return 0;
	}
	catch (const KException& ex)
	{
		KErr.FormatLine("koidc_bff: {}", ex.what());
		return 1;
	}

} // main
