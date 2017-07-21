#include "kcurl.h"

#include <iostream>

namespace dekaf2
{
// temporarily here.
//KCurl::CIKHeader                     m_ciRequestHeaders;
KCurl::KCurl()
{

}

KCurl::KCurl(const KString& sRequestURL, bool bEchoHeader /*=false*/, bool bEchoBody /*=false*/)
    : m_bEchoHeader{bEchoHeader}, m_bEchoBody{bEchoBody}, m_sRequestURL{sRequestURL}
{

}

KCurl::~KCurl(){}

bool KCurl::setRequestURL(const KString& sRequestURL)
{
	// TODO Verify this is actually a URL
	if (sRequestURL.empty())
	{
		m_sRequestURL = sRequestURL;
		return true;
	}
	else return false;
}

bool KCurl::initiateRequest() // set header complete false
{
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
	return m_kpipe.Open(sCurlCMD, "r");
}

bool KCurl::getStreamChunk() // get next chunk
{
	KString sCurrentChunk;
	bool bSuccess = m_kpipe.getline(sCurrentChunk, 4096);
	//std::cout << "grabbing stream chunk" << std::endl;
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

	return bSuccess;
}

bool KCurl::getEchoHeader()
{
	return m_bEchoHeader;
}

bool KCurl::getEchoBody()
{
	return m_bEchoBody;
}

bool KCurl::setEchoHeader(bool bEchoHeader /*= true*/)
{
	m_bEchoHeader = bEchoHeader;
	return true;
}

bool KCurl::setEchoBody(bool bEchoBody /*= true*/)
{
	m_bEchoBody = bEchoBody;
	return true;
}

bool KCurl::getRequestHeader(const KString& sHeaderName, KString& sHeaderValue) const
{
	KString sSearchHeader = sHeaderName;
	sSearchHeader.MakeLower();
	KCurl::KHeaderPair header = m_requestHeaders.Get(sSearchHeader);
	if (header.first.empty() || header.second.empty())
	{
		return false;
	}
	sHeaderValue = header.second;
	return true;

	/*
	sHeaderValue = m_requestHeaders.Get(sHeaderName);
	if (sHeaderValue.empty())
	{
		return false;
	}
	return true;
	*/
}

bool KCurl::setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Set(sHeaderKey, KHeaderPair(sHeaderName,sHeaderValue));
	//m_requestHeaders.Set(sHeaderName, sHeaderValue);
	return true;
}

bool KCurl::addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue)
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	//m_requestHeaders.Add(sHeaderName, sHeaderValue);
	return true;
}

bool KCurl::delRequestHeader(const KString& sHeaderName)
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	m_requestHeaders.Remove(sHeaderKey);
	//m_requestHeaders.Remove(sHeaderName);
	return true;
}

bool KCurl::getCookie(const KString& sCookieName, KString& sCookieValue) const
{
	KString sCookieKey = sCookieName;
	sCookieKey.MakeLower();
	KCurl::KHeaderPair cookie = m_requestCookies.Get(sCookieKey);
	if (cookie.first.empty() || cookie.second.empty())
	{
		return false;
	}
	sCookieValue = cookie.second;
	return true;
}

bool KCurl::setCookie(const KString& sCookieName, const KString& sCookieValue)
{
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Set(sCookieKey, KHeaderPair(sCookieName,sCookieValue));
	return true;
/*
	KString& cookies = m_requestHeaders.Get(CookieHeader);
	if (cookies.empty()) // no cookies yet
	{
		return addCookie(sCookieName, sCookieValue);
	}
	size_t cookiePos = cookies.find(sCookieName);
	if (cookiePos != KString::npos) // cookie found
	{
		//swap value of matching cookie
		size_t equalPos = cookies.find("=", cookiePos);
		size_t endCookie = cookies.find(';', cookiePos);
		if (endCookie == KString::npos)
		{
			endCookie = cookies.size();
		}
		if (equalPos == KString::npos) // invalid cookie found
		{
			return false;
		}
		//KString oldValue = cookies.substr(equalPos+1, endCookie-equalPos-1);
		cookies.replace(equalPos+1, endCookie-equalPos-1, sCookieValue);
		return true;
	}
	else // cookie name not found
	{
		return addCookie(sCookieName, sCookieValue);
	}
	*/
}

bool KCurl::addCookie (const KString& sCookieName, const KString& sCookieValue)
{
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Add(sCookieKey, KHeaderPair(sCookieKey, sCookieValue));
	return true;
	/*
	// check if cookies already exist
	KString& cookies = m_requestHeaders.Get(CookieHeader);
	if (cookies.empty()) // no cookies yet
	{
		m_requestHeaders.Add(CookieHeader, sCookieName+"="+sCookieValue);
	}
	else // cookies already exist
	{
		cookies.Append((";"+sCookieName+"="+sCookieValue).c_str());
	}
	return true;
	*/
}

bool KCurl::delCookie(const KString& sCookieName, const KString& sCookieValue /*= ""*/)
{
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	m_requestCookies.Remove(sCookieName);
	return true;
	/*
	KString& cookies = m_requestHeaders.Get(CookieHeader);
	if (cookies.empty())
	{
		return false; // no cookies
	}

	size_t cookiePos = cookies.find(sCookieName);
	if (cookiePos == KString::npos)
	{
		return false; // specified cookie does not exist
	}

	size_t equalPos = cookies.find('=', cookiePos);
	size_t cookieEndPos = cookies.find(';', cookiePos);
	if (cookieEndPos == KString::npos)
	{
		cookieEndPos = cookies.length();
	}

	if (!sCookieValue.empty())
	{
		// if value is specified, may potentially have to look at several matching cookies
		do
		{
			equalPos = cookies.find('=', cookiePos); // won't change first iteration
			cookieEndPos = cookies.find(";", cookiePos); //
			if (cookieEndPos == KString::npos)
			{
				cookieEndPos = cookies.length();
			}
			size_t cookieValPos = cookies.find(sCookieValue, cookiePos);
			if (cookieValPos == KString::npos)
			{
				return false; // specified value for cookie couldn't be found
			}
			if (equalPos != KString::npos && cookieValPos == equalPos+1)
			{
				// delete this cookie
				cookies.erase(cookiePos, cookieEndPos-cookiePos);
				break;
			}

			cookiePos = cookies.find(sCookieName, cookieValPos);
		}	while (cookiePos != KString::npos);
	}

	cookies.erase(cookiePos, cookieEndPos-cookiePos);
	return true;
	*/
}

bool KCurl::addToResponseHeader(KString& sHeaderPart){ return true; }
bool KCurl::addToResponseBody  (KString& sBodyPart){ return true; }
bool KCurl::printResponseHeader  (){ return true; }

bool KCurl::serializeRequestHeader(KString& sCurlHeaders)
{
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
	return !sCurlHeaders.empty();
	/*
	for (auto iter = m_requestHeaders.begin(); iter != m_requestHeaders.end(); iter++)
	{
		sCurlHeaders += "-H '" + iter->first + ": " + iter->second + "' ";
	}
	return !sCurlHeaders.empty();
	*/
}

}
