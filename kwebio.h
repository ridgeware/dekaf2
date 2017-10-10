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

#include <cinttypes>
#include "kstring.h"
#include "kprops.h"
#include "kcurl.h"
#include "kwriter.h"


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KWebIO : public KCurl
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringView svBrokenHeader = "!?.garbage";

	//-----------------------------------------------------------------------------
	/// KWebIO default constructor, must be initialized after construction.
	KWebIO()
	//-----------------------------------------------------------------------------
	{}

	//-----------------------------------------------------------------------------
	/// KWebIO Constructor that allows full initialization on construction.
	KWebIO(const KString& requestURL, RequestType requestType = GET, bool bEchoHeader = false, bool bEchoBody = false)
	//-----------------------------------------------------------------------------
	    : KCurl(requestURL, requestType, bEchoHeader, bEchoBody)
	{}

	//-----------------------------------------------------------------------------
	/// Default virutal destructor
	virtual ~KWebIO()
	//-----------------------------------------------------------------------------
	{}

	//-----------------------------------------------------------------------------
	/// Overriden virtual method that parses response header
	virtual KStringView Parse(KStringView sPart, bool bParseCookies = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Overriden virtual method that prints out parsed response header
	virtual bool Serialize(KOutStream& outStream); // prints response header from m_responseHeaders
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Overriden virtual method that prints out parsed response header
	virtual bool Serialize() // prints response header from m_responseHeaders
	//-----------------------------------------------------------------------------
	{
		KOutStream os(std::cout);
		return Serialize(os);
	}

	//-----------------------------------------------------------------------------
	/// Get all response headers except Cookies as a KHeader
	const KHeader& getResponseHeaders() const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders;
	}

	//-----------------------------------------------------------------------------
	/// Get response header value from given header name (case insensitive)
	const KString& getResponseHeader(KCaseTrimStringView sHeaderName) const
	//-----------------------------------------------------------------------------
	{
		return m_responseHeaders.Get(sHeaderName);
	}

	//-----------------------------------------------------------------------------
	/// Get all response cookies as a KHeader
	const KHeader& getResponseCookies() const
	//-----------------------------------------------------------------------------
	{
		return m_responseCookies;
	}

	//-----------------------------------------------------------------------------
	/// Get response cookie value from given cookie name (case insensitive)
	const KString& getResponseCookie(KCaseTrimStringView sCookieName) const
	//-----------------------------------------------------------------------------
	{
		return m_responseCookies.Get(sCookieName);
	}

	//-----------------------------------------------------------------------------
	virtual bool HeaderComplete() const
	//-----------------------------------------------------------------------------
	{
		return m_bHeaderComplete;
	}

//------
private:
//------

	KHeader        m_responseHeaders; // response headers read in
	KHeader        m_responseCookies; // response cookies read in
	KString        m_sPartialHeader; // when streaming can't guarantee always have full header.
	KString        m_sResponseVersion; // HTTP resonse version
	KString        m_sResponseStatus; // HTTP response status
	uint16_t       m_iResponseStatusCode{0}; // HTTP response code
	bool           m_bHeaderComplete{false}; // Whether to interpret response chunk as header or body
// TODO we need to reset the header complete flag if we want to reuse this class

	//-----------------------------------------------------------------------------
	/// method that takes care of case-insentive header add logic and cookie add logic
	bool           addResponseHeader(KStringView sHeaderName, KStringView sHeaderValue, bool bParseCookies);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// if parsing multi line header, this gets to the end of it
	size_t         findEndOfHeader(KStringView svHeaderPart, size_t lineEndPos);
	//-----------------------------------------------------------------------------
};

} // end namespace dekaf2
