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

#include "kwebio.h"
#include "klog.h"

#include "kstringutils.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KWebIO::addToResponseHeader(KString& sHeaderPart)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KWebIO::addToResponseHeader({}) start", sHeaderPart);
	// Garbage in garbage out!
	size_t iCurrentPos = 0;
	// Check if we're holding onto an incomplete header
	if (!m_sPartialHeader.empty())
	{
		sHeaderPart = std::move(m_sPartialHeader) + sHeaderPart; // append new content to previous partial content
		m_sPartialHeader.clear(); // empty partial header
	}
	// Process up to 1 header at a time
	do
	{
		size_t colonPos = sHeaderPart.find(':', iCurrentPos);
		size_t lineEndPos = sHeaderPart.find('\n', iCurrentPos);
		size_t nextEnd = findEndOfHeader(sHeaderPart, lineEndPos);
		KString sHeaderName, sHeaderValue;

		// edge case, blank (\r)\n line comes in by itself
		if (sHeaderPart.StartsWith("\r\n") || sHeaderPart.StartsWith("\n"))
		{
			// add it to the last header
			KString& lastHeaderValue = m_responseHeaders.at(m_responseHeaders.size()-1).second.second;
			lastHeaderValue += (sHeaderPart.StartsWith("\r\n")) ? "\r\n" :"\n";
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
				// Start positions of elements
				size_t versionPos = 5;
				size_t statusCodePos = 9;
				size_t statusPos = 13;
				// Grab status line given assumed positions
				m_sResponseVersion = sHeaderPart.substr(versionPos, 3);
				KString responseCode = sHeaderPart.substr(statusCodePos, 3);
				m_sResponseStatus = sHeaderPart.substr(statusPos, lineEndPos-statusPos);
				m_iResponseStatusCode = KToUShort(responseCode);
				//update beginning of line to process next line
				iCurrentPos = lineEndPos+1;
				continue;
			}
		}
		// last header, try to grab extra newline as part of last header
		if (isLastHeader(sHeaderPart, nextEnd))
		{
			nextEnd = (sHeaderPart.find('\r') == KString::npos) ? nextEnd+1 : nextEnd+2;
			if (colonPos != KString::npos) // valid
			{
				sHeaderName = sHeaderPart.substr(iCurrentPos, colonPos-iCurrentPos);
				sHeaderValue = sHeaderPart.substr(colonPos + 1, nextEnd-colonPos);
			}
			else // invalid
			{
				sHeaderName = sGarbageHeader;
				lineEndPos = (nextEnd != KString::npos) ? nextEnd : sHeaderPart.size() ;
				sHeaderValue = sHeaderPart.substr(iCurrentPos , lineEndPos-iCurrentPos);
			}
			// Offload logic of adding so lookup is case insensitve while data is not
			addResponseHeader(std::move(sHeaderName), std::move(sHeaderValue));
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
		//not last header, valid
		else if (colonPos != KString::npos && lineEndPos != KString::npos && colonPos < lineEndPos)
		{
			lineEndPos = nextEnd;
			// iCurrentPos is beginning of line, lineEndPos is end of line, colonPos is end of header name for line
			sHeaderName = sHeaderPart.substr(iCurrentPos, colonPos-iCurrentPos);
			sHeaderValue = sHeaderPart.substr(colonPos + 1, lineEndPos-colonPos);
			addResponseHeader(std::move(sHeaderName), std::move(sHeaderValue));
			iCurrentPos = lineEndPos+1; //update beginning of line
			continue;
		}
		//not last header, invalid
		else
		{
			lineEndPos = (nextEnd != KString::npos) ? nextEnd : sHeaderPart.size();
			sHeaderValue = sHeaderPart.substr(iCurrentPos, lineEndPos-iCurrentPos);
			addResponseHeader(std::move(sGarbageHeader), std::move(sHeaderValue));
			iCurrentPos = lineEndPos+1;//update beginning of line
			continue;
		}
	} while (iCurrentPos < sHeaderPart.size()); // until end of given content is reached

	KLog().debug(3, "KWebIO::addToResponseHeader({}}) end", sHeaderPart);
	return true;

} // addToResponseHeader

//-----------------------------------------------------------------------------
bool KWebIO::addToResponseBody(KString& sBodyPart)
//-----------------------------------------------------------------------------
{
	if (m_bEchoBody)
	{
		m_outStream.Write(sBodyPart);
	}
	return true;

} // addToResponseBody

//-----------------------------------------------------------------------------
bool KWebIO::printResponseHeader()
//-----------------------------------------------------------------------------
{
	// Dont forget status line
	if (m_iResponseStatusCode > 0)
	{
		//HTTP/1.1 200 OK
		m_outStream.WriteLine("HTTP/" + m_sResponseVersion + ' ' + std::to_string(m_iResponseStatusCode) + ' ' + m_sResponseStatus);
	}
	auto cookieIter = m_responseCookies.begin();
	for (const auto& iter : m_responseHeaders)
	{
		if (iter.first == sGarbageHeader)
		{
			m_outStream.WriteLine(iter.second.second);
		}
		else if (iter.first == CookieHeader)
		{
			m_outStream.Write(iter.second.first + ':');
			bool isFirst = true;
			for (;cookieIter != m_responseCookies.end(); ++cookieIter)
			{
				if (cookieIter->second.first.empty()) // empty means multiple cookie fields in header, separator
				{
					++cookieIter;
					break;
				}
				if (!isFirst)
				{
					KString semi(";");
					m_outStream.Write(semi);
				}
				KString sFirst = cookieIter->second.first;
				KString sSecond;
				if (!cookieIter->second.second.empty())
				{
					sSecond = '=' ;
					sSecond += cookieIter->second.second;
				}
				m_outStream.Write(sFirst + sSecond);
				isFirst = false;
			}
		}
		else
		{
			m_outStream.Write(iter.second.first + ':' + iter.second.second);
		}
	}
	return true;

} // printResponseHeader

//-----------------------------------------------------------------------------
const KString& KWebIO::getResponseHeader(const KString& sHeaderName) const
//-----------------------------------------------------------------------------
{
	KString sHeaderKey(sHeaderName);
	sHeaderKey.MakeLower().Trim();
	const KHeaderPair& header(m_responseHeaders.Get(sHeaderKey));
	const KString& sHeaderVal = header.second;
	return sHeaderVal;

} // getResponseHeader

//-----------------------------------------------------------------------------
const KString& KWebIO::getResponseCookie(const KString& sCookieName) //const
//-----------------------------------------------------------------------------
{
	KString sCookieKey(sCookieName);
	sCookieKey.MakeLower().Trim();
	return m_responseCookies.Get(sCookieKey).second;

} // getResponseCookie

//-----------------------------------------------------------------------------
/// This method takes care of the logic for storing case sensitive data so
/// that it can be looked up in a case insensitive fashion.
bool KWebIO::addResponseHeader(const KString&& sHeaderName, const KString&& sHeaderValue)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KWebIO::addResponseHeader({},{}}) start", sHeaderName, sHeaderValue);
	KString sHeaderKey(sHeaderName.ToLower());
	sHeaderKey.Trim();
	if (sHeaderKey == CookieHeader)
	{
		// ALWAYS STARTS A NEW COOKIE HEADER, only full headers can end up here.
		if (!m_responseCookies.empty())
		{
			m_responseCookies.Add(KString{}, KHeaderPair(KString{},KString{}));
		}
		m_responseHeaders.Add(std::move(sHeaderKey), KHeaderPair(std::move(sHeaderName), KString{})); // don't forget cookie header:

		// parse cookies and add them one by one.
		size_t startPos = 0;
		size_t equalPos = sHeaderValue.find('=', startPos);
		size_t semiPos = sHeaderValue.find(';', startPos);
		size_t endPos = sHeaderValue.find_last_not_of(" ;\r\n"); // account for extra whitespace at end of line
		endPos = sHeaderValue.find(';', endPos);

		KString sCookieName = sHeaderValue.substr(startPos, equalPos - startPos);
		KString sCookieKey(sCookieName.ToLower());
		sCookieKey.Trim();
		KString sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos -1);

		if (equalPos == KString::npos) // not a single '=', no valid cookies
		{
			KLog().debug(3, "KWebIO::addResponseHeader({},{}}) end. Invalid cookie found.", sHeaderName, sHeaderValue);
			m_responseCookies.Add(std::move(sCookieKey), KHeaderPair(std::move(sHeaderValue), KString{}));// for serialization just put on what is there
			return false; // invalid cookie format
		}

		while (semiPos != KString::npos && equalPos != KString::npos) // account for n cookies
		{
			sCookieName = sHeaderValue.substr(startPos, equalPos - startPos);
			sCookieKey = KString(sCookieName.ToLower()); // need a new instance of this KString
			sCookieKey.Trim();
			if (semiPos != KString::npos && semiPos == endPos) // If this ends the cookies don't forget to take to EOL.
			{
				sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size()-equalPos -1);
				KLog().debug(3, "KWebIO::addResponseHeader({},{}) end", sHeaderName, sHeaderValue);
				m_responseCookies.Add(std::move(sCookieKey), KHeaderPair(sCookieName, sCookieValue));
				return true;
			}
			else
			{
				sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos -1);
				m_responseCookies.Add(std::move(sCookieKey), KHeaderPair(std::move(sCookieName), std::move(sCookieValue)));
			}
			// Update markets for next cookie
			startPos = semiPos+1;
			equalPos = sHeaderValue.find('=', startPos);
			semiPos = sHeaderValue.find(';', startPos);
		}

		if (equalPos != KString::npos && semiPos == KString::npos) //does not end with ';'
		{
			sCookieName = sHeaderValue.substr(startPos, equalPos - startPos);
			sCookieKey = KString(sCookieName); // need a new instance of this KString
			sCookieKey.MakeLower().Trim();
			sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size() - equalPos); // don't forget newline
			m_responseCookies.Add(std::move(sCookieKey), KHeaderPair(std::move(sCookieName), std::move(sCookieValue)));
		}
		else if (semiPos == KString::npos && equalPos == KString::npos) //ends with invalid cookie
		{
			// TODO add rest of header
			sCookieName = sHeaderValue.substr(startPos, sHeaderValue.size() - startPos);
			sCookieKey = KString(sCookieName); // need a new instance of this KString
			sCookieKey.MakeLower().Trim();
			KLog().debug(3, "KWebIO::addResponseHeader({},{}) end. Ended with invalid cookie", sHeaderName, sHeaderValue);
			m_responseCookies.Add(std::move(sCookieKey), KHeaderPair(std::move(sCookieName), KString{}));
			return true;
		}
		else
		{
			KLog().debug(3, "KWebIO::addResponseHeader({},{}) end. Found malformed cookie.", sHeaderName, sHeaderValue);
			return false; // last cookie is malformed
		}
	}
	else
	{
		// most headers
		m_responseHeaders.Add(std::move(sHeaderKey), KHeaderPair(std::move(sHeaderName), std::move(sHeaderValue)));
	}
	KLog().debug(3, "KWebIO::addResponseHeader({},{}) end. Non cookie header added.", sHeaderName, sHeaderValue);
	return true;

} // addResponseHeader

//-----------------------------------------------------------------------------
bool KWebIO::isLastHeader(const KString& sHeaderPart, size_t lineEndPos)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KWebIO::isLastHeader({},{}) start", sHeaderPart, lineEndPos);
	// Check if there is more to the String
	if (sHeaderPart.size() > lineEndPos)
	{
		// Check if last line
		char nextChar = sHeaderPart[lineEndPos+1];
		if (nextChar == '\n')
		{
			KLog().debug(3, "KWebIO::isLastHeader{},{}) end. Last UNIX header found.", sHeaderPart, lineEndPos);
			return true;
		}
		//Sometimes the last line is \r\n ... winblows
		else if (nextChar == '\r')//
		{
			if (sHeaderPart.size() > lineEndPos + 1)
			{
				if (sHeaderPart[lineEndPos+2] == '\n')
				{
					KLog().debug(3, "KWebIO::isLastHeader({},{}) end. Last Windows Header Found", sHeaderPart, lineEndPos);
					return true;
				}
			}
		}
	}
	KLog().debug(3, "KWebIO::isLastHeader({},{}) end. Last header NOT found.", sHeaderPart, sHeaderPart);
	return false;

} // isLastHeader

// Return current end of header line if not multiline header
// if multiline header return end pos of multi line header
// or NPOS if multiline is found that doesn't end in newline, indicating a junk end of stream, but not end of header.
//-----------------------------------------------------------------------------
size_t KWebIO::findEndOfHeader(const KString& sHeaderPart, size_t lineEndPos)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KWebIO::findEndOfHeader({},{}) start", sHeaderPart, lineEndPos);
	if (lineEndPos == KString::npos)
	{
		return KString::npos;
	}
	// Check for continuation line starting with whitespace
	else if (sHeaderPart.size() > lineEndPos)
	{
		// If not last line, then keep looking!
		do
		{
			if (sHeaderPart[lineEndPos+1] == ' ') // just looking to find end of continuation line
			{
				size_t newEnd = sHeaderPart.find('\n', lineEndPos+1);
				if (newEnd != KString::npos)
				{
					lineEndPos = newEnd;
				}
				else
				{
					//we got junk
					KLog().debug(3, "KWebIO::findEndOfHeader({},{}) end. Found bad header.", sHeaderPart, lineEndPos);
					return KString::npos;
				}
			}
			else
			{
				break;
			}
		} while (true);
	}
	KLog().debug(3, "KWebIO::findEndOfHeader({},{}) end. No problems.", sHeaderPart, lineEndPos);
	return lineEndPos;
} // findEndOfHeader

} // endnamespace dekaf2
