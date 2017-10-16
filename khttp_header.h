/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kstringview.h"
#include "kprops.h"
#include "kcasestring.h"
#include "kurl.h"
#include "kconnection.h"

namespace dekaf2 {

namespace detail {
namespace http {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHeader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// The key for the Header is trimmed and lower cased on comparisons, but stores the original string
	using KHeaderMap = KProps<KCaseTrimString, KString>; // case insensitive map for header info

	//-----------------------------------------------------------------------------
	KStringView Parse(KStringView svBuffer, bool bParseCookies = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& outStream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHeaderMap::const_iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.begin();
	}

	//-----------------------------------------------------------------------------
	KHeaderMap::const_iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.end();
	}

	//-----------------------------------------------------------------------------
	KHeaderMap::const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.cbegin();
	}

	//-----------------------------------------------------------------------------
	KHeaderMap::const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.cend();
	}

	//-----------------------------------------------------------------------------
	KHeaderMap::iterator begin()
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.begin();
	}

	//-----------------------------------------------------------------------------
	KHeaderMap::iterator end()
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.end();
	}

	//-----------------------------------------------------------------------------
	const KHeaderMap* operator->() const
	//-----------------------------------------------------------------------------
	{
		return &m_responseHeaders;
	}

	//-----------------------------------------------------------------------------
	KHeaderMap* operator->()
	//-----------------------------------------------------------------------------
	{
		return &m_responseHeaders;
	}

	//-----------------------------------------------------------------------------
	KStringView Get(KCaseStringView sv) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template <class K, class V>
	KHeaderMap::iterator Set(K&& sv, V&& svv)
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.Set(std::forward<K>(sv), std::forward<V>(svv));
	}

	//-----------------------------------------------------------------------------
	/// Get all response cookies as a KHeaderMap
	const KHeaderMap& getResponseCookies() const
	//-----------------------------------------------------------------------------
	{
		return m_responseCookies;
	}

	//-----------------------------------------------------------------------------
	/// Get all response cookies as a KHeaderMap
	KHeaderMap& getResponseCookies()
	//-----------------------------------------------------------------------------
	{
		return m_responseCookies;
	}

	//-----------------------------------------------------------------------------
	/// Get response cookie value from given cookie name (case insensitive)
	template <class K>
	const KString& getResponseCookie(K&& sCookieName) const
	//-----------------------------------------------------------------------------
	{
		return m_responseCookies.Get(std::forward<K>(sCookieName));
	}

	//-----------------------------------------------------------------------------
	/// returns true if all headers have been parsed
	bool HeaderComplete() const
	//-----------------------------------------------------------------------------
	{
		return m_bHeaderComplete;
	}

	// Header names in "official" Pascal case
	static constexpr KStringView AUTHORIZATION       = "Authorization";
	static constexpr KStringView HOST                = "Host";
	static constexpr KStringView HOST_OVERRIDE       = "HostOverride";
	static constexpr KStringView USER_AGENT          = "User-Agent";
	static constexpr KStringView CONTENT_TYPE        = "Content-Type";
	static constexpr KStringView CONTENT_LENGTH      = "Content-Length";
	static constexpr KStringView CONTENT_MD5         = "Content-MD5";
	static constexpr KStringView CONNECTION          = "Connection";
	static constexpr KStringView LOCATION            = "Location";
	static constexpr KStringView REQUEST_COOKIE      = "Cookie";
	static constexpr KStringView RESPONSE_COOKIE     = "Set-Cookie";
	static constexpr KStringView ACCEPT_ENCODING     = "Accept-Encoding";
	static constexpr KStringView TRANSFER_ENCODING   = "Transfer-Encoding";
	static constexpr KStringView VARY                = "Vary";
	static constexpr KStringView REFERER             = "Referer";
	static constexpr KStringView ACCEPT              = "Accept";
	static constexpr KStringView X_FORWARDED_FOR     = "X-Forwarded-For";

	// Header names in lowercase for normalized compares
	static constexpr KStringView authorization       = "authorization";
	static constexpr KStringView host                = "host";
	static constexpr KStringView host_override       = "hostoverride";
	static constexpr KStringView user_agent          = "user-agent";
	static constexpr KStringView content_type        = "content-type";
	static constexpr KStringView content_length      = "content-length";
	static constexpr KStringView content_md5         = "content-md5";
	static constexpr KStringView connection          = "connection";
	static constexpr KStringView location            = "location";
	static constexpr KStringView request_cookie      = "cookie";
	static constexpr KStringView response_cookie     = "set-cookie";
	static constexpr KStringView accept_encoding     = "accept-encoding";
	static constexpr KStringView transfer_encoding   = "transfer-encoding";
	static constexpr KStringView vary                = "vary";
	static constexpr KStringView referer             = "referer";
	static constexpr KStringView accept              = "accept";
	static constexpr KStringView x_forwarded_for     = "x-forwarded-for";

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// method that takes care of case-insentive header add logic and cookie add logic
	bool addResponseHeader(KStringView sHeaderName, KStringView sHeaderValue, bool bParseCookies);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// if parsing multi line header, this gets to the end of it
	size_t findEndOfHeader(KStringView svHeaderPart, size_t lineEndPos);
	//-----------------------------------------------------------------------------

	static constexpr KStringView svBrokenHeader = "!?.garbage";

	KHeaderMap     m_responseHeaders; // response headers read in
	KHeaderMap     m_responseCookies; // response cookies read in
	KString        m_sPartialHeader; // when streaming can't guarantee always have full header.
	KString        m_sResponseVersion; // HTTP resonse version
	KString        m_sResponseStatus; // HTTP response status
	uint16_t       m_iResponseStatusCode{0}; // HTTP response code
	bool           m_bHeaderComplete{false}; // Whether to interpret response chunk as header or body

}; // KHeader

} // end of namespace http
} // end of namespace detail
} // end of namespace dekaf2
