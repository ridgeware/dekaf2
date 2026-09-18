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

#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/limits/kratelimiter.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/util/i18n/kstringcatalog.h>
#include <dekaf2/web/app/bits/kwebapp_platform.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace html { class Page; }

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
/// window.kNative.\<name\>(arg), taking one JSON argument and returning a Promise
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
/// @par Desktop integration
/// The window comes with the standard application, Edit and Window menus, and
/// Options.jMenus adds the application's own: an entry's action calls a bound
/// handler of that name, or reaches the page as a "kwa-menu" event with the
/// action in detail. The window remembers its position and size between runs,
/// and one instance runs per user: a second start brings the first window to
/// the front and reports IsOtherInstanceRunning(). Notify(), OpenExternal() and
/// the file dialogs are available to C++ and, as window.kNative.notify(),
/// .openExternal(), .openFile() and .saveFile(), to the page. On macOS the
/// notification center wants an application bundle - a bare binary falls back
/// to the scripting bridge. SetBadge(), RequestAttention() and Activate() reach
/// the user while the window is in the background, and the page has them as
/// window.kNative.setBadge(), .requestAttention(), .cancelAttention() and .activate().
///
/// @par Hosted web applications
/// Navigate() with an absolute http(s) URL shows that site in the window instead
/// of a loopback page - a native shell around a web application that a server
/// hosts. window.kNative and the bindings are there as well, Options.sInitScript
/// runs before every document, and Options.bAllowMediaCapture lets the site use
/// camera and microphone. The loopback server keeps serving what the shell adds
/// itself, e.g. a first-run page; the site's own requests to it are cross-origin
/// and get 403.
///
/// @par Navigation rules and downloads
/// The window shows the loopback server and the sites in Options.AllowedOrigins,
/// nothing else: a link elsewhere, window.open() and target="_blank" open the
/// system browser, and the window stays where it is - window.kNative never
/// reaches a foreign page; AddAllowedOrigin() widens the list at run time. A
/// response the view cannot show, or one the server
/// sends as attachment, becomes a download into Options.sDownloadDir (default
/// the user's Downloads folder) under a free name, announced with a notification
/// and to OnDownload().
///
/// @par Staying in the background
/// With Options.bHideOnClose the close button hides the window, and Run() lasts
/// until Quit(): an application that has to keep running - calls come in, a
/// presence stays online - hides and comes back with Show(), a click on its
/// icon, or a second start. Notify() takes a tag that replaces an earlier
/// notification and a picture, notifications show while the application is
/// active, and a click on one brings the window back and reaches
/// OnNotificationClick(). Options.bTrayIcon puts a symbol into the menu bar or
/// the notification area, with Options.jTrayMenu or an "Open" and "Quit" of its
/// own, and SetTrayIcon() swaps its image for an attention state.
/// Options.bBackgroundActivity keeps the page running while the window is
/// hidden or on another desktop, for a page that must answer while nobody
/// looks. Options.SitePaths opens single paths of the loopback server to the
/// hosted site, token in the query. Confirm() asks a yes-or-no question in the
/// system's own dialog, where the user is, for the page as
/// window.kNative.confirm({title, text, ok, cancel}). SaveSecret(), LoadSecret() and DeleteSecret() keep
/// e.g. a login in the system's credential store, for the page as
/// window.kNative.saveSecret(), .loadSecret() and .deleteSecret() - published
/// only where the navigation rules are enforced. ClearWebCache() before Run()
/// drops cached scripts and styles, e.g. after an update of the shell.
///
/// @par Languages
/// The texts of KWebApp itself (login page, menus, notifications) come from a
/// KStringCatalog in English, German, French, Italian, Spanish, Japanese, Korean,
/// Simplified and Traditional Chinese. Options.Catalog adds the application's
/// own catalog, and may override KWebApp's texts (identifiers "kwa.*"). A
/// request's language is the cookie "lang" (set by a POST to /_kwa/lang with the
/// fields lang and next), else Accept-Language, else the default language;
/// Text(HTTP) gives a page builder the catalog bound to it, AddStrings() hands a
/// namespace of messages to the page script as window.kText. Menus and
/// notifications use Options.sLanguage, by default the system's language; a menu
/// title that starts with "@" is a catalog identifier.
///
/// @par Platforms
/// The platform layer under bits/ has one file per operating system, all plain
/// C++. macOS has everything above. Linux (GTK 4 with WebKitGTK 6.0, or GTK 3
/// with WebKitGTK 4.1) puts a menu bar into the window, opens the browser and
/// posts notifications through xdg-open and notify-send (no click reports),
/// keeps secrets through secret-tool, has no badge, no window positions on
/// Wayland, and no way to bring back a hidden window but Show() - a second
/// start cannot raise the first. Windows (WebView2) puts a menu bar into the
/// window too, shows notifications as balloons on a tray icon that a plain
/// executable gets with its first notification or when the close button hides
/// the window - a click on the icon brings the window back - and keeps secrets
/// in the credential manager; the badge is missing there as well.
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
		/// KREST persists under ~/.config/\<program\>/tls/. Port 0 lets the OS pick
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
		/// the application's name for ~/.config/\<name\>/, where the window geometry and the
		/// instance lock live - empty means the program name
		KString        sAppName;
		/// one window per user - a second start brings the first to the front and ends
		bool           bSingleInstance    { true };
		/// remember the window's position and size between runs
		bool           bRememberWindow    { true };
		/// the application's menus for the window, as a JSON array of
		/// { "title": "File", "items": [ { "title": "Save", "key": "s", "action": "save" }, { "separator": true } ] }
		KJSON          jMenus;
		/// let pages use camera and microphone: the webview grants getUserMedia()
		/// without a prompt of its own, the operating system asks once. On macOS the
		/// bundle needs NSCameraUsageDescription and NSMicrophoneUsageDescription,
		/// else the process is terminated on first access
		bool           bAllowMediaCapture { false };
		/// JavaScript to run before every document, once window.kNative.platform() and
		/// .version() exist - e.g. to hand configuration to the page, or to publish
		/// the bridge under a second name
		KString        sInitScript;
		/// the origins the window may show besides the loopback server, e.g.
		/// "https://example.com" - a navigation elsewhere opens in the system browser
		/// instead, and so does every new window. "*" allows all http(s) sites
		std::vector<KString> AllowedOrigins;
		/// where downloads go, empty for the user's Downloads folder
		KString        sDownloadDir;
		/// the close button hides the window instead of closing it - Run() then lasts
		/// until Quit(), and a click on the application's icon brings the window back
		bool           bHideOnClose { false };
		/// the application's texts, merged over KWebApp's own - see @ref KStringCatalog.
		/// Must outlive the KWebApp constructor, the catalog is copied
		const KStringCatalog* Catalog { nullptr };
		/// the language of menus, notifications and dialogs, a BCP 47 tag - empty
		/// takes the user's setting from the last run, else the system's languages
		KString        sLanguage;
		/// give the Edit menu's Paste entry its keyboard shortcut (Cmd-V on macOS)?
		/// With the shortcut the menu takes the key before the page sees a keydown
		/// for it - a page that handles Cmd-V itself, e.g. to read a picture from
		/// the clipboard where the webview would only beep, switches this off
		bool           bPasteShortcut { true };
		/// a symbol in the menu bar (macOS) or the notification area (Windows), the
		/// way back to a hidden window - with the menu in jTrayMenu, or with "Open"
		/// and "Quit" when that is empty
		bool           bTrayIcon { false };
		/// the symbol's menu, entries like in jMenus: { "title": "@menu.open", "action": "show" },
		/// { "separator": true }. The actions "show" and "quit" are built in, any
		/// other one calls the bound handler of that name or reaches the page
		KJSON          jTrayMenu;
		/// the symbol's image: a file (PNG, on Windows ICO), on macOS also an SF Symbol
		/// as "sf:<name>" - empty takes the application's icon
		KString        sTrayIcon;
		/// keep the page fully running while the window is hidden, minimized or on
		/// another desktop. Without it macOS suspends the web content of an invisible
		/// window after a while. On for a communication application, off for a tool
		bool           bBackgroundActivity { false };
		/// paths of the loopback server that the hosted site (Options.AllowedOrigins)
		/// may call
		std::vector<KString> SitePaths;
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
	/// a download has finished, or failed - then sError is set, and sPath may be empty
	using DownloadHandler = std::function<void(KStringView sPath, KStringView sError)>;
	/// the user clicked a notification: the tag it was posted with, empty for none
	using NotificationHandler = std::function<void(KStringView sTag)>;

	/// starts the servers - check HasError() afterwards. Routes must outlive the
	/// KWebApp, which adds its own routes to the table: /_kwa/enter for the
	/// window, /login, /logout and /healthz for the network
	KWebApp(Options Options, KRESTRoutes& Routes);
	/// stops the servers and closes the window if Run() has not done so
	~KWebApp();

	KWebApp(const KWebApp&) = delete;
	KWebApp& operator=(const KWebApp&) = delete;

	/// publish a handler as window.kNative.\<name\>(arg). Before Run() the binding
	/// is queued, afterwards it becomes available at once
	/// @param sName a JavaScript identifier: letters, digits and underscores
	/// @param Handler the function called with the page's JSON argument, its JSON result resolves the Promise
	/// @return false for an invalid or duplicate name
	bool Bind(KStringView sName, Handler Handler);
	/// run JavaScript in the window, from any thread. A no-op without window
	void Eval(KStringView sJavaScript);
	/// navigate the window to a path on the loopback server, e.g. "/settings", or
	/// to an absolute http(s) URL in Options.AllowedOrigins - the window then shows
	/// that site, with window.kNative in place, while the loopback server keeps
	/// serving what the application adds itself. Before Run() this sets the start
	/// page (default "/")
	void Navigate(KStringView sPath);
	/// may the window show this URL? True for the loopback server and the origins
	/// in Options.AllowedOrigins
	bool IsAllowedURL(KStringView sURL) const;
	/// allow one more origin at run time, e.g. "https://example.com" - for a shell
	/// whose site the user picks in a first-run page. From any thread
	/// @return false when the text is no http(s) origin
	bool AddAllowedOrigin(KStringView sOrigin);

	/// the language of a request: the cookie "lang", else Accept-Language, else
	/// the default language - always one the catalog has
	KString GetLanguage(const KRESTServer& HTTP) const;
	/// the route a page posts the user's language choice to, with the form fields
	/// lang (a tag) and next (the path to return to)
	static constexpr KStringView LanguagePath = "/_kwa/lang";
	/// the language of menus and notifications
	const KString& GetLanguage() const { return m_sLanguage; }
	/// the catalog bound to the request's language, for the page builder:
	/// T("list.name"), T.Format("tail.lines", { { "count", 3 } })
	KStringCatalog::Language Text(const KRESTServer& HTTP) const;
	/// the merged catalog: KWebApp's texts and the application's
	const KStringCatalog& GetCatalog() const { return m_Catalog; }
	/// adds a script to the page that defines window.kText = { lang, messages }
	/// with the raw messages of the namespace in the request's language, for
	/// intl-messageformat in the page. Call it once per page
	void AddStrings(html::Page& Page, const KRESTServer& HTTP, KStringView sNamespace) const;
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

	/// another instance of this application runs already - it has been brought to the front
	bool IsOtherInstanceRunning() const { return m_bOtherInstance; }
	/// open a URL in the system's browser - http, https and mailto only
	bool OpenExternal(KStringView sURL);
	/// post a system notification. A tag replaces an earlier notification with the
	/// same tag, a picture ("data:image/png;base64,...") is attached where the
	/// platform shows one
	bool Notify(KStringView sTitle, KStringView sBody, KStringView sTag = {}, KStringView sImageDataURL = {});
	/// set the handler for clicks on notifications - the window comes to the front
	/// first, then the handler gets the tag. Runs on the UI thread
	void OnNotificationClick(NotificationHandler Handler);
	/// set the handler for finished downloads - runs on the UI thread
	void OnDownload(DownloadHandler Handler);
	/// native open dialog, from any thread, blocks until answered. Extensions like
	/// "log" limit the choice, empty allows any file. Empty result when cancelled
	std::vector<KString> OpenFileDialog(KStringView sTitle, const std::vector<KString>& Extensions = {}, bool bMultiple = false, bool bDirectories = false);
	/// native save dialog, from any thread, blocks until answered. Empty when cancelled
	KString SaveFileDialog(KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions = {});
	/// show a text on the application's icon, e.g. an unread count - empty clears it
	void SetBadge(KStringView sText);
	/// ask for the user's attention while the application is in the background: the
	/// dock icon bounces, the taskbar button flashes. Critical keeps asking until the
	/// application is activated, else it asks once. From any thread
	void RequestAttention(bool bCritical = false);
	/// stop asking for attention
	void CancelAttention();
	/// bring the window to the front and activate the application, from any thread
	void Activate();
	/// ask the user a yes-or-no question with the system's own dialog. It opens
	/// where the user is - on macOS the active space, on Windows the current
	/// virtual desktop - which the window itself may not be. Empty button
	/// titles take "OK" and "Cancel" from the catalog. On the UI thread only,
	/// which a bound handler is; for the page as window.kNative.confirm()
	/// @param sTitle the question, in bold
	/// @param sText the explanation below it, may be empty
	/// @param sOK the title of the first button, the answer "yes"
	/// @param sCancel the title of the second button, the answer "no"
	/// @return true for the first button, false for the second or without a window
	bool Confirm(KStringView sTitle, KStringView sText = KStringView{}, KStringView sOK = KStringView{}, KStringView sCancel = KStringView{});
	/// swap the tray symbol's image, e.g. while a call comes in - empty restores
	/// Options.sTrayIcon. From any thread, a no-op without tray symbol
	void SetTrayIcon(KStringView sIcon);
	/// show the window after Hide() or a close with Options.bHideOnClose, and activate
	/// the application. From any thread
	void Show();
	/// hide the window - the application keeps running, Show() brings it back
	void Hide();
	/// is the window on screen?
	bool IsWindowVisible() const;
	/// keep a secret in the system's credential store (the keychain, the credential
	/// manager, the secret service), under the application's name
	bool    SaveSecret  (KStringView sKey, KStringView sValue);
	/// read a secret back, false when there is none
	bool    LoadSecret  (KStringView sKey, KString& sValue);
	/// read a secret back, empty when there is none
	KString LoadSecret  (KStringView sKey);
	/// remove a secret, true also when there was none
	bool    DeleteSecret(KStringView sKey);
	/// drop the webview's cached scripts, styles, fetch responses and service worker
	/// registrations, not local storage and databases - before Run() the first page
	/// loads after the caches are gone, e.g. after an update of the shell
	void ClearWebCache();

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
		Client            Info;
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
	void    SetLanguage  (KRESTServer& HTTP);
	void    LoadSettings ();
	void    SaveSettings ();
	KJSON   TranslatedMenus() const;
	bool    StartLoopback();
	bool    StartNetwork ();
	void    CatchShutdownSignals();
	bool    LockInstance ();
	void    AddBuiltins  ();
	void    MenuAction   (KStringView sAction);
	void    TrayAction   (KStringView sAction);
	KJSON   TranslatedTrayMenu() const;
	void    LoadWindowFrame();
	void    SaveWindowFrame();
	void    RememberFrame(const kwebapp::WindowFrame& Frame);
	void    CloseWindow  ();
	void    RunOnUI      (std::function<void()> Call) const;
	void*   WindowHandle () const;
	void    AddBinding   (WebView& View, KStringView sName, const Handler& Handler);
	KString InitScript   () const;
	KString AppName      () const;
	bool    OnNavigate   (KStringView sURL, bool bNewWindow);
	KString DownloadPath (KStringView sSuggestedName);
	void    DownloadDone (KStringView sPath, KStringView sError);
	void    Activated    ();
	void    NotificationClicked(KStringView sTag);

	static bool    IsIdentifier(KStringView sName);
	static bool    IsWebURL    (KStringView sURL);
	static KString Origin      (KStringView sURL);
	static KString ImageFile   (KStringView sDataURL);
	static KString ShimScript  (KStringView sName);
	static KString SafePath    (KStringView sPath);
	static void    Redirect    (KRESTServer& HTTP, KStringView sLocation);

	Options                           m_Options;
	KRESTRoutes&                      m_Routes;
	KStringCatalog                    m_Catalog;
	KString                           m_sLanguage;
	KJSON                             m_jSettings;
	std::unique_ptr<KREST>            m_REST;
	std::unique_ptr<KREST>            m_Network;
	std::unique_ptr<KSession>         m_Session;
	std::unique_ptr<WebView>          m_WebView;
	std::function<void(KRESTServer&)> m_UserPreRoute;
	std::function<void(KRESTServer&)> m_UserNetworkPreRoute;
	KUnorderedMap<KString, Handler>        m_Bindings;
	KUnorderedMap<std::size_t, LiveClient> m_Clients;
	ConnectHandler                    m_OnConnect;
	MessageHandler                    m_OnMessage;
	DownloadHandler                   m_OnDownload;
	NotificationHandler               m_OnNotificationClick;
	mutable std::mutex                m_ClientMutex;
	std::size_t                       m_iNextClient    { 0 };
	KRateLimiter                      m_LoginLimiter   { 1.0 / 30, 10 };
	KString                           m_sToken;
	KString                           m_sStartPath     { "/" };
	mutable std::mutex                m_Mutex;
	mutable std::mutex                m_OriginMutex;
	std::condition_variable           m_Idle;
	// the two shutdown signals and the handlers they had before - written once, iterated once
	std::vector<std::pair<int, std::function<void(int)>>> m_PreviousSignalHandlers;
	std::unique_ptr<KFileLock>        m_InstanceLock;
	KString                           m_sConfigDir;
	KJSON                             m_jWindowFrame;
	int64_t                           m_iAttention     { 0 };
	uint16_t                          m_iPort          { 0 };
	uint16_t                          m_iNetworkPort   { 0 };
	std::atomic<bool>                 m_bQuit          { false };
	bool                              m_bOtherInstance { false };
	bool                              m_bClearWebCache { false };

}; // KWebApp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
