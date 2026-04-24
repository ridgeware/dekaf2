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
 //
 */

#pragma once

/// @file krestsession.h
/// Per-request helper that glues KSession to KRESTServer's cookie API.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/rest/framework/krestserver.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup rest_framework
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Per-request helper that reads the session cookie from a KRESTServer
/// request, validates it against a KSession, and exposes authentication
/// state plus Login/Logout convenience methods that emit the correct
/// Set-Cookie response headers.
///
/// Instantiate one of these at the top of each protected route handler:
///
/// @code
/// Routes.AddRoute("/Configure/").Get([&](KRESTServer& HTTP)
/// {
///     KRESTSession Sess(Session, HTTP);
///     if (!Sess.RequireLoginOrRedirect("/Configure/login")) return;
///     // ... authenticated logic; Sess.GetUser() is valid
/// });
/// @endcode
class DEKAF2_PUBLIC KRESTSession
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Read the session cookie and validate it against the given KSession.
	/// After construction, IsAuthenticated() reflects whether a valid
	/// session was found. Further Login() / Logout() calls update both
	/// the KSession store and the outgoing Set-Cookie headers.
	/// @param Session the KSession to delegate to (caller owns lifetime)
	/// @param HTTP    the REST server instance for this request
	KRESTSession(KSession& Session, KRESTServer& HTTP);

	KRESTSession(const KRESTSession&)            = delete;
	KRESTSession(KRESTSession&&)                 = delete;
	KRESTSession& operator=(const KRESTSession&) = delete;
	KRESTSession& operator=(KRESTSession&&)      = delete;

	/// @returns true if the request came with a valid, non-expired session cookie
	DEKAF2_NODISCARD
	bool IsAuthenticated () const { return m_bValid; }

	/// @returns the logged-in user name (empty if not authenticated)
	DEKAF2_NODISCARD
	KStringView GetUser  () const { return m_Record.sUsername; }

	/// @returns the full session record (only meaningful if IsAuthenticated())
	DEKAF2_NODISCARD
	const KSession::Record& GetRecord() const { return m_Record; }

	/// Verify credentials and, on success, create a new server-side session
	/// and emit a Set-Cookie header on the response. The ClientIP and
	/// UserAgent from the current HTTP request are stored in the session.
	/// @param sUsername the user to log in
	/// @param sPassword the submitted password
	/// @param sExtra    optional application-specific payload (e.g. role JSON)
	/// @returns true on successful authentication; false leaves the
	/// response state untouched so the caller can render a login-error page
	bool Login (KStringView sUsername, KStringView sPassword,
	            KStringView sExtra = {});

	/// Invalidate the current session (if any) on the server and emit
	/// a cookie-expiry Set-Cookie header so the browser drops the cookie.
	/// @returns true if a session existed and was erased
	bool Logout ();

	/// If not authenticated, emit a 302 redirect to @p sLoginURL and
	/// return false — the caller should immediately return from the
	/// route handler to avoid writing further output.
	/// If authenticated, returns true without touching the response.
	/// @param sLoginURL the location to redirect unauthenticated users to
	bool RequireLoginOrRedirect (KStringView sLoginURL);

//------
private:
//------

	KSession&           m_Session;
	KRESTServer&        m_HTTP;
	KSession::Record    m_Record;
	bool                m_bValid { false };

}; // KRESTSession

/// @}

DEKAF2_NAMESPACE_END
