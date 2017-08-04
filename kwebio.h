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

#pragma once
#include "kstring.h"
#include "kprops.h"
#include "kcurl.h"
#include "kwriter.h"

//include <locale>
#include <ctype.h>
//#include <iostream>

namespace dekaf2
{

//-----------------------------------------------------------------------------
class KWebIO : public KCurl
//-----------------------------------------------------------------------------
{

//------
public:
//------
	/// KWebIO default constructor, must be initialized after construction.
	KWebIO() {}
	/// KWebIO Constructor that allows full initialization on construction.
	KWebIO(const KString& requestURL, bool bEchoHeader = false, bool bEchoBody = false)
	    : KCurl(requestURL, bEchoHeader, bEchoBody) {}
	/// Default virutal destructor
	virtual ~KWebIO() {}

	/// Overriden virtual method that parses response header
	virtual bool   addToResponseHeader(KString& sHeaderPart);
	/// Overriden virtual method that parses response body
	virtual bool   addToResponseBody  (KString& sBodyPart);
	/// Overriden virtual method that prints out parsed response header
	virtual bool   printResponseHeader(); // prints response header from m_responseHeaderss

	/// Get all response headers except Cookies as a KHeader
	const KHeader& getResponseHeaders() const
	{
		return m_responseHeaders;
	}

	/// Get response header value from given header name (case insensitive)
	const KString& getResponseHeader(const KString& sHeaderName) const;
	/// Get all response cookies as a KHeader
	const KHeader& getResponseCookies() const
	{
		return m_responseCookies;
	}

	/// Get response cookie value from given cookie name (case insensitive)
	const KString& getResponseCookie(const KString& sCookieName);// const; // gets first cookie with name

//------
private:
//------

	KHeader        m_responseHeaders; // response headers read in
	KHeader        m_responseCookies; // response cookies read in
	KString        m_sPartialHeader; // when streaming can't guarantee always have full header.
	KString        m_sResponseVersion; // HTTP resonse version
	KString        m_sResponseStatus; // HTTP response status
	uint16_t       m_iResponseStatusCode{0}; // HTTP response code

	//KOutStream     m_outStream; // TODO Add KOutStream

	// method that takes care of case-insentive header add logic and cookie add logic
	bool           addResponseHeader(const KString&& sHeaderName, const KString&& sHeaderValue);
	// method to determine if header ends with \n\n or \r\n\r\n indicating end of header
	bool           isLastHeader(const KString& sHeaderPart, size_t lineEndPos);
	// if parsing multi line header, this gets to the end of it
	size_t         findEndOfHeader(const KString& sHeaderPart, size_t lineEndPos);
};

} // end namespace dekaf2
