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

#include "kcurl.h"
#include "klog.h"
#include "khttp.h"

#include <iostream>

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


namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KCurl::setRequestURL(const KString& sRequestURL)
//-----------------------------------------------------------------------------
{
	// TODO Verify this is actually a URL
	if (!sRequestURL.empty())
	{
		m_sRequestURL = sRequestURL;
		return true;
	}
	else
	{
		return false;
	}

} // setRequestURL

//-----------------------------------------------------------------------------
bool KCurl::initiateRequest()
//-----------------------------------------------------------------------------
{
	kDebug(3, "start.");
	// TODO check if is url
	if (m_sRequestURL.empty())
	{
		return false;
	}
	// Even if we only want to echo the body, header contains useful metadata.
	// We will still want this data in the request even if the end user
	// So either we get either header and body or just header.
	KString sFlags = "-i"; // by default assume we want both
	if (m_requestType == POST)
	{
		sFlags += " -X POST";
		sFlags += " -d '";
		sFlags += m_sPostData;
		sFlags += "'";
	}
	else if (m_bEchoHeader && !m_bEchoBody) // just header
	{
		sFlags = "-I";
	}
	KString headers("");
	serializeRequestHeader(headers);
	KString sCurlCMD = "curl ";
	sCurlCMD += sFlags;
	sCurlCMD += " ";
	sCurlCMD += m_sRequestURL;
	sCurlCMD += " ";
	sCurlCMD += headers;
	sCurlCMD += " 2> /dev/null";
	kDebug(3, "send. Command: {}", sCurlCMD);
	return m_kpipe.Open(sCurlCMD);

} // initiateRequest

//-----------------------------------------------------------------------------
bool KCurl::getStreamChunk()
//-----------------------------------------------------------------------------
{
	kDebug(3, "start.");
	KString sCurrentChunk("");
	bool bSuccess = m_kpipe.ReadLine(sCurrentChunk);
	if (bSuccess)
	{
		Parse(sCurrentChunk);
	}
	else //if (!bSuccess)
	{
		m_kpipe.Close();
	}
	kDebug(3, "end.");
	return bSuccess;

} // getStreamChunk

//-----------------------------------------------------------------------------
bool KCurl::setPostDataWithFile(const KString& sFileName)
//-----------------------------------------------------------------------------
{
	bool bExists = kExists(sFileName);
	if (bExists)
	{
		return kReadAll(sFileName, m_sPostData);
	}
	else
	{
		return bExists;
	}
}

//-----------------------------------------------------------------------------
bool KCurl::getRequestHeader(const KString& sHeaderName, KString& sHeaderValue) const
//-----------------------------------------------------------------------------
{
	sHeaderValue = m_requestHeaders.Get(sHeaderName);
	if (sHeaderValue.empty())
	{
		kDebug(3, "({}, {}) failure.", sHeaderName, sHeaderValue);
		return false;
	}
	return true;

} // getRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	m_requestHeaders.Set(sHeaderName, sHeaderValue);
	kDebug(3, "({}, {})", sHeaderName , sHeaderValue);
	return true;

} // setRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	m_requestHeaders.Add(sHeaderName, sHeaderValue);
	kDebug(3, "(){}, {})", sHeaderName, sHeaderValue);
	return true;

} // addRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::delRequestHeader(const KString& sHeaderName)
//-----------------------------------------------------------------------------
{
	m_requestHeaders.Remove(sHeaderName);
	return true;

} // delRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::getRequestCookie(const KString& sCookieName, KString& sCookieValue) const
//-----------------------------------------------------------------------------
{
	sCookieValue = m_requestCookies.Get(sCookieName);
	if (sCookieValue.empty())
	{
		kDebug(3, "'{}' no cookie value found", sCookieName);
		return false;
	}
	return true;

} // getCookie

//-----------------------------------------------------------------------------
bool KCurl::setRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	m_requestCookies.Set(sCookieName, sCookieValue);
	kDebug(3, "('{}' '{}')", sCookieName, sCookieValue);
	return true;

} // setCookie

//-----------------------------------------------------------------------------
bool KCurl::addRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	auto iter = m_requestCookies.Add(sCookieName, sCookieValue);
	if (iter == m_requestCookies.end())
	{
		kDebug(3, "key '{}': Insert fail", sCookieName);
	}
	kDebug(3, "key '{}' end.", sCookieName);
	return true;

} // addCookie

//-----------------------------------------------------------------------------
bool KCurl::delRequestCookie(const KString& sCookieName, const KString& sCookieValue /*= ""*/)
//-----------------------------------------------------------------------------
{
	m_requestCookies.Remove(sCookieName);
	return true;

} // delCookie

//-----------------------------------------------------------------------------
KStringView KCurl::Parse(KStringView sPart, bool bParseCookies)
//-----------------------------------------------------------------------------
{
	return sPart;
}

//-----------------------------------------------------------------------------
bool KCurl::Serialize(KOutStream& outStream)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KCurl::serializeRequestHeader(KString& sCurlHeaders)
//-----------------------------------------------------------------------------
{
	for (const auto& iter : m_requestHeaders)
	{
		sCurlHeaders += "-H '";
		sCurlHeaders += iter.first;
		sCurlHeaders += ": ";
		sCurlHeaders += iter.second;
		sCurlHeaders += "' ";
	}

	if (!m_requestCookies.empty())
	{
		sCurlHeaders += "-H '";
		sCurlHeaders += KHTTP::KHeader::REQUEST_COOKIE;
		sCurlHeaders += ": ";

		size_t counter = 0;
		for (const auto& iter : m_requestCookies)
		{
			if (counter++)
			{
				sCurlHeaders += ';';
			}
			sCurlHeaders += iter.first;
			sCurlHeaders += '=';
			sCurlHeaders += iter.second;
		}
		sCurlHeaders +='\'';
	}
	return !sCurlHeaders.empty();

} //serializeRequestHeader

} // end namespace dekaf2
