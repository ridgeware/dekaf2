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
/// bcrypt-hashed admin password that is seeded from the first CLI
/// `-secret` value at process startup.
class AdminUI
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct the AdminUI.
	/// @param Server the owning ExposedServer. Must outlive this instance.
	///               Used for access to the persistent store (users,
	///               tunnels, events), the shared bcrypt verifier, and
	///               the live active-tunnel map for dashboard stats.
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

	/// GET /Configure/users — list users + Add/Change-password forms.
	/// An optional flash message (banner) can be passed through the
	/// query string as `?notice=...` / `?error=...` and is surfaced at
	/// the top of the page.
	void ShowUsers        (KRESTServer& HTTP);

	/// POST /Configure/users/add — add a new user. Requires `username`,
	/// `password` and optional `is_admin` in the form body.
	void HandleUsersAdd   (KRESTServer& HTTP);

	/// POST /Configure/users/delete — delete a user by `username`.
	/// Refuses to delete the currently-logged-in user (self-locking).
	void HandleUsersDelete(KRESTServer& HTTP);

	/// POST /Configure/users/changepass — change the own password.
	/// Form body: `current_password`, `new_password`, `confirm_password`.
	void HandleUsersChangePass (KRESTServer& HTTP);

	/// GET /Configure/tunnels — list configured tunnels + Add form.
	/// Supports `?notice=…` / `?error=…` flash banners.
	void ShowTunnels          (KRESTServer& HTTP);

	/// POST /Configure/tunnels/add — create a tunnel listener row.
	/// Form body: `name`, `owner`, `listen_port` (the forward port the
	/// exposed host binds to, wildcard bind on all interfaces),
	/// `target_host`, `target_port`, and the optional `enabled` checkbox
	/// (defaults to true).
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
	/// target/owner) triggers a restart via ReconcileListeners.
	void HandleTunnelsUpdate  (KRESTServer& HTTP);

	/// GET /Configure/events — full audit log with optional filters in
	/// the URL query: `?kind=…`, `?user=…`, `?limit=100|500|1000`.
	/// Unknown limit values fall back to 100. The filters are also
	/// used to preselect the corresponding dropdown entries on render.
	void ShowEvents           (KRESTServer& HTTP);

	/// GET /Configure/peers — list currently connected tunnel peers
	/// (from ExposedServer::SnapshotActiveTunnels) with an "Open REPL"
	/// button per row. Admin-only.
	void ShowPeers            (KRESTServer& HTTP);

	/// GET /Configure/peers/repl?peer=<user> — minimal HTML+JS page
	/// that opens a WebSocket to /Configure/peers/repl/ws and renders
	/// the duplex text stream in a <pre>. Admin-only.
	void ShowPeerRepl         (KRESTServer& HTTP);

	/// GET /Configure/peers/repl/ws?peer=<user> — WebSocket endpoint
	/// that proxies frames between the browser and a freshly opened
	/// REPL channel on the named peer's active KTunnel. Admin-only.
	void HandlePeerReplWs     (KRESTServer& HTTP);

	/// Render the common top-bar (brand + nav) into the body of @p Page
	/// and mark the nav entry identified by @p sActive as the active one
	/// so it gets a highlighted pill. Admin-only nav entries (Users,
	/// Tunnels, Events) are hidden for non-admin users.
	void RenderTopBar     (html::Page& Page,
	                       KStringView sActive,
	                       KStringView sUser,
	                       bool        bIsAdmin) const;

	/// Helper: redirect the browser back to @p sURL with a one-shot
	/// flash notice appended as `?notice=...` or `?error=...`.
	/// Empty messages redirect without a flash.
	void RedirectWithFlash (KRESTServer& HTTP,
	                        KStringView sURL,
	                        KStringView sNotice,
	                        KStringView sError) const;

	/// Helper: confirm the currently-logged-in user has the `is_admin`
	/// flag set in the store. Non-admins are redirected to the dashboard
	/// with an error flash. Returns true iff the caller is an admin.
	/// The lookup happens on every call so flipping the bit in the DB
	/// takes effect at the next request (no cached session state).
	bool RequireAdminOrRedirect (KRESTServer& HTTP, KRESTSession& Sess);

	/// Return the admin flag for the user behind @p Sess. Returns false
	/// if the session references a user that no longer exists (stale
	/// cookie after account deletion).
	bool IsAdmin (KRESTSession& Sess);

	ExposedServer&               m_Server;      ///< non-owning back-ref
	const ExposedServer::Config& m_Config;      ///< == m_Server.config
	std::unique_ptr<KSession>    m_Session;

}; // AdminUI
