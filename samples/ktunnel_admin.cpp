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
#include <dekaf2/web/html/khtmlentities.h>
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
constexpr KStringView s_sUsersURL     = "/Configure/users";
constexpr KStringView s_sTunnelsURL   = "/Configure/tunnels";
constexpr KStringView s_sEventsURL    = "/Configure/events";
constexpr KStringView s_sPeersURL     = "/Configure/peers";
constexpr KStringView s_sPeerReplURL  = "/Configure/peers/repl";

// The matching routes — no trailing slashes, because KRESTServer strips
// them off the request path before looking up a route.
constexpr KStringView s_sLoginRoute         = "/Configure/login";
constexpr KStringView s_sLogoutRoute        = "/Configure/logout";
constexpr KStringView s_sDashboardRoute     = "/Configure";
constexpr KStringView s_sUsersRoute         = "/Configure/users";
constexpr KStringView s_sUsersAddRoute      = "/Configure/users/add";
constexpr KStringView s_sUsersDeleteRoute   = "/Configure/users/delete";
constexpr KStringView s_sUsersChangePwRoute = "/Configure/users/changepass";
constexpr KStringView s_sTunnelsRoute       = "/Configure/tunnels";
constexpr KStringView s_sTunnelsAddRoute    = "/Configure/tunnels/add";
constexpr KStringView s_sTunnelsToggleRoute = "/Configure/tunnels/toggle";
constexpr KStringView s_sTunnelsDeleteRoute = "/Configure/tunnels/delete";
constexpr KStringView s_sTunnelsEditRoute   = "/Configure/tunnels/edit";
constexpr KStringView s_sTunnelsUpdateRoute = "/Configure/tunnels/update";
constexpr KStringView s_sEventsRoute        = "/Configure/events";
constexpr KStringView s_sPeersRoute          = "/Configure/peers";
constexpr KStringView s_sPeerReplRoute       = "/Configure/peers/repl";
constexpr KStringView s_sPeerReplWsRoute     = "/Configure/peers/repl/ws";

// Matching user-visible URLs used for form actions and redirects.
constexpr KStringView s_sUsersAddURL        = "/Configure/users/add";
constexpr KStringView s_sUsersDeleteURL     = "/Configure/users/delete";
constexpr KStringView s_sUsersChangePwURL   = "/Configure/users/changepass";
constexpr KStringView s_sTunnelsAddURL      = "/Configure/tunnels/add";
constexpr KStringView s_sTunnelsToggleURL   = "/Configure/tunnels/toggle";
constexpr KStringView s_sTunnelsDeleteURL   = "/Configure/tunnels/delete";
constexpr KStringView s_sTunnelsEditURL     = "/Configure/tunnels/edit";
constexpr KStringView s_sTunnelsUpdateURL   = "/Configure/tunnels/update";

// --------------------------------------------------------------------------
// Small pure-functional helpers used by the dashboard rendering.
//
// HTML-escaping, byte formatting and timestamp formatting are delegated
// to dekaf2 (KHTMLEntity::EncodeMandatory / kFormBytes / KUnixTime::to_string).
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
	if (sKind == "login_ok")          return "ok";
	if (sKind == "login_fail"
	 || sKind == "handshake_fail"
	 || sKind == "tunnel_error")      return "fail";
	if (sKind == "tunnel_connect"
	 || sKind == "tunnel_start")      return "ok";
	if (sKind == "tunnel_disconnect"
	 || sKind == "tunnel_stop")       return "neutral";
	if (sKind == "bootstrap"
	 || sKind == "config_change")     return "info";
	return "neutral";
}

} // anonymous namespace

//-----------------------------------------------------------------------------
AdminUI::AdminUI (ExposedServer& Server)
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

	// Authenticator closure: validate (user, password) against the
	// persistent store. Unknown users trigger a dummy bcrypt compare
	// so that an attacker cannot time-oracle which usernames exist.
	m_Session->SetAuthenticator([this](KStringView sUser, KStringView sPass)
	{
		// bcrypt("<never-used>", cost 4) — matches VerifyTunnelLogin()
		static constexpr KStringViewZ s_sDummyHash =
			"$2a$04$abcdefghijklmnopqrstuuSQgFuZgk5ErgR6KPK8e6QlYxwZpzIbG";

		// ValidatePassword requires KStringViewZ (NUL-terminated), so we
		// round-trip the submitted password through a temporary KString.
		KString sPassword(sPass);

		auto& Store  = m_Server.GetStore();
		auto& BCrypt = m_Server.GetBCrypt();

		auto oUser = Store.GetUser(sUser);

		if (!oUser)
		{
			(void) BCrypt.ValidatePassword(sPassword, s_sDummyHash);
			KTunnelStore::Event ev;
			ev.sKind     = "login_fail";
			ev.sUsername = KString(sUser);
			ev.sDetail   = "admin UI: unknown user";
			Store.LogEvent(ev);
			return false;
		}

		if (!BCrypt.ValidatePassword(sPassword, oUser->sPasswordHash))
		{
			KTunnelStore::Event ev;
			ev.sKind     = "login_fail";
			ev.sUsername = KString(sUser);
			ev.sDetail   = "admin UI: bad password";
			Store.LogEvent(ev);
			return false;
		}

		Store.SetLastLogin(sUser, KUnixTime::now());
		KTunnelStore::Event ev;
		ev.sKind     = "login_ok";
		ev.sUsername = KString(sUser);
		ev.sDetail   = "admin UI";
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

	auto& wrap = Page.Body().Add(html::Div("", html::Classes("login-wrap")));
	auto& card = wrap.Add(html::Div("", html::Classes("card")));

	card.Add(html::Heading(1, "ktunnel admin"));

	if (!sError.empty())
	{
		auto& err = card.Add(html::Div("", html::Classes("error")));
		err.AddText(sError);
	}

	auto& form = card.Add(html::Form(s_sLoginURL));
	form.SetMethod(html::Form::POST);

	{
		auto& field = form.Add(html::Div("", html::Classes("field")));
		auto& label = field.Add("label");
		label.SetAttribute("for", "user");
		label.AddText("Username");
		html::Input userInput("username", sUsername, html::Input::TEXT, "user");
		userInput.SetAutofocus(true);
		field.Add(std::move(userInput));
	}

	{
		auto& field = form.Add(html::Div("", html::Classes("field")));
		auto& label = field.Add("label");
		label.SetAttribute("for", "pass");
		label.AddText("Password");
		field.Add(html::Input("password", "", html::Input::PASSWORD, "pass"));
	}

	form.Add(html::Button("Sign in", html::Button::SUBMIT, "", html::Classes("btn")));

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
void AdminUI::RenderTopBar (html::Page& Page,
                            KStringView sActive,
                            KStringView sUser,
                            bool        bIsAdmin) const
//-----------------------------------------------------------------------------
{
	auto& top = Page.Body().Add(html::Div("", html::Classes("top")));

	{
		auto& brand = top.Add(html::Div("", html::Classes("brand")));
		brand.AddText(kFormat("ktunnel admin · {}{}",
		                      sUser,
		                      bIsAdmin ? "" : " (read-only)"));
	}

	// We build the nav as a single RawText block so we can attach the
	// "active" class to exactly one entry without juggling Classes()
	// objects across conditions. Admin-only entries are filtered out
	// for non-admin users so they can't even see the links.
	// The `users` entry is always shown, but its label reads "Users"
	// for admins (who see the full list + add form) and "Profile" for
	// regular users (who only get the change-password form there).
	struct NavEntry
	{
		KStringView sKey;
		KStringView sURL;
		KStringView sAdminLabel;
		KStringView sUserLabel;   ///< empty => admin-only, hide for non-admins
	};
	static constexpr NavEntry s_NavEntries[] = {
		{ "dashboard", s_sDashboardURL, "Dashboard", "Dashboard" },
		{ "users",     s_sUsersURL,     "Users",     "Profile"   },
		{ "tunnels",   s_sTunnelsURL,   "Tunnels",   ""          },
		{ "peers",     s_sPeersURL,     "Peers",     ""          },
		{ "events",    s_sEventsURL,    "Events",    ""          },
		{ "logout",    s_sLogoutURL,    "Logout",    "Logout"    },
	};

	KString sNav = "<nav>";
	for (const auto& e : s_NavEntries)
	{
		const KStringView sLabel = bIsAdmin ? e.sAdminLabel : e.sUserLabel;
		if (sLabel.empty()) continue;   // admin-only entry for a non-admin

		sNav += kFormat("<a href=\"{}\"{}>{}</a>",
		                KHTMLEntity::EncodeMandatory(e.sURL),
		                (e.sKey == sActive) ? " class=\"active\"" : "",
		                KHTMLEntity::EncodeMandatory(sLabel));
	}
	sNav += "</nav>";

	top.Add(html::RawText(sNav));

} // RenderTopBar

//-----------------------------------------------------------------------------
void AdminUI::ShowDashboard (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	// Non-admin users are tunnel peers: they see only their own activity
	// here. Admins see the full fleet. We resolve both once up front so
	// the two sections below can branch consistently.
	const KString sMe(Sess.GetUser());
	const bool    bIsAdmin = IsAdmin(Sess);

	// Read optional ?notice=… / ?error=… flash (e.g. from a successful
	// password change by a non-admin that got redirected here).
	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Page = MakePage("ktunnel — Dashboard");
	RenderTopBar(Page, "dashboard", sMe, bIsAdmin);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	if (!sNotice.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash ok")));
		f.AddText(sNotice);
	}
	else if (!sError.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash err")));
		f.AddText(sError);
	}

	// --- Section 1: active tunnels ------------------------------------
	{
		auto Tunnels = m_Server.SnapshotActiveTunnels();

		// Non-admins only ever see peer sessions whose login user
		// matches their own identity (typically at most one row).
		if (!bIsAdmin)
		{
			Tunnels.erase(
				std::remove_if(Tunnels.begin(), Tunnels.end(),
					[&sMe](const auto& at) { return at.sUser != sMe; }),
				Tunnels.end());
		}

		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, bIsAdmin
			? kFormat("Active tunnels ({})", Tunnels.size())
			: KString("Your active session")));

		if (Tunnels.empty())
		{
			auto& p = sec.Add(html::Paragraph());
			p.SetAttribute("class", "muted");
			p.AddText(bIsAdmin
				? "No tunnel peers are currently connected."
				: "Your ktunnel peer is not currently connected.");
		}
		else
		{
			const auto tNow = KUnixTime::now();

			KString sTable;
			sTable += "<table class=\"grid\"><thead><tr>"
			         "<th>User</th><th>Remote</th><th>Connected</th>"
			         "<th class=\"num\">Conns</th>"
			         "<th class=\"num\">RX</th>"
			         "<th class=\"num\">TX</th>"
			         "</tr></thead><tbody>";

			for (const auto& at : Tunnels)
			{
				const auto iConn  = at.Tunnel->GetConnectionCount();
				const auto iRx    = at.Tunnel->GetBytesRx();
				const auto iTx    = at.Tunnel->GetBytesTx();
				const auto sDur   = FormatDuration(tNow - at.tConnected);

				sTable += kFormat(
					"<tr><td>{}</td><td>{}</td><td>{}</td>"
					"<td class=\"num\">{}</td>"
					"<td class=\"num\">{}</td>"
					"<td class=\"num\">{}</td></tr>",
					KHTMLEntity::EncodeMandatory(at.sUser),
					KHTMLEntity::EncodeMandatory(at.EndpointAddr.Serialize()),
					KHTMLEntity::EncodeMandatory(sDur),
					iConn,
					kFormBytes(iRx),
					kFormBytes(iTx));
			}

			sTable += "</tbody></table>";
			sec.Add(html::RawText(sTable));
		}
	}

	// --- Section 2: recent events -------------------------------------
	{
		static constexpr std::size_t kEventLimit = 10;

		// Admins get the global feed; non-admins get only events
		// concerning their own identity or the tunnels they own.
		auto Events = bIsAdmin
			? m_Server.GetStore().GetRecentEvents(kEventLimit)
			: m_Server.GetStore().GetRecentEventsForOwner(sMe, kEventLimit);

		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, bIsAdmin
			? kFormat("Recent events (last {})", kEventLimit)
			: KString("Your recent activity")));

		if (Events.empty())
		{
			auto& p = sec.Add(html::Paragraph());
			p.SetAttribute("class", "muted");
			p.AddText(bIsAdmin
				? "No events logged yet."
				: "No activity logged for your account yet.");
		}
		else
		{
			KString sTable;
			sTable += "<table class=\"grid\"><thead><tr>"
			         "<th>Time</th><th>Kind</th><th>User</th>"
			         "<th>Tunnel</th><th>Remote</th><th>Detail</th>"
			         "</tr></thead><tbody>";

			for (const auto& e : Events)
			{
				sTable += kFormat(
					"<tr><td>{} UTC</td>"
					"<td><span class=\"pill {}\">{}</span></td>"
					"<td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
					e.tTimestamp.to_string(),
					KHTMLEntity::EncodeMandatory(PillForEventKind(e.sKind)),
					KHTMLEntity::EncodeMandatory(e.sKind),
					KHTMLEntity::EncodeMandatory(e.sUsername),
					KHTMLEntity::EncodeMandatory(e.sTunnelName),
					KHTMLEntity::EncodeMandatory(e.sRemoteIP),
					KHTMLEntity::EncodeMandatory(e.sDetail));
			}

			sTable += "</tbody></table>";
			sec.Add(html::RawText(sTable));

			// The full-history link below goes to the admin-only
			// /Configure/events page — hide it from non-admins so we
			// don't advertise a door that will just slam shut.
			if (bIsAdmin)
			{
				auto& p = sec.Add(html::Paragraph());
				p.SetAttribute("class", "muted");
				p.AddText("Full history browsable under ");
				p.Add(html::Link(s_sEventsURL, "Events"));
				p.AddText(".");
			}
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
	RenderTopBar(Page, sSection, Sess.GetUser(), IsAdmin(Sess));

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));
	auto& ph = main.Add(html::Div("", html::Classes("placeholder")));

	ph.Add(html::Heading(2, kFormat("{} — coming soon", sTitle)));
	auto& p = ph.Add(html::Paragraph());
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
bool AdminUI::IsAdmin (KRESTSession& Sess)
//-----------------------------------------------------------------------------
{
	auto oMe = m_Server.GetStore().GetUser(Sess.GetUser());
	return oMe && oMe->bIsAdmin;

} // IsAdmin

//-----------------------------------------------------------------------------
bool AdminUI::RequireAdminOrRedirect (KRESTServer& HTTP, KRESTSession& Sess)
//-----------------------------------------------------------------------------
{
	if (IsAdmin(Sess)) return true;

	// Audit the rejection so a failed privilege-escalation attempt is
	// not invisible to the operator. Kept under a distinct kind so the
	// Events page can filter on it.
	KTunnelStore::Event ev;
	ev.sKind     = "auth_reject";
	ev.sUsername = KString(Sess.GetUser());
	ev.sRemoteIP = HTTP.GetRemoteIP();
	ev.sDetail   = kFormat("admin UI: non-admin accessed {}",
	                       HTTP.Request.Resource.Path);
	m_Server.GetStore().LogEvent(ev);

	RedirectWithFlash(HTTP, s_sDashboardURL, "",
	                  "Administrator privileges required for this page.");
	return false;

} // RequireAdminOrRedirect

//-----------------------------------------------------------------------------
void AdminUI::ShowUsers (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;

	// NOTE: this page is NOT admin-gated at the route level. The
	// change-password form at the bottom is a "profile" affordance
	// that every authenticated user needs. The Users list + Add form
	// at the top are, however, admin-only and get skipped for regular
	// users further down.
	const KString sMe(Sess.GetUser());
	const bool    bIsAdmin = IsAdmin(Sess);

	// Read optional ?notice=… / ?error=… flash from the query string.
	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto Users = bIsAdmin ? m_Server.GetStore().GetAllUsers()
	                      : std::vector<KTunnelStore::User>{};

	auto Page = MakePage(bIsAdmin ? "ktunnel — Users" : "ktunnel — Profile");
	RenderTopBar(Page, "users", sMe, bIsAdmin);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	// --- flash banner (if any) ----------------------------------------
	if (!sNotice.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash ok")));
		f.AddText(sNotice);
	}
	else if (!sError.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash err")));
		f.AddText(sError);
	}

	// --- Section 1: user list (admin-only) ----------------------------
	if (bIsAdmin)
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, kFormat("Users ({})", Users.size())));

		KString sTable;
		sTable += "<table class=\"grid\"><thead><tr>"
		          "<th>Username</th><th>Admin</th><th>Last login</th>"
		          "<th>Created</th><th></th>"
		          "</tr></thead><tbody>";

		for (const auto& u : Users)
		{
			KString sActions;
			if (u.sUsername == sMe)
			{
				sActions = "<span class=\"pill neutral\">You</span>";
			}
			else
			{
				sActions = kFormat(
					"<form method=\"post\" action=\"{}\" class=\"inline-form\" "
					"onsubmit=\"return confirm('Delete user {}?');\">"
					"<input type=\"hidden\" name=\"username\" value=\"{}\">"
					"<button type=\"submit\" class=\"btn danger small\">Delete</button>"
					"</form>",
					KHTMLEntity::EncodeMandatory(s_sUsersDeleteURL),
					KHTMLEntity::EncodeMandatory(u.sUsername),
					KHTMLEntity::EncodeMandatory(u.sUsername));
			}

			sTable += kFormat(
				"<tr><td>{}</td>"
				"<td><span class=\"pill {}\">{}</span></td>"
				"<td>{}</td><td>{}</td><td>{}</td></tr>",
				KHTMLEntity::EncodeMandatory(u.sUsername),
				u.bIsAdmin ? "info" : "neutral",
				u.bIsAdmin ? "admin" : "user",
				u.tLastLogin.to_time_t() > 0
					? kFormat("{} UTC", u.tLastLogin.to_string())
					: KString("—"),
				u.tCreated.to_time_t() > 0
					? kFormat("{} UTC", u.tCreated.to_string())
					: KString("—"),
				sActions);
		}

		sTable += "</tbody></table>";
		sec.Add(html::RawText(sTable));
	}

	// --- Section 2: add user form (admin-only) ------------------------
	if (bIsAdmin)
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, "Add user"));

		KString sForm = kFormat(
			"<form method=\"post\" action=\"{}\">"
			"<div class=\"row\">"
			"<div class=\"field\"><label>Username</label>"
			"<input type=\"text\" name=\"username\" required autocomplete=\"off\"></div>"
			"<div class=\"field\"><label>Password</label>"
			"<input type=\"password\" name=\"password\" required autocomplete=\"new-password\"></div>"
			"<div class=\"field\"><label class=\"checkbox\">"
			"<input type=\"checkbox\" name=\"is_admin\" value=\"1\">"
			"<span>Admin</span></label></div>"
			"<button type=\"submit\" class=\"btn\">Add</button>"
			"</div></form>",
			KHTMLEntity::EncodeMandatory(s_sUsersAddURL));
		sec.Add(html::RawText(sForm));
	}

	// --- Section 3: change own password -------------------------------
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, kFormat("Change password · {}", sMe)));

		KString sForm = kFormat(
			"<form method=\"post\" action=\"{}\">"
			"<div class=\"row\">"
			"<div class=\"field\"><label>Current password</label>"
			"<input type=\"password\" name=\"current_password\" required autocomplete=\"current-password\"></div>"
			"<div class=\"field\"><label>New password</label>"
			"<input type=\"password\" name=\"new_password\" required autocomplete=\"new-password\"></div>"
			"<div class=\"field\"><label>Confirm new password</label>"
			"<input type=\"password\" name=\"confirm_password\" required autocomplete=\"new-password\"></div>"
			"<button type=\"submit\" class=\"btn\">Change</button>"
			"</div></form>",
			KHTMLEntity::EncodeMandatory(s_sUsersChangePwURL));
		sec.Add(html::RawText(sForm));
	}

	RenderPage(HTTP, Page);

} // ShowUsers

//-----------------------------------------------------------------------------
void AdminUI::HandleUsersAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());

	// Form body was parsed because the route is registered with WWWFORM.
	// We use the same accessor style as the login handler.
	const auto& sUsername = HTTP.GetQueryParm("username");
	const auto& sPassword = HTTP.GetQueryParm("password");
	const auto& sIsAdmin  = HTTP.GetQueryParm("is_admin");

	if (sUsername.empty() || sPassword.empty())
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "Username and password must not be empty.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	if (Store.GetUser(sUsername))
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", kFormat("User '{}' already exists.", sUsername));
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sPassword));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "Failed to hash password.");
		return;
	}

	KTunnelStore::User u;
	u.sUsername     = sUsername;
	u.sPasswordHash = sHash;
	u.bIsAdmin      = !sIsAdmin.empty();

	if (!Store.AddUser(u))
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "",  kFormat("Cannot add user: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind     = "user_add";
	ev.sUsername = sMe;
	ev.sDetail   = kFormat("added user '{}'{}", sUsername, u.bIsAdmin ? " (admin)" : "");
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sUsersURL, kFormat("User '{}' added.", sUsername), "");

} // HandleUsersAdd

//-----------------------------------------------------------------------------
void AdminUI::HandleUsersDelete (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());
	const auto& sUsername = HTTP.GetQueryParm("username");

	if (sUsername.empty())
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "No username provided.");
		return;
	}
	if (sUsername == sMe)
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "You cannot delete the account you are logged in with.");
		return;
	}

	auto& Store = m_Server.GetStore();

	if (!Store.GetUser(sUsername))
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", kFormat("User '{}' does not exist.", sUsername));
		return;
	}

	if (!Store.DeleteUser(sUsername))
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", kFormat("Cannot delete user: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind     = "user_delete";
	ev.sUsername = sMe;
	ev.sDetail   = kFormat("deleted user '{}'", sUsername);
	Store.LogEvent(ev);

	RedirectWithFlash(HTTP, s_sUsersURL, kFormat("User '{}' deleted.", sUsername), "");

} // HandleUsersDelete

//-----------------------------------------------------------------------------
void AdminUI::HandleUsersChangePass (KRESTServer& HTTP)
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
		RedirectWithFlash(HTTP, s_sUsersURL, "", "All three password fields are required.");
		return;
	}
	if (sNew != sConfirm)
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "New password and confirmation do not match.");
		return;
	}
	if (sNew.size() < 6)
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "New password must be at least 6 characters long.");
		return;
	}

	auto& Store  = m_Server.GetStore();
	auto& BCrypt = m_Server.GetBCrypt();

	auto oMe = Store.GetUser(sMe);
	if (!oMe)
	{
		// The session references a user that no longer exists — force a
		// fresh login so the user ends up in a consistent state.
		Sess.Logout();
		RedirectWithFlash(HTTP, s_sLoginURL, "", "Your account was removed. Please log in again.");
		return;
	}

	if (!BCrypt.ValidatePassword(KStringViewZ(sCurrent), oMe->sPasswordHash))
	{
		KTunnelStore::Event ev;
		ev.sKind     = "passwd_fail";
		ev.sUsername = sMe;
		ev.sDetail   = "current password did not verify";
		Store.LogEvent(ev);

		RedirectWithFlash(HTTP, s_sUsersURL, "", "Current password is incorrect.");
		return;
	}

	auto sHash = BCrypt.GenerateHash(KStringViewZ(sNew));
	if (sHash.empty())
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", "Failed to hash new password.");
		return;
	}

	if (!Store.UpdateUserPasswordHash(sMe, sHash))
	{
		RedirectWithFlash(HTTP, s_sUsersURL, "", kFormat("Cannot update password: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind     = "passwd_change";
	ev.sUsername = sMe;
	ev.sDetail   = "admin UI";
	Store.LogEvent(ev);

	// Admins go back to the Users page (where they came from); regular
	// users don't have access there, so we send them to the dashboard.
	const KStringView sTarget = IsAdmin(Sess) ? s_sUsersURL : s_sDashboardURL;
	RedirectWithFlash(HTTP, sTarget, "Password changed.", "");

} // HandleUsersChangePass

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
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());

	const auto& sNotice = HTTP.GetQueryParm("notice");
	const auto& sError  = HTTP.GetQueryParm("error");

	auto& Store       = m_Server.GetStore();
	auto Tunnels      = Store.GetAllTunnels();
	auto Users        = Store.GetAllUsers();
	auto ListenerMap  = m_Server.SnapshotListenerStates();

	// Rendering helper for the listener-state pill.
	auto StatePill = [](ExposedServer::ListenerState s,
	                    KStringView sError) -> KString
	{
		using S = ExposedServer::ListenerState;
		switch (s)
		{
			case S::Listening:
				return "<span class=\"pill ok\">listening</span>";
			case S::OwnerOffline:
				return "<span class=\"pill info\">owner offline</span>";
			case S::PortError:
				return kFormat("<span class=\"pill fail\">port error</span>"
				               "<div class=\"muted\" style=\"font-size:0.7rem\">{}</div>",
				               KHTMLEntity::EncodeMandatory(sError));
			case S::Stopped:
			default:
				return "<span class=\"pill neutral\">stopped</span>";
		}
	};

	auto Page = MakePage("ktunnel — Tunnels");
	RenderTopBar(Page, "tunnels", sMe, /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	if (!sNotice.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash ok")));
		f.AddText(sNotice);
	}
	else if (!sError.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash err")));
		f.AddText(sError);
	}

	// --- Section 0: built-in forwarder from CLI -----------------------
	// Only shown when the process was launched with -f/-t. This row is
	// not backed by the tunnels table and therefore has no Edit / Toggle
	// / Delete actions — restart ktunnel with different flags to change
	// it. We still surface it here so the admin can see at a glance that
	// there is an additional raw listener hogging a port.
	if (m_Config.iRawPort)
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, "Built-in forwarder (from CLI)"));

		auto& p = sec.Add(html::Paragraph());
		p.SetAttribute("class", "muted");
		p.AddText("Configured via -f / -t at process start. "
		          "Not stored in the database; restart ktunnel with "
		          "different flags to change it.");

		KString sTarget = m_Config.DefaultTarget.empty()
			? KString("<em class=\"muted\">(no default target)</em>")
			: kFormat("{}:{}",
			          KHTMLEntity::EncodeMandatory(m_Config.DefaultTarget.Domain.get()),
			          m_Config.DefaultTarget.Port.get());

		KString sTable;
		sTable += "<table class=\"grid\"><thead><tr>"
		          "<th>Name</th><th>Forward port</th><th>Target</th><th>Runtime</th>"
		          "</tr></thead><tbody>";
		sTable += kFormat("<tr><td><em>(builtin)</em></td>"
		                  "<td>{}</td><td>{}</td>"
		                  "<td><span class=\"pill ok\">listening</span></td></tr>",
		                  m_Config.iRawPort,
		                  sTarget);
		sTable += "</tbody></table>";
		sec.Add(html::RawText(sTable));
	}

	// --- Section 1: tunnel list ---------------------------------------
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, kFormat("Tunnels ({})", Tunnels.size())));

		if (Tunnels.empty())
		{
			auto& p = sec.Add(html::Paragraph());
			p.SetAttribute("class", "muted");
			p.AddText("No tunnel listeners configured yet. Use the form below to add one.");
		}
		else
		{
			KString sTable;
			sTable += "<table class=\"grid\"><thead><tr>"
			          "<th>Name</th><th>Owner</th><th>Forward port</th>"
			          "<th>Target</th><th>Config</th><th>Runtime</th>"
			          "<th></th></tr></thead><tbody>";

			for (const auto& t : Tunnels)
			{
				KString sState;
				auto it = ListenerMap.find(t.sName);
				if (it != ListenerMap.end())
				{
					sState = StatePill(it->second.eState, it->second.sError);
				}
				else
				{
					sState = "<span class=\"pill neutral\">stopped</span>";
				}

				// Build the ?name=<n> edit-link safely even if the tunnel
				// name contains URL-special characters (unlikely since we
				// already require a non-empty trimmed string in Add, but
				// good hygiene).
				KString sEditURL = kFormat("{}?name={}",
					s_sTunnelsEditURL,
					kUrlEncode(t.sName, URIPart::Query));

				KString sActions = kFormat(
					"<a href=\"{}\" class=\"btn small\" "
					"style=\"text-decoration:none;display:inline-flex;"
					"align-items:center;justify-content:center;\">Edit</a> "
					"<form method=\"post\" action=\"{}\" class=\"inline-form\">"
					"<input type=\"hidden\" name=\"name\" value=\"{}\">"
					"<input type=\"hidden\" name=\"enable\" value=\"{}\">"
					"<button type=\"submit\" class=\"btn small\">{}</button>"
					"</form> "
					"<form method=\"post\" action=\"{}\" class=\"inline-form\" "
					"onsubmit=\"return confirm('Delete tunnel {}?');\">"
					"<input type=\"hidden\" name=\"name\" value=\"{}\">"
					"<button type=\"submit\" class=\"btn danger small\">Delete</button>"
					"</form>",
					KHTMLEntity::EncodeMandatory(sEditURL),
					KHTMLEntity::EncodeMandatory(s_sTunnelsToggleURL),
					KHTMLEntity::EncodeMandatory(t.sName),
					t.bEnabled ? "0" : "1",
					t.bEnabled ? "Disable" : "Enable",
					KHTMLEntity::EncodeMandatory(s_sTunnelsDeleteURL),
					KHTMLEntity::EncodeMandatory(t.sName),
					KHTMLEntity::EncodeMandatory(t.sName));

				sTable += kFormat(
					"<tr><td>{}</td><td>{}</td>"
					"<td>{}</td><td>{}:{}</td>"
					"<td><span class=\"pill {}\">{}</span></td>"
					"<td>{}</td>"
					"<td>{}</td></tr>",
					KHTMLEntity::EncodeMandatory(t.sName),
					KHTMLEntity::EncodeMandatory(t.sOwnerUser),
					t.iListenPort,
					KHTMLEntity::EncodeMandatory(t.sTargetHost),
					t.iTargetPort,
					t.bEnabled ? "ok"      : "neutral",
					t.bEnabled ? "enabled" : "disabled",
					sState,
					sActions);
			}

			sTable += "</tbody></table>";
			sec.Add(html::RawText(sTable));
		}
	}

	// --- Section 2: add tunnel form -----------------------------------
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, "Add tunnel"));

		// Owner <select> lists all known users. The default option is
		// the currently-logged-in admin so the form is usable without
		// further edits on a fresh install.
		KString sOwnerOptions;
		for (const auto& u : Users)
		{
			sOwnerOptions += kFormat(
				"<option value=\"{}\"{}>{}{}</option>",
				KHTMLEntity::EncodeMandatory(u.sUsername),
				(u.sUsername == sMe) ? " selected" : "",
				KHTMLEntity::EncodeMandatory(u.sUsername),
				u.bIsAdmin ? " (admin)" : "");
		}

		KString sForm = kFormat(
			"<form method=\"post\" action=\"{}\">"
			"<div class=\"row\">"
			"<div class=\"field\"><label>Name</label>"
			"<input type=\"text\" name=\"name\" required autocomplete=\"off\"></div>"
			"<div class=\"field\"><label>Owner</label>"
			"<select name=\"owner\" required>{}</select></div>"
			"</div>"
			"<div class=\"row\">"
			"<div class=\"field\"><label>Forward port</label>"
			"<input type=\"number\" name=\"listen_port\" min=\"1\" max=\"65535\" required></div>"
			"<div class=\"field\"><label>Target host</label>"
			"<input type=\"text\" name=\"target_host\" required></div>"
			"<div class=\"field\"><label>Target port</label>"
			"<input type=\"number\" name=\"target_port\" min=\"1\" max=\"65535\" required></div>"
			"</div>"
			"<p class=\"muted\" style=\"margin-top:0.25rem;font-size:0.75rem\">"
			"Forward port binds on all interfaces (0.0.0.0 + [::]). Keep it "
			"distinct from the admin/control port (-p on the CLI)."
			"</p>"
			"<div class=\"row\">"
			"<div class=\"field\"><label class=\"checkbox\">"
			"<input type=\"checkbox\" name=\"enabled\" value=\"1\" checked>"
			"<span>Enabled</span></label></div>"
			"<button type=\"submit\" class=\"btn\">Add tunnel</button>"
			"</div></form>",
			KHTMLEntity::EncodeMandatory(s_sTunnelsAddURL),
			sOwnerOptions);

		sec.Add(html::RawText(sForm));
	}

	RenderPage(HTTP, Page);

} // ShowTunnels

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsAdd (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString  sMe(Sess.GetUser());

	const auto& sName       = HTTP.GetQueryParm("name");
	const auto& sOwner      = HTTP.GetQueryParm("owner");
	const auto& sListenPort = HTTP.GetQueryParm("listen_port");
	const auto& sTargetHost = HTTP.GetQueryParm("target_host");
	const auto& sTargetPort = HTTP.GetQueryParm("target_port");
	const auto& sEnabled    = HTTP.GetQueryParm("enabled");

	if (sName.empty() || sOwner.empty() || sTargetHost.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "",
		                  "Name, owner and target host are required.");
		return;
	}

	const auto iListenPort = ParsePort(sListenPort);
	const auto iTargetPort = ParsePort(sTargetPort);
	if (iListenPort == 0 || iTargetPort == 0)
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "Ports must be valid numbers between 1 and 65535.");
		return;
	}

	auto& Store = m_Server.GetStore();

	if (!Store.GetUser(sOwner))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Owner '{}' is not a known user.", sOwner));
		return;
	}
	if (Store.GetTunnel(sName))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Tunnel '{}' already exists.", sName));
		return;
	}

	KTunnelStore::Tunnel t;
	t.sName       = sName;
	t.sOwnerUser  = sOwner;
	t.iListenPort = iListenPort;
	t.sTargetHost = sTargetHost;
	t.iTargetPort = iTargetPort;
	t.bEnabled    = !sEnabled.empty();

	if (!Store.AddTunnel(t))
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", kFormat("Cannot add tunnel: {}", Store.GetLastError()));
		return;
	}

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sUsername   = sMe;
	ev.sTunnelName = t.sName;
	ev.sDetail     = kFormat("added tunnel [::]:{} -> {}:{} (owner {}){}",
	                         t.iListenPort,
	                         t.sTargetHost, t.iTargetPort,
	                         t.sOwnerUser,
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
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

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
	ev.sUsername   = sMe;
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
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString  sMe(Sess.GetUser());
	const auto&    sName = HTTP.GetQueryParm("name");

	if (sName.empty())
	{
		RedirectWithFlash(HTTP, s_sTunnelsURL, "", "No tunnel name provided.");
		return;
	}

	auto& Store = m_Server.GetStore();
	if (!Store.GetTunnel(sName))
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
	ev.sUsername   = sMe;
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
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

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

	auto Users = Store.GetAllUsers();

	auto Page = MakePage(kFormat("ktunnel — Edit {}", sName));
	RenderTopBar(Page, "tunnels", sMe, /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	if (!sNotice.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash ok")));
		f.AddText(sNotice);
	}
	else if (!sError.empty())
	{
		auto& f = main.Add(html::Div("", html::Classes("flash err")));
		f.AddText(sError);
	}

	auto& sec = main.Add(html::Div("", html::Classes("section")));
	sec.Add(html::Heading(2, kFormat("Edit tunnel · {}", oT->sName)));

	// Owner <select> with the current owner pre-selected.
	KString sOwnerOptions;
	for (const auto& u : Users)
	{
		sOwnerOptions += kFormat(
			"<option value=\"{}\"{}>{}{}</option>",
			KHTMLEntity::EncodeMandatory(u.sUsername),
			(u.sUsername == oT->sOwnerUser) ? " selected" : "",
			KHTMLEntity::EncodeMandatory(u.sUsername),
			u.bIsAdmin ? " (admin)" : "");
	}

	// The name stays read-only — it is the primary key that matches
	// the listener registry. Renaming is a delete + add; we surface
	// that as a separate UX action later if anyone asks.
	KString sForm = kFormat(
		"<form method=\"post\" action=\"{}\">"
		"<input type=\"hidden\" name=\"name\" value=\"{}\">"
		"<div class=\"row\">"
		"<div class=\"field\"><label>Name (read-only)</label>"
		"<input type=\"text\" value=\"{}\" disabled></div>"
		"<div class=\"field\"><label>Owner</label>"
		"<select name=\"owner\" required>{}</select></div>"
		"</div>"
		"<div class=\"row\">"
		"<div class=\"field\"><label>Forward port</label>"
		"<input type=\"number\" name=\"listen_port\" value=\"{}\" min=\"1\" max=\"65535\" required></div>"
		"<div class=\"field\"><label>Target host</label>"
		"<input type=\"text\" name=\"target_host\" value=\"{}\" required></div>"
		"<div class=\"field\"><label>Target port</label>"
		"<input type=\"number\" name=\"target_port\" value=\"{}\" min=\"1\" max=\"65535\" required></div>"
		"</div>"
		"<p class=\"muted\" style=\"margin-top:0.25rem;font-size:0.75rem\">"
		"Forward port binds on all interfaces (0.0.0.0 + [::]). Keep it "
		"distinct from the admin/control port (-p on the CLI)."
		"</p>"
		"<div class=\"row\">"
		"<div class=\"field\"><label class=\"checkbox\">"
		"<input type=\"checkbox\" name=\"enabled\" value=\"1\"{}>"
		"<span>Enabled</span></label></div>"
		"<button type=\"submit\" class=\"btn\">Save</button>"
		"<a href=\"{}\" class=\"btn\" style=\"background:#475569;text-decoration:none;"
		"display:inline-flex;align-items:center;justify-content:center;\">Cancel</a>"
		"</div></form>",
		KHTMLEntity::EncodeMandatory(s_sTunnelsUpdateURL),
		KHTMLEntity::EncodeMandatory(oT->sName),
		KHTMLEntity::EncodeMandatory(oT->sName),
		sOwnerOptions,
		oT->iListenPort,
		KHTMLEntity::EncodeMandatory(oT->sTargetHost),
		oT->iTargetPort,
		oT->bEnabled ? " checked" : "",
		KHTMLEntity::EncodeMandatory(s_sTunnelsURL));

	sec.Add(html::RawText(sForm));

	RenderPage(HTTP, Page);

} // ShowTunnelEdit

//-----------------------------------------------------------------------------
void AdminUI::HandleTunnelsUpdate (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());

	const auto& sName       = HTTP.GetQueryParm("name");
	const auto& sOwner      = HTTP.GetQueryParm("owner");
	const auto& sListenPort = HTTP.GetQueryParm("listen_port");
	const auto& sTargetHost = HTTP.GetQueryParm("target_host");
	const auto& sTargetPort = HTTP.GetQueryParm("target_port");
	const auto& sEnabled    = HTTP.GetQueryParm("enabled");

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

	if (sOwner.empty() || sTargetHost.empty())
	{
		BackToEdit("Owner and target host are required.");
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

	if (!Store.GetUser(sOwner))
	{
		BackToEdit(kFormat("Owner '{}' is not a known user.", sOwner));
		return;
	}

	// Compose the new row. KTunnelStore::UpdateTunnel keeps id/created_utc,
	// so we only need to fill the editable fields + the name key.
	KTunnelStore::Tunnel t = *oExisting;
	t.sOwnerUser  = sOwner;
	t.iListenPort = iListenPort;
	t.sTargetHost = sTargetHost;
	t.iTargetPort = iTargetPort;
	t.bEnabled    = !sEnabled.empty();

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

	addDiff("owner",   oExisting->sOwnerUser,  t.sOwnerUser);
	addDiff("forward_port", kFormat("{}", oExisting->iListenPort),
	                        kFormat("{}", t.iListenPort));
	addDiff("target",  kFormat("{}:{}", oExisting->sTargetHost, oExisting->iTargetPort),
	                   kFormat("{}:{}", t.sTargetHost,          t.iTargetPort));
	addDiff("enabled", oExisting->bEnabled ? KStringView("yes") : KStringView("no"),
	                   t.bEnabled          ? KStringView("yes") : KStringView("no"));

	if (sDiff.empty()) sDiff = "(no changes)";

	KTunnelStore::Event ev;
	ev.sKind       = "config_change";
	ev.sUsername   = sMe;
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
void AdminUI::ShowEvents (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());

	// Filter inputs from the URL query. All three are safe to echo back
	// unchanged into the form because they land inside attribute values
	// that we put through KHTMLEntity::EncodeMandatory.
	const auto& sKind  = HTTP.GetQueryParm("kind");
	const auto& sUser  = HTTP.GetQueryParm("user");
	const auto& sLimit = HTTP.GetQueryParm("limit");

	// Whitelist the limit values — anything else collapses to 100. We
	// expose three steps to keep memory use bounded; a pathological
	// Browser-bar edit to limit=9999999 just caps to 1000.
	std::size_t iLimit = 100;
	if      (sLimit == "500")  iLimit = 500;
	else if (sLimit == "1000") iLimit = 1000;

	auto& Store  = m_Server.GetStore();
	auto Kinds   = Store.GetDistinctEventKinds();
	auto Events  = Store.GetEvents(sKind, sUser, iLimit);

	auto Page = MakePage("ktunnel — Events");
	RenderTopBar(Page, "events", sMe, /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	// --- filter form --------------------------------------------------
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, "Filter"));

		// Build the kind <select>: a blank first option means "no
		// constraint" and matches the (?1 = '') branch in GetEvents.
		KString sKindOptions;
		sKindOptions += kFormat("<option value=\"\"{}>all kinds</option>",
		                        sKind.empty() ? " selected" : "");
		for (const auto& k : Kinds)
		{
			sKindOptions += kFormat(
				"<option value=\"{}\"{}>{}</option>",
				KHTMLEntity::EncodeMandatory(k),
				(k == sKind) ? " selected" : "",
				KHTMLEntity::EncodeMandatory(k));
		}

		// Limit dropdown: three fixed steps — we always rank "current"
		// selection as selected, default 100.
		auto limitOpt = [&](KStringView sValue, KStringView sLabel)
		{
			const bool sel = (sLimit == sValue)
			              || (sLimit.empty() && sValue == "100");
			return kFormat("<option value=\"{}\"{}>{}</option>",
			               sValue, sel ? " selected" : "", sLabel);
		};
		KString sLimitOptions;
		sLimitOptions += limitOpt("100",  "100");
		sLimitOptions += limitOpt("500",  "500");
		sLimitOptions += limitOpt("1000", "1000");

		KString sForm = kFormat(
			"<form method=\"get\" action=\"{}\">"
			"<div class=\"row\">"
			"<div class=\"field\"><label>Kind</label>"
			"<select name=\"kind\">{}</select></div>"
			"<div class=\"field\"><label>User</label>"
			"<input type=\"text\" name=\"user\" value=\"{}\" autocomplete=\"off\"></div>"
			"<div class=\"field\"><label>Limit</label>"
			"<select name=\"limit\">{}</select></div>"
			"<button type=\"submit\" class=\"btn\">Apply</button>"
			"<a href=\"{}\" class=\"btn\" style=\"background:#475569;text-decoration:none;"
			"display:inline-flex;align-items:center;justify-content:center;\">Reset</a>"
			"</div></form>",
			KHTMLEntity::EncodeMandatory(s_sEventsURL),
			sKindOptions,
			KHTMLEntity::EncodeMandatory(sUser),
			sLimitOptions,
			KHTMLEntity::EncodeMandatory(s_sEventsURL));

		sec.Add(html::RawText(sForm));
	}

	// --- events table -------------------------------------------------
	{
		auto& sec = main.Add(html::Div("", html::Classes("section")));
		sec.Add(html::Heading(2, kFormat("Events ({} shown, capped at {})",
		                                 Events.size(), iLimit)));

		if (Events.empty())
		{
			auto& p = sec.Add(html::Paragraph());
			p.SetAttribute("class", "muted");
			p.AddText("No events match the current filters.");
		}
		else
		{
			KString sTable;
			sTable += "<table class=\"grid\"><thead><tr>"
			         "<th>Time</th><th>Kind</th><th>User</th>"
			         "<th>Tunnel</th><th>Remote</th><th>Detail</th>"
			         "</tr></thead><tbody>";

			for (const auto& e : Events)
			{
				sTable += kFormat(
					"<tr><td>{} UTC</td>"
					"<td><span class=\"pill {}\">{}</span></td>"
					"<td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
					e.tTimestamp.to_string(),
					KHTMLEntity::EncodeMandatory(PillForEventKind(e.sKind)),
					KHTMLEntity::EncodeMandatory(e.sKind),
					KHTMLEntity::EncodeMandatory(e.sUsername),
					KHTMLEntity::EncodeMandatory(e.sTunnelName),
					KHTMLEntity::EncodeMandatory(e.sRemoteIP),
					KHTMLEntity::EncodeMandatory(e.sDetail));
			}

			sTable += "</tbody></table>";
			sec.Add(html::RawText(sTable));
		}
	}

	RenderPage(HTTP, Page);

} // ShowEvents

//-----------------------------------------------------------------------------
void AdminUI::ShowPeers (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());
	auto Peers = m_Server.SnapshotActiveTunnels();

	auto Page = MakePage("ktunnel — Peers");
	RenderTopBar(Page, "peers", sMe, /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));

	auto& sec = main.Add(html::Div("", html::Classes("section")));
	sec.Add(html::Heading(2, kFormat("Connected peers ({})", Peers.size())));

	if (Peers.empty())
	{
		auto& p = sec.Add(html::Paragraph());
		p.SetAttribute("class", "muted");
		p.AddText("No tunnel peers currently connected. Peers appear here once "
		          "they complete the login handshake.");
	}
	else
	{
		KString sTable;
		sTable += "<table class=\"grid\"><thead><tr>"
		          "<th>Peer user</th><th>Remote</th><th>Connected since</th>"
		          "<th>Streams</th><th>Rx bytes</th><th>Tx bytes</th>"
		          "<th></th></tr></thead><tbody>";

		for (const auto& p : Peers)
		{
			// live counters via the shared_ptr — safe: the snapshot
			// returned copies of the shared_ptr, so the KTunnel cannot
			// be destroyed underneath us during this single rendering.
			std::size_t iStreams = 0;
			uint64_t    iRx = 0, iTx = 0;
			if (p.Tunnel)
			{
				iStreams = p.Tunnel->GetConnectionCount();
				iRx      = p.Tunnel->GetBytesRx();
				iTx      = p.Tunnel->GetBytesTx();
			}

			KString sReplURL = kFormat("{}?peer={}",
				s_sPeerReplURL,
				kUrlEncode(p.sUser, URIPart::Query));

			sTable += kFormat(
				"<tr>"
				"<td>{}</td>"
				"<td>{}</td>"
				"<td>{} UTC</td>"
				"<td>{}</td>"
				"<td>{}</td>"
				"<td>{}</td>"
				"<td><a class=\"btn small\" href=\"{}\">Open REPL</a></td>"
				"</tr>",
				KHTMLEntity::EncodeMandatory(p.sUser),
				KHTMLEntity::EncodeMandatory(p.EndpointAddr.Serialize()),
				p.tConnected.to_string(),
				iStreams,
				iRx,
				iTx,
				KHTMLEntity::EncodeMandatory(sReplURL));
		}

		sTable += "</tbody></table>";
		sec.Add(html::RawText(sTable));
	}

	RenderPage(HTTP, Page);

} // ShowPeers

//-----------------------------------------------------------------------------
void AdminUI::ShowPeerRepl (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());
	KString sPeer(HTTP.GetQueryParm("peer"));

	if (sPeer.empty())
	{
		RedirectWithFlash(HTTP, s_sPeersURL, "", "Missing peer parameter.");
		return;
	}

	// Verify the peer is currently online — otherwise no point
	// rendering the REPL UI.
	auto Tunnel = m_Server.GetTunnelForUser(sPeer);
	if (!Tunnel)
	{
		RedirectWithFlash(HTTP, s_sPeersURL, "",
		                  kFormat("Peer '{}' is not currently connected.", sPeer));
		return;
	}

	auto Page = MakePage(kFormat("ktunnel — REPL · {}", sPeer));
	RenderTopBar(Page, "peers", sMe, /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));
	auto& sec  = main.Add(html::Div("", html::Classes("section")));

	sec.Add(html::Heading(2, kFormat("REPL — {}", sPeer)));

	// The WebSocket URL is relative so it inherits the same scheme +
	// host + port as the page. The browser WebSocket API
	// auto-translates http: to ws: and https: to wss:.
	KString sWsPath = kFormat("{}?peer={}",
		s_sPeerReplWsRoute,
		kUrlEncode(sPeer, URIPart::Query));

	// Inline HTML + JS. <pre> for output (monospace, preserves
	// whitespace), <input> for input, enter to send. The client
	// sends whole lines with a trailing '\n' so the peer-side line
	// splitter in ProtectedHost::RunRepl() works.
	// NOTE: the inline script below must NOT contain // line comments,
	// because the HTML is emitted on a single line (no newlines between
	// the concatenated C++ string literals). A '//' comment would then
	// swallow the remainder of the file up to </script> and produce an
	// "Unexpected end of script" parse error in the browser. Use C++
	// comments here instead; every JS line ends with a '\n' so that
	// browser devtools can report useful line numbers.
	KString sBody = kFormat(
		"<style>\n"
		".repl-out {{ background:#0f1419; color:#d6deeb; padding:0.75rem;\n"
		"            border-radius:6px; min-height:22rem; max-height:55vh;\n"
		"            overflow:auto; font-family:ui-monospace,Menlo,monospace;\n"
		"            font-size:0.85rem; white-space:pre-wrap; word-break:break-all; }}\n"
		".repl-in  {{ display:flex; gap:0.5rem; margin-top:0.5rem; }}\n"
		".repl-in input {{ flex:1; font-family:ui-monospace,Menlo,monospace; }}\n"
		".repl-status {{ margin-top:0.5rem; font-size:0.8rem; }}\n"
		".repl-status.ok  {{ color:#8fd19e; }}\n"
		".repl-status.err {{ color:#f5a3a3; }}\n"
		"</style>\n"
		"<pre id=\"out\" class=\"repl-out\"></pre>\n"
		"<div class=\"repl-in\">\n"
		"  <input id=\"in\" type=\"text\" autocomplete=\"off\" autofocus\n"
		"         placeholder=\"type a command and press Enter\">\n"
		"  <button id=\"send\" class=\"btn small\" type=\"button\">Send</button>\n"
		"  <button id=\"close\" class=\"btn small danger\" type=\"button\">Close</button>\n"
		"</div>\n"
		"<div id=\"status\" class=\"repl-status\">connecting&hellip;</div>\n"
		"<div id=\"hint\" class=\"repl-status\" style=\"display:none;\"></div>\n"
		"<script>\n"
		"(function() {{\n"
		"  var wsProto  = (window.location.protocol === 'https:') ? 'wss:' : 'ws:';\n"
		"  var wsPath   = '{}';\n"
		"  var wsUrl    = wsProto + '//' + window.location.host + wsPath;\n"
		"  var out      = document.getElementById('out');\n"
		"  var inp      = document.getElementById('in');\n"
		"  var sendBtn  = document.getElementById('send');\n"
		"  var closeBtn = document.getElementById('close');\n"
		"  var status   = document.getElementById('status');\n"
		"  var hint     = document.getElementById('hint');\n"
		"  function setStatus(text, cls) {{\n"
		"    status.textContent = text;\n"
		"    status.className = 'repl-status ' + (cls || '');\n"
		"  }}\n"
		"  function append(text) {{\n"
		"    out.textContent += text;\n"
		"    out.scrollTop = out.scrollHeight;\n"
		"  }}\n"
		"  var ws = new WebSocket(wsUrl);\n"
		// Safari silently drops WSS handshakes when the page's self-signed
		// cert has not been separately trusted for wss://. onerror/onclose
		// never fire, so we fall back to a 3s timeout: if we are still in
		// CONNECTING, show a hint that links to the https:// variant of the
		// same path so the user can accept the cert in a fresh tab and
		// reload the REPL page.
		"  var hintTimer = setTimeout(function() {{\n"
		"    if (ws.readyState !== WebSocket.CONNECTING) return;\n"
		"    if (window.location.protocol !== 'https:') return;\n"
		"    var httpsUrl = 'https://' + window.location.host + wsPath;\n"
		"    hint.style.display = 'block';\n"
		"    hint.className = 'repl-status err';\n"
		"    hint.innerHTML =\n"
		"      'WebSocket still connecting after 3s. If you are using Safari '\n"
		"      + 'with a self-signed certificate, open '\n"
		"      + '<a href=\"' + httpsUrl + '\" target=\"_blank\" rel=\"noopener\">this URL</a> '\n"
		"      + 'in a new tab, accept the certificate, then reload this page.';\n"
		"  }}, 3000);\n"
		"  ws.onopen    = function() {{ clearTimeout(hintTimer); hint.style.display='none'; setStatus('connected', 'ok'); inp.focus(); }};\n"
		"  ws.onmessage = function(ev) {{ append(ev.data); }};\n"
		"  ws.onerror   = function()  {{ clearTimeout(hintTimer); setStatus('connection error', 'err'); }};\n"
		"  ws.onclose   = function()  {{ clearTimeout(hintTimer); setStatus('disconnected', 'err'); inp.disabled = true; }};\n"
		"  function send() {{\n"
		"    if (ws.readyState !== WebSocket.OPEN) return;\n"
		"    var line = inp.value;\n"
		"    inp.value = '';\n"
		"    append(line + '\\n');\n"
		"    ws.send(line + '\\n');\n"
		"  }}\n"
		"  inp.addEventListener('keydown', function(ev) {{\n"
		"    if (ev.key === 'Enter') {{ ev.preventDefault(); send(); }}\n"
		"  }});\n"
		"  sendBtn.addEventListener('click', send);\n"
		"  closeBtn.addEventListener('click', function() {{\n"
		"    try {{ ws.close(); }} catch (e) {{}}\n"
		"    window.location.href = '{}';\n"
		"  }});\n"
		"}})();\n"
		"</script>\n",
		KHTMLEntity::EncodeMandatory(sWsPath),
		KHTMLEntity::EncodeMandatory(s_sPeersURL));

	sec.Add(html::RawText(sBody));

	RenderPage(HTTP, Page);

} // ShowPeerRepl

//-----------------------------------------------------------------------------
void AdminUI::HandlePeerReplWs (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sMe(Sess.GetUser());
	KString sPeer(HTTP.GetQueryParm("peer"));

	auto LogReject = [&](KStringView sReason)
	{
		KTunnelStore::Event ev;
		ev.sKind       = "repl_reject";
		ev.sUsername   = sMe;
		ev.sTunnelName = sPeer;
		ev.sRemoteIP   = KString(HTTP.GetRemoteIP());
		ev.sDetail     = KString(sReason);
		m_Server.GetStore().LogEvent(ev);
	};

	if (sPeer.empty())
	{
		LogReject("missing peer parameter");
		HTTP.Response.SetStatus(KHTTPError::H4xx_BADREQUEST);
		return;
	}

	auto Tunnel = m_Server.GetTunnelForUser(sPeer);
	if (!Tunnel)
	{
		LogReject("peer not connected");
		HTTP.Response.SetStatus(KHTTPError::H5xx_UNAVAILABLE);
		return;
	}

	auto Connection = Tunnel->OpenRepl();
	if (!Connection)
	{
		LogReject("peer has no free channel");
		HTTP.Response.SetStatus(KHTTPError::H5xx_UNAVAILABLE);
		return;
	}

	// Success audit. The matching repl_close event is logged at the
	// end of the websocket handler below.
	{
		KTunnelStore::Event ev;
		ev.sKind       = "repl_open";
		ev.sUsername   = sMe;
		ev.sTunnelName = sPeer;
		ev.sRemoteIP   = KString(HTTP.GetRemoteIP());
		ev.sDetail     = kFormat("channel {}", Connection->GetID());
		m_Server.GetStore().LogEvent(ev);
	}

	HTTP.SetWebSocketHandler(
	[this, sPeer, sMe, sRemote = KString(HTTP.GetRemoteIP()), Connection]
	(KWebSocket& WebSocket)
	{
		// Dedicated pump thread: peer channel → browser WebSocket.
		// The main handler thread runs the reverse direction. Either
		// direction seeing EOF tears the other down so we get a clean
		// join() at the end.
		std::atomic<bool> bQuit { false };

		std::thread PeerToBrowser = kMakeThread([&bQuit, &WebSocket, Connection]()
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

		// Browser → peer channel. WebSocket::Read has a default read
		// timeout (60 minutes); set something shorter so we notice
		// bQuit flips from the other thread without waiting forever.
		WebSocket.SetReadTimeout(chrono::seconds(5));

		KString sFrame;
		while (!bQuit.load(std::memory_order_acquire))
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
		// Wake up PeerToBrowser if it is still blocked in ReadData
		Connection->Disconnect();
		PeerToBrowser.join();

		KTunnelStore::Event ev;
		ev.sKind       = "repl_close";
		ev.sUsername   = sMe;
		ev.sTunnelName = sPeer;
		ev.sRemoteIP   = sRemote;
		ev.sDetail     = kFormat("channel {}", Connection->GetID());
		m_Server.GetStore().LogEvent(ev);
	});

	HTTP.SetKeepWebSocketInRunningThread();

} // HandlePeerReplWs

//-----------------------------------------------------------------------------
void AdminUI::HandlePeerReplCert (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// This handler is matched for plain (non-upgrade) HTTPS navigations
	// to the same URL as HandlePeerReplWs. The sole purpose is to answer
	// 200 OK (instead of falling through to the default 302 redirect, which
	// Safari would follow without ever presenting the TLS warning) so the
	// user can accept the self-signed certificate for this exact URL. Once
	// accepted, WebKit also trusts the corresponding wss:// URL on reload.
	KRESTSession Sess(*m_Session, HTTP);
	if (!Sess.RequireLoginOrRedirect(s_sLoginURL)) return;
	if (!RequireAdminOrRedirect(HTTP, Sess))       return;

	const KString sPeer(HTTP.GetQueryParm("peer"));
	const KString sPeersURL(s_sPeersURL);

	auto Page = MakePage("ktunnel — Certificate accepted");
	RenderTopBar(Page, "peers", Sess.GetUser(), /*bIsAdmin=*/true);

	auto& main = Page.Body().Add(html::Div("", html::Classes("main")));
	auto& sec  = main.Add(html::Div("", html::Classes("section")));

	sec.Add(html::Heading(2, "Certificate accepted"));

	KString sBody = kFormat(
		"<p>The self-signed TLS certificate for this host is now trusted "
		"for WebSocket connections as well.</p>\n"
		"<p>Close this tab and reload the REPL page for peer "
		"<strong>{}</strong>.</p>\n"
		"<p><a href=\"{}?peer={}\" class=\"btn\">Back to REPL</a> "
		"<a href=\"{}\" class=\"btn small\">Peers list</a></p>\n",
		KHTMLEntity::EncodeMandatory(sPeer),
		s_sPeerReplURL,
		kUrlEncode(sPeer, URIPart::Query),
		sPeersURL);

	sec.Add(html::RawText(sBody));

	RenderPage(HTTP, Page);

} // HandlePeerReplCert

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

	// --- Users sub-tree (list + add + delete + change-own-password) ---
	Routes.AddRoute(KString(s_sUsersRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowUsers(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sUsersAddRoute))
	      .Post([this](KRESTServer& HTTP) { HandleUsersAdd(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sUsersDeleteRoute))
	      .Post([this](KRESTServer& HTTP) { HandleUsersDelete(HTTP); })
	      .Parse(KRESTRoute::ParserType::WWWFORM);

	Routes.AddRoute(KString(s_sUsersChangePwRoute))
	      .Post([this](KRESTServer& HTTP) { HandleUsersChangePass(HTTP); })
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

	// --- Events (read-only audit log with kind/user/limit filter) ----
	Routes.AddRoute(KString(s_sEventsRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowEvents(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// --- Peers (live tunnel peers + REPL bridge, admin-only) ---------
	Routes.AddRoute(KString(s_sPeersRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowPeers(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute(KString(s_sPeerReplRoute))
	      .Get ([this](KRESTServer& HTTP) { ShowPeerRepl(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

	// WebSocket endpoint for the browser REPL proxy. Same route shape
	// as /Tunnel in ExposedServer::Run(): NOREAD + WEBSOCKET option,
	// and the handler installs SetWebSocketHandler + the in-thread
	// flag before returning.
	Routes.AddRoute(KString(s_sPeerReplWsRoute))
	      .Get ([this](KRESTServer& HTTP) { HandlePeerReplWs(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD)
	      .Options(KRESTRoute::Options::WEBSOCKET);

	// Same URL without the WEBSOCKET option: matched for plain HTTPS
	// navigations (no Upgrade header). Used by the Safari fallback hint
	// link so that the browser sees a 200-OK TLS response on the exact
	// WSS URL and can cache the self-signed cert exception for it.
	// Without this route the request falls through to the default 302
	// redirect on /Configure/, which Safari follows without ever
	// presenting the TLS warning.
	Routes.AddRoute(KString(s_sPeerReplWsRoute))
	      .Get ([this](KRESTServer& HTTP) { HandlePeerReplCert(HTTP); })
	      .Parse(KRESTRoute::ParserType::NOREAD);

} // RegisterRoutes
