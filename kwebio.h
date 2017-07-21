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

#pragma once
#include "kstring.h"
#include "kprops.h"
#include "kcurl.h"

//include <locale>
#include <ctype.h>
#include <iostream>

namespace dekaf2
{

class KWebIO : public KCurl
{
public:
	KWebIO();
	KWebIO(const KString& requestURL, bool bEchoHeader = false, bool bEchoBody = false);
	~KWebIO();

	virtual bool   addToResponseHeader(KString& sHeaderPart);
	virtual bool   addToResponseBody  (KString& sBodyPart);
	virtual bool   printResponseHeader(); // prints response header from m_responseHeaderss

	const KHeader& getResponseHeaders() const;
	const KString& getResponseHeader(const KString& sHeaderName) const;
	const KHeader& getResponseCookies() const;
	const KString&  getResponseCookie(const KString& sCookieName);// const; // gets first cookie with name

private:
	KHeader        m_responseHeaders;
	KHeader        m_responseCookies;
	KString        m_sPartialHeader; // when streaming can't guarantee always have full header.
	KString        m_sResponseVersion;
	KString        m_sResponseStatus;
	uint16_t        m_iResponseStatusCode{0};

	bool           addResponseHeader(const KString& sHeaderName, const KString& sHeaderValue);
	bool           isLastHeader(KString& sHeaderPart, size_t lineEndPos);
	size_t         findEndOfHeader(KString& sHeaderPart, size_t lineEndPos);
};

} // end namespace dekaf2
