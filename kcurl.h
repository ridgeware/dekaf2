///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
//
///////////////////////////////////////////////////////////////////////////////

/*
Relevent data:
Server gives response:
HTTP/1.0 200 OK
Content-type: text/html
Cookie: foo=bar
Set-Cookie: yummy_cookie=choco
Set-Cookie: tasty_cookie=strawberry
X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]

[page content]

Later requests to server:
GET /sample_page.html HTTP/1.1
Host: www.example.org
Cookie: foo=bar; yummy_cookie=choco; tasty_cookie=strawberry
*/

#pragma once
#include "kstring.h"
#include "kprops.h"
#include "kpipe.h"

namespace dekaf2
{
// Macro alternative
// We could have 4 statics, or 4 methods in KCurl and 4 methods in KWebIO
// TODO It appears header case *doesn't matter*
static const KString xForwardedForHeader ("x-forwarded-for");
static const KString ForwardedHeader     ("forwarded");
static const KString HostHeader          ("host");
static const KString CookieHeader        ("cookie");
static const KString UserAgentHeader     ("user-agent");
static const KString sGarbageHeader      ("garbage");


class KCurl
{
public:
	//typedef KPropsTemplate<KString, KString> KHeader; // map for header info
	typedef std::pair<KString,KString> KHeaderPair;
	typedef KProps<KString, KHeaderPair> KHeader; // case insensitive map for header info

	KCurl();
	KCurl(const KString& sRequestURL, bool bEchoHeader = false, bool bEchoBody = false);
	virtual ~KCurl();

	bool         setRequestURL (const KString& sRequestURL);
	bool         initiateRequest(); // set header complete false
	bool         getStreamChunk(); // get next chunk
	bool         requestInProgress() { return m_kpipe.isOpen(); }
	bool         getEchoHeader();
	bool         getEchoBody();
	bool         setEchoHeader(bool bEchoHeader = true);
	bool         setEchoBody  (bool bEchoBody = true);


	bool         getRequestHeader(const KString& sHeaderName, KString& sHeaderValue) const; // so cookies can be appended
	bool         setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue);
	bool         addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue);
	bool         delRequestHeader(const KString& sHeaderName);

	bool         getCookie       (const KString& sCookieName, KString& sCookieValue) const;
	bool         setCookie       (const KString& sCookieName, const KString& sCookieValue);
	bool         addCookie       (const KString& sCookieName, const KString& sCookieValue);
	bool         delCookie       (const KString& sCookieName, const KString& sCookieValue = "");

	virtual bool addToResponseHeader(KString& sHeaderPart);
	virtual bool addToResponseBody  (KString& sBodyPart);
	virtual bool printResponseHeader();

protected:
	bool                          m_bHeaderComplete{false};
	bool                          m_bEchoHeader{false};
	bool                          m_bEchoBody{false};

private:
	KPIPE                         m_kpipe;
	KString                       m_sRequestURL;
	KHeader                       m_requestHeaders;
	KHeader                       m_requestCookies;

	bool                          serializeRequestHeader(KString& sCurlHeaders); // false means no headers
};

}
