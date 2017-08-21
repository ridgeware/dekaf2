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

#include <iostream>

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
	KLog().debug(3, "KCurl::initiateRequest() start.");
	m_bHeaderComplete = false;
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
	KLog().debug(3, "KCurl::initiateRequest() end. Command: {}", sCurlCMD);
	return m_kpipe.Open(sCurlCMD);

} // initiateRequest

//-----------------------------------------------------------------------------
bool KCurl::getStreamChunk()
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::getStreamChunk() start.");
	KString sCurrentChunk("");
	bool bSuccess = m_kpipe.ReadLine(sCurrentChunk);
	if (bSuccess)
	{
		if (!m_bHeaderComplete)
		{
			addToResponseHeader(sCurrentChunk);
		}

		else // parsing body
		{
			addToResponseBody(sCurrentChunk);
		}
	}
	else //if (!bSuccess)
	{
		m_kpipe.Close();
	}
	KLog().debug(3, "KCurl::getStreamChunk() end.");
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
	KString sSearchHeader(sHeaderName.ToLower());
	sSearchHeader.Trim();
	KCurl::KHeaderPair header = m_requestHeaders.Get(sSearchHeader);
	if (header.first.empty() || header.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestHeader({},{}) end failure.", sHeaderName, sHeaderValue);
		return false;
	}
	sHeaderValue = header.second;
	return true;

} // getRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KString sHeaderKey(sHeaderName.ToLower());
	sHeaderKey.Trim();
	m_requestHeaders.Set(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	KLog().debug(3, "KCurl::setRequestHeader({},{}) header",sHeaderName , sHeaderName);
	return true;

} // setRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KString sHeaderKey(sHeaderName.ToLower());
	sHeaderKey.Trim();
	m_requestHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	KLog().debug(3, "KCurl::addRequestHeader({},{}) ", sHeaderName, sHeaderValue);
	return true;

} // addRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::delRequestHeader(const KString& sHeaderName)
//-----------------------------------------------------------------------------
{
	KString sHeaderKey(sHeaderName.ToLower());
	sHeaderKey.Trim();
	m_requestHeaders.Remove(sHeaderKey);
	return true;

} // delRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::getRequestCookie(const KString& sCookieName, KString& sCookieValue) const
//-----------------------------------------------------------------------------
{
	KString sCookieKey(sCookieName.ToLower());
	sCookieKey.Trim();
	KCurl::KHeaderPair cookie = m_requestCookies.Get(sCookieKey);
	if (cookie.first.empty() || cookie.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestCookie({},{}) with key '{}' end unsuccessful.", sCookieName, sCookieName, sCookieKey);
		return false;
	}
	sCookieValue = cookie.second;
	return true;

} // getCookie

//-----------------------------------------------------------------------------
bool KCurl::setRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KString sCookieKey(sCookieName.ToLower());
	sCookieKey.Trim();
	m_requestCookies.Set(sCookieKey, KHeaderPair(sCookieName, sCookieValue));
	KLog().debug(3, "KCurl::setRequestCookie({},{}) with key '{}' end.", sCookieName, sCookieName, sCookieKey);
	return true;

} // setCookie

//-----------------------------------------------------------------------------
bool KCurl::addRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KString sCookieKey(sCookieName.ToLower());
	sCookieKey.Trim();
	auto iter = m_requestCookies.Add(sCookieKey, KHeaderPair(sCookieKey, sCookieValue));
	if (iter == m_requestCookies.end())
	{
		KLog().debug(3, "KCurl::addRequestCookie({},{}) with key '{}' end. Insert fail", sCookieName, sCookieName, sCookieKey);
	}
	KLog().debug(3, "KCurl::addRequestCookie({},{}) with key '{}' end.", sCookieName, sCookieName, sCookieKey);
	return true;

} // addCookie

//-----------------------------------------------------------------------------
bool KCurl::delRequestCookie(const KString& sCookieName, const KString& sCookieValue /*= ""*/)
//-----------------------------------------------------------------------------
{
	KString sCookieKey(sCookieName.ToLower());
	sCookieKey.Trim();
	m_requestCookies.Remove(sCookieName);
	return true;

} // delCookie

//-----------------------------------------------------------------------------
bool KCurl::addToResponseHeader(KString& sHeaderPart)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KCurl::addToResponseBody  (KString& sBodyPart)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KCurl::printResponseHeader  ()
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KCurl::serializeRequestHeader(KString& sCurlHeaders)
//-----------------------------------------------------------------------------
{
	for (auto iter = m_requestHeaders.begin(); iter != m_requestHeaders.end(); iter++)
	{
		sCurlHeaders += "-H '";
		sCurlHeaders += iter->second.first;
		sCurlHeaders += ": ";
		sCurlHeaders += iter->second.second;
		sCurlHeaders += "' ";
	}
	if (!m_requestCookies.empty())
	{
		sCurlHeaders += "-H '";
		sCurlHeaders += CookieHeader;
		sCurlHeaders += ": ";
	}
	for (auto iter = m_requestCookies.begin(); iter != m_requestCookies.end(); iter++)
	{
		if (iter != m_requestCookies.begin())
		{
			sCurlHeaders += ";";
		}
		sCurlHeaders += iter->second.first;
		sCurlHeaders += "=";
		sCurlHeaders += iter->second.second;
	}
	if (!m_requestCookies.empty())
	{
		sCurlHeaders += "'";
	}
	return !sCurlHeaders.empty();

} //serializeRequestHeader

} // end namespace dekaf2
