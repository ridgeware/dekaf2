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

#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>

#include <webview.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringView sCookieName = "kwa";
constexpr KStringView sEnterPath  = "/_kwa/enter";
constexpr KStringView sBindPrefix = "__kwa_";

#if DEKAF2_IS_MACOS
constexpr KStringView sPlatform   = "macos";
#elif DEKAF2_IS_WINDOWS
constexpr KStringView sPlatform   = "windows";
#else
constexpr KStringView sPlatform   = "linux";
#endif

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
KWebApp::KWebApp(Options Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
, m_Routes(Routes)
, m_sToken(kHex(kGetRandom(32)))
{
	// the loopback server: the address is not negotiable, and the window cannot
	// accept a self-signed certificate, so it stays plain HTTP. Port 0 lets the
	// OS pick one
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
		return;
	}

	m_iPort = m_REST->GetPort();

	kDebug(2, "loopback server listening on 127.0.0.1:{}", m_iPort);

} // ctor

//-----------------------------------------------------------------------------
KWebApp::~KWebApp()
//-----------------------------------------------------------------------------
{
	Quit();

} // dtor

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
void KWebApp::Guard(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// runs before routing, for every request - a rejection is a 403, and there
	// is no path that gets through without the token

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

		auto sNext = HTTP.GetQueryParm("next", "/");

		if (!sNext.starts_with('/') || sNext.starts_with("//"))
		{
			// stay on our own origin
			sNext = "/";
		}

		HTTP.SetCookie(sCookieName, m_sToken, "Path=/; HttpOnly; SameSite=Strict");
		HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sNext);
		throw KHTTPError { KHTTPError::H302_MOVED_TEMPORARILY, "" };
	}

	if (!KDigest::ConstantTimeCompare(HTTP.GetCookie(sCookieName), m_sToken))
	{
		kDebug(1, "rejecting request without token: {}", HTTP.Request.Resource.Path.get());
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "missing token" };
	}

} // Guard

//-----------------------------------------------------------------------------
int KWebApp::Run()
//-----------------------------------------------------------------------------
{
	if (!m_REST)
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

		// the UI loop, until the window closes or Quit() is called
		m_WebView->run();
	}
	else
	{
		std::unique_lock<std::mutex> Lock(m_Mutex);
		m_Idle.wait(Lock, [this] { return m_bQuit.load(); });
	}

	// shutdown: first no more calls into the window, then the server, then the window
	m_bQuit = true;
	m_REST.reset();

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
