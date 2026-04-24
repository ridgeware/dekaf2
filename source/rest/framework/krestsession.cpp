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

#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KRESTSession::KRESTSession(KSession& Session, KRESTServer& HTTP)
//-----------------------------------------------------------------------------
: m_Session(Session)
, m_HTTP(HTTP)
{
	// KRESTServer::GetCookie parses all incoming Cookie: headers once
	// and returns a stable KStringView into its internal map.
	auto sToken = m_HTTP.GetCookie(m_Session.GetCookieName());

	if (!sToken.empty())
	{
		m_bValid = m_Session.Validate(sToken, &m_Record);
	}

} // ctor

//-----------------------------------------------------------------------------
bool KRESTSession::Login(KStringView sUsername,
                         KStringView sPassword,
                         KStringView sExtra)
//-----------------------------------------------------------------------------
{
	auto sClientIP = m_HTTP.GetRemoteIP();

	// User-Agent header may be absent — Request.Headers.Get() returns
	// empty string in that case.
	auto sUserAgent = m_HTTP.Request.Headers.Get(KHTTPHeader::USER_AGENT);

	auto sToken = m_Session.Login(sUsername, sPassword, sClientIP, sUserAgent, sExtra);

	if (sToken.empty())
	{
		return false;
	}

	// emit the fully-serialized Set-Cookie header directly — KSession
	// already applied all configured attributes (Path, Secure, HttpOnly,
	// SameSite, ...). No need to round-trip through KRESTServer::SetCookie,
	// which expects the (name,value,options) form.
	m_HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
	                            m_Session.SerializeSetCookie(sToken));

	// update local state so caller can immediately observe the new session
	m_Record.sToken     = std::move(sToken);
	m_Record.sUsername  = sUsername;
	m_Record.tCreated   = KUnixTime::now();
	m_Record.tLastSeen  = m_Record.tCreated;
	m_Record.sClientIP  = std::move(sClientIP);
	m_Record.sUserAgent = sUserAgent;
	m_Record.sExtra     = sExtra;
	m_bValid            = true;

	return true;

} // Login

//-----------------------------------------------------------------------------
bool KRESTSession::Logout()
//-----------------------------------------------------------------------------
{
	bool bErased = false;

	if (m_bValid)
	{
		bErased = m_Session.Logout(m_Record.sToken);
	}
	else
	{
		// still honor a logout request even if the cookie happened to be
		// invalid on arrival — the browser should drop it either way
		auto sToken = m_HTTP.GetCookie(m_Session.GetCookieName());
		if (!sToken.empty())
		{
			bErased = m_Session.Logout(sToken);
		}
	}

	// always emit the expiry cookie, regardless of whether a server-side
	// record was present, so the browser stops sending the token.
	m_HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
	                            m_Session.SerializeExpiryCookie());

	m_Record = KSession::Record{};
	m_bValid = false;

	return bErased;

} // Logout

//-----------------------------------------------------------------------------
bool KRESTSession::RequireLoginOrRedirect(KStringView sLoginURL)
//-----------------------------------------------------------------------------
{
	if (m_bValid)
	{
		return true;
	}

	// 302 Found is the typical redirect for "go to login page" — we
	// want the method to change to GET regardless of what the original
	// request method was (the browser follows this convention).
	m_HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
	m_HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, KString(sLoginURL));

	return false;

} // RequireLoginOrRedirect

DEKAF2_NAMESPACE_END
