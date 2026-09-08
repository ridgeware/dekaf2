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


#include <dekaf2/web/app/kwebapp.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/web/app/bits/kwebapp_platform.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksignals.h>

#include <webview.h>
#include <csignal>
#include <future>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringView sCookieName = "kwa";
constexpr KStringView sEnterPath  = "/_kwa/enter";
constexpr KStringView sLoginPath  = "/login";
constexpr KStringView sLogoutPath = "/logout";
constexpr KStringView sHealthPath = "/healthz";
constexpr KStringView sLivePath   = "/_kwa/live";
constexpr KStringView sLiveScript = "/_kwa/live.js";
constexpr KStringView sBindPrefix = "__kwa_";
constexpr KStringView sWindowFile = "window.json";
constexpr KStringView sLockFile   = "instance.lock";

// the page side of the live connection: opens the websocket, reconnects, queues
// what is sent before the connection is up, hands parsed JSON to one listener
constexpr KStringView sLiveClient = R"js(
// kLive: the page's live connection to the application - the same in the window and in a browser
window.kLive = (() => {
	const url = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/_kwa/live';
	let ws = null, listener = null, opened = null, queue = [];
	const connect = () => {
		ws = new WebSocket(url);
		ws.onopen    = () => { for (const m of queue) ws.send(m); queue = []; if (opened) opened(); };
		ws.onmessage = (ev) => { if (listener) listener(JSON.parse(ev.data)); };
		ws.onclose   = () => { ws = null; setTimeout(connect, 2000); };
	};
	connect();
	return {
		send:   (obj) => { const m = JSON.stringify(obj); if (ws && ws.readyState === 1) ws.send(m); else queue.push(m); },
		on:     (fn)  => { listener = fn; },
		onopen: (fn)  => { opened = fn; if (ws && ws.readyState === 1) fn(); }
	};
})();
)js";

#if DEKAF2_IS_MACOS
constexpr KStringView sPlatform   = "macos";
#elif DEKAF2_IS_WINDOWS
constexpr KStringView sPlatform   = "windows";
#else
constexpr KStringView sPlatform   = "linux";
#endif

// the login page's look: the same tokens as a page would use, both themes
constexpr KStringView sLoginStyle = R"css(
:root { color-scheme: light dark;
	--bg: #ffffff; --fg: #1d1d1f; --muted: #6e6e73; --panel: #f5f5f7; --border: #d2d2d7; --accent: #0a66c2; --error: #c0392b; }
@media (prefers-color-scheme: dark) { :root {
	--bg: #1c1c1e; --fg: #f5f5f7; --muted: #98989d; --panel: #2c2c2e; --border: #3a3a3c; --accent: #4c9aff; --error: #ff6b6b; } }
body { margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center; background: var(--bg); color: var(--fg);
	font: 15px/1.4 -apple-system, "Segoe UI", Helvetica, sans-serif; }
form { width: 20em; padding: 2em; border: 1px solid var(--border); border-radius: 10px; background: var(--panel); display: flex; flex-direction: column; gap: .8em; }
h1 { font-size: 1.2em; margin: 0 0 .5em; }
label { display: flex; flex-direction: column; gap: .3em; color: var(--muted); font-size: .9em; }
input { font: inherit; padding: .5em; border: 1px solid var(--border); border-radius: 6px; background: var(--bg); color: var(--fg); }
button { font: inherit; padding: .6em; border: 0; border-radius: 6px; background: var(--accent); color: #fff; cursor: pointer; margin-top: .5em; }
.error { color: var(--error); margin: 0; }
)css";

//-----------------------------------------------------------------------------
std::string Std(KStringView sView)
//-----------------------------------------------------------------------------
{
	return std::string(sView.data(), sView.size());
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KStringViewZ kGetWebViewVersion()
//-----------------------------------------------------------------------------
{
	return WEBVIEW_VERSION_NUMBER;

} // kGetWebViewVersion

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the window - webview::webview itself, but named in our header so that the
/// webview header stays private to this file
class KWebApp::WebView : public webview::webview
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	using webview::webview::webview;
};

//-----------------------------------------------------------------------------
KWebApp::KWebApp(Options Options, KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
, m_Routes(Routes)
, m_sToken(kHex(kGetRandom(32)))
{
	// our routes, in the table both servers serve - the handlers know which side
	// they answer for
	m_Routes.AddRoute(KString(sHealthPath)).Get ([this](KRESTServer& HTTP) { Health(HTTP);     });
	m_Routes.AddRoute(KString(sLoginPath )).Get ([this](KRESTServer& HTTP) { LoginPage(HTTP);  });
	m_Routes.AddRoute(KString(sLoginPath )).Post([this](KRESTServer& HTTP) { Login(HTTP);      }).Parse(KRESTRoute::WWWFORM);
	m_Routes.AddRoute(KString(sLogoutPath)).Post([this](KRESTServer& HTTP) { Logout(HTTP);     });
	m_Routes.AddRoute(KString(sLiveScript)).Get ([this](KRESTServer& HTTP) { LiveScript(HTTP); });
	m_Routes.AddRoute(KString(sLivePath  )).Get ([this](KRESTServer& HTTP) { Live(HTTP);       }).Parse(KRESTRoute::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	// ~/.config/<app>/ keeps the window geometry and the instance lock
	m_sConfigDir = m_Options.sAppName.empty() ? kGetConfigPath() : kFormat("{}/.config/{}", kGetHome(), m_Options.sAppName);
	kCreateDir(m_sConfigDir);

	if (m_Options.bWindow && m_Options.bSingleInstance && !LockInstance())
	{
		// the other instance's window is in front now, we are done
		m_bOtherInstance = true;
		SetError(kFormat("{} is already running", m_Options.sAppName.empty() ? Dekaf::getInstance().GetProgName() : KStringViewZ(m_Options.sAppName)));
		return;
	}

	AddBuiltins();

	// a shutdown signal ends Run(), which stops the servers in order - so the
	// servers themselves stay out of the signal handling
	m_Options.Loopback.RegisterSignalsForShutdown.clear();
	m_Options.Network.RegisterSignalsForShutdown.clear();
	CatchShutdownSignals();

	if (!StartLoopback())
	{
		return;
	}

	if (m_Options.bNetwork && !StartNetwork())
	{
		return;
	}

} // ctor

//-----------------------------------------------------------------------------
KWebApp::~KWebApp()
//-----------------------------------------------------------------------------
{
	Quit();

	// the signal handlers capture this
	if (auto* Signals = Dekaf::getInstance().Signals())
	{
		for (auto& Handler : m_PreviousSignalHandlers)
		{
			if (Handler.second)
			{
				Signals->SetSignalHandler(Handler.first, Handler.second);
			}
			else
			{
				Signals->SetDefaultHandler(Handler.first);
			}
		}
	}

} // dtor

//-----------------------------------------------------------------------------
void KWebApp::CatchShutdownSignals()
//-----------------------------------------------------------------------------
{
	auto* Signals = Dekaf::getInstance().Signals();

	if (!Signals)
	{
		kDebug(1, "no signal handler thread, a shutdown signal will not end Run() - start with KInit(true)");
		return;
	}

	for (auto iSignal : { SIGINT, SIGTERM })
	{
		// chain a handler the application had installed before
		auto Previous = Signals->GetSignalHandler(iSignal);
		m_PreviousSignalHandlers[iSignal] = Previous;

		Signals->SetSignalHandler(iSignal, [this, Previous](int iSignal)
		{
			kDebug(1, "received {}, ending Run()", kTranslateSignal(iSignal));

			if (auto* Signals = Dekaf::getInstance().Signals())
			{
				// the next one terminates the process the default way
				Signals->SetDefaultHandler(iSignal);
			}

			Quit();

			if (Previous)
			{
				Previous(iSignal);
			}
		});
	}

} // CatchShutdownSignals

//-----------------------------------------------------------------------------
bool KWebApp::StartLoopback()
//-----------------------------------------------------------------------------
{
	// the address is not negotiable, and the window cannot accept a self-signed
	// certificate, so it stays plain HTTP. Port 0 lets the OS pick one
	auto& Loopback = m_Options.Loopback;

	if (!Loopback.sCert.empty() || !Loopback.sKey.empty())
	{
		kDebug(1, "dropping TLS certificate and key, the loopback server is plain HTTP");
		Loopback.sCert.clear();
		Loopback.sKey.clear();
	}

	Loopback.Type                 = KREST::HTTP;
	Loopback.sBindAddress         = "127.0.0.1";
	Loopback.bBlocking            = false;
	Loopback.bCreateEphemeralCert = false;

	// our guard runs before every route, an existing callback afterwards
	m_UserPreRoute = std::move(Loopback.PreRouteCallback);

	Loopback.PreRouteCallback = [this](KRESTServer& HTTP)
	{
		Guard(HTTP);

		if (m_UserPreRoute)
		{
			m_UserPreRoute(HTTP);
		}
	};

	m_REST = std::make_unique<KREST>();

	if (!m_REST->Execute(Loopback, m_Routes))
	{
		SetError(*m_REST);
		m_REST.reset();
		return false;
	}

	m_iPort = m_REST->GetPort();

	kDebug(2, "loopback server listening on 127.0.0.1:{}", m_iPort);

	return true;

} // StartLoopback

//-----------------------------------------------------------------------------
bool KWebApp::StartNetwork()
//-----------------------------------------------------------------------------
{
	auto& Network = m_Options.Network;

	Network.Type      = KREST::HTTP;
	Network.bBlocking = false;

	if (Network.sCert.empty() && Network.sKey.empty())
	{
		// KREST creates the certificate and keeps it under ~/.config/<program>/tls/
		Network.bCreateEphemeralCert = true;
		kDebug(1, "the network server uses a self-signed certificate - browsers will warn, use a real one for production");
	}

	// the network is not the window: limit what a client may send and open
	if (Network.iMaxRequestBodySize == KRESTServer::Options{}.iMaxRequestBodySize)
	{
		Network.iMaxRequestBodySize = 16 * 1024 * 1024;
	}

	if (!Network.ConnectionLimiter)
	{
		Network.SetConnectionLimit(20);
	}

	// sessions: in memory unless the application brought a store
	if (!m_Options.SessionStore)
	{
		m_Options.SessionStore = std::make_unique<KSessionMemoryStore>();
	}

	auto Config      = m_Options.Session;
	Config.bSecure   = true;
	Config.bHttpOnly = true;

	m_Session = std::make_unique<KSession>(std::move(m_Options.SessionStore), std::move(Config));

	if (m_Options.Authenticate)
	{
		m_Session->SetAuthenticator(m_Options.Authenticate);
	}
	else
	{
		kDebug(1, "no authenticator set, nobody can log in on the network server");
	}

	m_UserNetworkPreRoute = std::move(Network.PreRouteCallback);

	Network.PreRouteCallback = [this](KRESTServer& HTTP)
	{
		NetworkGuard(HTTP);

		if (m_UserNetworkPreRoute)
		{
			m_UserNetworkPreRoute(HTTP);
		}
	};

	m_Network = std::make_unique<KREST>();

	if (!m_Network->Execute(Network, m_Routes))
	{
		SetError(*m_Network);
		m_Network.reset();
		return false;
	}

	m_iNetworkPort = m_Network->GetPort();

	kDebug(1, "network server listening on {}:{}", Network.sBindAddress.empty() ? "*" : Network.sBindAddress, m_iNetworkPort);

	return true;

} // StartNetwork

//-----------------------------------------------------------------------------
bool KWebApp::IsFromWindow(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	// each server runs with its own options object
	return &HTTP.GetOptions() == static_cast<const KRESTServer::Options*>(&m_Options.Loopback);

} // IsFromWindow

//-----------------------------------------------------------------------------
bool KWebApp::IsIdentifier(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (sName.empty() || KASCII::kIsDigit(sName.front()))
	{
		return false;
	}

	for (auto ch : sName)
	{
		if (!KASCII::kIsAlNum(ch) && ch != '_')
		{
			return false;
		}
	}

	return true;

} // IsIdentifier

//-----------------------------------------------------------------------------
KString KWebApp::SafePath(KStringView sPath)
//-----------------------------------------------------------------------------
{
	// a path on our own origin, nothing else
	if (sPath.starts_with('/') && !sPath.starts_with("//"))
	{
		return sPath;
	}

	return "/";

} // SafePath

//-----------------------------------------------------------------------------
void KWebApp::Redirect(KRESTServer& HTTP, KStringView sLocation)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sLocation);
	throw KHTTPError { KHTTPError::H302_MOVED_TEMPORARILY, "" };

} // Redirect

//-----------------------------------------------------------------------------
KString KWebApp::ShimScript(KStringView sName)
//-----------------------------------------------------------------------------
{
	// one JSON argument, undefined becomes null so that the handler always gets a value
	KString sScript;
	sScript += "window.kNative = window.kNative || {};\n";
	sScript += "window.kNative.";
	sScript += sName;
	sScript += " = (arg) => window.";
	sScript += sBindPrefix;
	sScript += sName;
	sScript += "(arg === undefined ? null : arg);\n";
	return sScript;

} // ShimScript

//-----------------------------------------------------------------------------
KString KWebApp::InitScript() const
//-----------------------------------------------------------------------------
{
	// runs before every document: the synchronous part of window.kNative
	KString sScript;
	sScript += "window.kNative = window.kNative || {};\n";
	sScript += "window.kNative.platform = () => \"";
	sScript += sPlatform;
	sScript += "\";\n";
	sScript += "window.kNative.version = () => ";
	sScript += KJSON(m_Options.sVersion).dump();
	sScript += ";\n";
	return sScript;

} // InitScript

//-----------------------------------------------------------------------------
void KWebApp::AddBinding(WebView& View, KStringView sName, const Handler& Handler)
//-----------------------------------------------------------------------------
{
	auto Callback = [this, Handler](const std::string& sID, const std::string& sArgs, void*)
	{
		// runs on the UI thread. The arguments arrive as a JSON array, we pass
		// the first one on and give the JSON result back, or a rejection
		KJSON jResult;
		int   iStatus { 0 };

		DEKAF2_TRY
		{
			auto jArgs = kjson::Parse(sArgs);
			jResult = Handler(jArgs.is_array() && !jArgs.empty() ? jArgs[0] : KJSON{});
		}
		DEKAF2_CATCH(const std::exception& ex)
		{
			kDebug(1, "bound handler threw: {}", ex.what());
			jResult = KJSON::object();
			jResult["error"] = ex.what();
			iStatus = 1;
		}

		std::lock_guard<std::mutex> Lock(m_Mutex);

		if (m_WebView && !m_bQuit)
		{
			m_WebView->resolve(sID, iStatus, jResult.dump().ToStdString());
		}
	};

	KString sBound(sBindPrefix);
	sBound += sName;

	View.bind(sBound.ToStdString(), std::move(Callback), nullptr);
	// the shim for documents to come, and for the one currently shown
	View.init(ShimScript(sName).ToStdString());
	View.eval(ShimScript(sName).ToStdString());

} // AddBinding

//-----------------------------------------------------------------------------
bool KWebApp::Bind(KStringView sName, Handler Handler)
//-----------------------------------------------------------------------------
{
	if (!IsIdentifier(sName))
	{
		kDebug(1, "not a JavaScript identifier: '{}'", sName);
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto Inserted = m_Bindings.emplace(sName, std::move(Handler));

	if (!Inserted.second)
	{
		kDebug(1, "already bound: '{}'", sName);
		return false;
	}

	if (m_WebView && !m_bQuit)
	{
		// the window exists: register on the UI thread
		auto* pView = m_WebView.get();
		auto& Entry = *Inserted.first;

		m_WebView->dispatch([this, pView, &Entry]
		{
			if (!m_bQuit)
			{
				AddBinding(*pView, Entry.first, Entry.second);
			}
		});
	}

	return true;

} // Bind

//-----------------------------------------------------------------------------
void KWebApp::Eval(KStringView sJavaScript)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_WebView || m_bQuit)
	{
		kDebug(2, "no window, ignoring script");
		return;
	}

	auto* pView = m_WebView.get();

	m_WebView->dispatch([this, pView, sScript = Std(sJavaScript)]
	{
		if (!m_bQuit)
		{
			pView->eval(sScript);
		}
	});

} // Eval

//-----------------------------------------------------------------------------
void KWebApp::Navigate(KStringView sPath)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_WebView)
	{
		// before Run(): this is the start page
		m_sStartPath = sPath;
		return;
	}

	if (m_bQuit)
	{
		return;
	}

	auto* pView = m_WebView.get();

	// the token is in the cookie by now
	m_WebView->dispatch([this, pView, sURL = Std(kFormat("http://127.0.0.1:{}{}", m_iPort, sPath))]
	{
		if (!m_bQuit)
		{
			pView->navigate(sURL);
		}
	});

} // Navigate

//-----------------------------------------------------------------------------
void KWebApp::SetTitle(KStringView sTitle)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_WebView)
	{
		m_Options.sTitle = sTitle;
		return;
	}

	if (m_bQuit)
	{
		return;
	}

	auto* pView = m_WebView.get();

	m_WebView->dispatch([this, pView, sTitle = Std(sTitle)]
	{
		if (!m_bQuit)
		{
			pView->set_title(sTitle);
		}
	});

} // SetTitle

//-----------------------------------------------------------------------------
KString KWebApp::GetEnterURL(KStringView sPath) const
//-----------------------------------------------------------------------------
{
	KString sURL = kFormat("http://127.0.0.1:{}{}?token={}&next=", m_iPort, sEnterPath, m_sToken);
	kUrlEncode(sPath, sURL, URIPart::Query);
	return sURL;

} // GetEnterURL

//-----------------------------------------------------------------------------
bool KWebApp::LockInstance()
//-----------------------------------------------------------------------------
{
	// the lock file carries the holder's process id, so that a second start can
	// bring the first one's window to the front
	auto sLock = kFormat("{}/{}", m_sConfigDir, sLockFile);

	if (!kFileExists(sLock))
	{
		kWriteFile(sLock, "");
	}

	m_InstanceLock = std::make_unique<KFileLock>(sLock, KFileLock::Exclusive, /*bWait*/false);

	if (*m_InstanceLock)
	{
		kWriteFile(sLock, kFormat("{}\n", kGetPid()));
		return true;
	}

	m_InstanceLock.reset();

	KString sPID;
	{
		KInFile File(sLock);
		File.ReadLine(sPID);
	}

	auto iPID = sPID.Trim().Int64();
	kDebug(1, "another instance runs as process {}", iPID);

	if (iPID > 0 && !kwebapp::ActivateProcess(iPID))
	{
		kDebug(1, "cannot bring process {} to the front", iPID);
	}

	return false;

} // LockInstance

//-----------------------------------------------------------------------------
void KWebApp::AddBuiltins()
//-----------------------------------------------------------------------------
{
	// what every page may ask the desktop for - the names are taken, Bind() refuses them
	m_Bindings.emplace("openExternal", [this](const KJSON& jArg) -> KJSON
	{
		return OpenExternal(jArg["url"].String());
	});

	m_Bindings.emplace("notify", [this](const KJSON& jArg) -> KJSON
	{
		return Notify(jArg["title"].String(), jArg["body"].String());
	});

	m_Bindings.emplace("openFile", [this](const KJSON& jArg) -> KJSON
	{
		std::vector<KString> Extensions;

		for (const auto& jExtension : jArg["extensions"])
		{
			Extensions.push_back(jExtension.String());
		}

		KJSON jFiles = KJSON::array();

		for (const auto& sFile : OpenFileDialog(jArg["title"].String(), Extensions, jArg["multiple"].Bool(), jArg["directories"].Bool()))
		{
			jFiles.push_back(sFile);
		}

		return jFiles;
	});

	m_Bindings.emplace("saveFile", [this](const KJSON& jArg) -> KJSON
	{
		std::vector<KString> Extensions;

		for (const auto& jExtension : jArg["extensions"])
		{
			Extensions.push_back(jExtension.String());
		}

		auto sFile = SaveFileDialog(jArg["title"].String(), jArg["name"].String(), Extensions);
		return sFile.empty() ? KJSON{} : KJSON(sFile);
	});

} // AddBuiltins

//-----------------------------------------------------------------------------
void* KWebApp::WindowHandle()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_WebView || m_bQuit)
	{
		return nullptr;
	}

	auto Window = m_WebView->window();
	return Window.ok() ? Window.value() : nullptr;

} // WindowHandle

//-----------------------------------------------------------------------------
void KWebApp::RunOnUI(std::function<void()> Call)
//-----------------------------------------------------------------------------
{
	if (kwebapp::IsMainThread())
	{
		Call();
		return;
	}

	// hand it to the UI loop and wait for it
	std::promise<void> Done;
	auto Waiter = Done.get_future();

	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		if (!m_WebView || m_bQuit)
		{
			kDebug(1, "no window, cannot run on the UI thread");
			return;
		}

		m_WebView->dispatch([&Call, &Done]
		{
			Call();
			Done.set_value();
		});
	}

	Waiter.wait();

} // RunOnUI

//-----------------------------------------------------------------------------
bool KWebApp::OpenExternal(KStringView sURL)
//-----------------------------------------------------------------------------
{
	// the browser, not the shell: no file, no javascript, nothing local
	auto sLower = KString(sURL.Left(8)).ToLowerASCII();

	if (!sLower.starts_with("http://") && !sLower.starts_with("https://") && !sLower.starts_with("mailto:"))
	{
		kDebug(1, "refusing to open '{}'", sURL);
		return false;
	}

	return kwebapp::OpenExternal(sURL);

} // OpenExternal

//-----------------------------------------------------------------------------
bool KWebApp::Notify(KStringView sTitle, KStringView sBody)
//-----------------------------------------------------------------------------
{
	return kwebapp::Notify(sTitle, sBody);

} // Notify

//-----------------------------------------------------------------------------
std::vector<KString> KWebApp::OpenFileDialog(KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Files;

	RunOnUI([&]
	{
		Files = kwebapp::OpenFileDialog(WindowHandle(), sTitle, Extensions, bMultiple, bDirectories);
	});

	return Files;

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString KWebApp::SaveFileDialog(KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	KString sFile;

	RunOnUI([&]
	{
		sFile = kwebapp::SaveFileDialog(WindowHandle(), sTitle, sSuggestedName, Extensions);
	});

	return sFile;

} // SaveFileDialog

//-----------------------------------------------------------------------------
void KWebApp::MenuAction(KStringView sAction)
//-----------------------------------------------------------------------------
{
	// on the UI thread: a bound handler of that name, else the page
	Handler Handler;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		auto it = m_Bindings.find(sAction);

		if (it != m_Bindings.end())
		{
			Handler = it->second;
		}
	}

	if (Handler)
	{
		DEKAF2_TRY
		{
			Handler(KJSON{});
		}
		DEKAF2_CATCH(const std::exception& ex)
		{
			kDebug(1, "menu action '{}' threw: {}", sAction, ex.what());
		}
		return;
	}

	Eval(kFormat("window.dispatchEvent(new CustomEvent('kwa-menu', {{ detail: {} }}));", KJSON(sAction).dump()));

} // MenuAction

//-----------------------------------------------------------------------------
void KWebApp::LoadWindowFrame()
//-----------------------------------------------------------------------------
{
	KString sJSON;
	{
		KInFile File(kFormat("{}/{}", m_sConfigDir, sWindowFile));

		if (!File.is_open() || !File.ReadRemaining(sJSON))
		{
			return;
		}
	}

	auto jFrame = kjson::Parse(sJSON);

	kwebapp::WindowFrame Frame;
	Frame.iX      = static_cast<int32_t>(jFrame["x"     ].Int64());
	Frame.iY      = static_cast<int32_t>(jFrame["y"     ].Int64());
	Frame.iWidth  = static_cast<int32_t>(jFrame["width" ].Int64());
	Frame.iHeight = static_cast<int32_t>(jFrame["height"].Int64());

	kwebapp::SetWindowFrame(WindowHandle(), Frame);

} // LoadWindowFrame

//-----------------------------------------------------------------------------
void KWebApp::SaveWindowFrame()
//-----------------------------------------------------------------------------
{
	// the last frame the window reported - the window itself is gone by now
	KJSON jFrame;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		jFrame = m_jWindowFrame;
	}

	if (!jFrame.is_null())
	{
		kWriteFile(kFormat("{}/{}", m_sConfigDir, sWindowFile), jFrame.dump());
	}

} // SaveWindowFrame

//-----------------------------------------------------------------------------
void KWebApp::Guard(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// runs before routing, for every request of the loopback server - a
	// rejection is a 403, and there is no path that gets through without the token

	const auto  sSelf = kFormat("127.0.0.1:{}", m_iPort);
	const auto& sHost = HTTP.Request.Headers.Get(KHTTPHeader::HOST);

	if (sHost != sSelf)
	{
		kDebug(1, "rejecting host '{}'", sHost);
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "wrong host" };
	}

	const auto& sOrigin = HTTP.Request.Headers.Get(KHTTPHeader::ORIGIN);

	if (!sOrigin.empty())
	{
		if (sOrigin != kFormat("http://{}", sSelf))
		{
			kDebug(1, "rejecting origin '{}'", sOrigin);
			throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "wrong origin" };
		}
	}
	else if (HTTP.Request.Headers.Get(KHTTPHeader::UPGRADE).ToLowerASCII() == "websocket")
	{
		// browsers always send the origin with a websocket upgrade
		kDebug(1, "rejecting websocket upgrade without origin");
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "missing origin" };
	}

	const auto& sFetchSite = HTTP.Request.Headers.Get(KHTTPHeader("Sec-Fetch-Site"));

	if (!sFetchSite.empty() && sFetchSite != "same-origin" && sFetchSite != "none")
	{
		kDebug(1, "rejecting fetch site '{}'", sFetchSite);
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "wrong fetch site" };
	}

	if (HTTP.Request.Resource.Path.get() == sEnterPath)
	{
		// the first request of the window carries the token as query parameter
		// and gets it back as cookie, then continues without it in the URL
		if (!KDigest::ConstantTimeCompare(HTTP.GetQueryParm("token"), m_sToken))
		{
			kDebug(1, "rejecting entry with wrong token");
			throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "wrong token" };
		}

		HTTP.SetCookie(sCookieName, m_sToken, "Path=/; HttpOnly; SameSite=Strict");
		Redirect(HTTP, SafePath(HTTP.GetQueryParm("next", "/")));
	}

	if (!KDigest::ConstantTimeCompare(HTTP.GetCookie(sCookieName), m_sToken))
	{
		kDebug(1, "rejecting request without token: {}", HTTP.Request.Resource.Path.get());
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "missing token" };
	}

} // Guard

//-----------------------------------------------------------------------------
void KWebApp::NetworkGuard(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// runs before routing, for every request of the network server

	auto& Headers = HTTP.Response.Headers;
	Headers.Set(KHTTPHeader::STRICT_TRANSPORT_SECURITY, "max-age=31536000");
	Headers.Set(KHTTPHeader::X_CONTENT_TYPE_OPTIONS,    "nosniff");
	Headers.Set(KHTTPHeader::X_FRAME_OPTIONS,           "DENY");
	Headers.Set(KHTTPHeader("Referrer-Policy"),         "same-origin");

	if (!m_Options.sContentSecurityPolicy.empty())
	{
		Headers.Set(KHTTPHeader("Content-Security-Policy"), m_Options.sContentSecurityPolicy);
	}

	const auto& sPath = HTTP.Request.Resource.Path.get();

	if (sPath == sHealthPath || sPath == sLoginPath || sPath == sLogoutPath)
	{
		// open, the handlers do the rest
		return;
	}

	KRESTSession Session(*m_Session, HTTP);

	if (!Session.IsAuthenticated())
	{
		bool bUpgrade = HTTP.Request.Headers.Get(KHTTPHeader::UPGRADE).ToLowerASCII() == "websocket";

		if (HTTP.Request.Method == KHTTPMethod::GET && !bUpgrade)
		{
			// a browser: to the login page, and back here afterwards
			KString sResource;
			HTTP.Request.Resource.Serialize(sResource);
			KString sLocation = kFormat("{}?next=", sLoginPath);
			kUrlEncode(sResource, sLocation, URIPart::Query);
			Redirect(HTTP, sLocation);
		}

		throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "login required" };
	}

	for (const auto& sPrefix : m_Options.WindowOnlyPaths)
	{
		if (sPath.starts_with(sPrefix))
		{
			kDebug(1, "refusing window-only path {} for network user '{}'", sPath, Session.GetUser());
			throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "only available in the desktop window" };
		}
	}

} // NetworkGuard

//-----------------------------------------------------------------------------
void KWebApp::LiveScript(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, "text/javascript; charset=UTF-8");
	HTTP.SetRawOutput(KString(sLiveClient));

} // LiveScript

//-----------------------------------------------------------------------------
void KWebApp::Live(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// the guards have let this request through: it is the window, or a login
	auto pClient = std::make_shared<Client>();
	pClient->bFromWindow = IsFromWindow(HTTP);

	if (!pClient->bFromWindow && m_Session)
	{
		pClient->sUser = KRESTSession(*m_Session, HTTP).GetUser();
	}

	{
		std::lock_guard<std::mutex> Lock(m_ClientMutex);
		pClient->iID = ++m_iNextClient;
	}

	// the connection lives in the server's websocket server from here on
	HTTP.SetWebSocketConnectHandler([this, pClient](KWebSocket& WebSocket)
	{
		{
			std::lock_guard<std::mutex> Lock(m_ClientMutex);
			m_Clients[pClient->iID] = LiveClient { *pClient, WebSocket.GetServer(), WebSocket.GetHandle() };
		}

		kDebug(2, "live connection {} from {}", pClient->iID, pClient->bFromWindow ? "the window" : pClient->sUser);

		if (m_OnConnect)
		{
			m_OnConnect(*pClient);
		}
	});

	HTTP.SetWebSocketHandler([this, pClient](KWebSocket& WebSocket)
	{
		if (!m_OnMessage)
		{
			return;
		}

		auto jMessage = kjson::Parse(WebSocket.GetFrame().GetPayload());

		if (jMessage.is_null())
		{
			kDebug(1, "live connection {} sent no JSON", pClient->iID);
			return;
		}

		m_OnMessage(*pClient, jMessage);
	});

	HTTP.SetWebSocketCloseHandler([this, pClient](std::size_t)
	{
		std::lock_guard<std::mutex> Lock(m_ClientMutex);
		m_Clients.erase(pClient->iID);
	});

} // Live

//-----------------------------------------------------------------------------
void KWebApp::OnConnect(ConnectHandler Handler)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ClientMutex);
	m_OnConnect = std::move(Handler);

} // OnConnect

//-----------------------------------------------------------------------------
void KWebApp::OnMessage(MessageHandler Handler)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ClientMutex);
	m_OnMessage = std::move(Handler);

} // OnMessage

//-----------------------------------------------------------------------------
std::size_t KWebApp::Broadcast(const KJSON& jMessage)
//-----------------------------------------------------------------------------
{
	// copy the targets, Send() may take its time
	std::vector<LiveClient> Targets;
	{
		std::lock_guard<std::mutex> Lock(m_ClientMutex);
		Targets.reserve(m_Clients.size());

		for (const auto& Entry : m_Clients)
		{
			Targets.push_back(Entry.second);
		}
	}

	std::size_t iSent { 0 };

	for (const auto& Target : Targets)
	{
		if (Target.pServer && Target.pServer->Send(Target.iHandle, jMessage))
		{
			++iSent;
		}
	}

	return iSent;

} // Broadcast

//-----------------------------------------------------------------------------
bool KWebApp::Send(const Client& Client, const KJSON& jMessage)
//-----------------------------------------------------------------------------
{
	LiveClient Target;
	{
		std::lock_guard<std::mutex> Lock(m_ClientMutex);
		auto it = m_Clients.find(Client.iID);

		if (it == m_Clients.end())
		{
			return false;
		}

		Target = it->second;
	}

	return Target.pServer && Target.pServer->Send(Target.iHandle, jMessage);

} // Send

//-----------------------------------------------------------------------------
std::size_t KWebApp::GetClientCount() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ClientMutex);
	return m_Clients.size();

} // GetClientCount

//-----------------------------------------------------------------------------
void KWebApp::Health(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	if (IsFromWindow(HTTP))
	{
		// the loopback server has no unauthenticated path
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "" };
	}

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
	HTTP.SetRawOutput("ok\n");

} // Health

//-----------------------------------------------------------------------------
void KWebApp::LoginPage(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	if (IsFromWindow(HTTP))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "" };
	}

	html::Page Page(kFormat("{} - login", m_Options.sTitle), "en");
	Page.Head().Add<html::Meta>("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(sLoginStyle);

	auto Form = Page.Add<html::Form>(sLoginPath);
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Heading>(1, m_Options.sTitle);

	if (HTTP.GetQueryParm("error") == "1")
	{
		Form.Add<html::Element>("p", "error").AddText("Wrong user name or password.");
	}

	Form.Add<html::Element>("label").AddText("User name")
		.Add<html::Input>("user", "", html::Input::TEXT).SetAutofocus(true).SetRequired(true).SetAttribute("autocomplete", "username");
	Form.Add<html::Element>("label").AddText("Password")
		.Add<html::Input>("password", "", html::Input::PASSWORD).SetRequired(true).SetAttribute("autocomplete", "current-password");
	Form.Add<html::Input>("next", SafePath(HTTP.GetQueryParm("next", "/")), html::Input::HIDDEN);
	Form.Add<html::Button>("Sign in");

	Page.Generate();

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Print());

} // LoginPage

//-----------------------------------------------------------------------------
void KWebApp::Login(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	if (IsFromWindow(HTTP))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "" };
	}

	auto sClient = HTTP.GetRemoteIP();

	// password guessing: a few attempts per address, then a long wait
	if (!m_LoginLimiter.Check(sClient))
	{
		kDebug(1, "too many login attempts from {}", sClient);
		throw KHTTPError { KHTTPError::H4xx_TOOMANYREQUESTS, "too many login attempts, try again later" };
	}

	// the form fields arrive as query parms through the WWWFORM parser
	auto sUser = HTTP.GetQueryParm("user");
	auto sNext = SafePath(HTTP.GetQueryParm("next", "/"));

	KRESTSession Session(*m_Session, HTTP);

	if (!Session.Login(sUser, HTTP.GetQueryParm("password")))
	{
		kDebug(1, "failed login for '{}' from {}", sUser, sClient);
		KString sLocation = kFormat("{}?error=1&next=", sLoginPath);
		kUrlEncode(sNext, sLocation, URIPart::Query);
		Redirect(HTTP, sLocation);
	}

	kDebug(1, "user '{}' logged in from {}", sUser, sClient);
	Redirect(HTTP, sNext);

} // Login

//-----------------------------------------------------------------------------
void KWebApp::Logout(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	if (IsFromWindow(HTTP))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "" };
	}

	KRESTSession Session(*m_Session, HTTP);
	Session.Logout();
	Redirect(HTTP, sLoginPath);

} // Logout

//-----------------------------------------------------------------------------
int KWebApp::Run()
//-----------------------------------------------------------------------------
{
	if (!m_REST || (m_Options.bNetwork && !m_Network))
	{
		// the constructor has set the error
		return 1;
	}

	if (m_Options.bWindow)
	{
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);

			m_WebView = std::make_unique<WebView>(m_Options.bDebug, nullptr);
			m_WebView->set_title(m_Options.sTitle.ToStdString());
			m_WebView->set_size(static_cast<int>(m_Options.iWidth), static_cast<int>(m_Options.iHeight), WEBVIEW_HINT_NONE);
			// the bridge has to be complete before the first document loads
			m_WebView->init(InitScript().ToStdString());

			for (const auto& Binding : m_Bindings)
			{
				AddBinding(*m_WebView, Binding.first, Binding.second);
			}

			m_WebView->navigate(GetEnterURL(m_sStartPath).ToStdString());
		}

		// the desktop side: the remembered geometry, and the menus - installed once
		// the loop runs, else AppKit adds its own Edit entries more than once
		if (m_Options.bRememberWindow)
		{
			LoadWindowFrame();
			kwebapp::WatchWindowFrame(WindowHandle(), [this](const kwebapp::WindowFrame& Frame)
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_jWindowFrame["x"]      = Frame.iX;
				m_jWindowFrame["y"]      = Frame.iY;
				m_jWindowFrame["width"]  = Frame.iWidth;
				m_jWindowFrame["height"] = Frame.iHeight;
			});
		}

		auto sAppName = m_Options.sAppName.empty() ? KString(Dekaf::getInstance().GetProgName()) : m_Options.sAppName;

		m_WebView->dispatch([this, sAppName]
		{
			kwebapp::SetMenu(sAppName, m_Options.jMenus,
			                 [this](KStringView sAction) { MenuAction(sAction); },
			                 [this]                      { Quit();              });
		});

		// the UI loop, until the window closes or Quit() is called
		m_WebView->run();

		if (m_Options.bRememberWindow)
		{
			SaveWindowFrame();
		}

		if (!m_bQuit && m_Network && !m_Options.bQuitOnWindowClose)
		{
			// the window is gone but the servers stay - the loopback one too, its
			// long running handlers end with IsQuitting() at the final shutdown
			kDebug(1, "window closed, the servers keep running");
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_WebView.reset();
		}
		else
		{
			m_bQuit = true;
		}
	}

	if (!m_bQuit)
	{
		// the servers alone, until Quit() or a shutdown signal
		std::unique_lock<std::mutex> Lock(m_Mutex);
		m_Idle.wait(Lock, [this] { return m_bQuit.load(); });
	}

	// shutdown: first no more calls into the window, then the servers, then the window
	m_bQuit = true;
	m_REST.reset();
	m_Network.reset();

	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_WebView.reset();

	return 0;

} // Run

//-----------------------------------------------------------------------------
void KWebApp::Quit()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	m_bQuit = true;

	if (m_WebView)
	{
		// terminate() dispatches to the UI thread itself
		m_WebView->terminate();
	}

	m_Idle.notify_all();

} // Quit

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
