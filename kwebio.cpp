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
#include "khttp.h"
#include "kstringutils.h"

namespace dekaf2 {

constexpr KStringView KWebIO::svBrokenHeader;

//-----------------------------------------------------------------------------
KStringView KWebIO::Parse(KStringView svBuffer, bool bParseCookies)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(m_parseState == headerFinished))
	{
		return svBuffer;
	}

	kDebug(3, "({}) start", svBuffer);
	size_t lineEndPos = KStringView::npos;
	size_t nextEnd = KStringView::npos;

	// This way we can know if are looking from the start of svBuffer
	// Or continuing if looking for EOL
	bool bFresh = true;

	// Look at each character once when parsing. Unless \n is found as second letter
	// in the svBuffer. Then the char right before it is checked for \r
	// find... methods use SSE optimizations.
	while (!svBuffer.empty())
	{
		// Calculate this once at the start of every loop, required for logic
		size_t leftoverSize = m_sPartialHeader.size();
		switch(m_parseState)
		{
			case KParseState::headerFinished:
				return svBuffer;
				break;
			//=================================================================
			case KParseState::needStatus:
			//=================================================================
				// Handle if first line is not status line
				if (svBuffer[0] == '\n' || svBuffer.StartsWith("\r\n"))
				{}
				else
				{
					if (svBuffer.StartsWith("HTTP/"))
					{
						// Find end of Status Line
						lineEndPos = svBuffer.find('\n', 5);
						if (DEKAF2_UNLIKELY(lineEndPos == KStringView::npos))
						{
							m_sPartialHeader = svBuffer;
						}

						// HTTP/1.1 200 OK
						// Always HTTP/<version> where version is 3 char
						// Status code always 3 digits
						// Status variable, but assume to end
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
						svBuffer.remove_prefix(lineEndPos - leftoverSize + 1);
						m_sPartialHeader.clear();

					}
					m_parseState = needColon;
				}
				break;
			//=================================================================
			case KParseState::needColon:
			//=================================================================
				// edge case, blank \n or \r\n line comes in by itself
				m_iColonPos = svBuffer.find_first_of(":\n");
				if (DEKAF2_UNLIKELY(m_iColonPos != KStringView::npos && svBuffer[m_iColonPos] == '\n'))
				{
					if (m_iColonPos == 0) // starts with \n
					{
						if (leftoverSize == 0)
						{
							// add it to the last header
							KString& lastHeaderValue = m_responseHeaders.at(m_responseHeaders.size()-1).second;
							lastHeaderValue += '\n';
							m_parseState = headerFinished;
							svBuffer.remove_prefix(1);
							return svBuffer;
						}
					}
					else if (m_iColonPos == 1 && svBuffer[0] == '\r') // starts with \r\n
					{
						if (leftoverSize == 0)
						{
							// add it to the last header
							KString& lastHeaderValue = m_responseHeaders.at(m_responseHeaders.size()-1).second;
							lastHeaderValue += "\r\n";
							m_parseState = headerFinished;
							svBuffer.remove_prefix(2);
							return svBuffer;
						}
					}

					//Garbage header, no colon
					nextEnd = findEndOfHeader(svBuffer, m_iColonPos);
					if (nextEnd == KStringView::npos)
					{
						// Continuation line must be in value, not name
						m_sPartialHeader += svBuffer;
						m_parseState = needEOL;
						return svBuffer;
					}

					lineEndPos = nextEnd += leftoverSize;
					m_iColonPos = KStringView::npos;
					addResponseHeader(svBuffer, m_iColonPos, lineEndPos, bParseCookies);
					svBuffer.remove_prefix(lineEndPos - leftoverSize + 1);
					m_sPartialHeader.clear();// remove all stored garbage, already saved
				}
				else if (DEKAF2_LIKELY(m_iColonPos != KStringView::npos))
				{
					m_iColonPos += leftoverSize;
					m_parseState = needEOL;
					break;
				}
				else
				{
					// save for later
					m_sPartialHeader += svBuffer;
					return m_sPartialHeader;
				}
				break;

			//=================================================================
			case KParseState::needEOL:
			//=================================================================
				if (bFresh)	lineEndPos = svBuffer.find('\n');
				else lineEndPos = svBuffer.find('\n', m_iColonPos - leftoverSize);
				if (lineEndPos == (svBuffer.size() - 1))
				{
					// Found EOL, can't verify no continuation line because data stops
					m_parseState = possibleEOL;
					m_sPartialHeader += svBuffer;
					return svBuffer;
				}
				nextEnd = findEndOfHeader(svBuffer, lineEndPos);
				if (nextEnd != KStringView::npos)
				{
					lineEndPos = nextEnd;
					lineEndPos += leftoverSize;
					// Add header
					addResponseHeader(svBuffer, m_iColonPos, lineEndPos, bParseCookies);
					// Update state and remove what is processed
					m_parseState = needColon;
					svBuffer.remove_prefix(lineEndPos - leftoverSize + 1);
					m_sPartialHeader.clear();
					break;
				}
				else
				{
					m_sPartialHeader += svBuffer;
					return svBuffer;
				}
				break;

			//=================================================================
			case KParseState::possibleEOL:
			//=================================================================
				if (svBuffer[0] != ' ' || svBuffer[0] == '\t') // space or HT mean continuation
				{
					// Add m_sPartialHeader as a header
					lineEndPos = leftoverSize - 1;
					addResponseHeader(svBuffer, m_iColonPos, lineEndPos, bParseCookies);
					m_parseState = needColon;
					svBuffer.remove_prefix(lineEndPos - leftoverSize + 1); // only need to remove from underlying KString
					m_sPartialHeader.clear();
					break;
				}
				lineEndPos = svBuffer.find('\n');
				if (lineEndPos == (svBuffer.size() - 1))
				{
					// Found EOL, can't verify no continuation line because data stops
					m_sPartialHeader += svBuffer;
					return svBuffer;
				}

				nextEnd = findEndOfHeader(svBuffer, lineEndPos);

				if (nextEnd != KStringView::npos)
				{
					lineEndPos = nextEnd;
					lineEndPos += leftoverSize;

					// Add header
					addResponseHeader(svBuffer, m_iColonPos, lineEndPos, bParseCookies);
					// Update state and remove what is processed
					m_parseState = needColon;
					svBuffer.remove_prefix(lineEndPos - leftoverSize+ 1);
					m_sPartialHeader.remove_prefix(lineEndPos + 1);
					svBuffer = m_sPartialHeader;
				}
				else
				{
					// save for later
					m_sPartialHeader += svBuffer;
					m_parseState = needEOL;
					return m_sPartialHeader;
				}
				break;
		};
		bFresh = false; // look for EOL from a continuation point
	}

	return svBuffer;

} // addToResponseHeader


//-----------------------------------------------------------------------------
bool KWebIO::Serialize(KOutStream& outStream)
//-----------------------------------------------------------------------------
{
	// Dont forget status line
	if (m_iResponseStatusCode > 0)
	{
		//HTTP/1.1 200 OK
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
		else if (iter.first == KHTTP::KHeader::request_cookie)
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
bool KWebIO::addResponseHeader(KStringView svBuffer, size_t colonPos, size_t lineEndPos, bool bParseCookies)
//-----------------------------------------------------------------------------
{
	//kDebug(3, "name {}, value {} start", sHeaderName, sHeaderValue);

	m_sPartialHeader += svBuffer;
	svBuffer = m_sPartialHeader;

	// "Garbage" header edge case
	if (DEKAF2_UNLIKELY(
	    colonPos == KString::npos || lineEndPos == KString::npos || colonPos >= lineEndPos))
	{
		KStringView sHeaderValue = svBuffer.substr(0, lineEndPos);
		m_responseHeaders.Add(svBrokenHeader, sHeaderValue);
		kDebug(3, "({},{}) end. Garbage header added.", svBrokenHeader, sHeaderValue);
		return true;
	}

	KStringView sHeaderName = svBuffer.substr(0, colonPos);
	KStringView sHeaderValue = svBuffer.substr(colonPos + 1, lineEndPos-colonPos);
	if (DEKAF2_LIKELY(!bParseCookies || !kCaseEqualTrimLeft(sHeaderName, KHTTP::KHeader::request_cookie)))
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

		bool needEquals = true;
		bool bFresh = true; // detect empty cookie value
		size_t equalPos = KStringView::npos;
		size_t semiPos = KStringView::npos;
		while (!sHeaderValue.empty())
		{
			//=================================================================
			if (needEquals)
			//=================================================================
			{
				equalPos = sHeaderValue.find_first_of("=;");
				if (equalPos == KStringView::npos)
				{
					// If it's just whitespace, add to last cookie,
					// else there is junk at the end.
					size_t whitespace = sHeaderValue.find_first_not_of(" \r\n\t");
					if (whitespace == KStringView::npos && !bFresh)
					{
						KString& lastCookieValue = m_responseCookies.at(m_responseCookies.size()-1).second;
						lastCookieValue += ';'; // can only get here if there are leftovers after ;
						lastCookieValue += sHeaderValue;
						return true;
					}
					// bad cookie take to eol
					kDebug(3, "({},{}) end. Ended with invalid cookie", sHeaderName, sHeaderValue);
					m_responseCookies.Add(sHeaderValue, KString{});
					return false;
				}
				else if (sHeaderValue[equalPos] == ';')
				{
					// Invalid cookie, terminated
					KStringView sCookieName = sHeaderValue.substr(0, equalPos - 1);
					kDebug(3, "({},{}) Found invalid cookie", sHeaderName, sHeaderValue);
					m_responseCookies.Add(sCookieName, KString{});
					sHeaderValue.remove_prefix(equalPos + 1);
					bFresh = false;
					continue;
				}

				needEquals = false;
				bFresh = false;
				continue;

			}
			//=================================================================
			else // look for semi or take to eol
			//=================================================================
			{
				semiPos  = sHeaderValue.find(';', equalPos);
				if (semiPos == KStringView::npos)
				{
					// last cookie without semi, take to eol
					KStringView sCookieName  = sHeaderValue.substr(0, equalPos);
					KStringView sCookieValue = sHeaderValue.substr(equalPos + 1, sHeaderValue.size() - equalPos); // don't forget newline
					m_responseCookies.Add(sCookieName, sCookieValue);
					return true;
				}
				KStringView sCookieName  = sHeaderValue.substr(0, equalPos);
				KStringView sCookieValue = sHeaderValue.substr(equalPos + 1, semiPos - equalPos - 1);
				m_responseCookies.Add(sCookieName, sCookieValue);
				needEquals = true;
				sHeaderValue.remove_prefix(semiPos + 1);
				bFresh = false;
				continue;
			}
		}
	}
	return true; // no errors detected
} // addResponseHeader


// Return current end of header line if not multiline header
// if multiline header return end pos of multi line header
// or NPOS if multiline is found that doesn't end in newline, indicating a junk end of stream, but not end of header.
//-----------------------------------------------------------------------------
size_t KWebIO::findEndOfHeader(KStringView svHeaderPart, size_t lineEndPos)
//-----------------------------------------------------------------------------
{
	kDebug(3, "({},{}) start", svHeaderPart, lineEndPos);

	if (lineEndPos == KStringView::npos)
	{
		return KStringView::npos;
	}
	do
	{
		if (svHeaderPart[lineEndPos+1] == ' ' || svHeaderPart[lineEndPos+1] == '\t') // looking for space or HT indicating continuation line
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

	kDebug(3, "({},{}) end. No problems.", svHeaderPart, lineEndPos);
	return lineEndPos;

} // findEndOfHeader

} // endnamespace dekaf2
