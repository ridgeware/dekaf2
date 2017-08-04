#include "kcurl.h"
#include "klog.h"

#include <iostream>

#define btoa(b) ((b)?"true":"false")

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KCurl::setRequestURL(const KString& sRequestURL)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::setRequestURL(%s) start", sRequestURL.c_str());
	// TODO Verify this is actually a URL
	if (!sRequestURL.empty())
	{
		m_sRequestURL = sRequestURL;
		KLog().debug(3, "KCurl::setRequestURL(%s) end successful.", sRequestURL.c_str());
		return true;
	}
	else {
		KLog().debug(3, "KCurl::setRequestURL(%s) end failure.", sRequestURL.c_str());
		return false;
	}

} // setRequestURL

//-----------------------------------------------------------------------------
bool KCurl::initiateRequest()
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::initiateRequest() start.");
	m_bHeaderComplete = false;
	// open kpipe with curl command
	// start simple, just curl site
	// TODO check if is url
	if (m_sRequestURL.empty())
	{
		return false;
	}
	KString sFlags = "-i"; // by default assume we want both
	if (m_bEchoHeader && !m_bEchoBody) // just header
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
	// We never want just body, must read header to know size of body in many cases
	KString sCurlCMD = "curl " + sFlags + " " + m_sRequestURL + " " + headers +" 2> /dev/null";// + " | cat";
	//std::cout << "curl cmd: " << sCurlCMD << std::endl;
	KLog().debug(3, "KCurl::initiateRequest() end.");
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
bool KCurl::getEchoHeader()
//-----------------------------------------------------------------------------
{
	return m_bEchoHeader;

} // getEchoHeader

//-----------------------------------------------------------------------------
bool KCurl::getEchoBody()
//-----------------------------------------------------------------------------
{
	return m_bEchoBody;

} // getEchoBody

//-----------------------------------------------------------------------------
bool KCurl::setEchoHeader(bool bEchoHeader /*= true*/)
//-----------------------------------------------------------------------------
{
	m_bEchoHeader = bEchoHeader;
	return true;

} // setEchoHeader

//-----------------------------------------------------------------------------
bool KCurl::setEchoBody(bool bEchoBody /*= true*/)
//-----------------------------------------------------------------------------
{
	m_bEchoBody = bEchoBody;
	return true;

} // setEchoBody

//-----------------------------------------------------------------------------
bool KCurl::getRequestHeader(const KString& sHeaderName, KString& sHeaderValue) const
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::getRequestHeader(%s,%s) start.", sHeaderName.c_str(), sHeaderValue.c_str());
	KString sSearchHeader = sHeaderName;
	sSearchHeader.MakeLower();
	KCurl::KHeaderPair header = m_requestHeaders.Get(sSearchHeader);
	if (header.first.empty() || header.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestHeader(%s,%s) end failure.", sHeaderName.c_str(), sHeaderValue.c_str());
		return false;
	}
	sHeaderValue = header.second;
	KLog().debug(3, "KCurl::getRequestHeader(%s,%s) end sucess.", sHeaderName.c_str(), sHeaderValue.c_str());
	return true;

} // getRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::setRequestHeader(%s,%s) start.", sHeaderName.c_str(), sHeaderValue.c_str());
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Set(sHeaderKey, KHeaderPair(sHeaderName,sHeaderValue));
	KLog().debug(3, "KCurl::setRequestHeader(%s,%s) with key '%s' end.", sHeaderName.c_str(), sHeaderValue.c_str(), sHeaderKey.c_str());
	return true;

} // setRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::addRequestHeader(%s,%s) start.", sHeaderName.c_str(), sHeaderValue.c_str());
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	KLog().debug(3, "KCurl::addRequestHeader(%s,%s) with key '%s' end.", sHeaderName.c_str(), sHeaderValue.c_str(), sHeaderKey.c_str());
	return true;

} // addRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::delRequestHeader(const KString& sHeaderName)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::delRequestHeader(%s) start.", sHeaderName.c_str());
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Remove(sHeaderKey);
	KLog().debug(3, "KCurl::addRequestHeader(%s) end.", sHeaderName.c_str());
	return true;

} // delRequestHeader

//-----------------------------------------------------------------------------
bool KCurl::getRequestCookie(const KString& sCookieName, KString& sCookieValue) const
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::getRequestCookie(%s,%s) start.", sCookieName.c_str(), sCookieName.c_str());
	KString sCookieKey = sCookieName;
	sCookieKey.MakeLower();
	KCurl::KHeaderPair cookie = m_requestCookies.Get(sCookieKey);
	if (cookie.first.empty() || cookie.second.empty())
	{
		KLog().debug(3, "KCurl::getRequestCookie(%s,%s) with key '%s' end unsuccessful.", sCookieName.c_str(), sCookieName.c_str(), sCookieKey.c_str());
		return false;
	}
	sCookieValue = cookie.second;
	KLog().debug(3, "KCurl::getRequestCookie(%s,%s) with key '%s' end successful.", sCookieName.c_str(), sCookieName.c_str(), sCookieKey.c_str());
	return true;

} // getCookie

//-----------------------------------------------------------------------------
bool KCurl::setRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::setRequestCookie(%s,%s) start.", sCookieName.c_str(), sCookieName.c_str());
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Set(sCookieKey, KHeaderPair(sCookieName,sCookieValue));
	KLog().debug(3, "KCurl::setRequestCookie(%s,%s) with key '%s' end.", sCookieName.c_str(), sCookieName.c_str(), sCookieKey.c_str());
	return true;

} // setCookie

//-----------------------------------------------------------------------------
bool KCurl::addRequestCookie(const KString& sCookieName, const KString& sCookieValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::addRequestCookie(%s,%s) start.", sCookieName.c_str(), sCookieName.c_str());
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Add(sCookieKey, KHeaderPair(sCookieKey, sCookieValue));
	KLog().debug(3, "KCurl::addRequestCookie(%s,%s) with key '%s' end.", sCookieName.c_str(), sCookieName.c_str(), sCookieKey.c_str());
	return true;

} // addCookie

//-----------------------------------------------------------------------------
bool KCurl::delRequestCookie(const KString& sCookieName, const KString& sCookieValue /*= ""*/)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KCurl::delRequestCookie(%s) start.", sCookieValue.c_str());
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Remove(sCookieName);
	KLog().debug(3, "KCurl::delRequestCookie(%s) end.", sCookieValue.c_str());
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
	KLog().debug(3, "KCurl::serializeRequestHeader(%s) start.", sCurlHeaders.c_str());
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
	KLog().debug(3, "KCurl::serializeRequestHeader(%s) end.", sCurlHeaders.c_str());
	return !sCurlHeaders.empty();

} //serializeRequestHeader

} // end namespace dekaf2
