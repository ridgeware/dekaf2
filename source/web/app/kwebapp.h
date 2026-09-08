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
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

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
///
/// KWebApp App(std::move(Options), Routes); // KREST::Options is move-only
///
/// // in the page: const ok = await window.kNative.save({ name: "x" }); - ok is a boolean
/// App.Bind("save", [](const KJSON& jArg) -> KJSON
/// {
///     return jArg["name"].String() == "x";
/// });
///
/// return App.Run(); // blocks in the UI loop until the window closes or Quit() is called
/// @endcode
///
/// @par Threads
/// Run() must be called on the main thread, it owns the UI loop. Route handlers
/// run in KREST threads and may call Eval(), Navigate() and SetTitle() at any
/// time, the calls are dispatched to the UI thread. Bind() handlers run on the
/// UI thread and should return quickly.
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
		/// initial window size in points
		uint32_t       iWidth  { 1024 };
		uint32_t       iHeight {  768 };
		/// open a window? false runs the server alone until Quit()
		bool           bWindow { true };
		/// enable the web inspector in the window
		bool           bDebug  { false };
	};

	/// JavaScript to C++ handler: one JSON argument in, one JSON result out.
	/// An exception rejects the Promise with { "error": "<what>" }
	using Handler = std::function<KJSON(const KJSON& jArg)>;

	/// starts the loopback server - check HasError() afterwards. Routes must
	/// outlive the KWebApp
	KWebApp(Options Options, const KRESTRoutes& Routes);
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
	/// run the window until it is closed or Quit() is called - without window
	/// until Quit(). Stops the server before returning
	/// @return 0, or 1 when the server had not started
	int  Run();
	/// end Run(), from any thread
	void Quit();
	/// true once Quit() was called or the window has closed. Long running route
	/// handlers, e.g. websocket loops, have to end then, else Run() cannot stop
	/// the server and return
	bool IsQuitting() const { return m_bQuit; }

	/// the loopback server's port
	uint16_t    GetPort()  const { return m_iPort;  }
	/// this instance's access token - a request has to carry it in the cookie "kwa"
	KStringView GetToken() const { return m_sToken; }
	/// the URL for a first request to a path on the loopback server, carrying the token
	KString     GetEnterURL(KStringView sPath = "/") const;

//----------
private:
//----------

	class WebView; // wraps the webview object, keeps its header out of ours

	void    Guard      (KRESTServer& HTTP);
	void    AddBinding (WebView& View, KStringView sName, const Handler& Handler);
	KString InitScript () const;

	static bool    IsIdentifier(KStringView sName);
	static KString ShimScript  (KStringView sName);

	Options                           m_Options;
	const KRESTRoutes&                m_Routes;
	std::unique_ptr<KREST>            m_REST;
	std::unique_ptr<WebView>          m_WebView;
	std::function<void(KRESTServer&)> m_UserPreRoute;
	std::map<KString, Handler>        m_Bindings;
	KString                           m_sToken;
	KString                           m_sStartPath { "/" };
	mutable std::mutex                m_Mutex;
	std::condition_variable           m_Idle;
	std::atomic<bool>                 m_bQuit { false };
	uint16_t                          m_iPort { 0 };

}; // KWebApp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
