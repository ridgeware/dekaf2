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

#include "kwebio.h"

#include <iostream>

namespace dekaf2 {

KWebIO::KWebIO() : KCurl ()
{
}

KWebIO::KWebIO(const KString& requestURL, bool bEchoHeader /*=false*/, bool bEchoBody /*=false*/)
    : KCurl(requestURL, bEchoHeader, bEchoBody)
{
}

KWebIO::~KWebIO()
{
}

// placeholder atoi() function
uint16_t katoi(KString& num)
{
	int res = 0; // Initialize result

	// Iterate through all characters of input string and
	for (int i = 0; i < num.size(); i++)
	{
		res = res*10 + num[i] - '0';
	}
	// return result.
	return static_cast<uint16_t>(res);
}

bool KWebIO::addToResponseHeader(KString& sHeaderPart)
{
	// JUST SWAP OUT m_responseHeaders.Add(sHeaderName, sHeaderValue);
	// FOR KWebIO::addResponseHeader(sHeaderName,sHeaderValue);
	// And put cookie info here

	// Garbage in garbase out!
	// whitespace at beginning of line means continuation.
	size_t iCurrentPos = 0;
	// Check if we're holding onto an incomplete header
	if (!m_sPartialHeader.empty())
	{
		//std::cout << "partial header concat" << std::endl;
		sHeaderPart = m_sPartialHeader + sHeaderPart; // append new content to previous partial content
		m_sPartialHeader.clear(); // empty partial header
	}
	// Process up to 1 header at a time
	do
	{
		size_t colonPos = sHeaderPart.find(":", iCurrentPos);
		size_t lineEndPos = sHeaderPart.find("\n", iCurrentPos);
		size_t nextEnd = findEndOfHeader(sHeaderPart, lineEndPos);
		KString sHeaderName, sHeaderValue;

		// edge case, blank \r\n line comes in by itself
		// old mac style newlines: "\n\r"
		if (sHeaderPart.StartsWith("\r\n"))
		{
			// add it to the last header
			KString& lastHeaderValue = m_responseHeaders[m_responseHeaders.size()].second.second;
			//KString& lastHeaderValue = m_responseHeaders[m_responseHeaders.size()].second;
			lastHeaderValue += "\r\n";
			m_bHeaderComplete = true;
			// Before body stream, output header if desired
			if (m_bEchoHeader)
			{
				printResponseHeader();
			}
			// What if we have content beyond header in stream?
			if (lineEndPos < sHeaderPart.size())
			{
				KString leftOvers = sHeaderPart.substr(lineEndPos + 1, sHeaderPart.size() -lineEndPos-1);
				if (!leftOvers.empty())
				{
					addToResponseBody(leftOvers);
				}
			}
			break;
		}

		// not terminated by newline, save until newline is found
		if (nextEnd == KString::npos)
		{
			lineEndPos = sHeaderPart.size();
			m_sPartialHeader += sHeaderPart.substr(iCurrentPos , lineEndPos-iCurrentPos);
			break;
		}

		// If first header line, try to parse HTTP status header
		if (m_responseHeaders.empty()) // no headers parsed yet, first full line not reached
		{
			// HTTP/1.1 200 OK
			// Always HTTP/<version> where version is 3 char
			// Status code always 3 digits
			// Status variable, but assume to end
			if (sHeaderPart.StartsWith("HTTP/"))
			{
				size_t versionPos = 5;
				size_t statusCodePos = 9;
				size_t statusPos = 13;
				m_sResponseVersion = sHeaderPart.substr(versionPos, 3);
				KString responseCode = sHeaderPart.substr(statusCodePos, 3);
				m_sResponseStatus = sHeaderPart.substr(statusPos, lineEndPos-statusPos);
				//m_iResponseStatusCode = responseCode.
				// TODO implement int KString::toInt();
				m_iResponseStatusCode = katoi(responseCode);
				iCurrentPos = lineEndPos+1;//update beginning of line
				continue;
			}
		}
		// last line
		if (isLastHeader(sHeaderPart, nextEnd))
		{
			if (colonPos != KString::npos) // valid
			{
				sHeaderName = sHeaderPart.substr(iCurrentPos, colonPos-iCurrentPos);
				//sHeaderValue = sHeaderPart.substr(colonPos + 1, nextEnd-colonPos-1);
				sHeaderValue = sHeaderPart.substr(colonPos + 1, nextEnd-colonPos);
			}
			else // invalid
			{
				sHeaderName = sGarbageHeader;
				lineEndPos = (nextEnd != KString::npos) ? nextEnd : sHeaderPart.size() ;
				sHeaderValue = sHeaderPart.substr(iCurrentPos , lineEndPos-iCurrentPos);
			}
			//sHeaderValue.Trim();
			//m_responseHeaders.Add(sHeaderName, sHeaderValue);
			addResponseHeader(sHeaderName, sHeaderValue);
			m_bHeaderComplete = true;
			// Before body stream, output header if desired
			if (m_bEchoHeader)
			{
				printResponseHeader();
			}
			// What if we have content beyond header in stream?
			if (lineEndPos < sHeaderPart.size())
			{
				KString leftOvers = sHeaderPart.substr(lineEndPos + 1, sHeaderPart.size() -lineEndPos-1);
				if (!leftOvers.empty())
				{
					addToResponseBody(leftOvers);
				}
			}
			break;
		}
		// not last header, valid
		else if (colonPos != KString::npos && lineEndPos != KString::npos)
		{
			lineEndPos = nextEnd;
			// iCurrentPos is beginning of line, lineEndPos is end of line, colonPos is end of header name for line
			sHeaderName = sHeaderPart.substr(iCurrentPos, colonPos-iCurrentPos);
			//sHeaderValue = sHeaderPart.substr(colonPos + 1, lineEndPos-colonPos-1);
			sHeaderValue = sHeaderPart.substr(colonPos + 1, lineEndPos-colonPos);
			//sHeaderValue.Trim();
			//m_responseHeaders.Add(sHeaderName, sHeaderValue);
			addResponseHeader(sHeaderName, sHeaderValue);
			iCurrentPos = lineEndPos+1;//update beginning of line
			continue;
		}
		// not last header, invalid
		else
		{
			lineEndPos = (nextEnd != KString::npos) ? nextEnd : sHeaderPart.size() ;
			sHeaderValue = sHeaderPart.substr(iCurrentPos, lineEndPos-iCurrentPos);
			//sHeaderValue.Trim();
			//m_responseHeaders.Add(sGarbageHeader, sHeaderValue);
			addResponseHeader(sGarbageHeader, sHeaderValue);
			iCurrentPos = lineEndPos+1;//update beginning of line
			continue;
		}
	} while (iCurrentPos < sHeaderPart.size()); // until end of given content is reached

	return true;
}

bool KWebIO::addToResponseBody(KString& sBodyPart)
{
	// m_sResponseBody.push_back(KString(sBodyPart)); // if wanted to save
	if (m_bEchoBody)
	{
		std::cout << sBodyPart;
	}
	return true;
}

bool KWebIO::printResponseHeader()
{
	// Dont forget status line
	if (m_iResponseStatusCode > 0)
	{
		//HTTP/1.1 200 OK
		std::cout << "HTTP/" << m_sResponseVersion << " " << m_iResponseStatusCode << " " << m_sResponseStatus << std::endl;
	}
	auto cookieIter = m_responseCookies.begin();
	for (const auto& iter : m_responseHeaders)
	{
		if (iter.first == sGarbageHeader)
		{
			std::cout << iter.second.second << std::endl;
		}
		else if (iter.first == CookieHeader)
		{
			std::cout << iter.second.first << ":";
			bool isFirst = true;
			for (;cookieIter != m_responseCookies.end(); cookieIter++)
			{
				if (cookieIter->first.empty()) // empty means multiple cookie fields in header, separator
				{
					cookieIter++;
					break;
				}
				if (!isFirst)
				{
					std::cout << ";";
				}
				std::cout << cookieIter->second.first << "=" << cookieIter->second.second;
				isFirst = false;
			}
		}
		else
		{
			std::cout << iter.second.first << ":" << iter.second.second;// << std::endl;
		}
	}
	/*
	for (const auto& iter : m_responseHeaders)
	{
		if (iter.first == sGarbageHeader)
		{
			std::cout << iter.second << std::endl;
		}
		else
		{
			std::cout << iter.first << ":" << iter.second << std::endl;
		}
	}
	*/
	return true;
}

const KCurl::KHeader&  KWebIO::getResponseHeaders() const
{
	return m_responseHeaders;
}

const KString& KWebIO::getResponseHeader(const KString& sHeaderName) const
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower();
	return m_responseHeaders.Get(sHeaderKey).second;
	//return m_responseHeaders.Get(sHeaderName);
}

const KCurl::KHeader&  KWebIO::getResponseCookies() const
{
	return m_responseCookies;
}

const KString& KWebIO::getResponseCookie(const KString& sCookieName) //const
{
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower();
	return m_responseCookies.Get(sCookieKey).second;
	/*
	KString& cookies = m_responseHeaders.Get(CookieHeader);
	if (cookies.empty())
	{
		return ""; // there are no cookies at all
	}
	size_t startPos = cookies.find(sCookieName);
	if (startPos == KString::npos)
	{
		return ""; // no cookie matches
	}
	size_t equalPos = cookies.find('=', startPos);
	size_t endPos = cookies.find(';', startPos);
	if (endPos == KString::npos)
	{
		endPos = cookies.size();
	}
	if (equalPos == KString::npos)
	{
		return ""; //matching cookie format is invalid
	}
	return cookies.substr(equalPos+1, endPos-equalPos-1);
	*/
}

bool KWebIO::addResponseHeader(const KString& sHeaderName, const KString& sHeaderValue)
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower().Trim();
	if (sHeaderKey == CookieHeader)
	{
		// do cookies logic
		// ALWAYS STARTS A NEW COOKIE HEADER, only full headers can end up here.
		if (!m_responseCookies.empty())
		{
			m_responseCookies.Add("", KHeaderPair("",""));
		}
		// don't forget cookie header:
		m_responseHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, ""));
		// TODO what if "Cookie: \n"?

		// parse cookies and add them one by one.
		//name=value;name2=value2;name3=value3...
		// return false if cookies don't have proper format
		// i.e. "Cookies: name_no_value"
		size_t startPos = 0;
		size_t equalPos = sHeaderValue.find("=", startPos);
		size_t semiPos = sHeaderValue.find(";", startPos);
		if (equalPos == KString::npos)
		{
			return false; // invalid cookie format
		}
		KString sCookieName = sHeaderValue.substr(startPos, equalPos - startPos);
		KString sCookieKey(sCookieName);
		sCookieKey.MakeLower().Trim();
		KString sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos -1);
		while (semiPos != KString::npos && equalPos != KString::npos)
		{
			sCookieName = sHeaderValue.substr(startPos, equalPos - startPos);
			sCookieKey = KString(sCookieName); // need a new instance of this KString
			sCookieKey.MakeLower().Trim();
			KString sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos -1);

			m_responseCookies.Add(sCookieKey, KHeaderPair(sCookieName, sCookieValue));
			startPos = semiPos+1;
			equalPos = sHeaderValue.find("=", startPos);
			semiPos = sHeaderValue.find(";", startPos);
		}
		//name=value;name2=value2;name3=value3;
		//name=value;name2=value2;name3=value3
		if (equalPos != KString::npos && semiPos == KString::npos) //does not end with ';'
		{
			sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size()); // don't forget newline
			m_responseCookies.Add(sCookieKey, KHeaderPair(sCookieName, sCookieValue));
		}
		else if (semiPos == KString::npos && equalPos == KString::npos) //ends with ';'
		{
			// TODO need to add newline...
			return true;
		}
		else
		{
			return false; // last cookie is malformed
		}
	}
	else
	{
		// most headers
		m_responseHeaders.Add(sHeaderKey, KHeaderPair(sHeaderName, sHeaderValue));
	}
	return true;
}

bool KWebIO::isLastHeader(KString& sHeaderPart, size_t lineEndPos)
{
	// Check if there is more to the String
	if (sHeaderPart.size() > lineEndPos)
	{
		// Check if last line
		char nextChar = sHeaderPart[lineEndPos+1];
		if (nextChar == '\n')
		{
			return true;
		}
		//Sometimes the last line is \r\n ... winblows
		else if (nextChar == '\r')//
		{
			//std::cout << "identified windows line ending" << std::endl;
			if (sHeaderPart.size() > lineEndPos + 1)
			{
				if (sHeaderPart[lineEndPos+2] == '\n')
				{
					return true;
				}
			}
		}
	}
	return false;
}

// Return current end of header line if not multiline header
// if multiline header return end pos of multi line header
// or NPOS if multiline is found that doesn't end in newline, indicating a junk end of stream, but not end of header.
size_t KWebIO::findEndOfHeader(KString& sHeaderPart, size_t lineEndPos)
{
	if (lineEndPos == KString::npos)
	{
		return KString::npos;
	}
	// Check for continuation line starting with whitespace
	else if (sHeaderPart.size() > lineEndPos)
	{
		//const char* nextChar = sHeaderPart.substr(lineEndPos+1, 1).c_str();
		// If not last line, then keep looking!
		do
		{
			//nextChar = sHeaderPart.substr(lineEndPos+1, 1).c_str();
			if (std::isspace(sHeaderPart[lineEndPos+1]))
			{
				size_t newEnd = sHeaderPart.find("\n", lineEndPos+1);
				if (newEnd != KString::npos)
				{
					lineEndPos = newEnd;
				}
				else
				{
					//we got junk
					return KString::npos;
				}
			}
			else
			{
				break;
			}
		} while (true);
	}
	return lineEndPos;
}

} // end namespace dekaf2
