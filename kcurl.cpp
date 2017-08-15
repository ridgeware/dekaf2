#include "kcurl.h"
#include "klog.h"

#include <iostream>

//#define btoa(b) ((b)?"true":"false")

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
	else {
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
	/* Even if we only want to echo the body, header contains useful metadata
	else if (!m_bEchoHeader && m_bEchoBody)
	{
		sFlags = "";
	}
	*/
	KString headers("");
	serializeRequestHeader(headers);
	KString sCurlCMD = "curl " + sFlags + " " + m_sRequestURL + " " + headers +" 2> /dev/null";// + " | cat";
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
	//std::cout << "grabbing stream chunk:";
	//std::cout << sCurrentChunk;
	if (bSuccess)
	{
		if (!m_bHeaderComplete)
		{
			//std::cout << "adding to response header" << std::endl;
			addToResponseHeader(sCurrentChunk);
		}

		else // parsing body
		{
			//std::cout << "adding to response body" << std::endl;
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
	KLog().debug(3, "KCurl::getRequestHeader({},{}) start.", sHeaderName, sHeaderValue);
	KString sSearchHeader = sHeaderName;
	sSearchHeader.MakeLower();
	KCurl::KHeaderPair header = m_requestHeaders.Get(sSearchHeader);
	if (header.first.empty() || header.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestHeader({},{}) end failure.", sHeaderName, sHeaderValue);
		return false;
	}
	sHeaderValue = header.second;
	KLog().debug(3, "KCurl::getRequestHeader({},{}) end sucess.", sHeaderName, sHeaderValue);
	return true;

} // getRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::setRequestHeader({},{}) start.", sHeaderName.c_str(), sHeaderValue.c_str());
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Set(sHeaderKey, KHeaderPair(sHeaderName,sHeaderValue));
	KLog().debug(3, "KCurl::setRequestHeader({},{}) with key '{}' end.", sHeaderName, sHeaderValue, sHeaderKey);
	return true;

} // setRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::addRequestHeader({},{}) start.", sHeaderName, sHeaderValue);
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	KLog().debug(3, "KCurl::addRequestHeader({},{}) with key '{}' end.", sHeaderName, sHeaderValue, sHeaderKey);
	return true;

} // addRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::delRequestHeader(const KString& sHeaderName)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::delRequestHeader({}) start.", sHeaderName);
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Remove(sHeaderKey);
	KLog().debug(3, "KCurl::addRequestHeader({}) end.", sHeaderName);
	return true;

} // delRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::getRequestCookie(const KString& sCookieName, KString& sCookieValue) const
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::getRequestCookie({},{}) start.", sCookieName, sCookieName);
	KString sCookieKey = sCookieName;
	sCookieKey.MakeLower();
	KCurl::KHeaderPair cookie = m_requestCookies.Get(sCookieKey);
	if (cookie.first.empty() || cookie.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestCookie({},{}) with key '{}' end unsuccessful.", sCookieName, sCookieName, sCookieKey);
		return false;
	}
	sCookieValue = cookie.second;
	KLog().debug(3, "KCurl::getRequestCookie({},{}) with key '{}' end successful.", sCookieName, sCookieName, sCookieKey);
	return true;

} // getCookie

//-----------------------------------------------------------------------------
bool KCurl::setRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::setRequestCookie({},{}) start.", sCookieName, sCookieName);
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Set(sCookieKey, KHeaderPair(sCookieName,sCookieValue));
	KLog().debug(3, "KCurl::setRequestCookie({},{}) with key '{}' end.", sCookieName, sCookieName, sCookieKey);
	return true;

} // setCookie

//-----------------------------------------------------------------------------
bool KCurl::addRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::addRequestCookie({},{}) start.", sCookieName, sCookieName);
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Add(sCookieKey, KHeaderPair(sCookieKey, sCookieValue));
	KLog().debug(3, "KCurl::addRequestCookie({},{}) with key '{}' end.", sCookieName, sCookieName, sCookieKey);
	return true;

} // addCookie

//-----------------------------------------------------------------------------
bool KCurl::delRequestCookie(const KString& sCookieName, const KString& sCookieValue /*= ""*/)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::delRequestCookie({}) start.", sCookieValue);
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Remove(sCookieName);
	KLog().debug(3, "KCurl::delRequestCookie({}) end.", sCookieValue);
	return true;

} // delCookie

//-----------------------------------------------------------------------------
bool KCurl::addToResponseHeader(KString& sHeaderPart){ return true; }
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool KCurl::addToResponseBody  (KString& sBodyPart){ return true; }
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool KCurl::printResponseHeader  (){ return true; }
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool KCurl::serializeRequestHeader(KString& sCurlHeaders)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::serializeRequestHeader({}) start.", sCurlHeaders);
	for (auto iter = m_requestHeaders.begin(); iter != m_requestHeaders.end(); iter++)
	{
		sCurlHeaders += "-H '" + iter->second.first + ": " + iter->second.second + "' ";
	}
	if (!m_requestCookies.empty())
	{
		sCurlHeaders += "-H '" + CookieHeader + ": ";
	}
	for (auto iter = m_requestCookies.begin(); iter != m_requestCookies.end(); iter++)
	{
		if (iter != m_requestCookies.begin())
		{
			sCurlHeaders += ";";
		}
		sCurlHeaders += iter->second.first + "=" + iter->second.second;
	}
	if (!m_requestCookies.empty())
	{
		sCurlHeaders += "'";
	}
	KLog().debug(3, "KCurl::serializeRequestHeader({}) end.", sCurlHeaders);
	return !sCurlHeaders.empty();

} //serializeRequestHeader

} // end namespace dekaf2
