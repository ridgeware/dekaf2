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


#pragma once

/// @file kwebapp.h
/// desktop application shell on the system webview (webview/webview)

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/limits/kratelimiter.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// returns the version of the embedded webview/webview library, e.g. "0.12.0"
DEKAF2_PUBLIC
KStringViewZ kGetWebViewVersion();

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A desktop application shell: a native window with the system webview shows
/// pages served by a KREST server in the same process, bound to the loopback
/// interface only. Routes, KWebObjects pages and WebSocket handlers are the
/// normal KREST ones - the window is just another HTTP client.
///
/// The loopback server is reachable by every local process and browser tab,
/// so KWebApp guards it: a random token per start, handed to the window with
/// the first navigation and kept in an HttpOnly cookie, a Host allowlist, and
/// an Origin check. A request failing any of these gets a 403, without exception.
///
/// JavaScript reaches C++ through window.kNative: Bind() publishes a handler as
/// window.kNative.<name>(arg), taking one JSON argument and returning a Promise
/// of the handler's JSON result. window.kNative.platform() and .version() are
/// available synchronously, and `typeof window.kNative !== 'undefined'` tells
/// a page whether it runs in the window or in a plain browser.
///
/// @par Network mode
/// With Options.bNetwork set, a second KREST serves the same routes to
/// browsers on the network, always over TLS, behind a login: /login shows a
/// form and verifies the credentials through Options.Authenticate, the session
/// lives in a cookie (Secure, HttpOnly, SameSite=Strict) and in Options.SessionStore,
/// /logout ends it, /healthz answers without login. Login attempts are limited
/// per client address, connections per address and the request body size get
/// limits too, and every response carries HSTS, nosniff, frame and referrer
/// policies and Options.sContentSecurityPolicy. Paths in Options.WindowOnlyPaths
/// are refused with 403 on the network server, and a route handler can ask
/// IsFromWindow() to decide the same for itself. Without window (bWindow false)
/// Run() serves the network until Quit() or a shutdown signal; with a window,
/// bQuitOnWindowClose decides whether closing it ends Run() or leaves the
/// network server running.
///
/// @par Live data
/// Every page can hold one live connection to the application, a websocket at
/// /_kwa/live that the helper script /_kwa/live.js opens and keeps open: in the
/// window and in browsers alike. Broadcast() sends a JSON message to all of
/// them, Send() to one, OnMessage() receives what a page sent with kLive.send(),
/// OnConnect() sees a page arrive. A Client tells whether it is the window or a
/// network login. This is the way for "C++ has news, every view shows them",
/// Bind() stays for what only the desktop can do.
///
/// @par Usage
/// @code
/// KRESTRoutes Routes;
///
/// // the root is the empty route
/// Routes.AddRoute({ KHTTPMethod::GET, false, "", [](KRESTServer& HTTP)
/// {
///     html::Page Page("Hello");
///     Page.Add<html::Heading>(1, "Hello from KREST");
///     HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
///     HTTP.SetRawOutput(Page.Print());
/// }});
///
/// KWebApp::Options Options;
/// Options.sTitle   = "Hello";
/// Options.sVersion = "1.0";
/// // optional: the same pages for browsers on the network, with a login
/// Options.bNetwork      = true;
/// Options.Network.iPort = 8443;
/// Options.Authenticate  = [](KStringView sUser, KStringView sPassword) { return Users.Check(sUser, sPassword); };
///
/// KWebApp App(std::move(Options), Routes); // KREST::Options is move-only
///
/// // in the page: const ok = await window.kNative.save({ name: "x" }); - ok is a boolean
/// App.Bind("save", [](const KJSON& jArg) -> KJSON
/// {
///     return jArg["name"].String() == "x";
/// });
///
/// // in the page: <script src="/_kwa/live.js"></script> kLive.on(m => ...); kLive.send({ t: "hello" });
/// App.OnMessage([&](const KWebApp::Client& Client, const KJSON& jMessage)
/// {
///     App.Broadcast({ { "t", "news" }, { "from", Client.bFromWindow ? "window" : Client.sUser } });
/// });
///
/// return App.Run(); // blocks in the UI loop until the window closes or Quit() is called
/// @endcode
///
/// @par Threads
/// Run() must be called on the main thread, it owns the UI loop. Route handlers
/// run in KREST threads and may call Eval(), Navigate() and SetTitle() at any
/// time, the calls are dispatched to the UI thread. Bind() handlers run on the
/// UI thread and should return quickly. SIGINT and SIGTERM end Run() when the
/// signal handler thread of KInit(true) is running.
class DEKAF2_PUBLIC KWebApp : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Options
	{
		/// window title
		KString        sTitle;
		/// program version, reported by window.kNative.version()
		KString        sVersion;
		/// the server for the window. KWebApp binds it to 127.0.0.1 without TLS,
		/// installs its access guard, and lets the OS pick the port unless iPort is set
		KREST::Options Loopback;
		/// the server for browsers on the network, started with bNetwork. It always
		/// speaks TLS: with sCert and sKey, else with an ephemeral certificate that
		/// KREST persists under ~/.config/<program>/tls/. Port 0 lets the OS pick
		KREST::Options Network;
		/// start the network server?
		bool           bNetwork { false };
		/// verifies a login on the network server - without it nobody can log in
		KSession::Authenticator Authenticate;
		/// where network sessions are kept, default in memory - pass e.g. a
		/// KSessionSQLiteStore for logins that survive a restart
		std::unique_ptr<KSession::Store> SessionStore;
		/// cookie and lifetime of network sessions - Secure and HttpOnly are forced
		KSession::Config Session;
		/// paths that only the window may call - the network server answers 403
		std::vector<KString> WindowOnlyPaths;
		/// the Content-Security-Policy of the network server, empty for none. The
		/// default allows the page's own inline scripts and styles, nothing external
		KString sContentSecurityPolicy { "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
		                                 "img-src 'self' data:; connect-src 'self' wss:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'" };
		/// initial window size in points
		uint32_t       iWidth  { 1024 };
		uint32_t       iHeight {  768 };
		/// open a window? false serves the network alone until Quit()
		bool           bWindow { true };
		/// enable the web inspector in the window
		bool           bDebug  { false };
		/// end Run() when the window closes? false keeps a network server running until Quit()
		bool           bQuitOnWindowClose { true };
	};

	/// JavaScript to C++ handler: one JSON argument in, one JSON result out.
	/// An exception rejects the Promise with { "error": "<what>" }
	using Handler = std::function<KJSON(const KJSON& jArg)>;

	/// a page's live connection, in the window or in a browser
	struct Client
	{
		/// identifies the connection for Send()
		std::size_t iID { 0 };
		/// came in through the window's loopback server?
		bool        bFromWindow { false };
		/// the network login, empty for the window
		KString     sUser;
	};

	/// called when a page's live connection is up, e.g. to send it the current state
	using ConnectHandler = std::function<void(const Client& Client)>;
	/// receives a JSON message a page sent over its live connection
	using MessageHandler = std::function<void(const Client& Client, const KJSON& jMessage)>;

	/// starts the servers - check HasError() afterwards. Routes must outlive the
	/// KWebApp, which adds its own routes to the table: /_kwa/enter for the
	/// window, /login, /logout and /healthz for the network
	KWebApp(Options Options, KRESTRoutes& Routes);
	~KWebApp();

	KWebApp(const KWebApp&) = delete;
	KWebApp& operator=(const KWebApp&) = delete;

	/// publish a handler as window.kNative.<name>(arg). Before Run() the binding
	/// is queued, afterwards it becomes available at once
	/// @param sName a JavaScript identifier: letters, digits and underscores
	/// @return false for an invalid or duplicate name
	bool Bind(KStringView sName, Handler Handler);
	/// run JavaScript in the window, from any thread. A no-op without window
	void Eval(KStringView sJavaScript);
	/// navigate the window to a path on the loopback server, e.g. "/settings".
	/// Before Run() this sets the start page (default "/")
	void Navigate(KStringView sPath);
	/// set the window title, from any thread
	void SetTitle(KStringView sTitle);
	/// run the window until it is closed or Quit() is called, then, or without
	/// window, the network server until Quit() or a shutdown signal. Stops the
	/// servers before returning
	/// @return 0, or 1 when a server had not started
	int  Run();
	/// end Run(), from any thread
	void Quit();
	/// true once Quit() was called or the window has closed. Long running route
	/// handlers, e.g. websocket loops, have to end then, else Run() cannot stop
	/// the server and return
	bool IsQuitting() const { return m_bQuit; }

	/// true when the request came in through the window's loopback server - a
	/// handler that does something only the desktop may do checks this
	bool IsFromWindow(const KRESTServer& HTTP) const;

	/// set the handler for arriving live connections - runs in the websocket server's thread
	void OnConnect(ConnectHandler Handler);
	/// set the handler for messages from live connections - runs in the websocket
	/// server's thread, so it should return quickly
	void OnMessage(MessageHandler Handler);
	/// send a JSON message to every live connection, in the window and in browsers
	/// @return the number of connections the message was accepted for
	std::size_t Broadcast(const KJSON& jMessage);
	/// send a JSON message to one live connection
	bool Send(const Client& Client, const KJSON& jMessage);
	/// the number of live connections
	std::size_t GetClientCount() const;

	/// the loopback server's port
	uint16_t    GetPort()        const { return m_iPort;        }
	/// the network server's port, 0 without network server
	uint16_t    GetNetworkPort() const { return m_iNetworkPort; }
	/// this instance's access token - a request to the loopback server has to carry it in the cookie "kwa"
	KStringView GetToken()       const { return m_sToken;       }
	/// the URL for a first request to a path on the loopback server, carrying the token
	KString     GetEnterURL(KStringView sPath = "/") const;
	/// the network sessions, e.g. to look up the user of a request through KRESTSession
	KSession*   GetSession()           { return m_Session.get(); }

//----------
private:
//----------

	class WebView; // wraps the webview object, keeps its header out of ours

	struct LiveClient
	{
		Client            Client;
		KWebSocketServer* pServer { nullptr };
		std::size_t       iHandle { 0 };
	};

	void    Guard        (KRESTServer& HTTP);
	void    NetworkGuard (KRESTServer& HTTP);
	void    Live         (KRESTServer& HTTP);
	void    LiveScript   (KRESTServer& HTTP);
	void    LoginPage    (KRESTServer& HTTP);
	void    Login        (KRESTServer& HTTP);
	void    Logout       (KRESTServer& HTTP);
	void    Health       (KRESTServer& HTTP);
	bool    StartLoopback();
	bool    StartNetwork ();
	void    CatchShutdownSignals();
	void    AddBinding   (WebView& View, KStringView sName, const Handler& Handler);
	KString InitScript   () const;

	static bool    IsIdentifier(KStringView sName);
	static KString ShimScript  (KStringView sName);
	static KString SafePath    (KStringView sPath);
	static void    Redirect    (KRESTServer& HTTP, KStringView sLocation);

	Options                           m_Options;
	KRESTRoutes&                      m_Routes;
	std::unique_ptr<KREST>            m_REST;
	std::unique_ptr<KREST>            m_Network;
	std::unique_ptr<KSession>         m_Session;
	std::unique_ptr<WebView>          m_WebView;
	std::function<void(KRESTServer&)> m_UserPreRoute;
	std::function<void(KRESTServer&)> m_UserNetworkPreRoute;
	std::map<KString, Handler>        m_Bindings;
	std::map<std::size_t, LiveClient> m_Clients;
	ConnectHandler                    m_OnConnect;
	MessageHandler                    m_OnMessage;
	mutable std::mutex                m_ClientMutex;
	std::size_t                       m_iNextClient  { 0 };
	KRateLimiter                      m_LoginLimiter { 1.0 / 30, 10 };
	KString                           m_sToken;
	KString                           m_sStartPath { "/" };
	mutable std::mutex                m_Mutex;
	std::condition_variable           m_Idle;
	std::map<int, std::function<void(int)>> m_PreviousSignalHandlers;
	std::atomic<bool>                 m_bQuit { false };
	uint16_t                          m_iPort        { 0 };
	uint16_t                          m_iNetworkPort { 0 };

}; // KWebApp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
