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
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |\|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */

#include "ktunnel_admin.h"

#include "ktunnel_store.h"
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/core/init/dekaf2.h>  // Dekaf::getInstance().GetTimer() for the check watchdog
#include <dekaf2/web/ui/kwebui.h>
#include <dekaf2/web/url/kmime.h>
#include <algorithm>
#include <unordered_set>

using namespace dekaf2;

namespace {

// --------------------------------------------------------------------------
// Style sheet used by both the login page and the dashboard shell.
// Kept minimal on purpose — this is a functional admin UI, not a product.
// --------------------------------------------------------------------------
constexpr KStringView s_sAdminCSS = R"CSS(
* { box-sizing: border-box; }
body {
	font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
	background: #0f172a;
	color: #e2e8f0;
	margin: 0;
	min-height: 100vh;
}
a { color: #38bdf8; }
a:hover { color: #7dd3fc; }

.login-wrap {
	display: flex;
	align-items: center;
	justify-content: center;
	min-height: 100vh;
	padding: 1rem;
}
.card {
	background: #1e293b;
	border: 1px solid #334155;
	border-radius: 10px;
	padding: 2rem;
	width: 100%;
	max-width: 360px;
	box-shadow: 0 10px 30px rgba(0,0,0,0.4);
}
.card h1 {
	margin: 0 0 1.25rem;
	font-size: 1.25rem;
	text-align: center;
	letter-spacing: 0.04em;
}
.field { margin-bottom: 0.9rem; }
.field label { display: block; font-size: 0.8rem; color: #94a3b8; margin-bottom: 0.25rem; }
.field input {
	width: 100%;
	padding: 0.55rem 0.7rem;
	border-radius: 6px;
	border: 1px solid #475569;
	background: #0f172a;
	color: #e2e8f0;
	font-size: 0.95rem;
}
.field input:focus {
	outline: none;
	border-color: #38bdf8;
	box-shadow: 0 0 0 2px rgba(56,189,248,0.25);
}
.btn {
	width: 100%;
	padding: 0.6rem;
	border-radius: 6px;
	border: none;
	background: #0ea5e9;
	color: white;
	font-weight: 600;
	cursor: pointer;
	font-size: 0.95rem;
}
.btn:hover { background: #0284c7; }
.error {
	background: #7f1d1d;
	color: #fecaca;
	border: 1px solid #b91c1c;
	padding: 0.6rem 0.8rem;
	border-radius: 6px;
	margin-bottom: 1rem;
	font-size: 0.85rem;
}
.top {
	background: #1e293b;
	border-bottom: 1px solid #334155;
	padding: 0.75rem 1.25rem;
	display: flex;
	align-items: center;
	justify-content: space-between;
	flex-wrap: wrap;
	gap: 0.75rem;
}
.top .brand { font-weight: 600; letter-spacing: 0.05em; }
.top nav a {
	margin-right: 1rem;
	text-decoration: none;
	padding: 0.35rem 0.6rem;
	border-radius: 6px;
}
.top nav a.active { background: #0ea5e9; color: white; }
.top nav a:hover:not(.active) { background: #334155; }
.main { padding: 1.5rem; max-width: 1100px; margin: 0 auto; }
.placeholder {
	background: #1e293b;
	border: 1px dashed #475569;
	border-radius: 10px;
	padding: 2rem;
	color: #94a3b8;
	text-align: center;
}
.section {
	background: #1e293b;
	border: 1px solid #334155;
	border-radius: 10px;
	padding: 1.25rem 1.5rem;
	margin-bottom: 1.25rem;
}
.section h2 {
	margin: 0 0 0.9rem;
	font-size: 1.05rem;
	letter-spacing: 0.03em;
	color: #cbd5e1;
}
.section .muted { color: #94a3b8; font-size: 0.9rem; }
table.grid {
	width: 100%;
	border-collapse: collapse;
	font-size: 0.88rem;
}
table.grid th, table.grid td {
	text-align: left;
	padding: 0.45rem 0.6rem;
	border-bottom: 1px solid #334155;
	vertical-align: middle;
}
table.grid th {
	color: #94a3b8;
	font-weight: 600;
	text-transform: uppercase;
	letter-spacing: 0.05em;
	font-size: 0.72rem;
	background: #111827;
}
table.grid tr:last-child td { border-bottom: none; }
table.grid td.num { text-align: right; font-variant-numeric: tabular-nums; }
.pill {
	display: inline-block;
	padding: 0.1rem 0.55rem;
	border-radius: 999px;
	font-size: 0.72rem;
	font-weight: 600;
	text-transform: uppercase;
	letter-spacing: 0.05em;
}
.pill.ok       { background: #064e3b; color: #6ee7b7; }
.pill.fail     { background: #7f1d1d; color: #fecaca; }
.pill.info     { background: #1e3a8a; color: #bfdbfe; }
.pill.neutral  { background: #334155; color: #cbd5e1; }
.flash {
	padding: 0.6rem 0.9rem;
	border-radius: 6px;
	margin: 0 0 1rem;
	font-size: 0.9rem;
}
.flash.ok  { background: #064e3b; color: #a7f3d0; border: 1px solid #065f46; }
.flash.err { background: #7f1d1d; color: #fecaca; border: 1px solid #b91c1c; }
.row {
	display: flex;
	flex-wrap: wrap;
	gap: 0.75rem;
	align-items: flex-end;
}
.row .field { flex: 1 1 160px; margin-bottom: 0; }
.row .btn   { flex: 0 0 auto; width: auto; padding: 0.55rem 1.1rem; }
.btn.danger  { background: #b91c1c; }
.btn.danger:hover { background: #991b1b; }
.btn.small {
	display: inline-block;
	width: auto;
	padding: 0.3rem 0.7rem;
	font-size: 0.8rem;
}
.inline-form { display: inline; margin: 0; }
.checkbox {
	display: flex;
	align-items: center;
	gap: 0.4rem;
	color: #cbd5e1;
	font-size: 0.85rem;
}
.checkbox input { width: 1.1rem; height: 1.1rem; }
)CSS";

// KRESTServer normalizes incoming paths by stripping any trailing slash
// before route matching (see krestserver.cpp) — so we register the routes
// without trailing slashes. We still publish the user-visible URL with a
// trailing slash though, because that is the conventional form for a
// "directory" path.
constexpr KStringView s_sLoginURL     = "/Configure/login";
constexpr KStringView s_sLogoutURL    = "/Configure/logout";
constexpr KStringView s_sDashboardURL = "/Configure/";
constexpr KStringView s_sAdminsURL    = "/Configure/admins";
constexpr KStringView s_sOutletsURL     = "/Configure/outlets";
constexpr KStringView s_sInletsURL   = "/Configure/inlets";
constexpr KStringView s_sTunnelsURL   = "/Configure/tunnels";
constexpr KStringView s_sEventsURL    = "/Configure/events";
constexpr KStringView s_sCertURL      = "/Configure/certificate";
constexpr KStringView s_sSettingsURL  = "/Configure/settings";
constexpr KStringView s_sLogURL       = "/Configure/log";
constexpr KStringView s_sOutletReplURL  = "/Configure/outlets/repl";

// The matching routes — no trailing slashes, because KRESTServer strips
// them off the request path before looking up a route.
constexpr KStringView s_sLoginRoute            = "/Configure/login";
constexpr KStringView s_sLogoutRoute           = "/Configure/logout";
constexpr KStringView s_sDashboardRoute        = "/Configure";
constexpr KStringView s_sAdminsRoute           = "/Configure/admins";
constexpr KStringView s_sAdminsAddRoute        = "/Configure/admins/add";
constexpr KStringView s_sAdminsDeleteRoute     = "/Configure/admins/delete";
constexpr KStringView s_sAdminsChangePwRoute   = "/Configure/admins/changepass";
constexpr KStringView s_sOutletsRoute            = "/Configure/outlets";
constexpr KStringView s_sOutletsAddRoute         = "/Configure/outlets/add";
constexpr KStringView s_sOutletsToggleRoute      = "/Configure/outlets/toggle";
constexpr KStringView s_sOutletsDeleteRoute      = "/Configure/outlets/delete";
constexpr KStringView s_sOutletsResetPwRoute     = "/Configure/outlets/resetpass";
constexpr KStringView s_sOutletsInstallRoute     = "/Configure/outlets/install";
constexpr KStringView s_sInletsRoute          = "/Configure/inlets";
constexpr KStringView s_sInletsAddRoute       = "/Configure/inlets/add";
constexpr KStringView s_sInletsToggleRoute    = "/Configure/inlets/toggle";
constexpr KStringView s_sInletsDeleteRoute    = "/Configure/inlets/delete";
constexpr KStringView s_sInletsResetPwRoute   = "/Configure/inlets/resetpass";
constexpr KStringView s_sInletsTunnelsRoute   = "/Configure/inlets/tunnels";
constexpr KStringView s_sTunnelsRoute          = "/Configure/tunnels";
constexpr KStringView s_sTunnelsAddRoute       = "/Configure/tunnels/add";
constexpr KStringView s_sTunnelsToggleRoute    = "/Configure/tunnels/toggle";
constexpr KStringView s_sTunnelsDeleteRoute    = "/Configure/tunnels/delete";
constexpr KStringView s_sTunnelsEditRoute      = "/Configure/tunnels/edit";
constexpr KStringView s_sTunnelsUpdateRoute    = "/Configure/tunnels/update";
constexpr KStringView s_sTunnelsCheckRoute     = "/Configure/tunnels/check";
constexpr KStringView s_sEventsRoute           = "/Configure/events";
constexpr KStringView s_sCertRoute             = "/Configure/certificate";
constexpr KStringView s_sCertUpdateRoute       = "/Configure/certificate/update";
constexpr KStringView s_sSettingsRoute         = "/Configure/settings";
constexpr KStringView s_sSettingsUpdateRoute   = "/Configure/settings/update";
constexpr KStringView s_sLogRoute              = "/Configure/log";
constexpr KStringView s_sLogStreamRoute        = "/Configure/log/stream";
constexpr KStringView s_sLogLevelRoute         = "/Configure/log/level";
// legacy URL - the peers page merged into the outlets page, the route only
// redirects there so that old bookmarks keep working
constexpr KStringView s_sPeersRoute            = "/Configure/peers";
constexpr KStringView s_sOutletReplRoute         = "/Configure/outlets/repl";
constexpr KStringView s_sOutletReplWsRoute       = "/Configure/outlets/repl/ws";

// Matching user-visible URLs used for form actions and redirects.
constexpr KStringView s_sAdminsAddURL          = "/Configure/admins/add";
constexpr KStringView s_sAdminsDeleteURL       = "/Configure/admins/delete";
constexpr KStringView s_sAdminsChangePwURL     = "/Configure/admins/changepass";
constexpr KStringView s_sOutletsAddURL           = "/Configure/outlets/add";
constexpr KStringView s_sOutletsToggleURL        = "/Configure/outlets/toggle";
constexpr KStringView s_sOutletsDeleteURL        = "/Configure/outlets/delete";
constexpr KStringView s_sOutletsResetPwURL       = "/Configure/outlets/resetpass";
constexpr KStringView s_sOutletsInstallURL       = "/Configure/outlets/install";
constexpr KStringView s_sInletsAddURL         = "/Configure/inlets/add";
constexpr KStringView s_sInletsToggleURL      = "/Configure/inlets/toggle";
constexpr KStringView s_sInletsDeleteURL      = "/Configure/inlets/delete";
constexpr KStringView s_sInletsResetPwURL     = "/Configure/inlets/resetpass";
constexpr KStringView s_sInletsTunnelsURL     = "/Configure/inlets/tunnels";
constexpr KStringView s_sTunnelsAddURL         = "/Configure/tunnels/add";
constexpr KStringView s_sTunnelsToggleURL      = "/Configure/tunnels/toggle";
constexpr KStringView s_sTunnelsDeleteURL      = "/Configure/tunnels/delete";
constexpr KStringView s_sTunnelsEditURL        = "/Configure/tunnels/edit";
constexpr KStringView s_sTunnelsUpdateURL      = "/Configure/tunnels/update";
constexpr KStringView s_sTunnelsCheckURL       = "/Configure/tunnels/check";
constexpr KStringView s_sCertUpdateURL         = "/Configure/certificate/update";
constexpr KStringView s_sSettingsUpdateURL     = "/Configure/settings/update";
// note: the log event-stream URL only appears inside the live view's inline script
constexpr KStringView s_sLogLevelURL           = "/Configure/log/level";

// --------------------------------------------------------------------------
// Small pure-functional helpers used by the dashboard rendering.
//
// HTML-escaping happens by construction in the web objects; byte and
// timestamp formatting are delegated to kFormBytes / KUnixTime::to_string.
// --------------------------------------------------------------------------

/// Format a duration as "5s", "12m 34s", "3h 12m", "2d 4h" for compact
/// "connected for..." columns. Shorter (and nicer) than KDuration::ToString,
/// which always prints all units even for sub-minute deltas.
KString FormatDuration (KDuration dur)
{
	auto iTotal = dur.seconds().count();
	if (iTotal < 0)  iTotal = 0;
	const auto d = iTotal / 86400;  iTotal %= 86400;
	const auto h = iTotal / 3600;   iTotal %= 3600;
	const auto m = iTotal / 60;     iTotal %= 60;
	const auto s = iTotal;
	if (d > 0) return kFormat("{}d {}h", d, h);
	if (h > 0) return kFormat("{}h {}m", h, m);
	if (m > 0) return kFormat("{}m {}s", m, s);
	return kFormat("{}s", s);
}

/// Map an event kind to a pill-style CSS class for quick visual grep.
KStringView PillForEventKind (KStringView sKind)
{
	if (sKind == "admin_login_ok"
	 || sKind == "inlet_login_ok"
	 || sKind == "inlet_forward"
	 || sKind == "node_login_ok"
	 || sKind == "tunnel_connect"
	 || sKind == "tunnel_start")      return "ok";
	if (sKind == "admin_login_fail"
	 || sKind == "node_login_fail"
	 || sKind == "handshake_fail"
	 || sKind == "tunnel_error"
	 || sKind == "conn_fail"
	 || sKind == "conn_reject"
	 || sKind == "inlet_login_fail"
	 || sKind == "inlet_reject"
	 || sKind == "auth_reject")       return "fail";
	if (sKind == "tunnel_disconnect"
	 || sKind == "tunnel_stop"
	 || sKind == "node_disable"
	 || sKind == "admin_del"
	 || sKind == "node_del")          return "neutral";
	if (sKind == "bootstrap"
	 || sKind == "config_change"
	 || sKind == "admin_add"
	 || sKind == "admin_password_change"
	 || sKind == "node_add"
	 || sKind == "node_enable"
	 || sKind == "inlet_add"
	 || sKind == "inlet_password_change"
	 || sKind == "node_password_change")
	                                  return "info";
	return "neutral";
}

} // anonymous namespace

//-----------------------------------------------------------------------------
AdminUI::AdminUI (RelayServer& Server)
//-----------------------------------------------------------------------------
: m_Server(Server)
, m_Config(Server.GetConfig())
{
	KSession::Config SessionCfg;

	if (m_Config.bNoTLS)
	{
		// __Host- cookies require Secure + HTTPS; drop both for plain HTTP
		SessionCfg.sCookieName = "ktunnel_session";
		SessionCfg.bSecure     = false;
	}
	else
	{
		SessionCfg.sCookieName = "__Host-ktunnel";
		SessionCfg.bSecure     = true;
	}

	SessionCfg.sCookiePath      = "/";
	SessionCfg.sSameSite        = "Strict";
	SessionCfg.bHttpOnly        = true;
	SessionCfg.IdleTimeout      = chrono::minutes(30);
	SessionCfg.AbsoluteTimeout  = chrono::hours(8);

	m_Session = std::make_unique<KSession>(std::make_unique<KSessionMemoryStore>(),
	                                       std::move(SessionCfg));

	// Authenticator closure: validate (admin-username, password) against
	// the `admins` table. Unknown logins trigger a dummy bcrypt compare
	// so that an attacker cannot time-oracle which admin names exist.
	m_Session->SetAuthenticator([this](KStringView sUser, KStringView sPass)
	{
		// bcrypt("<never-used>", cost 4) — matches VerifyNodeLogin()
		static constexpr KStringViewZ s_sDummyHash =
			"$2a$04$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG";

		// ValidatePassword requires KStringViewZ (NUL-terminated), so we
		// round-trip the submitted password through a temporary KString.
		KString sPassword(sPass);

		auto& Store  = m_Server.GetStore();
		auto& BCrypt = m_Server.GetBCrypt();

		auto oAdmin = Store.GetAdmin(sUser);

		if (!oAdmin)
		{
			(void) BCrypt.ValidatePassword(sPassword, s_sDummyHash);
			KTunnelStore::Event ev;
			ev.sKind   = "admin_login_fail";
			ev.sAdmin  = sUser;
			ev.sDetail = "admin UI: unknown admin";
			Store.LogEvent(ev);
			return false;
		}

		if (!BCrypt.ValidatePassword(sPassword, oAdmin->sPasswordHash))
		{
			KTunnelStore::Event ev;
			ev.sKind   = "admin_login_fail";
			ev.sAdmin  = sUser;
			ev.sDetail = "admin UI: bad password";
			Store.LogEvent(ev);
			return false;
		}

		Store.SetAdminLastLogin(sUser, KUnixTime::now());
		KTunnelStore::Event ev;
		ev.sKind   = "admin_login_ok";
		ev.sAdmin  = sUser;
		ev.sDetail = "admin UI";
		Store.LogEvent(ev);

		return true;
	});

} // ctor

//-----------------------------------------------------------------------------
AdminUI::~AdminUI () = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
html::Page AdminUI::MakePage (KStringView sTitle) const
//-----------------------------------------------------------------------------
{
	html::Page Page(sTitle, "en");
	Page.AddMeta("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(s_sAdminCSS);
	return Page;

} // MakePage

//-----------------------------------------------------------------------------
void AdminUI::RenderPage (KRESTServer& HTTP, html::Page& Page) const
//-----------------------------------------------------------------------------
{
	HTTP.SetRawOutput(Page.Serialize());
	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);

	// admin UI pages must never be cached by intermediaries or the browser
	HTTP.Response.Headers.Set(KHTTPHeader::CACHE_CONTROL,
	                          "no-store, no-cache, must-revalidate, private");
	HTTP.Response.Headers.Set("Pragma", "no-cache");

} // RenderPage

//-----------------------------------------------------------------------------
void AdminUI::ShowLogin (KRESTServer& HTTP,
                         KStringView sError,
                         KStringView sUsername)
//-----------------------------------------------------------------------------
{
	auto Page = MakePage("ktunnel — Login");

	auto wrap = Page.Body().Add<html::Div>(html::Classes("login-wrap"));
	auto card = wrap.Add<html::Div>(html::Classes("card"));

	card.Add<html::Heading>(1, "ktunnel admin");

	if (!sError.empty())
	{
		auto err = card.Add<html::Div>(html::Classes("error"));
		err.AddText(sError);
	}

	auto form = card.Add<html::Form>(s_sLoginURL);
	form.SetMethod(html::Form::POST);

	{
		auto field = form.Add<html::Div>(html::Classes("field"));
		auto label = field.AddElement("label");
		label.SetAttribute("for", "user");
		label.AddText("Username");
		field.Add<html::Input>("username", sUsername, html::Input::TEXT, "", "user")
		     .SetAutofocus(true);
	}

	{
		auto field = form.Add<html::Div>(html::Classes("field"));
		auto label = field.AddElement("label");
		label.SetAttribute("for", "pass");
		label.AddText("Password");
		field.Add<html::Input>("password", "", html::Input::PASSWORD, "", "pass");
	}

	form.Add<html::Button>("Sign in", html::Button::SUBMIT, html::Classes("btn"));

	RenderPage(HTTP, Page);

} // ShowLogin

//-----------------------------------------------------------------------------
void AdminUI::HandleLogin (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// form fields for a WWWFORM parser land in the query-parm map just like
	// URL query arguments. GetQueryParm returns a const KString& so we bind
	// by reference to avoid copies. All outgoing text is either bound via
	// ksqlite prepared statements or HTML-escaped at the output boundary,
	// so no character-whitelist accessor is needed here.
	const auto& sUser = HTTP.GetQueryParm("username");
	const auto& sPass = HTTP.GetQueryParm("password");

	if (sUser.empty() || sPass.empty())
	{
		ShowLogin(HTTP, "Please provide username and password", sUser);
		return;
	}

	KRESTSession Sess(*m_Session, HTTP);

	if (!Sess.Login(sUser, sPass))
	{
		kDebug(1, "admin login failed for user {} from {}", sUser, HTTP.GetRemoteIP());
		ShowLogin(HTTP, "Invalid username or password", sUser);
		return;
	}

	kDebug(1, "admin login succeeded for user {} from {}", sUser, HTTP.GetRemoteIP());

	// PRG (Post/Redirect/Get) — drop the form data from the browser
	// history and land the user on the dashboard
	HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, KString(s_sDashboardURL));

} // HandleLogin

//-----------------------------------------------------------------------------
void AdminUI::HandleLogout (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	Sess.Logout();

	HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, KString(s_sLoginURL));

} // HandleLogout

//-----------------------------------------------------------------------------
/// render the request feedback banner - a notice wins over an error
static void RenderFlash (KHTMLNode Parent, KStringView sNotice, KStringView sError)
//-----------------------------------------------------------------------------
{
	if (!sNotice.empty())
	{
		Parent.Add<html::ui::Flash>(sNotice);
	}
	else if (!sError.empty())
	{
		Parent.Add<html::ui::Flash>(sError, html::ui::Flash::Error);
	}

} // RenderFlash

//-----------------------------------------------------------------------------
void AdminUI::RenderTopBar (html::Page& Page,
                            KStringView sActive,
                            KStringView sAdmin) const
//-----------------------------------------------------------------------------
{
	// The UI is admin-only — every signed-in user has full access — so all
	// nav entries are always shown.
	struct NavEntry
	{
		KStringView sKey;
		KStringView sURL;
		KStringView sLabel;
	};
	static constexpr NavEntry s_NavEntries[] = {
		{ "dashboard", s_sDashboardURL, "Dashboard"   },
		{ "admins",    s_sAdminsURL,    "Admins"      },
		{ "outlets",     s_sOutletsURL,     "Outlets"       },
		{ "inlets",   s_sInletsURL,   "Inlets"     },
		{ "tunnels",   s_sTunnelsURL,   "Tunnels"     },
		{ "cert",      s_sCertURL,      "Certificate" },
		{ "settings",  s_sSettingsURL,  "Settings"    },
		{ "log",       s_sLogURL,       "Log"         },
		{ "events",    s_sEventsURL,    "Events"      },
		{ "logout",    s_sLogoutURL,    "Logout"      },
	};

	auto Nav = Page.Body().Add<html::ui::NavBar>(kFormat("ktunnel admin · {}", sAdmin));

	for (const auto& e : s_NavEntries)
	{
		Nav.Link(e.sLabel, e.sURL, e.sKey == sActive);
	}

} // RenderTopBar

//-----------------------------------------------------------------------------
/// Validate a bind address (empty = wildcard) and an allow list of comma
/// separated IPs / CIDR networks (empty = unrestricted). Returns an error
/// message for the flash banner, or the empty string when both are fine.
static KString ValidateAccess (KStringView sBindAddress, KStringView sAllowFrom)
//-----------------------------------------------------------------------------
{
	if (!sBindAddress.empty())
	{
		KIPError ec;
		KIPAddress Address(sBindAddress, ec);

		if (ec)
		{
			return kFormat("'{}' is not a valid bind address: {}", sBindAddress, ec.message());
		}
	}

	for (auto sPart : sAllowFrom.Split(","))
	{
		if (sPart.empty()) continue;

		KIPError ec;
		// bAcceptSingleHost: a bare address counts as a /32 resp. /128
		KIPNetwork Net(sPart, true, ec);

		if (ec)
		{
			return kFormat("'{}' in the allow list is not an IP or CIDR network: {}",
			               sPart, ec.message());
		}
	}

	return {};

} // ValidateAccess

//-----------------------------------------------------------------------------
/// describe a listener's access control for events and the UI
static KString DescribeAccess (KStringView sBindAddress, KStringView sAllowFrom)
//-----------------------------------------------------------------------------
{
	return kFormat("bind {}, allow {}",
	               sBindAddress.empty() ? KStringView("*")   : sBindAddress,
	               sAllowFrom.empty()   ? KStringView("any") : sAllowFrom);

} // DescribeAccess

//-----------------------------------------------------------------------------
/// render a colored status pill: span.pill.<sPillClass>
static void RenderPill (KHTMLNode Parent, KStringView sPillClass, KStringView sText)
//-----------------------------------------------------------------------------
{
	Parent.Add<html::Span>(html::Classes(kFormat("pill {}", sPillClass))).AddText(sText);

} // RenderPill

//-----------------------------------------------------------------------------
/// fill an outlet <select> with all known outlets. Disabled outlets are included
/// for convenience (an admin may want to pre-stage a tunnel for an outlet that
/// is not yet enabled), with a hint label.
static void AddOutletOptions (html::Select& Select,
                            const std::vector<KTunnelStore::Outlet>& Outlets,
                            KStringView sSelected = KStringView{})
//-----------------------------------------------------------------------------
{
	for (const auto& n : Outlets)
	{
		Select.Add<html::Option>(n.bEnabled ? n.sName
		                                    : kFormat("{} (disabled)", n.sName),
		                         n.sName)
		      .SetSelected(n.sName == sSelected);
	}

} // AddOutletOptions

//-----------------------------------------------------------------------------
/// render an event table — shared by the dashboard and the events page
static void RenderEventTable (KHTMLNode Parent, const std::vector<KTunnelStore::Event>& Events)
//-----------------------------------------------------------------------------
{
	auto Table = Parent.Add<html::ui::Table>();
	Table.Headers({ "Time", "Kind", "Admin", "Outlet", "Tunnel", "Remote", "Detail" });

	for (const auto& e : Events)
	{
		auto Row = Table.AddRow();
		Row.Add<html::TableData>(kFormat("{} UTC", e.tTimestamp.to_string()));
		RenderPill(Row.Add<html::TableData>(), PillForEventKind(e.sKind), e.sKind);
		Row.Add<html::TableData>(e.sAdmin);
		Row.Add<html::TableData>(e.sOutlet);
		Row.Add<html::TableData>(e.sTunnelName);
		Row.Add<html::TableData>(e.sRemoteIP);
		Row.Add<html::TableData>(e.sDetail);
	}

} // RenderEventTable

//-----------------------------------------------------------------------------
/// Render the state pill for a tunnel listener — shared by the dashboard
/// and the tunnels page.
static void RenderListenerStatePill (KHTMLNode Parent, RelayServer::ListenerState eState, KStringView sError)
//-----------------------------------------------------------------------------
{
	using S = RelayServer::ListenerState;
	switch (eState)
	{
		case S::Listening:
			RenderPill(Parent, "ok", "listening");
			return;
		case S::OwnerOffline:
			RenderPill(Parent, "info", "outlet offline");
			return;
		case S::PortError:
			RenderPill(Parent, "fail", "port error");
			{
				auto div = Parent.Add<html::Div>(html::Classes("muted"));
				div.SetAttribute("style", "font-size:0.7rem");
				div.AddText(sError);
			}
			return;
		case S::Stopped:
			break;
	}
	RenderPill(Parent, "neutral", "stopped");

} // RenderListenerStatePill

//-----------------------------------------------------------------------------
void AdminUI::ShowDashboard (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	// Read optional ?notice=… / ?error=… flash (e.g. from a successful
	// password change that got redirected here).
	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Page = MakePage("ktunnel — Dashboard");
	RenderTopBar(Page, "dashboard", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// One-line mental model — the three terms the whole UI is built on.
	{
		auto p = main.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("An outlet connects in from behind its firewall. A tunnel "
		          "forwards a listen port to a target its outlet can reach. "
		          "Each use of a tunnel opens a connection.");
	}

	// --- Section 1: connected outlets -----------------------------------
	{
		auto Connected = m_Server.SnapshotActiveTunnels();

		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Connected outlets ({})", Connected.size()));

		if (Connected.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No outlets are currently connected. Outlets appear here "
			          "once they complete the login handshake.");
		}
		else
		{
			const auto tNow = KUnixTime::now();

			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Outlet", "Remote", "Connected", "Conns", "RX", "TX", "" });

			for (const auto& at : Connected)
			{
				const auto iConn  = at.Tunnel->GetConnectionCount();
				const auto iRx    = at.Tunnel->GetBytesRx();
				const auto iTx    = at.Tunnel->GetBytesTx();
				const auto sDur   = FormatDuration(tNow - at.tConnected);

				KString sReplURL = kFormat("{}?outlet={}",
					s_sOutletReplURL,
					kUrlEncode(at.sOutlet, URIPart::Query));

				auto Row = Table.AddRow();
				Row.Add<html::TableData>().Add<html::Link>(s_sOutletsURL, at.sOutlet);
				Row.Add<html::TableData>(at.EndpointAddr.Serialize());
				Row.Add<html::TableData>(sDur);
				Row.Add<html::TableData>(kFormat("{}", iConn)).SetAttribute("class", "num");
				Row.Add<html::TableData>(kFormBytes(iRx)).SetAttribute("class", "num");
				Row.Add<html::TableData>(kFormBytes(iTx)).SetAttribute("class", "num");
				Row.Add<html::TableData>().Add<html::Link>(sReplURL, "Open REPL", html::Classes{"btn small"});
			}
		}
	}

	// --- Section 2: tunnel states -------------------------------------
	{
		auto ListenerMap = m_Server.SnapshotListenerStates();

		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Tunnels ({})", ListenerMap.size()));

		if (ListenerMap.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No tunnels configured yet. Add one under ");
			p.Add<html::Link>(s_sTunnelsURL, "Tunnels");
			p.AddText(".");
		}
		else
		{
			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Tunnel", "Runtime" });

			for (const auto& kv : ListenerMap)
			{
				auto Row = Table.AddRow();
				Row.Add<html::TableData>().Add<html::Link>(s_sTunnelsURL, kv.first);
				RenderListenerStatePill(Row.Add<html::TableData>(), kv.second.eState, kv.second.sError);
			}
		}
	}

	// --- Section 3: recent events -------------------------------------
	{
		static constexpr std::size_t kEventLimit = 10;

		auto Events = m_Server.GetStore().GetRecentEvents(kEventLimit);

		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Recent events (last {})", kEventLimit));

		if (Events.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No events logged yet.");
		}
		else
		{
			RenderEventTable(sec, Events);

			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("Full history browsable under ");
			p.Add<html::Link>(s_sEventsURL, "Events");
			p.AddText(".");
		}
	}

	RenderPage(HTTP, Page);

} // ShowDashboard

//-----------------------------------------------------------------------------
void AdminUI::ShowStubPage (KRESTServer& HTTP,
                            KStringView sSection,
                            KStringView sTitle,
                            KStringView sDescription)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	auto Page = MakePage(kFormat("ktunnel — {}", sTitle));
	RenderTopBar(Page, sSection, Sess.GetUser());

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));
	auto ph = main.Add<html::Div>(html::Classes("placeholder"));

	ph.Add<html::Heading>(2, kFormat("{} — coming soon", sTitle));
	auto p = ph.Add<html::Paragraph>();
	p.AddText(sDescription);

	RenderPage(HTTP, Page);

} // ShowStubPage

//-----------------------------------------------------------------------------
void AdminUI::RedirectWithFlash (KRESTServer& HTTP,
                                 KStringView sURL,
                                 KStringView sNotice,
                                 KStringView sError) const
//-----------------------------------------------------------------------------
{
	KString sTarget(sURL);

	// If the caller already included a query string, append with `&`
	// instead of opening a new one with `?`. This happens when we
	// redirect back to an `edit?name=<n>`-style URL with a flash.
	const char chSep = (sTarget.find('?') == KString::npos) ? '?' : '&';

	if (!sNotice.empty())
	{
		sTarget += chSep;
		sTarget += "notice=";
		sTarget += kUrlEncode(sNotice, URIPart::Query);
	}
	else if (!sError.empty())
	{
		sTarget += chSep;
		sTarget += "error=";
		sTarget += kUrlEncode(sError, URIPart::Query);
	}

	HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sTarget);

} // RedirectWithFlash

//-----------------------------------------------------------------------------
void AdminUI::ShowAdmins (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	// Read optional ?notice=… / ?error=… flash from the query string.
	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Admins = m_Server.GetStore().GetAllAdmins();

	auto Page = MakePage("ktunnel — Admins");
	RenderTopBar(Page, "admins", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	// --- flash banner (if any) ----------------------------------------
	RenderFlash(main, sNotice, sError);

	// --- Section 1: admin list ----------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Admins ({})", Admins.size()));

		auto Table = sec.Add<html::ui::Table>();
		Table.Headers({ "Username", "Last login", "Created", "" });

		for (const auto& a : Admins)
		{
			auto Row = Table.AddRow();
			Row.Add<html::TableData>(a.sUsername);
			Row.Add<html::TableData>(a.tLastLogin.to_time_t() > 0
			                             ? kFormat("{} UTC", a.tLastLogin.to_string())
			                             : KString("—"));
			Row.Add<html::TableData>(a.tCreated.to_time_t() > 0
			                             ? kFormat("{} UTC", a.tCreated.to_string())
			                             : KString("—"));

			auto Actions = Row.Add<html::TableData>();
			if (a.sUsername == sMe)
			{
				RenderPill(Actions, "neutral", "You");
			}
			else
			{
				auto Form = Actions.Add<html::Form>(s_sAdminsDeleteURL, html::Classes{"inline-form"});
				Form.SetMethod(html::Form::POST);
				Form.SetAttribute("onsubmit",
				                  kFormat("return confirm('Delete admin {}?');", a.sUsername));
				Form.Add<html::Input>("username", a.sUsername, html::Input::HIDDEN);
				Form.Add<html::Button>("Delete", html::Button::SUBMIT,
				                       html::Classes{"btn danger small"});
			}
		}
	}

	// --- Section 2: add admin form ------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Add admin");

		auto Form = sec.Add<html::Form>(s_sAdminsAddURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Username", "username")
		   .Input().SetRequired(true).SetAttribute("autocomplete", "off");
		Row.Add<html::ui::Field>("Password", "password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password");
		Row.Add<html::Button>("Add", html::Button::SUBMIT, html::Classes{"btn"});
	}

	// --- Section 3: change own password -------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Change password · {}", sMe));

		auto Form = sec.Add<html::Form>(s_sAdminsChangePwURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Current password", "current_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "current-password");
		Row.Add<html::ui::Field>("New password", "new_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password");
		Row.Add<html::ui::Field>("Confirm new password", "confirm_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password");
		Row.Add<html::Button>("Change", html::Button::SUBMIT, html::Classes{"btn"});
	}

	RenderPage(HTTP, Page);

} // ShowAdmins

//-----------------------------------------------------------------------------
void AdminUI::HandleAdminsAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	// Form body was parsed because the route is registered with WWWFORM.
	const auto& sUsername = HTTP.GetQueryParm("username");
	const auto& sPassword = HTTP.GetQueryParm("password");

	if (sUsername.empty() || sPassword.empty())
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "Username and password must not be empty.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	if (Store.GetAdmin(sUsername))
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", kFormat("Admin '{}' already exists.", sUsername));
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sPassword));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "Failed to hash password.");
		return;
	}

	KTunnelStore::Admin a;
	a.sUsername     = sUsername;
	a.sPasswordHash = sHash;

	if (!Store.AddAdmin(a))
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "",  kFormat("Cannot add admin: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "admin_add";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("added admin '{}'", sUsername);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sAdminsURL, kFormat("Admin '{}' added.", sUsername), "");

} // HandleAdminsAdd

//-----------------------------------------------------------------------------
void AdminUI::HandleAdminsDelete (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto& sUsername = HTTP.GetQueryParm("username");

	if (sUsername.empty())
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "No username provided.");
		return;
	}
	if (sUsername == sMe)
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "You cannot delete the account you are logged in with.");
		return;
	}

	auto& Store = m_Server.GetStore();

	if (!Store.GetAdmin(sUsername))
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", kFormat("Admin '{}' does not exist.", sUsername));
		return;
	}

	if (!Store.DeleteAdmin(sUsername))
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", kFormat("Cannot delete admin: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "admin_del";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("deleted admin '{}'", sUsername);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sAdminsURL, kFormat("Admin '{}' deleted.", sUsername), "");

} // HandleAdminsDelete

//-----------------------------------------------------------------------------
void AdminUI::HandleAdminsChangePass (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sCurrent = HTTP.GetQueryParm("current_password");
	const auto& sNew     = HTTP.GetQueryParm("new_password");
	const auto& sConfirm = HTTP.GetQueryParm("confirm_password");

	if (sCurrent.empty() || sNew.empty() || sConfirm.empty())
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "All three password fields are required.");
		return;
	}
	if (sNew != sConfirm)
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "New password and confirmation do not match.");
		return;
	}
	if (sNew.size() < 6)
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "New password must be at least 6 characters long.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	auto oMe = Store.GetAdmin(sMe);
	if (!oMe)
	{
		// The session references an admin that no longer exists — force
		// a fresh login so the operator ends up in a consistent state.
		Sess.Logout();
		RedirectWithFlash(HTTP, s_sLoginURL, "", "Your account was removed. Please log in again.");
		return;
	}

	if (!BCrypt.ValidatePassword(KStringViewZ(sCurrent), oMe->sPasswordHash))
	{
		KTunnelStore::Event ev;
		ev.sKind   = "admin_password_fail";
		ev.sAdmin  = sMe;
		ev.sDetail = "current password did not verify";
		Store.LogEvent(ev);

		RedirectWithFlash(HTTP, s_sAdminsURL, "", "Current password is incorrect.");
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sNew));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", "Failed to hash new password.");
		return;
	}

	if (!Store.UpdateAdminPasswordHash(sMe, sHash))
	{
		RedirectWithFlash(HTTP, s_sAdminsURL, "", kFormat("Cannot update password: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "admin_password_change";
	ev.sAdmin  = sMe;
	ev.sDetail = "admin UI";
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sAdminsURL, "Password changed.", "");

} // HandleAdminsChangePass

// =========================================================================
// Outlets (tunnel-endpoint accounts)
// =========================================================================

//-----------------------------------------------------------------------------
void AdminUI::ShowOutlets (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Outlets = m_Server.GetStore().GetAllOutlets();

	// Live state: which outlets are connected right now (name → snapshot),
	// and how many tunnels each outlet owns (for the cross-link column).
	KUnorderedMap<KString, RelayServer::ActiveTunnel> Online;
	for (auto& at : m_Server.SnapshotActiveTunnels())
	{
		Online.emplace(at.sOutlet, std::move(at));
	}

	KUnorderedMap<KString, std::size_t> TunnelCount;
	for (const auto& t : m_Server.GetStore().GetAllTunnels())
	{
		++TunnelCount[t.sOutlet];
	}

	auto Page = MakePage("ktunnel — Outlets");
	RenderTopBar(Page, "outlets", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// --- Section 1: outlet list -----------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Outlets ({})", Outlets.size()));

		if (Outlets.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No tunnel-endpoint accounts configured yet. "
			          "Use the form below to add one.");
		}
		else
		{
			const auto tNow = KUnixTime::now();

			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Name", "Status", "Enabled", "Last login", "Created", "Tunnels", "" });

			for (const auto& n : Outlets)
			{
				auto Row = Table.AddRow();
				Row.Add<html::TableData>(n.sName);

				// Live status: online with duration and remote address when a
				// control connection is up, plain offline otherwise.
				auto Status   = Row.Add<html::TableData>();
				auto itOnline = Online.find(n.sName);
				if (itOnline != Online.end())
				{
					RenderPill(Status, "ok", "online");
					Status.Add<html::Span>(html::Classes("muted"))
					      .SetStyle("font-size:0.7rem")
					      .AddText(kFormat("{} · {}",
					                       FormatDuration(tNow - itOnline->second.tConnected),
					                       itOnline->second.EndpointAddr.Serialize()));
				}
				else
				{
					RenderPill(Status, "neutral", "offline");
				}

				RenderPill(Row.Add<html::TableData>(),
				           n.bEnabled ? "ok"      : "neutral",
				           n.bEnabled ? "enabled" : "disabled");
				Row.Add<html::TableData>(n.tLastLogin.to_time_t() > 0
				                             ? kFormat("{} UTC", n.tLastLogin.to_string())
				                             : KString("—"));
				Row.Add<html::TableData>(n.tCreated.to_time_t() > 0
				                             ? kFormat("{} UTC", n.tCreated.to_string())
				                             : KString("—"));

				const auto itCount  = TunnelCount.find(n.sName);
				const auto iTunnels = (itCount != TunnelCount.end()) ? itCount->second : 0;

				Row.Add<html::TableData>().SetAttribute("class", "num")
				   .Add<html::Link>(s_sTunnelsURL, kFormat("{}", iTunnels));

				auto Actions = Row.Add<html::TableData>();

				Actions.Add<html::Link>(kFormat("{}?outlet={}",
				                                s_sOutletsInstallURL,
				                                kUrlEncode(n.sName, URIPart::Query)),
				                        "Install", html::Classes{"btn small"});

				if (itOnline != Online.end())
				{
					Actions.Add<html::Link>(kFormat("{}?outlet={}",
					                                s_sOutletReplURL,
					                                kUrlEncode(n.sName, URIPart::Query)),
					                        "Open REPL", html::Classes{"btn small"});
				}

				auto Toggle = Actions.Add<html::Form>(s_sOutletsToggleURL, html::Classes{"inline-form"});
				Toggle.SetMethod(html::Form::POST);
				Toggle.Add<html::Input>("name",   n.sName,                html::Input::HIDDEN);
				Toggle.Add<html::Input>("enable", n.bEnabled ? "0" : "1", html::Input::HIDDEN);
				Toggle.Add<html::Button>(n.bEnabled ? "Disable" : "Enable",
				                         html::Button::SUBMIT, html::Classes{"btn small"});

				auto Delete = Actions.Add<html::Form>(s_sOutletsDeleteURL, html::Classes{"inline-form"});
				Delete.SetMethod(html::Form::POST);
				Delete.SetAttribute("onsubmit",
				                    kFormat("return confirm('Delete outlet {}?');", n.sName));
				Delete.Add<html::Input>("name", n.sName, html::Input::HIDDEN);
				Delete.Add<html::Button>("Delete", html::Button::SUBMIT,
				                         html::Classes{"btn danger small"});
			}
		}
	}

	// --- Section 2: add outlet ------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Add outlet");

		auto Form = sec.Add<html::Form>(s_sOutletsAddURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Name", "name")
		   .Input().SetRequired(true).SetAttribute("autocomplete", "off");
		Row.Add<html::ui::Field>("Password", "password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password")
		           .SetAttribute("id", "node_add_pw");
		Row.Add<html::Button>("Generate", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktGenPassword(this, ['node_add_pw'])");

		auto Field = Row.Add<html::Div>(html::Classes("field"));
		auto Label = Field.AddElement("label");
		Label.SetAttribute("class", "checkbox");
		Label.Add<html::Input>("enabled", "1", html::Input::CHECKBOX).SetChecked(true);
		Label.Add<html::Span>().AddText("Enabled");

		Row.Add<html::Button>("Add", html::Button::SUBMIT, html::Classes{"btn"});
	}

	// --- Section 3: reset outlet password -------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Reset outlet password");

		auto Form = sec.Add<html::Form>(s_sOutletsResetPwURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));

		auto Field = Row.Add<html::Div>(html::Classes("field"));
		Field.AddElement("label").AddText("Outlet");
		auto Select = Field.Add<html::Select>("name");
		Select.SetRequired(true);
		AddOutletOptions(Select, Outlets);

		Row.Add<html::ui::Field>("New password", "new_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password")
		           .SetAttribute("id", "node_reset_pw");
		Row.Add<html::ui::Field>("Confirm new password", "confirm_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password")
		           .SetAttribute("id", "node_reset_pw2");
		Row.Add<html::Button>("Generate", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktGenPassword(this, ['node_reset_pw', 'node_reset_pw2'])");
		Row.Add<html::Button>("Reset", html::Button::SUBMIT, html::Classes{"btn"});
	}

	// generate a strong random password into the given fields, make it
	// visible, and copy it to the clipboard (with a button-label ack)
	main.Add<html::Script>(R"(
function ktGenPassword(button, fieldIds) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  var pw = '';
  while (pw.length < 24) {
    var a = new Uint8Array(32);
    crypto.getRandomValues(a);
    for (var i = 0; i < a.length && pw.length < 24; ++i) {
      // 248 = 4 * 62: reject the tail to keep the distribution uniform
      if (a[i] < 248) pw += chars[a[i] % 62];
    }
  }
  fieldIds.forEach(function(id) {
    var f = document.getElementById(id);
    if (f) { f.type = 'text'; f.value = pw; }
  });
  function ack(label) {
    var old = button.textContent;
    button.textContent = label;
    setTimeout(function() { button.textContent = old; }, 2000);
  }
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(pw).then(function() { ack('Copied!'); },
                                           function() { ack('Generated'); });
  } else {
    ack('Generated');
  }
}
)");

	RenderPage(HTTP, Page);

} // ShowOutlets

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletsAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sName     = HTTP.GetQueryParm("name");
	const auto& sPassword = HTTP.GetQueryParm("password");
	const auto& sEnabled  = HTTP.GetQueryParm("enabled");

	if (sName.empty() || sPassword.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "Name and password must not be empty.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	if (Store.GetOutlet(sName))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Outlet '{}' already exists.", sName));
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sPassword));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "Failed to hash password.");
		return;
	}

	KTunnelStore::Outlet n;
	n.sName         = sName;
	n.sPasswordHash = sHash;
	n.bEnabled      = !sEnabled.empty();

	if (!Store.AddOutlet(n))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Cannot add outlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "node_add";
	ev.sAdmin  = sMe;
	ev.sOutlet   = n.sName;
	ev.sDetail = kFormat("added outlet '{}'{}", n.sName, n.bEnabled ? "" : " (disabled)");
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sOutletsURL, kFormat("Outlet '{}' added.", n.sName), "");

} // HandleOutletsAdd

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletsToggle (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName   = HTTP.GetQueryParm("name");
	const auto&   sEnable = HTTP.GetQueryParm("enable");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "No outlet name provided.");
		return;
	}

	const bool bEnable = (sEnable == "1");

	auto& Store = m_Server.GetStore();
	if (!Store.GetOutlet(sName))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Outlet '{}' does not exist.", sName));
		return;
	}

	if (!Store.SetOutletEnabled(sName, bEnable))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Cannot update outlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = bEnable ? "node_enable" : "node_disable";
	ev.sAdmin  = sMe;
	ev.sOutlet   = sName;
	ev.sDetail = bEnable ? "enabled" : "disabled";
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sOutletsURL,
	                  kFormat("Outlet '{}' {}.", sName, bEnable ? "enabled" : "disabled"),
	                  "");

} // HandleOutletsToggle

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletsDelete (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName = HTTP.GetQueryParm("name");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "No outlet name provided.");
		return;
	}

	auto& Store = m_Server.GetStore();
	if (!Store.GetOutlet(sName))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Outlet '{}' does not exist.", sName));
		return;
	}

	if (!Store.DeleteOutlet(sName))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Cannot delete outlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "node_del";
	ev.sAdmin  = sMe;
	ev.sOutlet   = sName;
	ev.sDetail = kFormat("deleted outlet '{}'", sName);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sOutletsURL, kFormat("Outlet '{}' deleted.", sName), "");

} // HandleOutletsDelete

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletsResetPass (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sName    = HTTP.GetQueryParm("name");
	const auto& sNew     = HTTP.GetQueryParm("new_password");
	const auto& sConfirm = HTTP.GetQueryParm("confirm_password");

	if (sName.empty() || sNew.empty() || sConfirm.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "All fields are required.");
		return;
	}
	if (sNew != sConfirm)
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "New password and confirmation do not match.");
		return;
	}
	if (sNew.size() < 6)
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "New password must be at least 6 characters long.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	if (!Store.GetOutlet(sName))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Outlet '{}' does not exist.", sName));
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sNew));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "Failed to hash new password.");
		return;
	}

	if (!Store.UpdateOutletPasswordHash(sName, sHash))
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", kFormat("Cannot update password: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "node_password_change";
	ev.sAdmin  = sMe;
	ev.sOutlet   = sName;
	ev.sDetail = "admin UI";
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sOutletsURL,
	                  kFormat("Password reset for outlet '{}'.", sName), "");

} // HandleOutletsResetPass

//-----------------------------------------------------------------------------
void AdminUI::HandleInletsAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName     = HTTP.GetQueryParm("name");
	const auto&   sPassword = HTTP.GetQueryParm("password");
	const auto&   sTunnels  = HTTP.GetQueryParm("allow_tunnels");
	const auto&   sEnabled  = HTTP.GetQueryParm("enabled");

	if (sName.empty() || sPassword.empty())
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", "Name and password must not be empty.");
		return;
	}

	auto& Store = m_Server.GetStore();

	if (Store.GetInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", kFormat("Inlet '{}' already exists.", sName));
		return;
	}

	auto sHash = m_Server.GetBCrypt().GenerateHash(KStringViewZ(sPassword));

	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", "Failed to hash the password.");
		return;
	}

	KTunnelStore::Inlet c;
	c.sName         = sName;
	c.sPasswordHash = sHash;
	c.bEnabled      = !sEnabled.empty();
	c.sAllowTunnels = sTunnels;

	if (!Store.AddInlet(c))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "",
		                  kFormat("Cannot add inlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "inlet_add";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("added inlet '{}' ({}){}", c.sName,
	                     c.sAllowTunnels.empty() ? KString("all tunnels")
	                                             : kFormat("tunnels: {}", c.sAllowTunnels),
	                     c.bEnabled ? "" : " [disabled]");
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sInletsURL, kFormat("Inlet '{}' added.", c.sName), "");

} // HandleInletsAdd

//-----------------------------------------------------------------------------
void AdminUI::HandleInletsToggle (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName   = HTTP.GetQueryParm("name");
	const bool    bEnable = (HTTP.GetQueryParm("enable") == "1");

	auto& Store = m_Server.GetStore();

	if (sName.empty() || !Store.GetInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", kFormat("Inlet '{}' does not exist.", sName));
		return;
	}

	if (!Store.SetInletEnabled(sName, bEnable))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "",
		                  kFormat("Cannot change inlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = bEnable ? "inlet_enable" : "inlet_disable";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("inlet '{}'", sName);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sInletsURL,
	                  kFormat("Inlet '{}' {}.", sName, bEnable ? "enabled" : "disabled"), "");

} // HandleInletsToggle

//-----------------------------------------------------------------------------
void AdminUI::HandleInletsDelete (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName = HTTP.GetQueryParm("name");

	auto& Store = m_Server.GetStore();

	if (sName.empty() || !Store.GetInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", kFormat("Inlet '{}' does not exist.", sName));
		return;
	}

	if (!Store.DeleteInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "",
		                  kFormat("Cannot delete inlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "inlet_del";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("deleted inlet '{}'", sName);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sInletsURL, kFormat("Inlet '{}' deleted.", sName), "");

} // HandleInletsDelete

//-----------------------------------------------------------------------------
void AdminUI::HandleInletsResetPass (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName = HTTP.GetQueryParm("name");
	const auto&   sNew  = HTTP.GetQueryParm("new_password");

	if (sName.empty() || sNew.empty())
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", "Inlet and new password are required.");
		return;
	}
	if (sNew.size() < 6)
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", "New password must be at least 6 characters long.");
		return;
	}

	auto& Store = m_Server.GetStore();

	if (!Store.GetInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", kFormat("Inlet '{}' does not exist.", sName));
		return;
	}

	auto sHash = m_Server.GetBCrypt().GenerateHash(KStringViewZ(sNew));

	if (sHash.empty() || !Store.UpdateInletPasswordHash(sName, sHash))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "",
		                  kFormat("Cannot update password: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "inlet_password_change";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("inlet '{}'", sName);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sInletsURL, kFormat("Password reset for inlet '{}'.", sName), "");

} // HandleInletsResetPass

//-----------------------------------------------------------------------------
void AdminUI::HandleInletsTunnels (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName    = HTTP.GetQueryParm("name");
	const auto&   sTunnels = HTTP.GetQueryParm("allow_tunnels");

	auto& Store = m_Server.GetStore();

	if (sName.empty() || !Store.GetInlet(sName))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "", kFormat("Inlet '{}' does not exist.", sName));
		return;
	}

	// warn instead of failing: a name that does not exist yet may be
	// created later, but a typo should not go unnoticed
	KString sUnknown;

	for (auto sWanted : sTunnels.Split(","))
	{
		if (sWanted.empty()) continue;

		if (!Store.GetTunnel(sWanted))
		{
			if (!sUnknown.empty()) sUnknown += ", ";
			sUnknown += sWanted;
		}
	}

	if (!Store.SetInletAllowTunnels(sName, sTunnels))
	{
		RedirectWithFlash(HTTP, s_sInletsURL, "",
		                  kFormat("Cannot update inlet: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind   = "config_change";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("inlet '{}' may use {}", sName,
	                     sTunnels.empty() ? KString("all tunnels") : KString(sTunnels));
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sInletsURL,
	                  kFormat("Inlet '{}' updated.{}", sName,
	                          sUnknown.empty() ? KString()
	                                           : kFormat(" Note: no tunnel named {} exists (yet).", sUnknown)),
	                  "");

} // HandleInletsTunnels

//-----------------------------------------------------------------------------
void AdminUI::ShowInlets (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sNotice = HTTP.GetQueryParm("notice");
	const auto&   sError  = HTTP.GetQueryParm("error");

	auto& Store   = m_Server.GetStore();
	auto  Inlets = Store.GetAllInlets();
	auto  Tunnels = Store.GetAllTunnels();
	auto  Live    = m_Server.SnapshotActiveInlets();

	KUnorderedSet<KString> OnlineClients;
	for (const auto& c : Live) OnlineClients.insert(c.sName);

	auto Page = MakePage("ktunnel — Inlets");
	RenderTopBar(Page, "inlets", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	{
		auto p = main.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("An inlet is an operator-side ktunnel started with -L forwards. It "
		          "connects in here, authenticates, and asks for a channel to a "
		          "tunnel BY NAME - it never names a target host itself. Use this "
		          "instead of exposing a forward port: bind such a tunnel to "
		          "127.0.0.1 and it is only reachable through an authenticated inlet.");
	}

	// --- Section 1: inlet list ---------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Inlets ({})", Inlets.size()));

		if (Inlets.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No inlet identities yet. Add one below, then start "
			          "ktunnel on the operator's machine with "
			          "-relay <this host> -n <name> -s <password> "
			          "-L <localport>:<tunnel>.");
		}
		else
		{
			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Name", "Enabled", "May use", "Last login", "Created", "" });

			for (const auto& c : Inlets)
			{
				auto Row  = Table.AddRow();
				auto Name = Row.Add<html::TableData>();
				Name.AddText(c.sName);
				if (OnlineClients.count(c.sName))
				{
					RenderPill(Name, "ok", "connected");
				}

				RenderPill(Row.Add<html::TableData>(),
				           c.bEnabled ? "ok"      : "neutral",
				           c.bEnabled ? "enabled" : "disabled");

				auto Allowed = Row.Add<html::TableData>();
				if (c.sAllowTunnels.empty())
				{
					RenderPill(Allowed, "info", "all tunnels");
				}
				else
				{
					Allowed.AddText(c.sAllowTunnels);
				}

				Row.Add<html::TableData>(c.tLastLogin.to_time_t() > 0
				                             ? kFormat("{} UTC", c.tLastLogin.to_string())
				                             : KString("—"));
				Row.Add<html::TableData>(c.tCreated.to_time_t() > 0
				                             ? kFormat("{} UTC", c.tCreated.to_string())
				                             : KString("—"));

				auto Actions = Row.Add<html::TableData>();

				auto Toggle = Actions.Add<html::Form>(s_sInletsToggleURL, html::Classes{"inline-form"});
				Toggle.SetMethod(html::Form::POST);
				Toggle.Add<html::Input>("name",   c.sName,                html::Input::HIDDEN);
				Toggle.Add<html::Input>("enable", c.bEnabled ? "0" : "1", html::Input::HIDDEN);
				Toggle.Add<html::Button>(c.bEnabled ? "Disable" : "Enable",
				                         html::Button::SUBMIT, html::Classes{"btn small"});

				auto Delete = Actions.Add<html::Form>(s_sInletsDeleteURL, html::Classes{"inline-form"});
				Delete.SetMethod(html::Form::POST);
				Delete.SetAttribute("onsubmit",
				                    kFormat("return confirm('Delete inlet {}?');", c.sName));
				Delete.Add<html::Input>("name", c.sName, html::Input::HIDDEN);
				Delete.Add<html::Button>("Delete", html::Button::SUBMIT,
				                         html::Classes{"btn danger small"});
			}
		}
	}

	// --- Section 1b: live inlet sessions ------------------------------
	// One inlet identity may hold several connections at once, so this
	// lists sessions, not accounts.
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Connected inlets ({})", Live.size()));

		if (Live.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No inlet is connected right now. An inlet only holds a "
			          "connection while its ktunnel runs - it has no admin UI of "
			          "its own, so this is where you watch it.");
		}
		else
		{
			const auto tNow = KUnixTime::now();

			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Inlet", "From", "Connected", "Channels", "RX", "TX" });

			for (const auto& c : Live)
			{
				auto Row = Table.AddRow();
				Row.Add<html::TableData>(c.sName);
				Row.Add<html::TableData>(c.EndpointAddr.Serialize());
				Row.Add<html::TableData>(FormatDuration(tNow - c.tConnected));
				Row.Add<html::TableData>(kFormat("{}", c.Tunnel->GetConnectionCount()))
				   .SetAttribute("class", "num");
				Row.Add<html::TableData>(kFormBytes(c.Tunnel->GetBytesRx()))
				   .SetAttribute("class", "num");
				Row.Add<html::TableData>(kFormBytes(c.Tunnel->GetBytesTx()))
				   .SetAttribute("class", "num");
			}
		}
	}

	// --- Section 2: add inlet ----------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Add inlet");

		auto Form = sec.Add<html::Form>(s_sInletsAddURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Name", "name")
		   .Input().SetRequired(true).SetAttribute("autocomplete", "off");
		Row.Add<html::ui::Field>("Password", "password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password")
		           .SetAttribute("id", "inlet_add_pw");
		Row.Add<html::Button>("Generate", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktGenPassword(this, ['inlet_add_pw'])");
		Row.Add<html::ui::Field>("May use tunnels (empty = all)", "allow_tunnels")
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "db, web");

		auto Field = Row.Add<html::Div>(html::Classes("field"));
		auto Label = Field.AddElement("label");
		Label.SetAttribute("class", "checkbox");
		Label.Add<html::Input>("enabled", "1", html::Input::CHECKBOX).SetChecked(true);
		Label.Add<html::Span>().AddText("Enabled");

		Row.Add<html::Button>("Add", html::Button::SUBMIT, html::Classes{"btn"});

		if (!Tunnels.empty())
		{
			KString sNames;
			for (const auto& t : Tunnels)
			{
				if (!sNames.empty()) sNames += ", ";
				sNames += t.sName;
			}
			sec.Add<html::Div>(html::Classes("muted"))
			   .AddText(kFormat("Configured tunnels: {}", sNames));
		}
	}

	// --- Section 3: reset password / change allow list ------------------
	if (!Inlets.empty())
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Reset inlet password");

		auto Form = sec.Add<html::Form>(s_sInletsResetPwURL);
		Form.SetMethod(html::Form::POST);

		auto Row   = Form.Add<html::Div>(html::Classes("row"));
		auto Field = Row.Add<html::Div>(html::Classes("field"));
		Field.AddElement("label").AddText("Inlet");
		auto Select = Field.Add<html::Select>("name");
		Select.SetRequired(true);
		for (const auto& c : Inlets)
		{
			Select.Add<html::Option>(c.bEnabled ? c.sName
			                                    : kFormat("{} (disabled)", c.sName),
			                         c.sName);
		}

		Row.Add<html::ui::Field>("New password", "new_password", "", html::Input::PASSWORD)
		   .Input().SetRequired(true).SetAttribute("autocomplete", "new-password")
		           .SetAttribute("id", "inlet_reset_pw");
		Row.Add<html::Button>("Generate", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktGenPassword(this, ['inlet_reset_pw'])");
		Row.Add<html::Button>("Reset", html::Button::SUBMIT, html::Classes{"btn"});

		auto sec2 = main.Add<html::Div>(html::Classes("section"));
		sec2.Add<html::Heading>(2, "Change allowed tunnels");

		auto Form2 = sec2.Add<html::Form>(s_sInletsTunnelsURL);
		Form2.SetMethod(html::Form::POST);

		auto Row2   = Form2.Add<html::Div>(html::Classes("row"));
		auto Field2 = Row2.Add<html::Div>(html::Classes("field"));
		Field2.AddElement("label").AddText("Inlet");
		auto Select2 = Field2.Add<html::Select>("name");
		Select2.SetRequired(true);
		for (const auto& c : Inlets)
		{
			Select2.Add<html::Option>(c.sName, c.sName);
		}

		Row2.Add<html::ui::Field>("May use tunnels (empty = all)", "allow_tunnels")
		    .Input().SetAttribute("autocomplete", "off")
		            .SetAttribute("placeholder", "db, web");
		Row2.Add<html::Button>("Save", html::Button::SUBMIT, html::Classes{"btn"});
	}

	// same generator as on the outlets page
	main.Add<html::Script>(R"(
function ktGenPassword(button, fieldIds) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  var pw = '';
  while (pw.length < 24) {
    var a = new Uint8Array(32);
    crypto.getRandomValues(a);
    for (var i = 0; i < a.length && pw.length < 24; ++i) {
      // 248 = 4 * 62: reject the tail to keep the distribution uniform
      if (a[i] < 248) pw += chars[a[i] % 62];
    }
  }
  fieldIds.forEach(function(id) {
    var f = document.getElementById(id);
    if (f) { f.type = 'text'; f.value = pw; }
  });
  function ack(label) {
    var old = button.textContent;
    button.textContent = label;
    setTimeout(function() { button.textContent = old; }, 2000);
  }
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(pw).then(function() { ack('Copied!'); },
                                           function() { ack('Generated'); });
  } else {
    ack('Generated');
  }
}
)");

	RenderPage(HTTP, Page);

} // ShowInlets

//-----------------------------------------------------------------------------
void AdminUI::ShowOutletInstall (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sNode = HTTP.GetQueryParm("outlet");

	auto oOutlet = m_Server.GetStore().GetOutlet(sNode);
	if (!oOutlet)
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "",
		                  kFormat("Outlet '{}' does not exist.", sNode));
		return;
	}

	// the name is embedded in a shell script and a JS string below -
	// only pass it through when it cannot break out of either context
	if (oOutlet->sName.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                                   "abcdefghijklmnopqrstuvwxyz"
	                                   "0123456789._-") != KString::npos)
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "",
		                  kFormat("Outlet '{}' contains characters that cannot be "
		                          "used in a setup script - stick to letters, "
		                          "digits, '.', '_' and '-'.", oOutlet->sName));
		return;
	}

	// best-effort default for the public address of this server: the
	// Host header the admin used to reach this page (minus the port)
	KTCPEndPoint HostHeader(HTTP.Request.Headers.Get(KHTTPHeader::HOST));
	KString sDefaultHost = HostHeader.Domain.get();

	auto Page = MakePage(kFormat("ktunnel — Install {}", sNode));
	RenderTopBar(Page, "outlets", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	// --- Section 1: parameters ------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Install outlet · {}", sNode));

		auto p = sec.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("The script below is assembled in your browser. The outlet "
		          "host needs a ktunnel binary first (e.g. copy a static build "
		          "to /usr/local/bin).");

		auto Row = sec.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Relay host (public address)", "", sDefaultHost)
		   .Input().SetAttribute("id", "ki_host").SetAttribute("autocomplete", "off");
		Row.Add<html::ui::Field>("Outlet password", "")
		   .Input().SetAttribute("id", "ki_pw").SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "paste it, or generate a new one");
		Row.Add<html::Button>("Generate & set", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktGenNodePassword(this)");

		// Stored passwords are bcrypt hashes, so an existing password cannot
		// be shown here - the button generates a new one, sets it on the outlet
		// account (same path as "Reset outlet password"), and puts it into the
		// script. Only that button sends the password to this server; a value
		// you paste yourself stays in the browser.
		sec.Add<html::Div>(html::Classes("muted"))
		   .AddText("Outlet passwords are stored as bcrypt hashes and cannot be "
		            "shown again. Either paste the password you kept when the "
		            "outlet was created, or press \"Generate & set\" to give the "
		            "outlet a fresh one right now - that replaces its current "
		            "password immediately, so an already running outlet "
		            "with the old one stops logging in.");
	}

	// --- Section 2: setup script ----------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Setup script (run as root on the outlet host)");

		sec.AddElement("textarea").SetAttribute("id", "ki_script")
		   .SetAttribute("readonly", "readonly").SetAttribute("rows", "14")
		   .SetAttribute("style", "width:100%;font-family:ui-monospace,Menlo,monospace;"
		                          "font-size:0.75rem;white-space:pre;overflow-x:auto");

		sec.Add<html::Button>("Copy script", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktCopy(this, 'ki_script')");
	}

	// --- Section 3: one-shot install over SSH ----------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Or install in one step over SSH");

		auto Row = sec.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("SSH target", "", "")
		   .Input().SetAttribute("id", "ki_ssh").SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "root@outlet-host");

		sec.AddElement("textarea").SetAttribute("id", "ki_sshcmd")
		   .SetAttribute("readonly", "readonly").SetAttribute("rows", "16")
		   .SetAttribute("style", "width:100%;font-family:ui-monospace,Menlo,monospace;"
		                          "font-size:0.75rem;white-space:pre;overflow-x:auto");

		sec.Add<html::Button>("Copy SSH command", html::Button::BUTTON, html::Classes{"btn small"})
		   .SetAttribute("onclick", "ktCopy(this, 'ki_sshcmd')");

		auto p = sec.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("Run this from your workstation. To also copy the binary "
		          "first: scp ktunnel root@outlet-host:/usr/local/bin/");
	}

	// build the script inlet-side from the fields above; server-fixed
	// parts (outlet, port, AES pinning) are substituted before delivery
	KString sScript(R"(
(function() {
  var tpl =
    "#!/bin/sh\n" +
    "# ktunnel outlet setup for '__OUTLET__'\n" +
    "# generated by the admin UI of __HOSTNAME__\n" +
    "set -e\n" +
    "command -v ktunnel >/dev/null 2>&1 || {\n" +
    "  echo 'ktunnel binary not found - copy a static build to /usr/local/bin first' >&2\n" +
    "  exit 1\n" +
    "}\n" +
    "umask 077\n" +
    "mkdir -p /etc/ktunnel\n" +
    "cat > /etc/ktunnel/secret.__OUTLET__ << 'KTEOF'\n" +
    "__PASSWORD__\n" +
    "KTEOF\n" +
    "ktunnel -install -relay __HOST__ -p __PORT__ -n __OUTLET__ -secret-file /etc/ktunnel/secret.__OUTLET____EXTRA__\n" +
    "ktunnel -start\n" +
    "ktunnel -status\n";

  var elHost   = document.getElementById('ki_host');
  var elPw     = document.getElementById('ki_pw');
  var elSsh    = document.getElementById('ki_ssh');
  var elScript = document.getElementById('ki_script');
  var elSshCmd = document.getElementById('ki_sshcmd');

  function rebuild() {
    var host = elHost.value.trim() || '<relay-host>';
    var pw   = elPw.value          || '<outlet-password>';
    var s    = tpl.split('__HOST__').join(host).split('__PASSWORD__').join(pw);
    elScript.value = s;
    var ssh = elSsh.value.trim() || 'root@' + host;
    elSshCmd.value = "ssh " + ssh + " sh << 'KTSH'\n" + s + "KTSH";
  }

  [elHost, elPw, elSsh].forEach(function(el) { el.addEventListener('input', rebuild); });
  rebuild();
})();

function ktAck(button, label) {
  var old = button.textContent;
  button.textContent = label;
  setTimeout(function() { button.textContent = old; }, 2500);
}

function ktCopy(button, id) {
  var el = document.getElementById(id);
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(el.value).then(function() { ktAck(button, 'Copied!'); },
                                                 function() { el.select(); ktAck(button, 'Select & copy'); });
  } else {
    el.select();
    ktAck(button, 'Select & copy');
  }
}

// generate a fresh outlet password, set it on the outlet account through the
// regular reset endpoint, and drop it into the script fields
function ktGenNodePassword(button) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  var pw = '';
  while (pw.length < 24) {
    var a = new Uint8Array(32);
    crypto.getRandomValues(a);
    for (var i = 0; i < a.length && pw.length < 24; ++i) {
      // 248 = 4 * 62: reject the tail to keep the distribution uniform
      if (a[i] < 248) pw += chars[a[i] % 62];
    }
  }
  var body = new URLSearchParams();
  body.append('name',             '__OUTLET__');
  body.append('new_password',     pw);
  body.append('confirm_password', pw);
  button.disabled = true;
  fetch('__RESETURL__', { method: 'POST', credentials: 'same-origin', body: body })
    .then(function(r) {
      button.disabled = false;
      if (!r.ok) { ktAck(button, 'Failed'); return; }
      var f = document.getElementById('ki_pw');
      f.value = pw;
      f.dispatchEvent(new Event('input'));
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(pw).then(function() { ktAck(button, 'Set & copied'); },
                                               function() { ktAck(button, 'Password set'); });
      } else {
        ktAck(button, 'Password set');
      }
    })
    .catch(function() { button.disabled = false; ktAck(button, 'Failed'); });
}
)");

	// the AES trust pin and a possible -notls travel inside __EXTRA__
	KString sExtra;
	if (m_Config.bNoTLS)
	{
		sExtra += " -notls";
	}
	const auto sFingerprint = m_Server.GetServerFingerprint();
	if (m_Config.bAESPayload && !sFingerprint.empty())
	{
		sExtra += kFormat(" -aes -trust-fingerprint {}", sFingerprint);
	}

	sScript.Replace("__OUTLET__",     oOutlet->sName);
	sScript.Replace("__RESETURL__", s_sOutletsResetPwURL);
	sScript.Replace("__PORT__",     KString::to_string(m_Config.iPort));
	sScript.Replace("__HOSTNAME__", sDefaultHost);
	sScript.Replace("__EXTRA__",    sExtra);

	main.Add<html::Script>(sScript);

	RenderPage(HTTP, Page);

} // ShowOutletInstall

namespace {

/// Extract a decimal port number from a form field. Returns 0 if the
/// string is empty, not fully numeric, or out of the 1..65535 range.
/// Callers surface a user-visible error when the result is 0.
uint16_t ParsePort (KStringView sValue)
{
	if (sValue.empty()) return 0;
	auto i = sValue.UInt32(/*bIsHex=*/false);
	if (i == 0 || i > 65535) return 0;
	return static_cast<uint16_t>(i);
}

} // anonymous namespace

//-----------------------------------------------------------------------------
void AdminUI::ShowTunnels (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto& Store       = m_Server.GetStore();
	auto Tunnels      = Store.GetAllTunnels();
	auto Outlets        = Store.GetAllOutlets();
	auto ListenerMap  = m_Server.SnapshotListenerStates();
	auto Connections  = m_Server.SnapshotConnections();
	auto ActiveNodes  = m_Server.SnapshotActiveTunnels();

	KUnorderedSet<KString> OnlineNodes;
	for (const auto& at : ActiveNodes) OnlineNodes.insert(at.sOutlet);

	auto Page = MakePage("ktunnel — Tunnels");
	RenderTopBar(Page, "tunnels", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// --- Section 0: built-in forwarder from CLI -----------------------
	// Only shown when the process was launched with -f/-t. This row is
	// not backed by the tunnels table and therefore has no Edit / Toggle
	// / Delete actions — restart ktunnel with different flags to change
	// it. We still surface it here so the admin can see at a glance that
	// there is an additional raw listener hogging a port.
	if (m_Config.iRawPort)
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Built-in forwarder (from CLI)");

		auto p = sec.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("Configured via -f / -t at process start. "
		          "Not stored in the database; restart ktunnel with "
		          "different flags to change it.");

		auto Table = sec.Add<html::ui::Table>();
		Table.Headers({ "Name", "Forward port", "Target", "Runtime" });

		auto Row = Table.AddRow();
		Row.Add<html::TableData>().AddElement("em").AddText("(builtin)");
		Row.Add<html::TableData>(kFormat("{}", m_Config.iRawPort));

		auto Target = Row.Add<html::TableData>();
		if (m_Config.DefaultTarget.empty())
		{
			Target.AddElement("em").SetAttribute("class", "muted").AddText("(no default target)");
		}
		else
		{
			Target.AddText(kFormat("{}:{}",
			                       m_Config.DefaultTarget.Domain.get(),
			                       m_Config.DefaultTarget.Port.get()));
		}

		RenderPill(Row.Add<html::TableData>(), "ok", "listening");
	}

	// --- Section 1: tunnel list ---------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Tunnels ({})", Tunnels.size()));

		if (Tunnels.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No tunnel listeners configured yet. Use the form below to add one.");
		}
		else
		{
			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Name", "Outlet", "Forward port", "Target", "Access", "Config", "Runtime", "" });

			for (const auto& t : Tunnels)
			{
				auto Row = Table.AddRow();
				Row.Add<html::TableData>(t.sName);
				Row.Add<html::TableData>().Add<html::Link>(s_sOutletsURL, t.sOutlet);
				Row.Add<html::TableData>(t.sBindAddress.empty()
				                             ? kFormat("{}", t.iListenPort)
				                             : kFormat("{}:{}", t.sBindAddress, t.iListenPort));
				Row.Add<html::TableData>(kFormat("{}:{}", t.sTargetHost, t.iTargetPort));

				// access control at a glance: a listener on the wildcard
				// without an allow list is reachable by anyone
				{
					auto Access = Row.Add<html::TableData>();

					if (!t.sAllowFrom.empty())
					{
						RenderPill(Access, "ok", "allow list");
						auto div = Access.Add<html::Div>(html::Classes("muted"));
						div.SetStyle("font-size:0.7rem");
						div.AddText(t.sAllowFrom);
					}
					else if (!t.sBindAddress.empty())
					{
						RenderPill(Access, "info", "bound");
						auto div = Access.Add<html::Div>(html::Classes("muted"));
						div.SetStyle("font-size:0.7rem");
						div.AddText(kFormat("{} only", t.sBindAddress));
					}
					else
					{
						RenderPill(Access, "fail", "open");
						auto div = Access.Add<html::Div>(html::Classes("muted"));
						div.SetStyle("font-size:0.7rem");
						div.AddText("any source may reach the target");
					}
				}

				RenderPill(Row.Add<html::TableData>(),
				           t.bEnabled ? "ok"      : "neutral",
				           t.bEnabled ? "enabled" : "disabled");

				auto State = Row.Add<html::TableData>();
				auto it    = ListenerMap.find(t.sName);
				if (it != ListenerMap.end())
				{
					const auto& li = it->second;

					RenderListenerStatePill(State, li.eState, li.sError);

					if (li.iForwarded || li.iFailed || li.iRejected)
					{
						auto Stats = State.Add<html::Div>(html::Classes("muted"));
						Stats.SetStyle("font-size:0.7rem");
						Stats.AddText(li.iRejected
							? kFormat("{} forwarded · {} failed · {} rejected",
							          li.iForwarded, li.iFailed, li.iRejected)
							: kFormat("{} forwarded · {} failed",
							          li.iForwarded, li.iFailed));
					}

					if (!li.sLastConnError.empty())
					{
						auto Err = State.Add<html::Div>(html::Classes("muted"));
						Err.SetStyle("font-size:0.7rem;color:#f5a3a3");
						Err.AddText(kFormat("{} UTC: {}",
						                    li.tLastConnError.to_string(),
						                    li.sLastConnError));
					}
				}
				else
				{
					RenderPill(State, "neutral", "stopped");
				}

				// Build the ?name=<n> edit-link safely even if the tunnel
				// name contains URL-special characters (unlikely since we
				// already require a non-empty trimmed string in Add, but
				// good hygiene).
				KString sEditURL = kFormat("{}?name={}",
					s_sTunnelsEditURL,
					kUrlEncode(t.sName, URIPart::Query));

				auto Actions = Row.Add<html::TableData>();

				Actions.Add<html::Link>(sEditURL, "Edit", html::Classes{"btn small"})
				       .SetStyle("text-decoration:none;display:inline-flex;"
				                 "align-items:center;justify-content:center;");

				if (OnlineNodes.count(t.sOutlet))
				{
					// ask the outlet to try a TCP connect to the target -
					// tells reachability apart from protocol problems
					auto Check = Actions.Add<html::Form>(s_sTunnelsCheckURL, html::Classes{"inline-form"});
					Check.SetMethod(html::Form::POST);
					Check.Add<html::Input>("name", t.sName, html::Input::HIDDEN);
					Check.Add<html::Button>("Test target", html::Button::SUBMIT,
					                        html::Classes{"btn small"});
				}

				auto Toggle = Actions.Add<html::Form>(s_sTunnelsToggleURL, html::Classes{"inline-form"});
				Toggle.SetMethod(html::Form::POST);
				Toggle.Add<html::Input>("name",   t.sName,               html::Input::HIDDEN);
				Toggle.Add<html::Input>("enable", t.bEnabled ? "0" : "1", html::Input::HIDDEN);
				Toggle.Add<html::Button>(t.bEnabled ? "Disable" : "Enable",
				                         html::Button::SUBMIT, html::Classes{"btn small"});

				auto Delete = Actions.Add<html::Form>(s_sTunnelsDeleteURL, html::Classes{"inline-form"});
				Delete.SetMethod(html::Form::POST);
				Delete.SetAttribute("onsubmit",
				                    kFormat("return confirm('Delete tunnel {}?');", t.sName));
				Delete.Add<html::Input>("name", t.sName, html::Input::HIDDEN);
				Delete.Add<html::Button>("Delete", html::Button::SUBMIT,
				                         html::Classes{"btn danger small"});
			}
		}
	}

	// --- Section 2: active connections --------------------------------
	// Live view of every multiplexed connection currently running over
	// any tunnel — the fastest way to see whether traffic actually flows.
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Active connections ({})", Connections.size()));

		if (Connections.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No connections are open right now. This list shows "
			          "every TCP connection currently forwarded through a tunnel.");
		}
		else
		{
			const auto tNow = KUnixTime::now();

			auto Table = sec.Add<html::ui::Table>();
			Table.Headers({ "Outlet", "Channel", "Target", "From", "Age", "To target", "From target" });

			for (const auto& c : Connections)
			{
				auto Row = Table.AddRow();
				Row.Add<html::TableData>(c.sOutlet);
				Row.Add<html::TableData>(kFormat("{}", c.iChannel));
				Row.Add<html::TableData>(c.sTarget);
				Row.Add<html::TableData>(c.sPeer);
				Row.Add<html::TableData>(FormatDuration(tNow - c.tStart));
				Row.Add<html::TableData>(kFormBytes(c.iBytesToTarget)).SetAttribute("class", "num");
				Row.Add<html::TableData>(kFormBytes(c.iBytesFromTarget)).SetAttribute("class", "num");
			}
		}
	}

	// --- Section 3: add tunnel form -----------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Add tunnel");

		if (Outlets.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No outlets are configured yet — a tunnel must point at "
			          "an existing outlet. Add one under ");
			p.Add<html::Link>(s_sOutletsURL, "Outlets");
			p.AddText(" first.");
		}
		else
		{
			auto Form = sec.Add<html::Form>(s_sTunnelsAddURL);
			Form.SetMethod(html::Form::POST);

			{
				auto Row = Form.Add<html::Div>(html::Classes("row"));

				Row.Add<html::ui::Field>("Name", "name")
				   .Input().SetRequired(true).SetAttribute("autocomplete", "off");

				auto Field = Row.Add<html::Div>(html::Classes("field"));
				Field.AddElement("label").AddText("Outlet");
				auto Select = Field.Add<html::Select>("outlet");
				Select.SetRequired(true);
				AddOutletOptions(Select, Outlets);
			}

			{
				auto Row = Form.Add<html::Div>(html::Classes("row"));

				Row.Add<html::ui::Field>("Forward port", "listen_port", "", html::Input::NUMBER)
				   .Input().SetRange(1, 65535).SetRequired(true);
				Row.Add<html::ui::Field>("Target host", "target_host")
				   .Input().SetRequired(true);
				Row.Add<html::ui::Field>("Target port", "target_port", "", html::Input::NUMBER)
				   .Input().SetRange(1, 65535).SetRequired(true);
			}

			{
				auto Row = Form.Add<html::Div>(html::Classes("row"));

				Row.Add<html::ui::Field>("Bind address (empty = all interfaces)", "bind_address")
				   .Input().SetAttribute("autocomplete", "off")
				           .SetAttribute("placeholder", "127.0.0.1");
				Row.Add<html::ui::Field>("Allow from (empty = any source)", "allow_from")
				   .Input().SetAttribute("autocomplete", "off")
				           .SetAttribute("placeholder", "192.168.1.0/24, 10.0.0.5");
			}

			Form.Add<html::Paragraph>(html::Classes("muted"))
			    .SetStyle("margin-top:0.25rem;font-size:0.75rem")
			    .AddText("Keep the forward port distinct from the admin/control port "
			             "(-p on the CLI). Without a bind address the port is open on "
			             "every interface, and without an allow list any source may "
			             "connect and reach the target - the target's own "
			             "authentication is then the only gate. Bind to 127.0.0.1 to "
			             "reach it through `ssh -L` instead.");

			{
				auto Row   = Form.Add<html::Div>(html::Classes("row"));
				auto Field = Row.Add<html::Div>(html::Classes("field"));
				auto Label = Field.AddElement("label");
				Label.SetAttribute("class", "checkbox");
				Label.Add<html::Input>("enabled", "1", html::Input::CHECKBOX).SetChecked(true);
				Label.Add<html::Span>().AddText("Enabled");

				Row.Add<html::Button>("Add tunnel", html::Button::SUBMIT, html::Classes{"btn"});
			}
		}
	}

	RenderPage(HTTP, Page);

} // ShowTunnels

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString  sMe(Sess.GetUser());

	const auto& sName       = HTTP.GetQueryParm("name");
	const auto& sNodeName   = HTTP.GetQueryParm("outlet");
	const auto& sListenPort = HTTP.GetQueryParm("listen_port");
	const auto& sTargetHost = HTTP.GetQueryParm("target_host");
	const auto& sTargetPort = HTTP.GetQueryParm("target_port");
	const auto& sEnabled    = HTTP.GetQueryParm("enabled");
	const auto& sBindAddr   = HTTP.GetQueryParm("bind_address");
	const auto& sAllowFrom  = HTTP.GetQueryParm("allow_from");

	if (sName.empty() || sNodeName.empty() || sTargetHost.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "",
		                  "Name, outlet and target host are required.");
		return;
	}

	const auto iListenPort = ParsePort(sListenPort);
	const auto iTargetPort = ParsePort(sTargetPort);
	if (iListenPort == 0 || iTargetPort == 0)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "Ports must be valid numbers between 1 and 65535.");
		return;
	}

	auto sAccessError = ValidateAccess(sBindAddr, sAllowFrom);
	if (!sAccessError.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", sAccessError);
		return;
	}

	auto& Store = m_Server.GetStore();

	if (!Store.GetOutlet(sNodeName))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Outlet '{}' is not a known outlet.", sNodeName));
		return;
	}
	if (Store.GetTunnel(sName))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' already exists.", sName));
		return;
	}

	KTunnelStore::Tunnel t;
	t.sName       = sName;
	t.sOutlet       = sNodeName;
	t.iListenPort = iListenPort;
	t.sTargetHost = sTargetHost;
	t.iTargetPort  = iTargetPort;
	t.bEnabled     = !sEnabled.empty();
	t.sBindAddress = sBindAddr;
	t.sAllowFrom   = sAllowFrom;

	if (!Store.AddTunnel(t))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Cannot add tunnel: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sAdmin      = sMe;
	ev.sOutlet       = t.sOutlet;
	ev.sTunnelName = t.sName;
	ev.sDetail     = kFormat("added tunnel {}:{} -> {}:{} (outlet {}, {}){}",
	                         t.sBindAddress.empty() ? KStringView("[::]") : KStringView(t.sBindAddress),
	                         t.iListenPort,
	                         t.sTargetHost, t.iTargetPort,
	                         t.sOutlet,
	                         DescribeAccess(t.sBindAddress, t.sAllowFrom),
	                         t.bEnabled ? "" : " [disabled]");
	Store.LogEvent(ev);

	// Hot-reload: pick up the new row without a process restart.
	m_Server.ReconcileListeners(sMe);

	RedirectWithFlash(HTTP, s_sTunnelsURL, kFormat("Tunnel '{}' added.", t.sName), "");

} // HandleTunnelsAdd

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsToggle (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString  sMe(Sess.GetUser());
	const auto&    sName   = HTTP.GetQueryParm("name");
	const auto&    sEnable = HTTP.GetQueryParm("enable");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "No tunnel name provided.");
		return;
	}

	const bool bEnable = (sEnable == "1");

	auto& Store = m_Server.GetStore();
	auto oT = Store.GetTunnel(sName);
	if (!oT)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' does not exist.", sName));
		return;
	}

	if (!Store.SetTunnelEnabled(sName, bEnable))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Cannot update tunnel: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sAdmin      = sMe;
	ev.sOutlet       = oT->sOutlet;
	ev.sTunnelName = sName;
	ev.sDetail     = bEnable ? "enabled" : "disabled";
	Store.LogEvent(ev);

	// Hot-reload: enabling a row starts the listener, disabling stops it.
	m_Server.ReconcileListeners(sMe);

	RedirectWithFlash(HTTP, s_sTunnelsURL,
	                  kFormat("Tunnel '{}' {}.", sName, bEnable ? "enabled" : "disabled"),
	                  "");

} // HandleTunnelsToggle

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsDelete (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString  sMe(Sess.GetUser());
	const auto&    sName = HTTP.GetQueryParm("name");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "No tunnel name provided.");
		return;
	}

	auto& Store = m_Server.GetStore();
	auto oT = Store.GetTunnel(sName);
	if (!oT)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' does not exist.", sName));
		return;
	}

	if (!Store.DeleteTunnel(sName))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Cannot delete tunnel: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sAdmin      = sMe;
	ev.sOutlet       = oT->sOutlet;
	ev.sTunnelName = sName;
	ev.sDetail     = "deleted";
	Store.LogEvent(ev);

	// Hot-reload: stop and drop the listener for the removed row.
	m_Server.ReconcileListeners(sMe);

	RedirectWithFlash(HTTP, s_sTunnelsURL, kFormat("Tunnel '{}' deleted.", sName), "");

} // HandleTunnelsDelete

//-----------------------------------------------------------------------------
void AdminUI::ShowTunnelEdit (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sName = HTTP.GetQueryParm("name");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "No tunnel name provided.");
		return;
	}

	auto& Store = m_Server.GetStore();
	auto oT = Store.GetTunnel(sName);
	if (!oT)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' does not exist.", sName));
		return;
	}

	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Outlets = Store.GetAllOutlets();

	auto Page = MakePage(kFormat("ktunnel — Edit {}", sName));
	RenderTopBar(Page, "tunnels", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	auto sec = main.Add<html::Div>(html::Classes("section"));
	sec.Add<html::Heading>(2, kFormat("Edit tunnel · {}", oT->sName));

	auto Form = sec.Add<html::Form>(s_sTunnelsUpdateURL);
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("name", oT->sName, html::Input::HIDDEN);

	{
		auto Row = Form.Add<html::Div>(html::Classes("row"));

		// The name stays read-only — it is the primary key that matches
		// the listener registry. Renaming is a delete + add; we surface
		// that as a separate UX action later if anyone asks.
		Row.Add<html::ui::Field>("Name (read-only)", "", oT->sName)
		   .Input().SetDisabled(true);

		auto Field = Row.Add<html::Div>(html::Classes("field"));
		Field.AddElement("label").AddText("Outlet");
		auto Select = Field.Add<html::Select>("outlet");
		Select.SetRequired(true);
		AddOutletOptions(Select, Outlets, oT->sOutlet);
	}

	{
		auto Row = Form.Add<html::Div>(html::Classes("row"));

		Row.Add<html::ui::Field>("Forward port", "listen_port",
		                         kFormat("{}", oT->iListenPort), html::Input::NUMBER)
		   .Input().SetRange(1, 65535).SetRequired(true);
		Row.Add<html::ui::Field>("Target host", "target_host", oT->sTargetHost)
		   .Input().SetRequired(true);
		Row.Add<html::ui::Field>("Target port", "target_port",
		                         kFormat("{}", oT->iTargetPort), html::Input::NUMBER)
		   .Input().SetRange(1, 65535).SetRequired(true);
	}

	{
		auto Row = Form.Add<html::Div>(html::Classes("row"));

		Row.Add<html::ui::Field>("Bind address (empty = all interfaces)",
		                         "bind_address", oT->sBindAddress)
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "127.0.0.1");
		Row.Add<html::ui::Field>("Allow from (empty = any source)",
		                         "allow_from", oT->sAllowFrom)
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "192.168.1.0/24, 10.0.0.5");
	}

	Form.Add<html::Paragraph>(html::Classes("muted"))
	    .SetStyle("margin-top:0.25rem;font-size:0.75rem")
	    .AddText("Keep the forward port distinct from the admin/control port "
	             "(-p on the CLI). Changing the bind address restarts the listener "
	             "and drops its live connections; changing the allow list only "
	             "affects new ones.");

	{
		auto Row   = Form.Add<html::Div>(html::Classes("row"));
		auto Field = Row.Add<html::Div>(html::Classes("field"));
		auto Label = Field.AddElement("label");
		Label.SetAttribute("class", "checkbox");
		Label.Add<html::Input>("enabled", "1", html::Input::CHECKBOX).SetChecked(oT->bEnabled);
		Label.Add<html::Span>().AddText("Enabled");

		Row.Add<html::Button>("Save", html::Button::SUBMIT, html::Classes{"btn"});
		Row.Add<html::Link>(s_sTunnelsURL, "Cancel", html::Classes{"btn"})
		   .SetStyle("background:#475569;text-decoration:none;"
		             "display:inline-flex;align-items:center;justify-content:center;");
	}

	RenderPage(HTTP, Page);

} // ShowTunnelEdit

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsUpdate (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	const auto& sName       = HTTP.GetQueryParm("name");
	const auto& sNodeName   = HTTP.GetQueryParm("outlet");
	const auto& sListenPort = HTTP.GetQueryParm("listen_port");
	const auto& sTargetHost = HTTP.GetQueryParm("target_host");
	const auto& sTargetPort = HTTP.GetQueryParm("target_port");
	const auto& sEnabled    = HTTP.GetQueryParm("enabled");
	const auto& sBindAddr   = HTTP.GetQueryParm("bind_address");
	const auto& sAllowFrom  = HTTP.GetQueryParm("allow_from");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "No tunnel name provided.");
		return;
	}

	// Short-hand for redirecting back to the edit page with an error
	// flash, so the operator can fix the input without retyping.
	auto BackToEdit = [&](KStringView sErr)
	{
		RedirectWithFlash(
			HTTP,
			kFormat("{}?name={}",
			        s_sTunnelsEditURL,
			        kUrlEncode(sName, URIPart::Query)),
			"", sErr);
	};

	if (sNodeName.empty() || sTargetHost.empty())
	{
		BackToEdit("Outlet and target host are required.");
		return;
	}

	const auto iListenPort = ParsePort(sListenPort);
	const auto iTargetPort = ParsePort(sTargetPort);
	if (iListenPort == 0 || iTargetPort == 0)
	{
		BackToEdit("Ports must be valid numbers between 1 and 65535.");
		return;
	}

	auto& Store = m_Server.GetStore();

	auto oExisting = Store.GetTunnel(sName);
	if (!oExisting)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' does not exist.", sName));
		return;
	}

	if (!Store.GetOutlet(sNodeName))
	{
		BackToEdit(kFormat("Outlet '{}' is not a known outlet.", sNodeName));
		return;
	}

	auto sAccessError = ValidateAccess(sBindAddr, sAllowFrom);
	if (!sAccessError.empty())
	{
		BackToEdit(sAccessError);
		return;
	}

	// Compose the new row. KTunnelStore::UpdateTunnel keeps id/created_utc,
	// so we only need to fill the editable fields + the name key.
	KTunnelStore::Tunnel t = *oExisting;
	t.sOutlet       = sNodeName;
	t.iListenPort = iListenPort;
	t.sTargetHost = sTargetHost;
	t.iTargetPort  = iTargetPort;
	t.bEnabled     = !sEnabled.empty();
	t.sBindAddress = sBindAddr;
	t.sAllowFrom   = sAllowFrom;

	if (!Store.UpdateTunnel(t))
	{
		BackToEdit(kFormat("Cannot update tunnel: {}", Store.GetLastError()));
		return;
	}

	// Build a human-friendly diff line for the audit log, only listing
	// fields that actually changed relative to the stored row.
	KString sDiff;
	auto addDiff = [&](KStringView sField, KStringView sOld, KStringView sNew)
	{
		if (sOld == sNew) return;
		if (!sDiff.empty()) sDiff += ", ";
		sDiff += kFormat("{}: {} -> {}", sField, sOld, sNew);
	};

	addDiff("outlet",  oExisting->sOutlet,     t.sOutlet);
	addDiff("forward_port", kFormat("{}", oExisting->iListenPort),
	                        kFormat("{}", t.iListenPort));
	addDiff("target",  kFormat("{}:{}", oExisting->sTargetHost, oExisting->iTargetPort),
	                   kFormat("{}:{}", t.sTargetHost,          t.iTargetPort));
	addDiff("enabled", oExisting->bEnabled ? KStringView("yes") : KStringView("no"),
	                   t.bEnabled          ? KStringView("yes") : KStringView("no"));
	addDiff("bind_address", oExisting->sBindAddress.empty() ? KStringView("*")
	                                                       : KStringView(oExisting->sBindAddress),
	                        t.sBindAddress.empty()         ? KStringView("*")
	                                                       : KStringView(t.sBindAddress));
	addDiff("allow_from",   oExisting->sAllowFrom.empty()  ? KStringView("any")
	                                                       : KStringView(oExisting->sAllowFrom),
	                        t.sAllowFrom.empty()           ? KStringView("any")
	                                                       : KStringView(t.sAllowFrom));

	if (sDiff.empty()) sDiff = "(no changes)";

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sAdmin      = sMe;
	ev.sOutlet       = t.sOutlet;
	ev.sTunnelName = t.sName;
	ev.sDetail     = kFormat("edited tunnel ({})", sDiff);
	Store.LogEvent(ev);

	// Hot-reload: if any listener-key field changed, the listener will
	// be stopped and restarted inside Reconcile; otherwise this is a
	// cheap no-op.
	m_Server.ReconcileListeners(sMe);

	RedirectWithFlash(HTTP, s_sTunnelsURL, kFormat("Tunnel '{}' updated.", t.sName), "");

} // HandleTunnelsUpdate

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsCheck (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const auto& sName = HTTP.GetQueryParm("name");

	auto oT = m_Server.GetStore().GetTunnel(sName);
	if (!oT)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "",
		                  kFormat("Tunnel '{}' does not exist.", sName));
		return;
	}

	const KTCPEndPoint Target(oT->sTargetHost, oT->iTargetPort);

	auto Tunnel = m_Server.GetTunnelForOutlet(oT->sOutlet);
	if (!Tunnel)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "",
		                  kFormat("Outlet '{}' is not currently connected.", oT->sOutlet));
		return;
	}

	const auto Tunables    = m_Server.GetTunnelSettings();
	// the outlet's own connect attempt runs with ConnectTimeout - give
	// its answer a little headroom on top of that
	const auto WaitForNode = Tunables.ConnectTimeout + chrono::seconds(5);

	// The REPL check is the preferred mode: it answers fast and reports
	// the connect latency. When the outlet's REPL does not know the
	// 'check' command (older ktunnel), does not answer, or `mode=probe`
	// was requested, fall back to a direct probe over the native
	// Connect path - the exact path a forwarded connection takes.
	KString sResult;

	if (HTTP.GetQueryParm("mode") != "probe")
	{
		auto Conn = Tunnel->OpenRepl();

		if (Conn)
		{
			// a watchdog unblocks the ReadData loop below if the outlet
			// never answers (e.g. the channel died mid-request)
			auto& Timer    = Dekaf::getInstance().GetTimer();
			auto  iTimerID = Timer.CallOnce(WaitForNode, [Conn](KUnixTime) { Conn->Disconnect(); });

			Conn->WriteData(kFormat("check {}\n", Target.Serialize()));

			// the REPL answers with its banner and prompts around the
			// result - collect frames until we see the result line (or
			// the channel dies)
			KString sChunk;
			bool    bUnknownCommand { false };

			while (sResult.empty() && !bUnknownCommand && Conn->ReadData(sChunk))
			{
				for (auto sLine : sChunk.Split("\n"))
				{
					if (sLine.starts_with("OK:")
					 || sLine.starts_with("FAIL:")
					 || sLine.starts_with("usage:"))
					{
						sResult = sLine;
						break;
					}

					if (sLine.starts_with("unknown command"))
					{
						// old outlet without 'check' - use the fallback
						bUnknownCommand = true;
						break;
					}
				}
			}

			Timer.Cancel(iTimerID);
			Tunnel->CloseRepl(Conn);
		}
	}

	if (!sResult.empty())
	{
		if (sResult.starts_with("OK:"))
		{
			RedirectWithFlash(HTTP, s_sTunnelsURL,
			                  kFormat("Outlet '{}': {}", oT->sOutlet, sResult), "");
		}
		else
		{
			RedirectWithFlash(HTTP, s_sTunnelsURL, "",
			                  kFormat("Outlet '{}': {}", oT->sOutlet, sResult));
		}
		return;
	}

	// direct mode: probe over the native Connect path
	auto Probe = Tunnel->ProbeConnect(Target, WaitForNode);

	if (Probe.bConnected)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL,
		                  kFormat("Direct probe: {}", Probe.sMessage), "");
	}
	else
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "",
		                  kFormat("Direct probe: {}", Probe.sMessage));
	}

} // HandleTunnelsCheck

//-----------------------------------------------------------------------------
void AdminUI::ShowEvents (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	// Filter inputs from the URL query. Both are safe to echo back
	// unchanged into the form because they land inside attribute values,
	// which the web objects escape on serialization.
	const auto& sKind  = HTTP.GetQueryParm("kind");
	const auto& sLimit = HTTP.GetQueryParm("limit");

	// Whitelist the limit values — anything else collapses to 100. We
	// expose three steps to keep memory use bounded; a pathological
	// Browser-bar edit to limit=9999999 just caps to 1000.
	std::size_t iLimit = 100;
	if      (sLimit == "500")  iLimit = 500;
	else if (sLimit == "1000") iLimit = 1000;

	auto& Store  = m_Server.GetStore();
	auto Kinds   = Store.GetDistinctEventKinds();
	auto Events  = Store.GetEvents(sKind, iLimit);

	auto Page = MakePage("ktunnel — Events");
	RenderTopBar(Page, "events", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	// --- filter form --------------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Filter");

		auto Form = sec.Add<html::Form>(s_sEventsURL);
		auto Row  = Form.Add<html::Div>(html::Classes("row"));

		// Build the kind <select>: a blank first option means "no
		// constraint" and matches the (?1 = '') branch in GetEvents.
		{
			auto Field = Row.Add<html::Div>(html::Classes("field"));
			Field.AddElement("label").AddText("Kind");
			auto Select = Field.Add<html::Select>("kind");
			// explicit value="" - a value-less option would submit its label
			Select.Add<html::Option>("all kinds").SetSelected(sKind.empty())
			      .SetAttribute("value", "");
			for (const auto& k : Kinds)
			{
				Select.Add<html::Option>(k, k).SetSelected(k == sKind);
			}
		}

		// Limit dropdown: three fixed steps — we always rank "current"
		// selection as selected, default 100.
		{
			auto Field = Row.Add<html::Div>(html::Classes("field"));
			Field.AddElement("label").AddText("Limit");
			auto Select = Field.Add<html::Select>("limit");
			for (KStringView sValue : { "100", "500", "1000" })
			{
				Select.Add<html::Option>(sValue, sValue)
				      .SetSelected(sLimit == sValue || (sLimit.empty() && sValue == "100"));
			}
		}

		Row.Add<html::Button>("Apply", html::Button::SUBMIT, html::Classes{"btn"});
		Row.Add<html::Link>(s_sEventsURL, "Reset", html::Classes{"btn"})
		   .SetStyle("background:#475569;text-decoration:none;"
		             "display:inline-flex;align-items:center;justify-content:center;");
	}

	// --- events table -------------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, kFormat("Events ({} shown, capped at {})",
		                                 Events.size(), iLimit));

		if (Events.empty())
		{
			auto p = sec.Add<html::Paragraph>();
			p.SetAttribute("class", "muted");
			p.AddText("No events match the current filters.");
		}
		else
		{
			RenderEventTable(sec, Events);
		}
	}

	RenderPage(HTTP, Page);

} // ShowEvents

//-----------------------------------------------------------------------------
void AdminUI::ShowCertificate (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sNotice = HTTP.GetQueryParm("notice");
	const auto&   sError  = HTTP.GetQueryParm("error");

	const auto Status = m_Server.GetACMEStatus();

	auto Page = MakePage("ktunnel — Certificate");
	RenderTopBar(Page, "cert", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// --- Section 1: status ---------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "ACME certificate");

		if (!Status.bEnabled)
		{
			RenderPill(sec, "neutral", "disabled");
			sec.Add<html::Div>(html::Classes("muted"))
			   .AddText("The server uses its configured or self-signed certificate.");
		}
		else if (Status.ValidUntil != KUnixTime())
		{
			RenderPill(sec, "ok", "active");
			auto div = sec.Add<html::Div>();
			div.AddText("Certificate for ");
			div.AddElement("b").AddText(Status.sDomains);
			div.AddText(kFormat(", valid until {} UTC. Renewal is automatic.",
			                    Status.ValidUntil.to_string()));
		}
		else
		{
			RenderPill(sec, "info", "pending");
			auto div = sec.Add<html::Div>();
			div.AddText("Ordering a certificate for ");
			div.AddElement("b").AddText(Status.sDomains);
			div.AddText(" ...");
		}

		if (!Status.sError.empty())
		{
			sec.Add<html::ui::Flash>(Status.sError, html::ui::Flash::Error);
		}
	}

	// --- Section 2: configuration form ----------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Configuration");

		auto Form = sec.Add<html::Form>(s_sCertUpdateURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Domains (comma separated, empty disables ACME)",
		                         "domains", Status.sDomains)
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "tunnel.example.com");
		Row.Add<html::ui::Field>("Contact (optional)", "contact", Status.sContact)
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "mailto:admin@example.com");
		Row.Add<html::ui::Field>("Directory URL (empty = Let's Encrypt)",
		                         "directory", Status.sDirectory)
		   .Input().SetAttribute("autocomplete", "off")
		           .SetAttribute("placeholder", "https://acme-v02.api.letsencrypt.org/directory");
		Row.Add<html::Button>("Save & apply", html::Button::SUBMIT, html::Classes{"btn"});

		sec.Add<html::Div>(html::Classes("muted"))
		   .AddText("Validation uses the tls-alpn-01 challenge on this "
		            "server: port 443 of the domains must reach it. Changes apply "
		            "immediately, without a restart.");
	}

	RenderPage(HTTP, Page);

} // ShowCertificate

//-----------------------------------------------------------------------------
void AdminUI::HandleCertificateUpdate (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	KString sDomains   = HTTP.GetQueryParm("domains");
	KString sContact   = HTTP.GetQueryParm("contact");
	KString sDirectory = HTTP.GetQueryParm("directory");

	sDomains.Trim();
	sContact.Trim();
	sDirectory.Trim();

	auto& Store = m_Server.GetStore();
	Store.SetSetting("acme_domains"  , sDomains);
	Store.SetSetting("acme_contact"  , sContact);
	Store.SetSetting("acme_directory", sDirectory);

	// keep the CLI test flag -acme-noverify as it is
	const bool bOk = m_Server.ConfigureACME(sDomains, sContact, sDirectory,
	                                        m_Server.GetACMEStatus().bNoVerify);

	KTunnelStore::Event ev;
	ev.sKind   = "config_change";
	ev.sAdmin  = sMe;
	ev.sDetail = sDomains.empty()
	           ? KString("acme disabled")
	           : kFormat("acme domains={} contact={} directory={}",
	                     sDomains, sContact,
	                     sDirectory.empty() ? "letsencrypt" : sDirectory);
	Store.LogEvent(ev);

	if (!bOk)
	{
		RedirectWithFlash(HTTP, s_sCertURL, "",
		                  kFormat("ACME setup failed: {}", m_Server.GetACMEStatus().sError));
		return;
	}

	RedirectWithFlash(HTTP, s_sCertURL,
	                  sDomains.empty()
	                  ? KString("ACME disabled.")
	                  : kFormat("ACME enabled for {} - ordering in the background.", sDomains),
	                  "");

} // HandleCertificateUpdate

//-----------------------------------------------------------------------------
void AdminUI::ShowSettings (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sNotice = HTTP.GetQueryParm("notice");
	const auto&   sError  = HTTP.GetQueryParm("error");

	const auto Tunables = m_Server.GetTunnelSettings();

	auto Page = MakePage("ktunnel — Settings");
	RenderTopBar(Page, "settings", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// --- Section 1: tunable settings -----------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Tunnel settings");

		auto Form = sec.Add<html::Form>(s_sSettingsUpdateURL);
		Form.SetMethod(html::Form::POST);

		auto Row = Form.Add<html::Div>(html::Classes("row"));
		Row.Add<html::ui::Field>("Data timeout (seconds)", "timeout",
		                         kFormat("{}", Tunables.Timeout.seconds().count()),
		                         html::Input::NUMBER)
		   .Input().SetRange(1, 86400).SetRequired(true);
		Row.Add<html::ui::Field>("Connect timeout (seconds)", "connect_timeout",
		                         kFormat("{}", Tunables.ConnectTimeout.seconds().count()),
		                         html::Input::NUMBER)
		   .Input().SetRange(1, 3600).SetRequired(true);
		Row.Add<html::ui::Field>("Control ping (seconds)", "control_ping",
		                         kFormat("{}", Tunables.ControlPing.seconds().count()),
		                         html::Input::NUMBER)
		   .Input().SetRange(5, 3600).SetRequired(true);
		Row.Add<html::ui::Field>("Max connections per tunnel", "max_connections",
		                         kFormat("{}", Tunables.iMaxTunneledConnections),
		                         html::Input::NUMBER)
		   .Input().SetRange(1, 1000000).SetRequired(true);
		Row.Add<html::Button>("Save & apply", html::Button::SUBMIT, html::Classes{"btn"});

		sec.Add<html::Div>(html::Classes("muted"))
		   .AddText("Applies to new tunnels and connections - existing ones keep "
		            "the values they started with. Persists across restarts and "
		            "overrides the matching CLI options at the next start. The "
		            "control ping must be the same on both tunnel ends.");
	}

	// --- Section 2: startup configuration (read-only) ------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Startup configuration");

		auto p = sec.Add<html::Paragraph>();
		p.SetAttribute("class", "muted");
		p.AddText("Fixed at process start - change via CLI options (or the "
		          "installed service definition) and restart.");

		auto Table = sec.Add<html::ui::Table>();
		Table.Headers({ "Option", "Value" });

		Table.AddRow({ "Admin / control port", kFormat("{}", m_Config.iPort) });

		KString sTLS;
		const auto ACME = m_Server.GetACMEStatus();
		if      (m_Config.bNoTLS)            sTLS = "disabled (-notls)";
		else if (ACME.bEnabled)              sTLS = kFormat("ACME certificate for {}", ACME.sDomains);
		else if (!m_Config.sCertFile.empty())sTLS = kFormat("certificate file {}", m_Config.sCertFile);
		else if (m_Config.bPersistCert)      sTLS = "self-signed (persisted)";
		else                                 sTLS = "self-signed (ephemeral)";
		Table.AddRow({ "TLS", sTLS });

		Table.AddRow({ "Cipher suites", m_Config.sCipherSuites.empty()
		                                    ? KStringView("PFS (default)")
		                                    : KStringView(m_Config.sCipherSuites) });
		Table.AddRow({ "AES payload encryption", m_Config.bAESPayload ? "on (-aes)" : "off" });

		if (m_Config.bAESPayload)
		{
			Table.AddRow({ "Identity key", m_Config.sIdentityKeyPath });
		}

		Table.AddRow({ "Database", m_Config.sDatabasePath });

		if (m_Config.iRawPort)
		{
			Table.AddRow({ "Built-in forwarder (-f/-t)",
			               kFormat("port {} -> {}", m_Config.iRawPort, m_Config.DefaultTarget) });
		}
	}

	RenderPage(HTTP, Page);

} // ShowSettings

//-----------------------------------------------------------------------------
void AdminUI::HandleSettingsUpdate (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	RelayServer::TunnelSettings Settings;
	Settings.Timeout                 = chrono::seconds(HTTP.GetQueryParm("timeout"        ).UInt64());
	Settings.ConnectTimeout          = chrono::seconds(HTTP.GetQueryParm("connect_timeout").UInt64());
	Settings.ControlPing             = chrono::seconds(HTTP.GetQueryParm("control_ping"   ).UInt64());
	Settings.iMaxTunneledConnections = HTTP.GetQueryParm("max_connections").UInt64();

	if (!m_Server.SetTunnelSettings(Settings, sMe))
	{
		RedirectWithFlash(HTTP, s_sSettingsURL, "",
		                  "Values out of range - timeouts need at least 1 second, "
		                  "the control ping at least 5 seconds, and at least one "
		                  "connection must be allowed.");
		return;
	}

	RedirectWithFlash(HTTP, s_sSettingsURL,
	                  "Settings saved. They apply to new tunnels and connections.", "");

} // HandleSettingsUpdate

//-----------------------------------------------------------------------------
void AdminUI::ShowLog (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	const auto&   sNotice = HTTP.GetQueryParm("notice");
	const auto&   sError  = HTTP.GetQueryParm("error");

	auto Page = MakePage("ktunnel — Log");
	RenderTopBar(Page, "log", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));

	RenderFlash(main, sNotice, sError);

	// --- Section 1: debug level -----------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Debug level");

		const int iLevel = KLog::GetLevel();

		auto Form = sec.Add<html::Form>(s_sLogLevelURL);
		Form.SetMethod(html::Form::POST);

		auto Row   = Form.Add<html::Div>(html::Classes("row"));
		auto Field = Row.Add<html::Div>(html::Classes("field"));
		Field.AddElement("label").AddText("KLog debug level");
		auto Select = Field.Add<html::Select>("level");
		for (int i = 0; i <= 3; ++i)
		{
			Select.Add<html::Option>(i == 0 ? KString("0 (warnings only)")
			                                : kFormat("{}", i),
			                         kFormat("{}", i))
			      .SetSelected(i == iLevel);
		}
		Row.Add<html::Button>("Apply", html::Button::SUBMIT, html::Classes{"btn"});

		sec.Add<html::Div>(html::Classes("muted"))
		   .AddText("Applies immediately to the running server and "
		            "persists across restarts.");
	}

	// --- Section 2: live log ---------------------------------------------
	{
		auto sec = main.Add<html::Div>(html::Classes("section"));
		sec.Add<html::Heading>(2, "Live log");

		sec.AddElement("pre").SetAttribute("id", "klog")
		   .SetAttribute("style",
		                 "max-height:60vh;min-height:10rem;overflow:auto;"
		                 "background:#14161a;color:#d6d8de;padding:0.6rem;"
		                 "border-radius:6px;font-size:0.72rem;line-height:1.35");

		// the live view gets new ring buffer lines pushed as Server-Sent
		// Events. Not HTTP polling (at debug levels >= 1 every poll logs
		// itself and floods the very view it feeds), and not a WebSocket
		// (Safari does not extend an accepted self-signed certificate to
		// wss:// - SSE rides on the page's own HTTPS connection). Reconnect
		// comes with EventSource for free.
		sec.Add<html::Script>(R"(
(function() {
  var pre = document.getElementById('klog');
  var es  = new EventSource('/Configure/log/stream');
  es.onmessage = function(e) {
    if (!e.data) return;
    var atEnd = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 4;
    pre.textContent += e.data + '\n';
    if (pre.textContent.length > 400000) {
      var cut = pre.textContent.indexOf('\n', 100000);
      if (cut > 0) pre.textContent = pre.textContent.slice(cut + 1);
    }
    if (atEnd) pre.scrollTop = pre.scrollHeight;
  };
})();
)");
	}

	RenderPage(HTTP, Page);

} // ShowLog

//-----------------------------------------------------------------------------
void AdminUI::HandleLogStream (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	auto Ring = m_Server.GetLogRing();

	if (!Ring)
	{
		HTTP.Response.SetStatus(KHTTPError::H5xx_UNAVAILABLE);
		return;
	}

	// keep this thread's own write path debug lines out of the ring - they
	// would otherwise feed back into the very stream they belong to
	LogRing::SuppressForThisThread Suppress;

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE , "text/event-stream");
	HTTP.Response.Headers.Set(KHTTPHeader::CACHE_CONTROL, "no-cache");

	// switch to streaming output, without compression - a compressor would
	// buffer the events instead of flushing them out line by line
	HTTP.Stream(/*bAllowCompressionIfPossible*/false);

	// reconnect delay for the browser's EventSource
	HTTP.Response.Write("retry: 3000\n\n");
	HTTP.Response.Flush();

	uint64_t iSince = 0;

	for (;;)
	{
		auto Result = Ring->GetSince(iSince);
		iSince      = Result.first;

		KString sEvent;

		if (Result.second.empty())
		{
			// SSE comment as liveness probe - a failing write is our only
			// reliable signal that the browser is gone
			sEvent = ": ping\n\n";
		}
		else
		{
			for (const auto& sLine : Result.second)
			{
				sEvent += "data: ";
				sEvent += sLine;
				sEvent += '\n';
			}
			sEvent += '\n';
		}

		if (HTTP.Response.Write(sEvent) != sEvent.size() || HTTP.GetLostConnection())
		{
			break;
		}

		HTTP.Response.Flush();

		// end the stream when the server shuts down - otherwise this
		// worker keeps running until the browser disconnects, and
		// `systemctl stop` runs into its kill timeout
		if (m_Server.IsShuttingDown())
		{
			break;
		}

		kSleep(chrono::seconds(1));
	}

} // HandleLogStream

//-----------------------------------------------------------------------------
void AdminUI::HandleLogLevel (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());

	int iLevel = HTTP.GetQueryParm("level").Int32();

	if (iLevel < 0) iLevel = 0;
	if (iLevel > 3) iLevel = 3;

	m_Server.SetLogLevel(iLevel);

	KTunnelStore::Event ev;
	ev.sKind   = "config_change";
	ev.sAdmin  = sMe;
	ev.sDetail = kFormat("log level {}", iLevel);
	m_Server.GetStore().LogEvent(ev);

	RedirectWithFlash(HTTP, s_sLogURL, kFormat("Log level set to {}.", iLevel), "");

} // HandleLogLevel

//-----------------------------------------------------------------------------
void AdminUI::ShowOutletRepl (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	KString sNode(HTTP.GetQueryParm("outlet"));

	if (sNode.empty())
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "", "Missing outlet parameter.");
		return;
	}

	// Verify the outlet is currently online — otherwise no point
	// rendering the REPL UI.
	auto Tunnel = m_Server.GetTunnelForOutlet(sNode);
	if (!Tunnel)
	{
		RedirectWithFlash(HTTP, s_sOutletsURL, "",
		                  kFormat("Outlet '{}' is not currently connected.", sNode));
		return;
	}

	auto Page = MakePage(kFormat("ktunnel — REPL · {}", sNode));

	Page.AddStyle(R"(
.repl-out { background:#0f1419; color:#d6deeb; padding:0.75rem;
            border-radius:6px; min-height:22rem; max-height:55vh;
            overflow:auto; font-family:ui-monospace,Menlo,monospace;
            font-size:0.85rem; white-space:pre-wrap; word-break:break-all; }
.repl-in  { display:flex; gap:0.5rem; margin-top:0.5rem; }
.repl-in input { flex:1; font-family:ui-monospace,Menlo,monospace; }
.repl-status { margin-top:0.5rem; font-size:0.8rem; }
.repl-status.ok  { color:#8fd19e; }
.repl-status.err { color:#f5a3a3; }
)");

	RenderTopBar(Page, "outlets", sMe);

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));
	auto sec  = main.Add<html::Div>(html::Classes("section"));

	sec.Add<html::Heading>(2, kFormat("REPL — {}", sNode));

	// <pre> for output (monospace, preserves whitespace), <input> for
	// input, enter to send. The inlet sends whole lines with a trailing
	// '\n' so the outlet-side line splitter in OutletHost::RunRepl() works.
	sec.AddElement("pre").SetAttribute("id", "out").SetAttribute("class", "repl-out");

	{
		auto Row = sec.Add<html::Div>(html::Classes("repl-in"));
		Row.Add<html::Input>("", "", html::Input::TEXT, html::Classes{}, "in")
		   .SetAutofocus(true)
		   .SetAttribute("autocomplete", "off")
		   .SetAttribute("placeholder", "type a command and press Enter");
		Row.Add<html::Button>("Send",  html::Button::BUTTON, html::Classes{"btn small"},        "send");
		Row.Add<html::Button>("Close", html::Button::BUTTON, html::Classes{"btn small danger"}, "close");
	}

	sec.Add<html::Div>(html::Classes("repl-status"))
	   .SetAttribute("id", "status").AddText("connecting…");
	sec.Add<html::Div>(html::Classes("repl-status"))
	   .SetAttribute("id", "hint").SetStyle("display:none;");

	// The WebSocket URL is relative so it inherits the same scheme +
	// host + port as the page. The browser WebSocket API
	// auto-translates http: to ws: and https: to wss:.
	KString sScript(R"(
(function() {
  var wsProto  = (window.location.protocol === 'https:') ? 'wss:' : 'ws:';
  var wsPath   = '__WSPATH__';
  var wsUrl    = wsProto + '//' + window.location.host + wsPath;
  var out      = document.getElementById('out');
  var inp      = document.getElementById('in');
  var sendBtn  = document.getElementById('send');
  var closeBtn = document.getElementById('close');
  var status   = document.getElementById('status');
  var hint     = document.getElementById('hint');
  function setStatus(text, cls) {
    status.textContent = text;
    status.className = 'repl-status ' + (cls || '');
  }
  function append(text) {
    out.textContent += text;
    out.scrollTop = out.scrollHeight;
  }
  var ws = new WebSocket(wsUrl);
  // Safari silently drops WSS handshakes when the page's self-signed
  // cert has not been separately trusted for wss://. onerror/onclose
  // never fire, so we fall back to a 3s timeout: if we are still in
  // CONNECTING, show a hint that links to the https:// variant of the
  // same path so the user can accept the cert in a fresh tab and
  // reload the REPL page.
  var hintTimer = setTimeout(function() {
    if (ws.readyState !== WebSocket.CONNECTING) return;
    if (window.location.protocol !== 'https:') return;
    var httpsUrl = 'https://' + window.location.host + wsPath;
    hint.style.display = 'block';
    hint.className = 'repl-status err';
    hint.innerHTML =
      'WebSocket still connecting after 3s. If you are using Safari '
      + 'with a self-signed certificate, open '
      + '<a href="' + httpsUrl + '" target="_blank" rel="noopener">this URL</a> '
      + 'in a new tab, accept the certificate, then reload this page.';
  }, 3000);
  ws.onopen    = function() { clearTimeout(hintTimer); hint.style.display='none'; setStatus('connected', 'ok'); inp.focus(); };
  ws.onmessage = function(ev) { append(ev.data); };
  ws.onerror   = function()  { clearTimeout(hintTimer); setStatus('connection error', 'err'); };
  ws.onclose   = function()  { clearTimeout(hintTimer); setStatus('disconnected', 'err'); inp.disabled = true; };
  function send() {
    if (ws.readyState !== WebSocket.OPEN) return;
    var line = inp.value;
    inp.value = '';
    append(line + '\n');
    ws.send(line + '\n');
  }
  inp.addEventListener('keydown', function(ev) {
    if (ev.key === 'Enter') { ev.preventDefault(); send(); }
  });
  sendBtn.addEventListener('click', send);
  closeBtn.addEventListener('click', function() {
    try { ws.close(); } catch (e) {}
    window.location.href = '__NODESURL__';
  });
})();
)");

	sScript.Replace("__WSPATH__",   kFormat("{}?outlet={}",
	                                        s_sOutletReplWsRoute,
	                                        kUrlEncode(sNode, URIPart::Query)));
	sScript.Replace("__NODESURL__", s_sOutletsURL);

	sec.Add<html::Script>(sScript);

	RenderPage(HTTP, Page);

} // ShowOutletRepl

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletReplWs (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sMe(Sess.GetUser());
	KString sNode(HTTP.GetQueryParm("outlet"));

	auto LogReject = [&](KStringView sReason)
	{
		KTunnelStore::Event ev;
		ev.sKind     = "repl_reject";
		ev.sAdmin    = sMe;
		ev.sOutlet     = sNode;
		ev.sRemoteIP = HTTP.GetRemoteIP();
		ev.sDetail   = sReason;
		m_Server.GetStore().LogEvent(ev);
	};

	if (sNode.empty())
	{
		LogReject("missing outlet parameter");
		HTTP.Response.SetStatus(KHTTPError::H4xx_BADREQUEST);
		return;
	}

	auto Tunnel = m_Server.GetTunnelForOutlet(sNode);
	if (!Tunnel)
	{
		LogReject("outlet not connected");
		HTTP.Response.SetStatus(KHTTPError::H5xx_UNAVAILABLE);
		return;
	}

	auto Connection = Tunnel->OpenRepl();
	if (!Connection)
	{
		LogReject("outlet has no free channel");
		HTTP.Response.SetStatus(KHTTPError::H5xx_UNAVAILABLE);
		return;
	}

	// Success audit. The matching repl_close event is logged at the
	// end of the websocket handler below.
	{
		KTunnelStore::Event ev;
		ev.sKind     = "repl_open";
		ev.sAdmin    = sMe;
		ev.sOutlet     = sNode;
		ev.sRemoteIP = HTTP.GetRemoteIP();
		ev.sDetail   = kFormat("channel {}", Connection->GetID());
		m_Server.GetStore().LogEvent(ev);
	}

	HTTP.SetWebSocketHandler(
	[this, sNode, sMe, sRemote = HTTP.GetRemoteIP(), Tunnel, Connection]
	(KWebSocket& WebSocket)
	{
		// Dedicated pump thread: outlet channel → browser WebSocket.
		// The main handler thread runs the reverse direction. Either
		// direction seeing EOF tears the other down so we get a clean
		// join() at the end.
		std::atomic<bool> bQuit { false };

		std::thread NodeToBrowser = kMakeThread([&bQuit, &WebSocket, Connection]()
		{
			for (;!bQuit.load(std::memory_order_acquire);)
			{
				KString sData;
				if (!Connection->ReadData(sData)) break;
				if (!WebSocket.Write(std::move(sData), /*bIsBinary=*/false)) break;
			}
			bQuit.store(true, std::memory_order_release);
			// Best-effort: tell the browser we're done. The matching
			// close from the browser will then wake up its WS.Read().
			WebSocket.Close(KWebSocket::Frame::NormalClosure);
		});

		// Browser → outlet channel. WebSocket::Read has a default read
		// timeout (60 minutes); set something shorter so we notice
		// bQuit flips from the other thread without waiting forever.
		WebSocket.SetReadTimeout(chrono::seconds(5));

		KString sFrame;
		while (!bQuit.load(std::memory_order_acquire) && !m_Server.IsShuttingDown())
		{
			if (!WebSocket.Read(sFrame))
			{
				// timeout: re-check bQuit and keep waiting unless
				// close/error happened (Read() returns false in both
				// cases — use the atomic as the tie-breaker)
				if (bQuit.load(std::memory_order_acquire)) break;
				continue;
			}
			Connection->WriteData(std::move(sFrame));
		}
		bQuit.store(true, std::memory_order_release);
		// Wake up NodeToBrowser if it is still blocked in ReadData,
		// unblock the outlet-side handler, and free the channel slot
		Tunnel->CloseRepl(Connection);
		NodeToBrowser.join();

		KTunnelStore::Event ev;
		ev.sKind     = "repl_close";
		ev.sAdmin    = sMe;
		ev.sOutlet     = sNode;
		ev.sRemoteIP = sRemote;
		ev.sDetail   = kFormat("channel {}", Connection->GetID());
		m_Server.GetStore().LogEvent(ev);
	});

	HTTP.SetKeepWebSocketInRunningThread();

} // HandleOutletReplWs

//-----------------------------------------------------------------------------
void AdminUI::HandleOutletReplCert (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// This handler is matched for plain (non-upgrade) HTTPS navigations
	// to the same URL as HandleOutletReplWs. The sole purpose is to answer
	// 200 OK (instead of falling through to the default 302 redirect, which
	// Safari would follow without ever presenting the TLS warning) so the
	// user can accept the self-signed certificate for this exact URL. Once
	// accepted, WebKit also trusts the corresponding wss:// URL on reload.
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	const KString sNode(HTTP.GetQueryParm("outlet"));
	const KString sNodesURL(s_sOutletsURL);

	auto Page = MakePage("ktunnel — Certificate accepted");
	RenderTopBar(Page, "outlets", Sess.GetUser());

	auto main = Page.Body().Add<html::Div>(html::Classes("main"));
	auto sec  = main.Add<html::Div>(html::Classes("section"));

	sec.Add<html::Heading>(2, "Certificate accepted");

	sec.Add<html::Paragraph>()
	   .AddText("The self-signed TLS certificate for this host is now trusted "
	            "for WebSocket connections as well.");

	{
		auto p = sec.Add<html::Paragraph>();
		p.AddText("Close this tab and reload the REPL page for outlet ");
		p.AddElement("strong").AddText(sNode);
		p.AddText(".");
	}

	{
		auto p = sec.Add<html::Paragraph>();
		p.Add<html::Link>(kFormat("{}?outlet={}",
		                          s_sOutletReplURL,
		                          kUrlEncode(sNode, URIPart::Query)),
		                  "Back to REPL", html::Classes{"btn"});
		p.Add<html::Link>(sNodesURL, "Outlets list", html::Classes{"btn small"});
	}

	RenderPage(HTTP, Page);

} // HandleOutletReplCert

//-----------------------------------------------------------------------------
void AdminUI::RegisterRoutes (KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	// NOTE: GET and POST of /Configure/login need different parsers
	// (NOREAD vs WWWFORM) so we register them as two separate routes
	// instead of chaining Get().Post() on a single RouteBuilder —
	// RouteBuilder::Parse() applies to the last committed route only.

	Routes.AddRoute(KString(s_sLoginRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowLogin(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sLoginRoute))
	      .Post([this](KRESTServer& HTTP) { HandleLogin(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sLogoutRoute))
	      .Get ([this](KRESTServer& HTTP) { HandleLogout(HTTP); })
	      .Post([this](KRESTServer& HTTP) { HandleLogout(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sDashboardRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowDashboard(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// --- Admins sub-tree (list + add + delete + change-own-password) ---
	Routes.AddRoute(KString(s_sAdminsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowAdmins(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sAdminsAddRoute))
	      .Post([this](KRESTServer& HTTP) { HandleAdminsAdd(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sAdminsDeleteRoute))
	      .Post([this](KRESTServer& HTTP) { HandleAdminsDelete(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sAdminsChangePwRoute))
	      .Post([this](KRESTServer& HTTP) { HandleAdminsChangePass(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Outlets sub-tree (list + add + toggle + delete + reset-password)
	Routes.AddRoute(KString(s_sOutletsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowOutlets(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sOutletsAddRoute))
	      .Post([this](KRESTServer& HTTP) { HandleOutletsAdd(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sOutletsToggleRoute))
	      .Post([this](KRESTServer& HTTP) { HandleOutletsToggle(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sOutletsDeleteRoute))
	      .Post([this](KRESTServer& HTTP) { HandleOutletsDelete(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sOutletsResetPwRoute))
	      .Post([this](KRESTServer& HTTP) { HandleOutletsResetPass(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sOutletsInstallRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowOutletInstall(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// --- Inlets (operator-side ktunnel identities) -------------------
	Routes.AddRoute(KString(s_sInletsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowInlets(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sInletsAddRoute))
	      .Post([this](KRESTServer& HTTP) { HandleInletsAdd(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sInletsToggleRoute))
	      .Post([this](KRESTServer& HTTP) { HandleInletsToggle(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sInletsDeleteRoute))
	      .Post([this](KRESTServer& HTTP) { HandleInletsDelete(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sInletsResetPwRoute))
	      .Post([this](KRESTServer& HTTP) { HandleInletsResetPass(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sInletsTunnelsRoute))
	      .Post([this](KRESTServer& HTTP) { HandleInletsTunnels(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Tunnels sub-tree (list + add + enable/disable + delete) -----
	Routes.AddRoute(KString(s_sTunnelsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowTunnels(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sTunnelsAddRoute))
	      .Post([this](KRESTServer& HTTP) { HandleTunnelsAdd(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sTunnelsToggleRoute))
	      .Post([this](KRESTServer& HTTP) { HandleTunnelsToggle(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sTunnelsDeleteRoute))
	      .Post([this](KRESTServer& HTTP) { HandleTunnelsDelete(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sTunnelsEditRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowTunnelEdit(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sTunnelsUpdateRoute))
	      .Post([this](KRESTServer& HTTP) { HandleTunnelsUpdate(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sTunnelsCheckRoute))
	      .Post([this](KRESTServer& HTTP) { HandleTunnelsCheck(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Settings (tunable tunnel parameters + read-only startup config) ---
	Routes.AddRoute(KString(s_sSettingsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowSettings(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sSettingsUpdateRoute))
	      .Post([this](KRESTServer& HTTP) { HandleSettingsUpdate(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Events (read-only audit log with kind/user/limit filter) ----
	Routes.AddRoute(KString(s_sEventsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowEvents(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// --- ACME certificate management (status + configuration) --------
	Routes.AddRoute(KString(s_sCertRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowCertificate(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sCertUpdateRoute))
	      .Post([this](KRESTServer& HTTP) { HandleCertificateUpdate(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Log (live view + debug level) --------------------------------
	Routes.AddRoute(KString(s_sLogRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowLog(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sLogStreamRoute))
	      .Get ([this](KRESTServer& HTTP) { HandleLogStream(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sLogLevelRoute))
	      .Post([this](KRESTServer& HTTP) { HandleLogLevel(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	// --- Outlet REPL bridge (admin-only) ------------------------------
	// Legacy URLs: the former peers page merged into what is now the
	// outlets page, and the nodes / clients pages were renamed to
	// outlets / inlets. Keep the old GET URLs as redirects so bookmarks
	// and muscle memory continue to work.
	for (auto Legacy : { s_sPeersRoute,
	                     KStringView("/Configure/nodes"),
	                     KStringView("/Configure/clients") })
	{
		const KStringView sTarget = (Legacy == "/Configure/clients")
			? s_sInletsURL
			: s_sOutletsURL;

		Routes.AddRoute(KString(Legacy))
		      .Get ([sTarget](KRESTServer& HTTP)
		      {
		          HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
		          HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, sTarget);
		      })
		      .Parse(KRESTRoute::ParserType::NOREAD);
	}

	Routes.AddRoute(KString(s_sOutletReplRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowOutletRepl(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// WebSocket endpoint for the browser REPL proxy. Same route shape
	// as /Tunnel in RelayServer::Run(): NOREAD + WEBSOCKET option,
	// and the handler installs SetWebSocketHandler + the in-thread
	// flag before returning.
	Routes.AddRoute(KString(s_sOutletReplWsRoute))
	      .Get ([this](KRESTServer& HTTP) { HandleOutletReplWs(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD)
	      .Options(KRESTRoute::Options::WEBSOCKET);

	// Same URL without the WEBSOCKET option: matched for plain HTTPS
	// navigations (no Upgrade header). Used by the Safari fallback hint
	// link so that the browser sees a 200-OK TLS response on the exact
	// WSS URL and can cache the self-signed cert exception for it.
	// Without this route the request falls through to the default 302
	// redirect on /Configure/, which Safari follows without ever
	// presenting the TLS warning.
	Routes.AddRoute(KString(s_sOutletReplWsRoute))
	      .Get ([this](KRESTServer& HTTP) { HandleOutletReplCert(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

} // RegisterRoutes
