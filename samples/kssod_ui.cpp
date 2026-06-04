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

// kssod_ui.cpp
//
// The HTML user interface for the kssod sample, built with dekaf2's html::
// (KWebObjects) and html::ui:: (KWebUI) C++ builders. Kept separate from
// kssod.cpp so that file can focus purely on the OpenID Connect protocol wiring.
// The DOM escapes text nodes, so user-supplied values can't break out into markup.

#include "kssod_ui.h"

#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/ui/kwebui.h>
#include <dekaf2/util/qrcode/kqrcode.h>
#include <dekaf2/crypto/auth/ktotp.h>           // KTOTP::URI() for the enrolment QR
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kwords.h>        // KSimpleSpacedWords (role string -> tokens)

using namespace dekaf2;

//=============================================================================
//  HTML rendering (KWebObjects / KWebUI)
//=============================================================================

constexpr KStringView CSS =
	// theme palette via CSS variables; the dark override only swaps the values
	":root{--bg:#f5f5f7;--surface:#fff;--text:#1d1d1f;--muted:#888;--label:#444;"
	      "--border:#eee;--input-border:#ccc;--th:#666;--primary:#0b5fff;--primary-h:#0a4fd0;"
	      "--secondary-bg:#e5e5ea;--secondary-fg:#333;--secondary-h:#d8d8dd;--shadow:rgba(0,0,0,.08);"
	      "--warn-bg:#fdecea;--warn-border:#f1b0b7;--warn-fg:#8a1f1f}"
	"html[data-theme=dark]{--bg:#15171a;--surface:#1f2227;--text:#e6e6e6;--muted:#9aa0a6;--label:#c2c6cc;"
	      "--border:#33373d;--input-border:#474d55;--th:#9aa0a6;--primary:#3b82f6;--primary-h:#2f6fe0;"
	      "--secondary-bg:#33373d;--secondary-fg:#e6e6e6;--secondary-h:#3d424a;--shadow:rgba(0,0,0,.45);"
	      "--warn-bg:#3a1d1d;--warn-border:#7a2a2a;--warn-fg:#f3b0b0}"
	"*{box-sizing:border-box}"
	"body{font-family:system-ui,-apple-system,sans-serif;margin:0;background:var(--bg);color:var(--text)}"
	"nav.nav{display:flex;gap:1rem;align-items:center;background:var(--primary);padding:.8rem 1.2rem}"
	"nav.nav a{color:#fff;text-decoration:none;font-size:.95rem}"
	"nav.nav a.brand{font-weight:700;margin-right:auto}"
	"button.themetoggle{margin:0;padding:.3rem;background:transparent;color:#fff;border:0;border-radius:6px;cursor:pointer;display:inline-flex;align-items:center}"
	"button.themetoggle:hover{background:rgba(255,255,255,.18)}"
	"button.themetoggle svg{width:1.2rem;height:1.2rem}"
	".themetoggle .icon-sun{display:none}"
	"html[data-theme=dark] .themetoggle .icon-moon{display:none}"
	"html[data-theme=dark] .themetoggle .icon-sun{display:inline}"
	"main.wrap{max-width:46rem;margin:2rem auto;padding:0 1rem}"
	".card{background:var(--surface);border-radius:12px;box-shadow:0 1px 6px var(--shadow);margin-bottom:1.5rem;overflow:hidden}"
	".card-header{padding:.9rem 1.3rem;border-bottom:1px solid var(--border)}"
	".card-header h3{margin:0;font-size:1.15rem}"
	".card-body{padding:1.3rem}.card-footer{padding:.9rem 1.3rem;border-top:1px solid var(--border)}"
	"h3{font-size:1.05rem;margin:1.4rem 0 .4rem}"
	"label{display:block;margin:.6rem 0;font-size:.9rem;color:var(--label)}"
	"input,textarea,select{display:block;width:100%;padding:.5rem;margin-top:.25rem;border:1px solid var(--input-border);"
	      "border-radius:6px;font:inherit;background:var(--surface);color:var(--text)}"
	"input[type=checkbox]{width:auto;display:inline-block;margin:0 .4rem 0 0}"
	"label.role{display:inline-block;width:auto;margin:.2rem 1.2rem .2rem 0;color:var(--text)}"
	"button{margin-top:.8rem;padding:.5rem 1rem;background:var(--primary);color:#fff;border:0;border-radius:6px;cursor:pointer;font-size:.9rem}"
	"button:hover{background:var(--primary-h)}"
	"table{width:100%;border-collapse:collapse;margin:.5rem 0}"
	"th,td{text-align:left;padding:.5rem .6rem;border-bottom:1px solid var(--border);font-size:.88rem;vertical-align:top}"
	"th{color:var(--th);font-weight:600}"
	"td form{margin:0;display:inline-block;vertical-align:middle}td button{margin:0;padding:.3rem .6rem;background:#c0392b;font-size:.8rem}"
	"td button.secondary{background:#6b7280}"
	".actions{display:flex;gap:.6rem;align-items:center;margin-top:1.2rem}"
	".actions button{margin-top:0}"
	"a.btn{display:inline-block;padding:.5rem 1rem;border-radius:6px;background:var(--secondary-bg);color:var(--secondary-fg);text-decoration:none;font-size:.9rem}"
	"a.btn:hover{background:var(--secondary-h)}"
	// compact button-links inside table cells (e.g. Manage / Edit), sized to match the row Delete button
	"td a.btn{padding:.3rem .6rem;font-size:.8rem;margin-right:.4rem;white-space:nowrap;vertical-align:middle}"
	".tiles{display:flex;flex-wrap:wrap;gap:1rem;margin-top:1rem}"
	".tile{flex:1;min-width:9rem;aspect-ratio:1/.85;display:flex;flex-direction:column;align-items:center;"
	      "padding:1.2rem;border-radius:12px;background:var(--primary);color:#fff;text-decoration:none;transition:background .15s}"
	".tile:hover{background:var(--primary-h)}"
	".tile-icon{flex:1;display:flex;align-items:center;justify-content:center}"
	".tile-icon svg{width:3.4rem;height:3.4rem}"
	".tile-sub{font-size:.85rem;opacity:.9;text-align:center}"
	"p.back{margin-top:1.5rem}p.back a{color:var(--primary);text-decoration:none;display:inline-flex;align-items:center;gap:.35rem}"
	"p.back svg{width:1.05rem;height:1.05rem}"
	".subtitle{color:var(--muted);font-size:.85rem;margin:-.3rem 0 .8rem}"
	".help{font-size:.8rem;color:var(--muted);margin:.2rem 0 .8rem}"
	// show the client-secret field only when the app is marked confidential (has a
	// backend). Using :not(:has(...)) means that if a browser lacks :has() the
	// whole rule is dropped and the field simply stays visible - a safe fallback.
	"form:not(:has(input[name=confidential]:checked)) .secretfield{display:none}"
	"details.add{margin-top:1.5rem;border-top:1px solid var(--border);padding-top:1rem}"
	"details.add summary{display:inline-block;cursor:pointer;padding:.5rem 1rem;background:var(--primary);color:#fff;border-radius:6px;font-size:.9rem;list-style:none}"
	"details.add summary::-webkit-details-marker{display:none}"
	"details.add[open]>summary{background:var(--primary-h);margin-bottom:.8rem}"
	".warnbox{background:var(--warn-bg);border:1px solid var(--warn-border);color:var(--warn-fg);border-radius:8px;padding:1rem 1.2rem;margin:.5rem 0 1rem}"
	".warnbox p{margin:.45rem 0}.warnbox strong{font-weight:700}"
	"button.danger{background:#c0392b}button.danger:hover{background:#a93226}"
	"a{color:var(--primary)}.err{color:#e06c75}.ok{color:#3fae5a}.muted{color:var(--muted);font-size:.85rem}"
	".mono{font-family:ui-monospace,monospace;font-size:.82rem;word-break:break-all}"
	// 2FA: the enrolment key shown big and selectable, and the one-time backup codes grid
	".keybox{font-family:ui-monospace,monospace;font-size:1.15rem;letter-spacing:.12em;background:var(--secondary-bg);"
	      "color:var(--text);padding:.7rem 1rem;border-radius:8px;text-align:center;user-select:all;word-break:break-all;margin:.4rem 0}"
	".codes{display:grid;grid-template-columns:repeat(2,1fr);gap:.4rem .8rem;font-family:ui-monospace,monospace;font-size:1rem;"
	      "background:var(--secondary-bg);padding:1rem 1.2rem;border-radius:8px;margin:.6rem 0}"
	".otp{width:100%;font-family:ui-monospace,monospace;font-size:1.4rem;letter-spacing:.3em;text-align:center}"
	".badge{display:inline-block;font-size:.78rem;font-weight:600;padding:.15rem .55rem;border-radius:999px}"
	".badge.on{background:#1f7a3f;color:#fff}.badge.off{background:var(--secondary-bg);color:var(--muted)}"
	// the enrolment QR: always dark-on-white (a QR must keep that contrast), with a
	// white quiet-zone padding so it scans even in dark mode
	".qr{text-align:center;margin:.8rem 0}"
	".qr svg{width:12rem;height:12rem;background:#fff;padding:.5rem;border-radius:8px}";

// theme persistence (localStorage) + toggle. The early IIFE applies the saved
// theme during head parsing so there is no flash before the body paints.
constexpr KStringView THEME_JS =
	"(function(){try{if(localStorage.getItem('kssod-theme')==='dark')"
	"document.documentElement.setAttribute('data-theme','dark');}catch(e){}})();"
	"function kssodToggleTheme(){var d=document.documentElement,k=d.getAttribute('data-theme')==='dark';"
	"try{if(k){d.removeAttribute('data-theme');localStorage.setItem('kssod-theme','light');}"
	"else{d.setAttribute('data-theme','dark');localStorage.setItem('kssod-theme','dark');}}catch(e){}}";

// Inline outline icons (Feather-style, stroke=currentColor so they pick up the
// tile's white text colour). Recognisable admin iconography: people, app grid,
// shield-with-check (access).
constexpr KStringView SVG_USERS  = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>)";
constexpr KStringView SVG_APPS   = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/></svg>)";
constexpr KStringView SVG_ACCESS = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><path d="m9 12 2 2 4-4"/></svg>)";
constexpr KStringView SVG_BACK   = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="19" y1="12" x2="5" y2="12"/><polyline points="12 19 5 12 12 5"/></svg>)";
// theme-toggle glyphs: moon shown in light mode (-> go dark), sun in dark mode
constexpr KStringView SVG_MOON   = R"(<svg class="icon-moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>)";
constexpr KStringView SVG_SUN    = R"(<svg class="icon-sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg>)";
constexpr KStringView SVG_MAIL   = R"(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="m22 7-10 6L2 7"/></svg>)";
//-----------------------------------------------------------------------------
void SendPage(KRESTServer& HTTP, html::Page& Page, uint16_t iStatus = 200)
//-----------------------------------------------------------------------------
{
	Page.Generate();
	HTTP.SetStatus(iStatus);
	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Serialize());
}
//-----------------------------------------------------------------------------
/// open a page: head meta + stylesheet + nav bar; returns the <main> content node
KHTMLNode BeginPage(html::Page& Page, KStringView sUser, bool bAdmin)
//-----------------------------------------------------------------------------
{
	Page.AddMeta("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(CSS);
	// applies the saved theme during head parsing (no flash) + defines the toggle
	Page.Head().Add<html::Script>(THEME_JS);

	auto Nav = Page.Body().Add<html::Element>("nav", html::Classes{"nav"});
	Nav.Add<html::Link>("/", "kssod").SetClass(html::Classes{"brand"});
	if (sUser.empty())
	{
		Nav.Add<html::Link>("/login", "Sign in");
	}
	else
	{
		if (bAdmin) Nav.Add<html::Link>("/admin", "Admin");
		Nav.Add<html::Link>("/account", "Account");
		Nav.Add<html::Link>("/logout", "Sign out");
	}
	// dark-mode toggle (moon/sun), far right of the nav
	auto Toggle = Nav.Add<html::Element>("button", html::Classes{"themetoggle"});
	Toggle.SetAttribute("type", "button");
	Toggle.SetAttribute("onclick", "kssodToggleTheme()");
	Toggle.SetAttribute("aria-label", "Toggle dark mode");
	Toggle.SetAttribute("title", "Toggle dark mode");
	Toggle.AddRawText(SVG_MOON);
	Toggle.AddRawText(SVG_SUN);

	return Page.Body().Add<html::Element>("main", html::Classes{"wrap"});
}

//-----------------------------------------------------------------------------
void Msg(KHTMLNode Parent, KStringView sText, bool bError)
//-----------------------------------------------------------------------------
{
	if (sText.empty()) return;
	Parent.Add<html::Paragraph>(html::Classes{bError ? "err" : "ok"}).AddText(sText);
}

//-----------------------------------------------------------------------------
html::Input LabeledInput(KHTMLNode Parent, KStringView sLabel, KStringView sName,
                         html::Input::INPUTTYPE eType, bool bRequired = true)
//-----------------------------------------------------------------------------
{
	auto Label = Parent.Add<html::Element>("label");
	Label.AddText(sLabel);
	auto Field = Label.Add<html::Input>(sName, "", eType);
	Field.SetRequired(bRequired);
	return Field;
}

//-----------------------------------------------------------------------------
void LabeledTextArea(KHTMLNode Parent, KStringView sLabel, KStringView sName,
                     KStringView sPlaceholder = {}, KStringView sContent = {})
//-----------------------------------------------------------------------------
{
	auto Label = Parent.Add<html::Element>("label");
	Label.AddText(sLabel);
	auto Area = Label.Add<html::Element>("textarea");
	Area.SetAttribute("name", sName);
	Area.SetAttribute("rows", "2");
	if (!sPlaceholder.empty()) Area.SetAttribute("placeholder", sPlaceholder);
	if (!sContent.empty())     Area.AddText(sContent); // repopulate on validation error
}

//-----------------------------------------------------------------------------
// form-repopulation helpers: on a validation error the POST handler re-renders
// the page and passes the submitted values back in, so nothing the admin typed
// is lost. An empty Prefill (the GET case) falls back to the field defaults.
KString PrefillVal(const KJSON& Prefill, KStringView sKey, KStringView sDefault = {})
//-----------------------------------------------------------------------------
{
	return kjson::Exists(Prefill, sKey) ? kjson::GetString(Prefill, sKey) : KString(sDefault);
}

//-----------------------------------------------------------------------------
bool PrefillChecked(const KJSON& Prefill, KStringView sKey)
//-----------------------------------------------------------------------------
{
	return kjson::Exists(Prefill, sKey) && !kjson::GetStringRef(Prefill, sKey).empty();
}

//-----------------------------------------------------------------------------
/// a graphical back link (left-arrow icon + label) placed below a view's content
void BackLink(KHTMLNode Body, KStringView sURL = "/admin", KStringView sLabel = "Back")
//-----------------------------------------------------------------------------
{
	auto Link = Body.Add<html::Paragraph>(html::Classes{"back"}).Add<html::Link>(sURL);
	Link.AddRawText(SVG_BACK);
	Link.AddText(sLabel);
}
// -- pages -------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// public landing for anonymous visitors (signed-in users are routed to their
/// home by the "/" handler, so this only ever renders the sign-in entry point)
void RenderLanding(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	html::Page Page("kssod", "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("kssod");
	auto CB   = Card.Body();
	CB.Add<html::Paragraph>().AddText("A minimal OpenID Connect provider with SQLite-backed users, clients and sessions.");
	CB.Add<html::Link>("/login", "Sign in");
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
void RenderLogin(KRESTServer& HTTP, KStringView sError, bool bShowRecovery, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Sign in", "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("Sign in");
	auto CB   = Card.Body();
	Msg(CB, sError, true);
	auto Form = CB.Add<html::Form>("/login");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Username", "username", html::Input::TEXT).SetAutofocus(true);
	LabeledInput(Form, "Password", "password", html::Input::PASSWORD);
	Form.Add<html::Button>("Sign in");
	// the recovery link only appears when an email relay is configured
	if (bShowRecovery)
	{
		CB.Add<html::Paragraph>(html::Classes{"help"}).Add<html::Link>("/forgot", "Forgot your password?");
	}
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// the second login step: prompt for the second-factor code. sMethod is "totp"
/// (authenticator app, backup codes accepted) or "email" (a code we mailed).
/// sPending ties this prompt to the already-verified password step.
void RenderTwoFactor(KRESTServer& HTTP, KStringView sPending, KStringView sMethod,
                     KStringView sMsg, bool bError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	const bool bEmail = (sMethod == "email");

	html::Page Page("Two-step verification", "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("Two-step verification");
	auto CB   = Card.Body();
	Msg(CB, sMsg, bError);
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(bEmail
	    ? "We emailed you a 6-digit code. Enter it here to finish signing in."
	    : "Enter the 6-digit code from your authenticator app. Lost your device? "
	      "Type one of your backup codes instead.");
	auto Form = CB.Add<html::Form>("/login/2fa");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("pending", sPending, html::Input::HIDDEN);
	auto Field = LabeledInput(Form, bEmail ? "Email code" : "Authentication code", "code", html::Input::TEXT);
	Field.SetClass(html::Classes{"otp"}).SetAutofocus(true);
	Field.SetAttribute("inputmode", "numeric");
	Field.SetAttribute("autocomplete", "one-time-code");
	Field.SetAttribute("placeholder", "123 456");
	Form.Add<html::Button>("Verify");

	// email codes can get lost; offer a resend
	if (bEmail)
	{
		auto Resend = CB.Add<html::Form>("/login/2fa/resend");
		Resend.SetMethod(html::Form::POST);
		Resend.Add<html::Input>("pending", sPending, html::Input::HIDDEN);
		Resend.Add<html::Button>("Resend the code", html::Button::SUBMIT, html::Classes{"secondary"});
	}
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
void RenderAccount(KRESTServer& HTTP, KStringView sUser, bool bAdmin, const KJSON& Claims,
                   KStringView sMsg, bool bError, const AccountState& St, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	KStringView sEmail = kjson::GetStringRef(Claims, "email");

	html::Page Page("Account", "en");
	auto Body = BeginPage(Page, sUser, bAdmin);
	auto Card = Body.Add<html::ui::Card>("Account");
	auto CB   = Card.Body();
	CB.Add<html::Paragraph>().AddText(kFormat("Signed in as {}", sUser));
	Msg(CB, sMsg, bError);

	// --- email address + verification ---
	CB.Add<html::Heading>(3, "Email");
	if (sEmail.empty())
	{
		CB.Add<html::Paragraph>(html::Classes{"muted"}).AddText("No email address on file.");
	}
	else
	{
		auto P = CB.Add<html::Paragraph>();
		P.Add<html::Span>().AddText(sEmail);
		P.AddText(" ");
		if (St.bEmailVerified) P.Add<html::Span>(html::Classes{"badge on"}).AddText("verified");
		else                   P.Add<html::Span>(html::Classes{"badge off"}).AddText("unverified");

		if (!St.bEmailVerified && St.bSmtp)
		{
			auto F = CB.Add<html::Form>("/account/email/verify-send");
			F.SetMethod(html::Form::POST);
			F.Add<html::Button>("Send verification email", html::Button::SUBMIT, html::Classes{"secondary"});
		}
		else if (!St.bSmtp)
		{
			CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
			    "Email features are off until an administrator configures an outgoing mail server.");
		}
	}

	// --- change password ---
	CB.Add<html::Heading>(3, "Change password");
	auto Form = CB.Add<html::Form>("/account/password");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Current password",     "current", html::Input::PASSWORD);
	LabeledInput(Form, "New password",         "new",     html::Input::PASSWORD);
	LabeledInput(Form, "Confirm new password", "confirm", html::Input::PASSWORD);
	Form.Add<html::Button>("Change password");

	// --- second factor: authenticator app (TOTP) ---
	CB.Add<html::Heading>(3, "Two-step verification");
	if (!St.bHasTotp)
	{
		CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
		    "Protect your account with a second factor: an authenticator app "
		    "(Google Authenticator, Authy, 1Password, ...) shows a 6-digit code "
		    "that you enter right after your password.");
		auto F = CB.Add<html::Form>("/account/2fa/setup");
		F.SetMethod(html::Form::POST);
		F.Add<html::Button>("Set up an authenticator app");
	}
	else
	{
		CB.Add<html::Paragraph>().Add<html::Span>(html::Classes{"badge on"}).AddText("Authenticator app");
		CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
		    kFormat("{} backup code{} left. Each code works once if you lose your authenticator.",
		            St.iBackupCodes, St.iBackupCodes == 1 ? "" : "s"));
		auto Row = CB.Add<html::Div>(html::Classes{"actions"});
		auto F1  = Row.Add<html::Form>("/account/2fa/backup");
		F1.SetMethod(html::Form::POST);
		F1.Add<html::Button>("Regenerate backup codes", html::Button::SUBMIT, html::Classes{"secondary"});
		auto F2  = Row.Add<html::Form>("/account/2fa/disable");
		F2.SetMethod(html::Form::POST);
		F2.Add<html::Button>("Turn off", html::Button::SUBMIT, html::Classes{"danger"});
	}

	// --- second factor: email one-time code (an alternative to the app) ---
	// only offered with a verified address and a configured relay; an active
	// authenticator app takes precedence at login, so we note that.
	if (St.bSmtp && St.bEmailVerified)
	{
		CB.Add<html::Heading>(3, "Email codes");
		if (!St.bEmailOtp)
		{
			CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
			    "Prefer no app? We can email you a 6-digit code at sign-in instead.");
			auto F = CB.Add<html::Form>("/account/2fa/email/on");
			F.SetMethod(html::Form::POST);
			F.Add<html::Button>("Turn on email codes", html::Button::SUBMIT, html::Classes{"secondary"});
		}
		else
		{
			CB.Add<html::Paragraph>().Add<html::Span>(html::Classes{"badge on"}).AddText("Email codes");
			if (St.bHasTotp)
			{
				CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
				    "Your authenticator app is used first at sign-in; email codes apply when no app is set up.");
			}
			auto F = CB.Add<html::Form>("/account/2fa/email/off");
			F.SetMethod(html::Form::POST);
			F.Add<html::Button>("Turn off email codes", html::Button::SUBMIT, html::Classes{"danger"});
		}
	}
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// a small standalone result page (used for verify/reset outcomes and the like)
void RenderInfo(KRESTServer& HTTP, KStringView sTitle, KStringView sMessage,
                KStringView sLinkURL, KStringView sLinkText, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page(KString(sTitle), "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>(sTitle);
	auto CB   = Card.Body();
	CB.Add<html::Paragraph>().AddText(sMessage);
	if (!sLinkURL.empty()) CB.Add<html::Paragraph>().Add<html::Link>(sLinkURL, sLinkText);
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// "forgot password" entry: ask for the address to send a reset link to
void RenderForgot(KRESTServer& HTTP, KStringView sMsg, bool bError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Reset your password", "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("Reset your password");
	auto CB   = Card.Body();
	Msg(CB, sMsg, bError);
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Enter the email address on your account and we will send you a reset link.");
	auto Form = CB.Add<html::Form>("/forgot");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Email", "email", html::Input::EMAIL).SetAutofocus(true);
	Form.Add<html::Button>("Send reset link");
	BackLink(CB, "/login", "Back to sign in");
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// set a new password after following a recovery link; sToken is carried through
void RenderReset(KRESTServer& HTTP, KStringView sToken, KStringView sError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Choose a new password", "en");
	auto Body = BeginPage(Page, /*sUser=*/{}, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("Choose a new password");
	auto CB   = Card.Body();
	Msg(CB, sError, true);
	auto Form = CB.Add<html::Form>("/reset");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("token", sToken, html::Input::HIDDEN);
	LabeledInput(Form, "New password",         "new",     html::Input::PASSWORD).SetAutofocus(true);
	LabeledInput(Form, "Confirm new password", "confirm", html::Input::PASSWORD);
	Form.Add<html::Button>("Set new password");
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// the enrolment page: show the new secret (otpauth link + bare key) and ask the
/// user to type a code to confirm their app is set up before we enable 2FA.
/// sSecret is carried in a hidden field to the /account/2fa/enable handler.
void Render2FASetup(KRESTServer& HTTP, KStringView sUser, bool bAdmin,
                    KStringView sSecret, KStringView sError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Set up two-step verification", "en");
	auto Body = BeginPage(Page, sUser, bAdmin);
	auto Card = Body.Add<html::ui::Card>("Set up two-step verification");
	auto CB   = Card.Body();
	Msg(CB, sError, true);
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Point your phone's camera at this code - or scan it from your authenticator "
	    "app (Google Authenticator, Authy, 1Password, ...):");
	// inline SVG QR of the otpauth:// URI - no external service, no image file
	KString sURI = KTOTP(KString(sSecret)).URI("kssod", sUser);
	KQRCode  QR(sURI, KQRCode::ECC::Medium);
	if (!QR.empty())
	{
		CB.Add<html::Div>(html::Classes{"qr"}).AddRawText(QR.ToSVG());
	}
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Can't scan it? Add the account by hand with this key:");
	CB.Add<html::Div>(html::Classes{"keybox"}).AddText(sSecret);
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Then enter the 6-digit code the app shows, to confirm it works:");
	auto Form = CB.Add<html::Form>("/account/2fa/enable");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("secret", sSecret, html::Input::HIDDEN);
	auto Field = LabeledInput(Form, "Authentication code", "code", html::Input::TEXT);
	Field.SetClass(html::Classes{"otp"}).SetAutofocus(true);
	Field.SetAttribute("inputmode", "numeric");
	Field.SetAttribute("autocomplete", "one-time-code");
	Field.SetAttribute("placeholder", "123 456");
	Form.Add<html::Button>("Turn on two-step verification");
	BackLink(CB, "/account", "Cancel");
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// show freshly generated backup codes exactly once (they are stored hashed, so
/// this is the only time the plaintext is visible)
void RenderBackupCodes(KRESTServer& HTTP, KStringView sUser, bool bAdmin,
                       const std::vector<KString>& Codes)
//-----------------------------------------------------------------------------
{
	html::Page Page("Your backup codes", "en");
	auto Body = BeginPage(Page, sUser, bAdmin);
	auto Card = Body.Add<html::ui::Card>("Your backup codes");
	auto CB   = Card.Body();
	auto Warn = CB.Add<html::Div>(html::Classes{"warnbox"});
	Warn.Add<html::Paragraph>().AddRawText(
	    "<strong>Save these codes now.</strong> Each one works a single time if you "
	    "lose access to your authenticator. They will not be shown again.");
	auto Grid = CB.Add<html::Div>(html::Classes{"codes"});
	for (const auto& sCode : Codes)
	{
		Grid.Add<html::Div>().AddText(sCode);
	}
	BackLink(CB, "/account", "Done");
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
void RenderAdminHome(KRESTServer& HTTP, KStringView sUser)
//-----------------------------------------------------------------------------
{
	html::Page Page("Administration", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>("Administration");
	auto CB   = Card.Body();
	auto Tiles = CB.Add<html::Div>(html::Classes{"tiles"});
	{
		auto T = Tiles.Add<html::Link>("/admin/users");
		T.SetClass(html::Classes{"tile"});
		T.Add<html::Div>(html::Classes{"tile-icon"}).AddRawText(SVG_USERS);
		T.Add<html::Div>(html::Classes{"tile-sub"}).AddText("Users");
	}
	{
		auto T = Tiles.Add<html::Link>("/admin/clients");
		T.SetClass(html::Classes{"tile"});
		T.Add<html::Div>(html::Classes{"tile-icon"}).AddRawText(SVG_APPS);
		T.Add<html::Div>(html::Classes{"tile-sub"}).AddText("Apps");
	}
	{
		auto T = Tiles.Add<html::Link>("/admin/access");
		T.SetClass(html::Classes{"tile"});
		T.Add<html::Div>(html::Classes{"tile-icon"}).AddRawText(SVG_ACCESS);
		T.Add<html::Div>(html::Classes{"tile-sub"}).AddText("Roles");
	}
	{
		auto T = Tiles.Add<html::Link>("/admin/settings");
		T.SetClass(html::Classes{"tile"});
		T.Add<html::Div>(html::Classes{"tile-icon"}).AddRawText(SVG_MAIL);
		T.Add<html::Div>(html::Classes{"tile-sub"}).AddText("Email");
	}
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
/// admin: the optional outgoing-mail (SMTP) configuration. With no relay set,
/// every email feature stays off. sTestMsg reports the result of a test send.
void RenderSettings(KRESTServer& HTTP, KStringView sUser, const KssodSettingsStore::Smtp& Smtp,
                    KStringView sMsg, bool bError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Email settings", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>("Email (SMTP)");
	auto CB   = Card.Body();

	auto Status = CB.Add<html::Paragraph>();
	if (Smtp.IsConfigured()) Status.Add<html::Span>(html::Classes{"badge on"}).AddText("Configured");
	else                     Status.Add<html::Span>(html::Classes{"badge off"}).AddText("Not configured");
	CB.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Outgoing mail is optional. Set a relay here to enable address verification, "
	    "password recovery and email sign-in codes. Without it those features stay off.");

	Msg(CB, sMsg, bError);

	auto Form = CB.Add<html::Form>("/admin/settings");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Relay URL", "smtp_url", html::Input::TEXT, /*bRequired=*/false)
	    .SetValue(Smtp.sURL).SetAttribute("placeholder", "smtp://mail.example.com:587");
	// keep this hint with the field it explains (added to the form, not the card)
	Form.Add<html::Paragraph>(html::Classes{"help"}).AddText(
	    "Use smtp:// for STARTTLS (often port 587) or smtps:// for implicit TLS (port 465).");
	LabeledInput(Form, "Username", "smtp_user", html::Input::TEXT, /*bRequired=*/false).SetValue(Smtp.sUser);
	LabeledInput(Form, "Password", "smtp_pass", html::Input::PASSWORD, /*bRequired=*/false).SetValue(Smtp.sPass);
	LabeledInput(Form, "From address", "smtp_from", html::Input::EMAIL, /*bRequired=*/false)
	    .SetValue(Smtp.sFrom).SetAttribute("placeholder", "kssod@example.com");
	LabeledInput(Form, "From name (optional)", "smtp_fromname", html::Input::TEXT, /*bRequired=*/false)
	    .SetValue(Smtp.sFromName).SetAttribute("placeholder", "kssod");
	Form.Add<html::Button>("Save");

	// a test send, available once a relay is configured
	if (Smtp.IsConfigured())
	{
		CB.Add<html::Heading>(3, "Send a test email");
		auto TF = CB.Add<html::Form>("/admin/settings/test");
		TF.SetMethod(html::Form::POST);
		LabeledInput(TF, "Send to", "to", html::Input::EMAIL).SetValue(Smtp.sFrom);
		TF.Add<html::Button>("Send test email", html::Button::SUBMIT, html::Classes{"secondary"});
	}
	BackLink(CB, "/admin", "Back");
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
void RenderUsers(KRESTServer& HTTP, KStringView sUser, KssodUserStore& Users,
                 KStringView sMsg, bool bError, uint16_t iStatus,
                 const KJSON& Prefill)
//-----------------------------------------------------------------------------
{
	html::Page Page("Users", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);

	// -- list --
	auto ListCard = Body.Add<html::ui::Card>("Users");
	auto LB       = ListCard.Body();

	auto Table = LB.Add<html::Table>();
	auto Head  = Table.Add<html::TableRow>();
	Head.Add<html::TableHeader>("Username");
	Head.Add<html::TableHeader>("Name");
	Head.Add<html::TableHeader>("Email");
	Head.Add<html::TableHeader>("Admin");
	Head.Add<html::TableHeader>("Access");
	Head.Add<html::TableHeader>("");

	for (const auto& User : Users.List())
	{
		auto Row = Table.Add<html::TableRow>();
		Row.Add<html::TableData>(User.sUsername);
		Row.Add<html::TableData>(User.sName);
		Row.Add<html::TableData>(User.sEmail);
		Row.Add<html::TableData>(User.bAdmin ? "yes" : "no");
		// button-link to this user's full access grid (clients x roles)
		Row.Add<html::TableData>()
		   .Add<html::Link>(kFormat("/admin/users/access?user={}", User.sUsername), "Manage")
		   .SetClass(html::Classes{"btn"});
		auto Actions = Row.Add<html::TableData>();
		if (User.sUsername != sUser) // can't delete yourself
		{
			auto Form = Actions.Add<html::Form>("/admin/users/delete");
			Form.SetMethod(html::Form::POST);
			Form.Add<html::Input>("username", User.sUsername, html::Input::HIDDEN);
			Form.Add<html::Button>("Delete");
		}
	}

	// -- add: collapsed disclosure, opened automatically on a validation error --
	auto Det = LB.Add<html::Element>("details", html::Classes{"add"});
	if (bError) Det.SetAttribute("open", true);
	Det.Add<html::Element>("summary").AddText("+ Add new user");
	Msg(Det, sMsg, bError);
	auto Form = Det.Add<html::Form>("/admin/users/add");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Username", "username", html::Input::TEXT).SetValue(PrefillVal(Prefill, "username"));
	LabeledInput(Form, "Full name", "name",    html::Input::TEXT, /*bRequired=*/false).SetValue(PrefillVal(Prefill, "name"));
	LabeledInput(Form, "Email",     "email",   html::Input::EMAIL, /*bRequired=*/false).SetValue(PrefillVal(Prefill, "email"));
	LabeledInput(Form, "Password",  "password",html::Input::PASSWORD); // never echo a password back
	{
		auto Label = Form.Add<html::Element>("label");
		Label.Add<html::Input>("is_admin", "1", html::Input::CHECKBOX).SetChecked(PrefillChecked(Prefill, "is_admin"));
		Label.AddText("Administrator");
	}
	Form.Add<html::Button>("Add user");

	BackLink(Body);
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// blunt confirmation before deleting a user (and their assignments)
void RenderUserDeleteConfirm(KRESTServer& HTTP, KStringView sAdmin, KStringView sTargetUser, KssodUserStore& Users)
//-----------------------------------------------------------------------------
{
	std::size_t nAssign = 0;
	for (const auto& A : Users.ListAllAssignments()) { if (A.sUsername == sTargetUser) ++nAssign; }

	html::Page Page("Delete user", "en");
	auto Body = BeginPage(Page, sAdmin, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>(kFormat("Delete user — {}", sTargetUser));
	auto CB   = Card.Body();

	auto Warn = CB.Add<html::Div>(html::Classes{"warnbox"});
	Warn.Add<html::Paragraph>()
	    .AddRawText("<strong>This permanently deletes the user and cannot be undone.</strong>");
	Warn.Add<html::Paragraph>().AddText(kFormat(
		"Deleting \"{}\" also removes their {} app assignment(s) and the roles granted there. "
		"They lose access to every app immediately, and the data is gone for good.",
		sTargetUser, nAssign));

	auto Actions = CB.Add<html::Div>(html::Classes{"actions"});
	Actions.Add<html::Link>("/admin/users", "Cancel").SetClass(html::Classes{"btn"});
	auto Form = Actions.Add<html::Form>("/admin/users/delete");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("username",  sTargetUser, html::Input::HIDDEN);
	Form.Add<html::Input>("confirmed", "1",         html::Input::HIDDEN);
	Form.Add<html::Button>("Delete permanently").SetClass(html::Classes{"danger"});

	BackLink(Body, "/admin/users", "Back to users");
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
void RenderClients(KRESTServer& HTTP, KStringView sUser, KssodClientStore& Clients,
                   KStringView sMsg, bool bError, uint16_t iStatus,
                   const KJSON& Prefill)
//-----------------------------------------------------------------------------
{
	html::Page Page("Apps", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);

	// -- list --
	auto ListCard = Body.Add<html::ui::Card>("Apps");
	auto LB       = ListCard.Body();
	// bridge the terminology: "app" is the term we use; OIDC calls the same thing
	// a "relying party" (the app relying on this provider) or a "client"
	LB.Add<html::Paragraph>(html::Classes{"subtitle"})
	  .AddText("Applications that sign their users in through this provider. In OIDC an app is called a \"relying party\" "
	           "(it relies on us to authenticate) and is identified by a \"client ID\".");
	LB.Add<html::Paragraph>(html::Classes{"muted"})
	  .AddText("Use \"Manage\" (under Users & roles) to control who may sign in to an app and what roles they receive in "
	           "its tokens, and \"Edit\" to change an app's settings.");

	auto Table = LB.Add<html::Table>();
	auto Head  = Table.Add<html::TableRow>();
	Head.Add<html::TableHeader>("Client ID");
	Head.Add<html::TableHeader>("Type");
	Head.Add<html::TableHeader>("Redirect URIs");
	Head.Add<html::TableHeader>("Scopes");
	Head.Add<html::TableHeader>("Access");
	Head.Add<html::TableHeader>("Users & roles");
	Head.Add<html::TableHeader>("");

	for (const auto& Info : Clients.List())
	{
		const auto& Client = Info.Client;
		auto Row = Table.Add<html::TableRow>();
		Row.Add<html::TableData>(Client.sClientID);
		Row.Add<html::TableData>(Client.bPublic ? "public" : "confidential");
		KString sRedirects; sRedirects.Join(Client.RedirectURIs, " ");
		KString sScopes;    sScopes   .Join(Client.Scopes,       " ");
		Row.Add<html::TableData>(sRedirects, html::Classes{"mono"});
		Row.Add<html::TableData>(sScopes);
		// who may sign in: every signed-in kssod user, or only assigned ones
		auto AccessCell = Row.Add<html::TableData>(Info.bRequireAssignment ? "Assigned only" : "All users");
		AccessCell.SetAttribute("title", Info.bRequireAssignment ? "Only assigned users can sign in"
		                                                         : "Any signed-in user can sign in");
		// button-link to the per-client user assignment + role management page
		Row.Add<html::TableData>()
		   .Add<html::Link>(kFormat("/admin/clients/access?client_id={}", Client.sClientID), "Manage")
		   .SetClass(html::Classes{"btn"});
		// actions: edit the app's settings, or delete it
		auto Actions = Row.Add<html::TableData>();
		Actions.Add<html::Link>(kFormat("/admin/clients/edit?client_id={}", Client.sClientID), "Edit")
		       .SetClass(html::Classes{"btn"});
		auto Form = Actions.Add<html::Form>("/admin/clients/delete");
		Form.SetMethod(html::Form::POST);
		Form.Add<html::Input>("client_id", Client.sClientID, html::Input::HIDDEN);
		Form.Add<html::Button>("Delete");
	}

	// -- add: collapsed disclosure, opened automatically when a validation error
	//    must be shown (so the message + the re-filled values are visible) --
	auto Det = LB.Add<html::Element>("details", html::Classes{"add"});
	if (bError) Det.SetAttribute("open", true);
	Det.Add<html::Element>("summary").AddText("+ Add new app");
	Msg(Det, sMsg, bError);
	auto Form = Det.Add<html::Form>("/admin/clients/add");
	Form.SetMethod(html::Form::POST);
	LabeledInput(Form, "Client ID", "client_id", html::Input::TEXT).SetValue(PrefillVal(Prefill, "client_id"));

	// the real question behind confidential/public: does a backend YOU run finish
	// the login (and hold the secret), or does code on the user's device do it?
	// Checked = confidential (default for a fresh form); the secret field below
	// is shown only while this is checked.
	{
		auto Label = Form.Add<html::Element>("label");
		// default to confidential on a fresh form; on a re-render follow what was submitted
		bool bConfidential = kjson::Exists(Prefill, "confidential") ? PrefillChecked(Prefill, "confidential") : true;
		Label.Add<html::Input>("confidential", "1", html::Input::CHECKBOX).SetChecked(bConfidential);
		Label.AddText("Has its own backend — the login is completed on a server you run, not in the browser or mobile app");
	}
	Form.Add<html::Paragraph>(html::Classes{"help"})
	    .AddText("Decisive question: who exchanges the login code for tokens at the end? A server you run -> keep this "
	             "checked; it holds the secret below. The browser or mobile app itself -> uncheck it; it has nowhere to "
	             "keep a secret and uses PKCE instead.");

	// the secret field is shown via CSS only while the box above is checked
	{
		auto SecretField = Form.Add<html::Div>(html::Classes{"secretfield"});
		LabeledInput(SecretField, "Client secret", "secret", html::Input::TEXT, /*bRequired=*/false).SetValue(PrefillVal(Prefill, "secret"));
		SecretField.Add<html::Paragraph>(html::Classes{"help"})
		           .AddText("A password only this provider and the app's backend know. The backend presents it when "
		                    "it exchanges the login code for tokens — and because it stays on your server, no user can read it.");
	}

	LabeledTextArea(Form, "Redirect URIs (one per line)", "redirect_uris", "https://app.example.com/auth/callback", PrefillVal(Prefill, "redirect_uris"));
	LabeledTextArea(Form, "Post-logout redirect URIs (one per line)", "postlogout_uris", {}, PrefillVal(Prefill, "postlogout_uris"));
	LabeledInput(Form, "Scopes (space separated)", "scopes", html::Input::TEXT, /*bRequired=*/false).SetValue(PrefillVal(Prefill, "scopes", "openid profile email"));
	// the client declares the role catalog it supports (Azure appRoles style);
	// admins then assign from these, and can still edit the catalog afterwards
	LabeledInput(Form, "Supported roles (space separated, optional)", "supported_roles", html::Input::TEXT, /*bRequired=*/false).SetValue(PrefillVal(Prefill, "supported_roles"));
	{
		auto Label = Form.Add<html::Element>("label");
		Label.Add<html::Input>("require_assignment", "1", html::Input::CHECKBOX).SetChecked(PrefillChecked(Prefill, "require_assignment"));
		Label.AddText("Restrict to assigned users only");
	}
	Form.Add<html::Button>("Add app");

	BackLink(Body);
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// edit an existing app's settings. Mirrors the "add" form, but the client ID is
/// fixed and the secret is optional (blank keeps the current one). The role
/// catalog is managed on the per-app "Manage" page, so it is not shown here.
/// Values carries the field contents (from the stored client on GET, or from the
/// submitted form on a validation error).
void RenderClientEdit(KRESTServer& HTTP, KStringView sUser, KStringView sClientID,
                      const KJSON& Values, KStringView sMsg, bool bError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("Edit app", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>(kFormat("Edit app — {}", sClientID));
	auto CB   = Card.Body();
	CB.Add<html::Paragraph>(html::Classes{"muted"}).AddText("The client ID is the app's key and cannot be changed.");
	Msg(CB, sMsg, bError);

	auto Form = CB.Add<html::Form>("/admin/clients/edit");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("client_id", sClientID, html::Input::HIDDEN);

	// confidential/public (checked = confidential); secret field shown only while checked
	{
		auto Label = Form.Add<html::Element>("label");
		Label.Add<html::Input>("confidential", "1", html::Input::CHECKBOX).SetChecked(PrefillChecked(Values, "confidential"));
		Label.AddText("Has its own backend — the login is completed on a server you run, not in the browser or mobile app");
	}
	{
		auto SecretField = Form.Add<html::Div>(html::Classes{"secretfield"});
		LabeledInput(SecretField, "New client secret (leave blank to keep the current one)", "secret", html::Input::TEXT, /*bRequired=*/false);
		SecretField.Add<html::Paragraph>(html::Classes{"help"})
		           .AddText("Only fill this in to rotate the secret. Switching the app to \"no own backend\" clears the secret.");
	}

	LabeledTextArea(Form, "Redirect URIs (one per line)", "redirect_uris", "https://app.example.com/auth/callback", PrefillVal(Values, "redirect_uris"));
	LabeledTextArea(Form, "Post-logout redirect URIs (one per line)", "postlogout_uris", {}, PrefillVal(Values, "postlogout_uris"));
	LabeledInput(Form, "Scopes (space separated)", "scopes", html::Input::TEXT, /*bRequired=*/false).SetValue(PrefillVal(Values, "scopes"));
	{
		auto Label = Form.Add<html::Element>("label");
		Label.Add<html::Input>("require_assignment", "1", html::Input::CHECKBOX).SetChecked(PrefillChecked(Values, "require_assignment"));
		Label.AddText("Restrict to assigned users only");
	}
	Form.Add<html::Paragraph>(html::Classes{"help"})
	    .AddText("Manage this app's role catalog and user assignments on its \"Manage\" page.");

	// Cancel (back to the list, no changes) left of the primary Save action, per
	// the Apple/web convention - and consistent with the delete-confirm pages. The
	// Cancel link replaces the separate back link. Save stays the only submit, so
	// it remains the Enter-key default regardless of DOM order.
	auto Actions = Form.Add<html::Div>(html::Classes{"actions"});
	Actions.Add<html::Link>("/admin/clients", "Cancel").SetClass(html::Classes{"btn"});
	Actions.Add<html::Button>("Save changes");

	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// blunt confirmation before deleting an app (and everything tied to it)
void RenderClientDeleteConfirm(KRESTServer& HTTP, KStringView sAdmin, KStringView sClientID, KssodUserStore& Users)
//-----------------------------------------------------------------------------
{
	std::size_t nRoles  = Users.ListRoles(sClientID).size();
	std::size_t nAssign = Users.ListAssignments(sClientID).size();

	html::Page Page("Delete app", "en");
	auto Body = BeginPage(Page, sAdmin, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>(kFormat("Delete app — {}", sClientID));
	auto CB   = Card.Body();

	auto Warn = CB.Add<html::Div>(html::Classes{"warnbox"});
	Warn.Add<html::Paragraph>()
	    .AddRawText("<strong>This permanently deletes the app and cannot be undone.</strong>");
	Warn.Add<html::Paragraph>().AddText(kFormat(
		"Deleting \"{}\" also removes its {} defined role(s) and {} user assignment(s) to this app. "
		"Every user who signs in through it loses access immediately, and the role/assignment data is gone for good.",
		sClientID, nRoles, nAssign));

	auto Actions = CB.Add<html::Div>(html::Classes{"actions"});
	Actions.Add<html::Link>("/admin/clients", "Cancel").SetClass(html::Classes{"btn"});
	auto Form = Actions.Add<html::Form>("/admin/clients/delete");
	Form.SetMethod(html::Form::POST);
	Form.Add<html::Input>("client_id", sClientID, html::Input::HIDDEN);
	Form.Add<html::Input>("confirmed", "1",       html::Input::HIDDEN);
	Form.Add<html::Button>("Delete permanently").SetClass(html::Classes{"danger"});

	BackLink(Body, "/admin/clients", "Back to apps");
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
void RenderClientAccess(KRESTServer& HTTP, KStringView sUser, KStringView sClientID,
                        KssodUserStore& Users, KStringView sMsg, bool bError, uint16_t iStatus,
                        const KJSON& Prefill)
//-----------------------------------------------------------------------------
{
	html::Page Page("Client access", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/true);

	std::vector<KString> Roles    = Users.ListRoles(sClientID);
	KString              sDefault = Users.DefaultRole(sClientID);

	// ---- role catalog: define the roles available for this client ----
	auto RoleCard = Body.Add<html::ui::Card>(kFormat("Roles — {}", sClientID));
	auto RB       = RoleCard.Body();
	Msg(RB, sMsg, bError); // validation messages from add/assign land here
	if (Roles.empty())
	{
		RB.Add<html::Paragraph>(html::Classes{"muted"}).AddText("No roles defined yet.");
	}
	else
	{
		auto RoleTable = RB.Add<html::Table>();
		auto Head      = RoleTable.Add<html::TableRow>();
		Head.Add<html::TableHeader>("Role");
		Head.Add<html::TableHeader>("Default");
		Head.Add<html::TableHeader>("");
		for (const auto& sRole : Roles)
		{
			bool bIsDefault = (sRole == sDefault);
			auto Row = RoleTable.Add<html::TableRow>();
			Row.Add<html::TableData>(sRole);
			// default column: mark the default, or offer to make this one default
			auto DefCell = Row.Add<html::TableData>();
			auto DefForm = DefCell.Add<html::Form>("/admin/clients/roles/default");
			DefForm.SetMethod(html::Form::POST);
			DefForm.Add<html::Input>("client_id", sClientID, html::Input::HIDDEN);
			// clicking toggles: a default role's button clears it, others set it
			DefForm.Add<html::Input>("role", bIsDefault ? KStringView{} : KStringView(sRole), html::Input::HIDDEN);
			DefForm.Add<html::Button>(bIsDefault ? "default (click to clear)" : "make default",
			                          html::Button::SUBMIT, html::Classes{"secondary"});
			auto Actions = Row.Add<html::TableData>();
			auto Form    = Actions.Add<html::Form>("/admin/clients/roles/delete");
			Form.SetMethod(html::Form::POST);
			Form.Add<html::Input>("client_id", sClientID, html::Input::HIDDEN);
			Form.Add<html::Input>("role",      sRole,     html::Input::HIDDEN);
			Form.Add<html::Button>("Delete");
		}
		RB.Add<html::Paragraph>(html::Classes{"muted"})
		  .AddText("The default role is granted automatically when a user gets access without an explicit role. "
		           "Deleting a role also removes it from every user it was granted to.");
	}
	{
		auto Form = RB.Add<html::Form>("/admin/clients/roles/add");
		Form.SetMethod(html::Form::POST);
		Form.Add<html::Input>("client_id", sClientID, html::Input::HIDDEN);
		LabeledInput(Form, "New role name", "role", html::Input::TEXT).SetValue(PrefillVal(Prefill, "role"));
		Form.Add<html::Button>("Add role");
	}

	// ---- current members ----
	auto MemberCard = Body.Add<html::ui::Card>("Members");
	auto MB         = MemberCard.Body();
	auto Members    = Users.ListAssignments(sClientID);
	if (Members.empty())
	{
		MB.Add<html::Paragraph>(html::Classes{"muted"}).AddText("No users assigned yet.");
	}
	else
	{
		// read-only overview; access + roles are managed per user in the grid
		auto Table = MB.Add<html::Table>();
		auto Head  = Table.Add<html::TableRow>();
		Head.Add<html::TableHeader>("User");
		Head.Add<html::TableHeader>("Status");
		Head.Add<html::TableHeader>("Roles");
		for (const auto& A : Members)
		{
			auto Row = Table.Add<html::TableRow>();
			// the user name links to that user's full access grid (edit there)
			Row.Add<html::TableData>().Add<html::Link>(kFormat("/admin/users/access?user={}", A.sUsername), A.sUsername);
			auto Status = Row.Add<html::TableData>();
			if (A.bAccess) Status.AddText("active");
			else           Status.Add<html::Span>(html::Classes{"muted"}).AddText("suspended");
			Row.Add<html::TableData>(A.sRoles.empty() ? KStringView("—") : KStringView(A.sRoles));
		}
	}

	// access + roles are granted/suspended per user in the grid (suspending keeps
	// the roles; there is no separate "remove from app" - it would be identical to
	// suspend in the SSO flow, only losing the remembered roles)
	MB.Add<html::Paragraph>(html::Classes{"muted"})
	  .AddText("To grant or suspend access, open a user's access grid (click their name above, "
	           "or use \"Manage access\" on the Users page) and tick this app's roles.");

	BackLink(Body, "/admin/clients", "Back to apps");
	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// global "by user" overview: a users x clients matrix, each cell showing the
/// roles that user holds at that client. The user name links to the per-user
/// editor below.
void RenderAccessOverview(KRESTServer& HTTP, KStringView sAdmin,
                          KssodUserStore& Users, KssodClientStore& Clients)
//-----------------------------------------------------------------------------
{
	html::Page Page("Access overview", "en");
	auto Body = BeginPage(Page, sAdmin, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>("Users & access");
	auto CB   = Card.Body();
	CB.Add<html::Paragraph>(html::Classes{"muted"})
	  .AddText("Roles each user holds at each client. Click a user to edit all of their access on one screen.");

	auto ClientList = Clients.List();

	// user -> client -> assignment (roles + access flag)
	std::map<KString, std::map<KString, KssodUserStore::GlobalAssignment>> Grid;
	for (const auto& A : Users.ListAllAssignments()) Grid[A.sUsername][A.sClientID] = A;

	auto Table = CB.Add<html::Table>();
	auto Head  = Table.Add<html::TableRow>();
	Head.Add<html::TableHeader>("User");
	for (const auto& Info : ClientList) Head.Add<html::TableHeader>(Info.Client.sClientID);

	for (const auto& U : Users.List())
	{
		auto Row = Table.Add<html::TableRow>();
		Row.Add<html::TableData>().Add<html::Link>(kFormat("/admin/users/access?user={}", U.sUsername), U.sUsername);

		auto uit = Grid.find(U.sUsername);
		for (const auto& Info : ClientList)
		{
			auto Cell = Row.Add<html::TableData>();
			const KssodUserStore::GlobalAssignment* pA = nullptr;
			if (uit != Grid.end())
			{
				auto cit = uit->second.find(Info.Client.sClientID);
				if (cit != uit->second.end()) pA = &cit->second;
			}
			if      (!pA)             Cell.Add<html::Span>(html::Classes{"muted"}).AddText("—");
			else if (!pA->bAccess)    Cell.Add<html::Span>(html::Classes{"muted"}).AddText("suspended");
			else if (pA->sRoles.empty()) Cell.Add<html::Span>(html::Classes{"muted"}).AddText("(access)");
			else                      Cell.AddText(pA->sRoles);
		}
	}

	BackLink(Body);
	SendPage(HTTP, Page);
}

//-----------------------------------------------------------------------------
/// per-user grid: one row per client, with an Access checkbox and that client's
/// role checkboxes (pre-ticked to the user's current grants). One submit writes
/// the user's complete access across every client.
void RenderUserAccess(KRESTServer& HTTP, KStringView sAdmin, KStringView sTargetUser,
                      KssodUserStore& Users, KssodClientStore& Clients,
                      KStringView sMsg, bool bError, uint16_t iStatus)
//-----------------------------------------------------------------------------
{
	html::Page Page("User access", "en");
	auto Body = BeginPage(Page, sAdmin, /*bAdmin=*/true);
	auto Card = Body.Add<html::ui::Card>(kFormat("Access — {}", sTargetUser));
	auto CB   = Card.Body();
	Msg(CB, sMsg, bError);

	// this user's current roles + access flag per client
	std::map<KString, std::vector<KString>> CurrentRoles;
	std::map<KString, bool>                 CurrentAccess;
	for (const auto& A : Users.ListAllAssignments())
	{
		if (A.sUsername == sTargetUser)
		{
			KSimpleSpacedWords Roles(A.sRoles);
			CurrentRoles[A.sClientID].assign(Roles->begin(), Roles->end());
			CurrentAccess[A.sClientID] = A.bAccess;
		}
	}

	auto Form = CB.Add<html::Form>(kFormat("/admin/users/access/save?user={}", sTargetUser));
	Form.SetMethod(html::Form::POST);

	auto Table = Form.Add<html::Table>();
	auto Head  = Table.Add<html::TableRow>();
	Head.Add<html::TableHeader>("Client");
	Head.Add<html::TableHeader>("Access");
	Head.Add<html::TableHeader>("Roles");

	auto ClientList = Clients.List();
	for (std::size_t i = 0; i < ClientList.size(); ++i)
	{
		const auto&    Info      = ClientList[i];
		const KString& sClientID = Info.Client.sClientID;

		auto rit = CurrentRoles.find(sClientID);
		const std::vector<KString>* pRoles = (rit != CurrentRoles.end()) ? &rit->second : nullptr;
		auto ait = CurrentAccess.find(sClientID);
		bool bActive = (ait != CurrentAccess.end()) && ait->second; // access on iff active row

		auto Row = Table.Add<html::TableRow>();

		auto NameCell = Row.Add<html::TableData>();
		NameCell.AddText(sClientID);
		NameCell.Add<html::Break>();
		// this describes the APP's access model (not the user's status): an app
		// open to all signed-in users, or restricted to assigned ones
		auto Policy = NameCell.Add<html::Span>(html::Classes{"muted"});
		Policy.SetAttribute("title", Info.bRequireAssignment ? "Only assigned users can sign in to this app"
		                                                     : "Any signed-in user can use this app");
		Policy.AddText(Info.bRequireAssignment ? "Assigned only" : "All users");

		// Access checkbox reflects the access flag; the role checkboxes below are
		// pre-ticked from the stored roles regardless, so a suspended user's roles
		// stay visible and are restored when access is re-enabled.
		auto AccessCell = Row.Add<html::TableData>();
		AccessCell.Add<html::Input>(kFormat("access_{}", i), "1", html::Input::CHECKBOX).SetChecked(bActive);

		auto RoleCell = Row.Add<html::TableData>();
		auto    Defined  = Users.ListRoles(sClientID);
		KString sDefault = Users.DefaultRole(sClientID);
		if (Defined.empty())
		{
			RoleCell.Add<html::Span>(html::Classes{"muted"}).AddText("— no roles —");
		}
		else
		{
			for (std::size_t j = 0; j < Defined.size(); ++j)
			{
				const KString& sRole = Defined[j];
				bool bChecked = false;
				if (pRoles) { for (const auto& r : *pRoles) { if (r == sRole) { bChecked = true; break; } } }

				KString sLabel = sRole;
				if (sRole == sDefault) sLabel += " (default)";
				auto Label = RoleCell.Add<html::Element>("label", html::Classes{"role"});
				Label.Add<html::Input>(kFormat("role_{}_{}", i, j), "1", html::Input::CHECKBOX).SetChecked(bChecked);
				Label.AddText(sLabel);
			}
		}
	}

	// action row: Cancel (discards, back to overview) on the left, the primary
	// Save all on the right - the web/macOS/Material convention
	auto Actions = Form.Add<html::Div>(html::Classes{"actions"});
	Actions.Add<html::Link>("/admin/access", "Cancel").SetClass(html::Classes{"btn"});
	Actions.Add<html::Button>("Save all");

	SendPage(HTTP, Page, iStatus);
}

//-----------------------------------------------------------------------------
/// the 403 page shown when a signed-in non-admin hits an admin-only route
/// (GateAdmin, which lives in kssod.cpp, calls this on the not-admin branch)
void RenderForbidden(KRESTServer& HTTP, KStringView sUser)
//-----------------------------------------------------------------------------
{
	html::Page Page("Forbidden", "en");
	auto Body = BeginPage(Page, sUser, /*bAdmin=*/false);
	auto Card = Body.Add<html::ui::Card>("Forbidden");
	Card.Body().Add<html::Paragraph>().AddText("You need administrator rights to view this page.");
	SendPage(HTTP, Page, 403);

} // RenderForbidden
