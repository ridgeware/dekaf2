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

/*
Relevent data:
Server gives response:
HTTP/1.0 200 OK
Content-type: text/html
Cookie: foo=bar
Set-Cookie: yummy_cookie=choco
Set-Cookie: tasty_cookie=strawberry
X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]

[page content]

Later requests to server:
GET /sample_page.html HTTP/1.1
Host: www.example.org
Cookie: foo=bar; yummy_cookie=choco; tasty_cookie=strawberry
*/

#pragma once
#include "kstring.h"
#include "kprops.h"
#include "kinshell.h"

#include "kfile.h"
#include "kreader.h"

#include <iostream>
//#include "kpipe.h"

namespace dekaf2
{

// Common headers in KString form (to not be harcoded in the codee
static const KString xForwardedForHeader ("x-forwarded-for");
static const KString ForwardedHeader     ("forwarded");
static const KString HostHeader          ("host");
static const KString CookieHeader        ("cookie");
static const KString UserAgentHeader     ("user-agent");
static const KString sGarbageHeader      ("garbage");

//-----------------------------------------------------------------------------
class KCurl
//-----------------------------------------------------------------------------
{

//------
public:
//------

	/// Request Type Enum GET or POST
	typedef enum
	{
		GET, POST//, PUT, DELETE
	} RequestType;

	/// The data structure for original case-sensitive data
	typedef std::pair<KString,KString> KHeaderPair;
	/// The key for this is trimmed and lower cased, value is original key-value pair.
	typedef KPropsTemplate<KString, KHeaderPair> KHeader; // case insensitive map for header info

	/// Default KURL Constructor, must be initialized after construction
	KCurl() {}

	/// KURL Constructor that allows full initialization on construction.
	KCurl(const KString& sRequestURL, RequestType requestType, bool bEchoHeader = false, bool bEchoBody = false)
	    : m_bEchoHeader{bEchoHeader}, m_bEchoBody{bEchoBody},  m_requestType{requestType}, m_sRequestURL{sRequestURL}{}
	/// Default virtual constructor
	virtual ~KCurl(){}

	/// Sets the URL that web requests will go to
	bool         setRequestURL (const KString& sRequestURL);
	/// After request URL is set, this will open a pipe that the response will be streamed into
	bool         initiateRequest(); // set header complete false
	/// After a request has been initiated this will read in the next sreaming chunk of the response
	/// Returns false when there stream is done
	bool         getStreamChunk(); // get next chunk
	/// Returns true if request is still active
	bool         requestInProgress() {
		return m_kpipe.is_open();
	}

	/// Returns true if header will be output from a web response
	bool         getEchoHeader()
	{
		return m_bEchoHeader;
	}

	/// Returns true if body will be output from a web response
	bool         getEchoBody()
	{
		return m_bEchoBody;
	}

	/// Returns the current request type
	const RequestType& getRequestType()
	{
		return m_requestType;
	}

	/// Returns the current request type
	const KString& getPostData(){
		return m_sPostData;
	}

	/// Sets whether header will be output from a web response, defaults to true
	bool         setEchoHeader(bool bEchoHeader = true)
	{
		m_bEchoHeader = bEchoHeader;
		return true;
	}

	/// Sets whether body will be output from a web response, defaults to true
	bool         setEchoBody  (bool bEchoBody = true)
	{
		m_bEchoBody = bEchoBody;
		return true;
	}

	/// Sets the Request Type
	bool         setRequestType(RequestType requestType)
	{
		m_requestType = requestType;
		return true;
	}

	/// Sets request data string directly
	bool         setPostData(const KString& postData)
	{
		m_sPostData = postData;
		return true;
	}
	/// Sets request data via a file, reads in file and puts KString into m_sPostData
	bool         setPostDataWithFile(const KString& sFileName);

	/// Gets the case-sensitive request header value set for the given header name, case-insensitive
	bool         getRequestHeader(const KString& sHeaderName, KString& sHeaderValue) const;
	/// Sets a request header by case-insenstive header name, adds a new one if it doesn't exist
	bool         setRequestHeader(const KString& sHeaderName, const KString& sHeaderValue);
	/// Adds a new request header, can be used to add repeat headers
	bool         addRequestHeader(const KString& sHeaderName, const KString& sHeaderValue);
	/// Removes the request header by header name, case-insensitive
	bool         delRequestHeader(const KString& sHeaderName);

	/// Gets the case-sensitive request cookie value set for the given cookie name, case-insensitive
	bool         getRequestCookie       (const KString& sCookieName, KString& sCookieValue) const;
	/// Sets a request cookie by case-insenstive cookie name, adds a new one if it doesn't exist
	bool         setRequestCookie       (const KString& sCookieName, const KString& sCookieValue);
	/// Adds a new request cookie, can be used to add repeat cookies
	bool         addRequestCookie       (const KString& sCookieName, const KString& sCookieValue);
	/// Removes the request cookie by cookie name, case-insensitive
	bool         delRequestCookie       (const KString& sCookieName, const KString& sCookieValue = "");

	/// Method children must override to receive and process response header
	virtual bool addToResponseHeader(KString& sHeaderPart);
	/// Method children must override to receive and process response body
	virtual bool addToResponseBody  (KString& sBodyPart);
	/// A method to print reponse header as currently read in
	virtual bool printResponseHeader();

//--------
protected:
//--------

	bool         m_bHeaderComplete{false}; // Whether to interpret response chunk as header or body
	bool         m_bEchoHeader{false};     // Whether to output header
	bool         m_bEchoBody{false};       // Whether to output body
	RequestType  m_requestType{GET};       // HTTP Request type, GET or POST
	KString      m_sPostData;              // Post data for post request
	KInStream    m_inStream{std::cin};     // Input Reader

//--------
private:
//--------

	KInShell     m_kpipe;          // pipe to read response data from
	KString      m_sRequestURL;    // request url, must be set
	KHeader      m_requestHeaders; // headers to add to requests
	KHeader      m_requestCookies; // cookies to add to request

	bool         serializeRequestHeader(KString& sCurlHeaders); // false means no headers
};

} // END NAMESPACE dekaf2
