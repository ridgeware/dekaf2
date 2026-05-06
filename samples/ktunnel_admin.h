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

#pragma once

#include "ktunnel.h"
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <memory>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Password-protected admin UI for an exposed ktunnel host.
///
/// Exposes HTML pages under the `/Configure/` path tree on the same
/// REST server that also carries the `/Tunnel` websocket endpoint.
/// Authentication is cookie-session based (see KSession) with a
/// bcrypt-hashed admin password verified against the `admins` table.
///
/// Only rows in the `admins` table can sign into the UI; tunnel
/// endpoints (rows in the `nodes` table) authenticate the wire
/// protocol but have no UI access at all.
class AdminUI
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct the AdminUI.
	/// @param Server the owning ExposedServer. Must outlive this instance.
	///               Used for access to the persistent store (admins,
	///               nodes, tunnels, events), the shared bcrypt
	///               verifier, and the live active-tunnel map for
	///               dashboard stats.
	explicit AdminUI (ExposedServer& Server);

	~AdminUI();

	AdminUI (const AdminUI&)             = delete;
	AdminUI (AdminUI&&)                  = delete;
	AdminUI& operator= (const AdminUI&)  = delete;
	AdminUI& operator= (AdminUI&&)       = delete;

	/// Register all admin UI routes ("/Configure", "/Configure/login",
	/// "/Configure/logout") on the given routes table.
	void RegisterRoutes (KRESTRoutes& Routes);

//----------
private:
//----------

	/// Common Page skeleton: doctype, <head> with title + styles.
	/// Caller fills Body() with route-specific content.
	html::Page MakePage   (KStringView sTitle) const;

	/// Emit @p Page as text/html response body.
	void RenderPage       (KRESTServer& HTTP, html::Page& Page) const;

	/// GET /Configure/login — render login form, optionally with error banner.
	void ShowLogin        (KRESTServer& HTTP,
	                       KStringView sError    = {},
	                       KStringView sUsername = {});

	/// POST /Configure/login — verify credentials, set cookie, redirect.
	void HandleLogin      (KRESTServer& HTTP);

	/// GET/POST /Configure/logout — clear session + cookie, redirect to login.
	void HandleLogout     (KRESTServer& HTTP);

	/// GET /Configure/ — main dashboard shell. Requires a valid session.
	void ShowDashboard    (KRESTServer& HTTP);

	/// Placeholder page for future admin sub-pages that have not been
	/// implemented yet. @p sSection decides which nav entry is marked
	/// active, so the user still gets a consistent navigation experience.
	/// Requires a valid session.
	void ShowStubPage     (KRESTServer& HTTP,
	                       KStringView sSection,
	                       KStringView sTitle,
	                       KStringView sDescription);

	/// GET /Configure/admins — list admins + Add + Change-own-password.
	/// An optional flash message (banner) can be passed through the
	/// query string as `?notice=...` / `?error=...` and is surfaced at
	/// the top of the page.
	void ShowAdmins        (KRESTServer& HTTP);

	/// POST /Configure/admins/add — add a new admin. Requires `username`
	/// and `password` in the form body.
	void HandleAdminsAdd   (KRESTServer& HTTP);

	/// POST /Configure/admins/delete — delete an admin by `username`.
	/// Refuses to delete the currently-logged-in admin (self-locking).
	void HandleAdminsDelete(KRESTServer& HTTP);

	/// POST /Configure/admins/changepass — change the own password.
	/// Form body: `current_password`, `new_password`, `confirm_password`.
	void HandleAdminsChangePass (KRESTServer& HTTP);

	/// GET /Configure/nodes — list nodes + Add/Toggle/Reset-password.
	void ShowNodes              (KRESTServer& HTTP);

	/// POST /Configure/nodes/add — add a new node. Requires `name`,
	/// `password` and optional `enabled` in the form body.
	void HandleNodesAdd         (KRESTServer& HTTP);

	/// POST /Configure/nodes/toggle — flip the `enabled` flag of a
	/// node. Form body: `name`, `enable` ("1" enables, anything else
	/// disables).
	void HandleNodesToggle      (KRESTServer& HTTP);

	/// POST /Configure/nodes/delete — delete a node by `name`. Tunnels
	/// referencing it stay in the DB with a now-dangling owner reference;
	/// ReconcileListeners surfaces them as `OwnerOffline`.
	void HandleNodesDelete      (KRESTServer& HTTP);

	/// POST /Configure/nodes/resetpass — set a new password for a node.
	/// Form body: `name`, `new_password`, `confirm_password`.
	void HandleNodesResetPass   (KRESTServer& HTTP);

	/// GET /Configure/tunnels — list configured tunnels + Add form.
	/// Supports `?notice=…` / `?error=…` flash banners.
	void ShowTunnels          (KRESTServer& HTTP);

	/// POST /Configure/tunnels/add — create a tunnel listener row.
	/// Form body: `name`, `node` (owner-node name, must exist in the
	/// `nodes` table), `listen_port` (the forward port the exposed host
	/// binds to, wildcard bind on all interfaces), `target_host`,
	/// `target_port`, and the optional `enabled` checkbox (defaults to
	/// true).
	void HandleTunnelsAdd     (KRESTServer& HTTP);

	/// POST /Configure/tunnels/toggle — flip the enabled bit of a tunnel.
	/// Form body: `name`, `enable` ("1" for enable, anything else disables).
	void HandleTunnelsToggle  (KRESTServer& HTTP);

	/// POST /Configure/tunnels/delete — delete a tunnel by name.
	void HandleTunnelsDelete  (KRESTServer& HTTP);

	/// GET /Configure/tunnels/edit?name=<n> — render the edit form
	/// pre-filled from the store row. The row name itself is the
	/// primary key and shown read-only; everything else is editable.
	void ShowTunnelEdit       (KRESTServer& HTTP);

	/// POST /Configure/tunnels/update — apply edits from the form.
	/// Form body mirrors HandleTunnelsAdd plus a required `name`
	/// identifying the row. Changing the listener key (host/port/
	/// target/node) triggers a restart via ReconcileListeners.
	void HandleTunnelsUpdate  (KRESTServer& HTTP);

	/// GET /Configure/events — full audit log with optional filters in
	/// the URL query: `?kind=…`, `?limit=100|500|1000`.  Unknown limit
	/// values fall back to 100. The filters are also used to preselect
	/// the corresponding dropdown entries on render.
	void ShowEvents           (KRESTServer& HTTP);

	/// GET /Configure/peers — list currently connected tunnel peers
	/// (from ExposedServer::SnapshotActiveTunnels) with an "Open REPL"
	/// button per row.
	void ShowPeers            (KRESTServer& HTTP);

	/// GET /Configure/peers/repl?peer=<node> — minimal HTML+JS page
	/// that opens a WebSocket to /Configure/peers/repl/ws and renders
	/// the duplex text stream in a <pre>.
	void ShowPeerRepl         (KRESTServer& HTTP);

	/// GET /Configure/peers/repl/ws?peer=<node> — WebSocket endpoint
	/// that proxies frames between the browser and a freshly opened
	/// REPL channel on the named peer's active KTunnel.
	void HandlePeerReplWs     (KRESTServer& HTTP);

	/// GET /Configure/peers/repl/ws?peer=<node> — same URL but matched
	/// for plain (non-upgrade) HTTPS navigations. Renders a tiny 200-OK
	/// landing page so Safari can cache a self-signed certificate
	/// exception for this exact URL (WebKit tracks cert exceptions
	/// per URL for WSS, independently of the HTTPS page that loaded
	/// the REPL).
	void HandlePeerReplCert   (KRESTServer& HTTP);

	/// Render the common top-bar (brand + nav) into the body of @p Page
	/// and mark the nav entry identified by @p sActive as the active one
	/// so it gets a highlighted pill. Every signed-in user is an admin,
	/// so all nav entries are always shown.
	void RenderTopBar     (html::Page& Page,
	                       KStringView sActive,
	                       KStringView sAdmin) const;

	/// Helper: redirect the browser back to @p sURL with a one-shot
	/// flash notice appended as `?notice=...` or `?error=...`.
	/// Empty messages redirect without a flash.
	void RedirectWithFlash (KRESTServer& HTTP,
	                        KStringView sURL,
	                        KStringView sNotice,
	                        KStringView sError) const;

	ExposedServer&               m_Server;      ///< non-owning back-ref
	const ExposedServer::Config& m_Config;      ///< == m_Server.config
	std::unique_ptr<KSession>    m_Session;

}; // AdminUI
