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

#include "khttp_header.h"

namespace dekaf2 {
namespace detail {
namespace http {

//-----------------------------------------------------------------------------
KStringView KHeader::Parse(KStringView svBuffer, bool bParseCookies)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(m_bHeaderComplete))
	{
		return svBuffer;
	}

	bool bDeletePartialHeader;

	// Check if we're holding onto an incomplete header
	if (!m_sPartialHeader.empty())
	{
		m_sPartialHeader += svBuffer;
		svBuffer = m_sPartialHeader;
		bDeletePartialHeader = true;
	}
	else
	{
		bDeletePartialHeader = false;
	}

	// Process up to 1 header at a time
	while (!svBuffer.empty())
	{
		// edge case, blank \r\n line comes in by itself
		if (svBuffer.StartsWith("\r\n"))
		{
			if (!m_responseHeaders.empty())
			{
				// add it to the last header
				KString& lastHeaderValue = m_responseHeaders.at(m_responseHeaders.size()-1).second;
				lastHeaderValue += "\r\n";
				m_bHeaderComplete = true;
			}
			else
			{
				// some clients send an empty newline at start of the request - just ignore it, do not handle it
			}
			svBuffer.remove_prefix(2);
			break;
		}

		// edge case, blank \n line comes in by itself
		if (svBuffer[0] == '\n')
		{
			if (!m_responseHeaders.empty())
			{
				// add it to the last header
				KString& lastHeaderValue = m_responseHeaders.at(m_responseHeaders.size()-1).second;
				lastHeaderValue += '\n';
				m_bHeaderComplete = true;
				// Before body stream, output header if desired
			}
			else
			{
				// some clients send an empty newline at start of the request - just ignore it, do not handle it
			}
			svBuffer.remove_prefix(1);
			break;
		}

		auto colonPos   = svBuffer.find(':');
		auto lineEndPos = svBuffer.find('\n');
		auto nextEnd    = findEndOfHeader(svBuffer, lineEndPos);

		// not terminated by newline, save until newline is found
		if (nextEnd == KStringView::npos)
		{
			// TODO add check for over long input to defer DOS
			auto lineEndPos  = svBuffer.size();
			m_sPartialHeader = svBuffer.substr(0 , lineEndPos);
			bDeletePartialHeader = false;
			break;
		}

		// If first header line, try to parse HTTP status header
		if (m_responseHeaders.empty()) // no headers parsed yet, first full line not reached
		{
			// HTTP/1.1 200 OK
			// Always HTTP/<version> where version is 3 char
			// Status code always 3 digits
			// Status variable, but assume to end
			if (svBuffer.StartsWith("HTTP/"))
			{
				// Start positions of elements
				enum {
					versionPos    = 5,
					statusCodePos = 9,
					statusPos     = 13,
				};

				if (lineEndPos > statusPos)
				{
					// Grab status line given assumed positions
					m_sResponseVersion = svBuffer.substr(versionPos, 3);
					KStringView responseCode = svBuffer.substr(statusCodePos, 3);
					m_sResponseStatus = svBuffer.substr(statusPos, lineEndPos-statusPos);
					// TODO add conversions for string views
					KString hs(responseCode);
					m_iResponseStatusCode = kToUShort(hs);
				}
				// update beginning of line to process next line
				svBuffer.remove_prefix(lineEndPos + 1);
				continue;
			}
		}

		// valid header
		if (colonPos != KString::npos && lineEndPos != KString::npos && colonPos < lineEndPos)
		{
			lineEndPos = nextEnd;
			// iCurrentPos is beginning of line, lineEndPos is end of line, colonPos is end of header name for line
			KStringView sHeaderName = svBuffer.substr(0, colonPos);
			KStringView sHeaderValue = svBuffer.substr(colonPos + 1, lineEndPos-colonPos);
			addResponseHeader(sHeaderName, sHeaderValue, bParseCookies);
			svBuffer.remove_prefix(lineEndPos + 1);
			continue;
		}
		// invalid header
		else
		{
			lineEndPos = (nextEnd != KString::npos) ? nextEnd : svBuffer.size();
			KStringView sHeaderValue = svBuffer.substr(0, lineEndPos);
			addResponseHeader(svBrokenHeader, sHeaderValue, bParseCookies);
			svBuffer.remove_prefix(lineEndPos + 1);
			continue;
		}
	}

	if (bDeletePartialHeader)
	{
		m_sPartialHeader.clear();
	}

	return svBuffer;

} // addToResponseHeader


//-----------------------------------------------------------------------------
bool KHeader::Serialize(KOutStream& outStream)
//-----------------------------------------------------------------------------
{
	// status line
	if (m_iResponseStatusCode > 0)
	{
		// HTTP/1.1 200 OK
		outStream.Write("HTTP/");
		outStream.Write(m_sResponseVersion);
		outStream.Write(' ');
		outStream.Write(std::to_string(m_iResponseStatusCode));
		outStream.Write(' ');
		outStream.Write(m_sResponseStatus);
		outStream.WriteLine();
	}

	auto cookieIter = m_responseCookies.begin();
	for (const auto& iter : m_responseHeaders)
	{
		if (iter.first == svBrokenHeader)
		{
			outStream.WriteLine(iter.second);
		}
		else if (iter.first == KHeader::request_cookie)
		{
			outStream.Write(iter.first);
			outStream.Write(':');
			bool isFirst = true;
			for (;cookieIter != m_responseCookies.end(); ++cookieIter)
			{
				if (cookieIter->first.empty()) // empty means multiple cookie fields in header, separator
				{
					++cookieIter;
					break;
				}
				if (!isFirst)
				{
					outStream.Write(';');
				}
				outStream.Write(cookieIter->first);
				if (!cookieIter->second.empty())
				{
					outStream.Write('=');
					outStream.Write(cookieIter->second);
				}
				isFirst = false;
			}
		}
		else
		{
			outStream.Write(iter.first);
			outStream.Write(':');
			outStream.Write(iter.second);
		}
	}

	return true;

} // printResponseHeader


//-----------------------------------------------------------------------------
/// This method takes care of the logic for storing case sensitive data so
/// that it can be looked up in a case insensitive fashion.
/// It also splits up the cookie header.
bool KHeader::addResponseHeader(KStringView sHeaderName, KStringView sHeaderValue, bool bParseCookies)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(!bParseCookies || !kCaseEqualTrimLeft(sHeaderName, KHeader::request_cookie)))
	{
		// most headers
		m_responseHeaders.Add(sHeaderName, sHeaderValue);
	}
	else
	{
		// ALWAYS STARTS A NEW COOKIE HEADER, only full headers can end up here.
		if (!m_responseCookies.empty())
		{
			m_responseCookies.Add(KString{}, KString{});
		}

		m_responseHeaders.Add(std::move(sHeaderName), KString{}); // don't forget cookie header:

		// parse cookies and add them one by one.
		size_t semiPos  = sHeaderValue.find(';');
		size_t equalPos = sHeaderValue.find('=');
		size_t endPos   = sHeaderValue.find_last_not_of(" ;\r\n"); // account for extra whitespace at end of line
		endPos          = sHeaderValue.find(';', endPos);
		if (endPos == KStringView::npos)
		{
			endPos = sHeaderValue.size();
		}

		if (equalPos == KStringView::npos)
		{
			// not a single '=', no valid cookies
			kDebug(3, "({},{}}) end. Invalid cookie found.", sHeaderName, sHeaderValue);
			m_responseCookies.Add(sHeaderValue, KString{});// for serialization just put on what is there
			return false; // invalid cookie format
		}

		while (semiPos != KStringView::npos && equalPos != KStringView::npos) // account for n cookies
		{
			KStringView sCookieName  = sHeaderValue.substr(0, equalPos);

			if (semiPos != KStringView::npos && semiPos == endPos) // If this ends the cookies don't forget to take to EOL.
			{
				KStringView sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size() - equalPos - 1);
				m_responseCookies.Add(sCookieName, sCookieValue);
				return true;
			}
			else
			{
				KStringView sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos - 1);
				m_responseCookies.Add(sCookieName, sCookieValue);
			}

			// Update markers for next cookie
			sHeaderValue.remove_prefix(semiPos + 1);
			endPos  -= semiPos + 1;
			equalPos = sHeaderValue.find('=');
			semiPos  = sHeaderValue.find(';');
		}

		if (equalPos != KStringView::npos && semiPos == KStringView::npos) //does not end with ';'
		{
			KStringView sCookieName  = sHeaderValue.substr(0, equalPos);
			KStringView sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size() - equalPos); // don't forget newline
			m_responseCookies.Add(sCookieName, sCookieValue);
		}
		else if (semiPos == KStringView::npos && equalPos == KStringView::npos) //ends with invalid cookie
		{
			// TODO add rest of header
			KStringView sCookieName = sHeaderValue.substr(0, sHeaderValue.size());
			kDebug(3, "({},{}) end. Ended with invalid cookie", sHeaderName, sHeaderValue);
			m_responseCookies.Add(sCookieName, KString{});
			return true;
		}
		else
		{
			kDebug(3, "({},{}) end. Found malformed cookie.", sHeaderName, sHeaderValue);
			return false; // last cookie is malformed
		}
	}

	return true;

} // addResponseHeader

// Return current end of header line if not multiline header
// if multiline header return end pos of multi line header
// or NPOS if multiline is found that doesn't end in newline, indicating a junk end of stream, but not end of header.
//-----------------------------------------------------------------------------
size_t KHeader::findEndOfHeader(KStringView svHeaderPart, size_t lineEndPos)
//-----------------------------------------------------------------------------
{
	if (lineEndPos == KStringView::npos)
	{
		return KStringView::npos;
	}
	// Check for continuation line starting with whitespace
	else if (svHeaderPart.size() > lineEndPos)
	{
		// If not last line, then keep looking!
		do
		{
			if (svHeaderPart[lineEndPos+1] == ' ') // just looking to find end of continuation line
			{
				size_t newEnd = svHeaderPart.find('\n', lineEndPos+1);
				if (newEnd != KStringView::npos)
				{
					lineEndPos = newEnd;
				}
				else
				{
					//we got junk
					kDebug(3, "({},{}) end. Found bad header.", svHeaderPart, lineEndPos);
					return KStringView::npos;
				}
			}
			else
			{
				break;
			}
		} while (true);
	}

	return lineEndPos;

} // findEndOfHeader

//-----------------------------------------------------------------------------
void KHeader::clear()
//-----------------------------------------------------------------------------
{
	m_responseHeaders.clear();
	m_responseCookies.clear();
	m_sPartialHeader.clear();
	m_sResponseVersion.clear();
	m_sResponseStatus.clear();
	m_iResponseStatusCode = 0;
	m_bHeaderComplete = false;

} // clear

//-----------------------------------------------------------------------------
KStringView KHeader::Get(KCaseStringView sv) const
//-----------------------------------------------------------------------------
{
	KStringView svv(m_responseHeaders.Get(sv));
	kTrim(svv);
	return svv;
}

// for some strange reason gcc wants these repeated definitions
// once one puts a constexpr variable into a class. As long as
// they stay outside of a class, the linker does not complain about
// missing references, but once they are inside a class it does.

constexpr KStringView KHeader::AUTHORIZATION;
constexpr KStringView KHeader::HOST;
constexpr KStringView KHeader::HOST_OVERRIDE;
constexpr KStringView KHeader::USER_AGENT;
constexpr KStringView KHeader::CONTENT_TYPE;
constexpr KStringView KHeader::CONTENT_LENGTH;
constexpr KStringView KHeader::CONTENT_MD5;
constexpr KStringView KHeader::CONNECTION;
constexpr KStringView KHeader::LOCATION;
constexpr KStringView KHeader::REQUEST_COOKIE;
constexpr KStringView KHeader::RESPONSE_COOKIE;
constexpr KStringView KHeader::ACCEPT_ENCODING;
constexpr KStringView KHeader::TRANSFER_ENCODING;
constexpr KStringView KHeader::VARY;
constexpr KStringView KHeader::REFERER;
constexpr KStringView KHeader::ACCEPT;
constexpr KStringView KHeader::X_FORWARDED_FOR;

constexpr KStringView KHeader::authorization;
constexpr KStringView KHeader::host;
constexpr KStringView KHeader::host_override;
constexpr KStringView KHeader::user_agent;
constexpr KStringView KHeader::content_type;
constexpr KStringView KHeader::content_length;
constexpr KStringView KHeader::content_md5;
constexpr KStringView KHeader::connection;
constexpr KStringView KHeader::location;
constexpr KStringView KHeader::request_cookie;
constexpr KStringView KHeader::response_cookie;
constexpr KStringView KHeader::accept_encoding;
constexpr KStringView KHeader::transfer_encoding;
constexpr KStringView KHeader::vary;
constexpr KStringView KHeader::referer;
constexpr KStringView KHeader::accept;
constexpr KStringView KHeader::x_forwarded_for;

constexpr KStringView KHeader::svBrokenHeader;

} // end of namespace http
} // end of namespace detail
} // end of namespace dekaf2
