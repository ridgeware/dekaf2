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
#include "kstringview.h"
#include "kprops.h"
#include "ksplit.h"
#include <iostream>
#ifdef DEKAF2_WITH_FCGI
#include <fcgiapp.h>
#include <fcgio.h>
#endif

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A common interface class for both CGI and FCGI requests.
class KCGI
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	static constexpr KStringView AUTH_PASSWORD           = "AUTH_PASSWORD";
	static constexpr KStringView AUTH_TYPE               = "AUTH_TYPE";
	static constexpr KStringView AUTH_USER               = "AUTH_USER";
	static constexpr KStringView CERT_COOKIE             = "CERT_COOKIE";
	static constexpr KStringView CERT_FLAGS              = "CERT_FLAGS";
	static constexpr KStringView CERT_ISSUER             = "CERT_ISSUER";
	static constexpr KStringView CERT_KEYSIZE            = "CERT_KEYSIZE";
	static constexpr KStringView CERT_SECRETKEYSIZE      = "CERT_SECRETKEYSIZE";
	static constexpr KStringView CERT_SERIALNUMBER       = "CERT_SERIALNUMBER";
	static constexpr KStringView CERT_SERVER_ISSUER      = "CERT_SERVER_ISSUER";
	static constexpr KStringView CERT_SERVER_SUBJECT     = "CERT_SERVER_SUBJECT";
	static constexpr KStringView CERT_SUBJECT            = "CERT_SUBJECT";
	static constexpr KStringView CF_TEMPLATE_PATH        = "CF_TEMPLATE_PATH";
	static constexpr KStringView CONTENT_LENGTH          = "CONTENT_LENGTH";
	static constexpr KStringView CONTENT_TYPE            = "CONTENT_TYPE";
	static constexpr KStringView CONTEXT_PATH            = "CONTEXT_PATH";
	static constexpr KStringView GATEWAY_INTERFACE       = "GATEWAY_INTERFACE";
	static constexpr KStringView HTTPS                   = "HTTPS";
	static constexpr KStringView HTTPS_KEYSIZE           = "HTTPS_KEYSIZE";
	static constexpr KStringView HTTPS_SECRETKEYSIZE     = "HTTPS_SECRETKEYSIZE";
	static constexpr KStringView HTTPS_SERVER_ISSUER     = "HTTPS_SERVER_ISSUER";
	static constexpr KStringView HTTPS_SERVER_SUBJECT    = "HTTPS_SERVER_SUBJECT";
	static constexpr KStringView HTTP_ACCEPT             = "HTTP_ACCEPT";
	static constexpr KStringView HTTP_ACCEPT_ENCODING    = "HTTP_ACCEPT_ENCODING";
	static constexpr KStringView HTTP_ACCEPT_LANGUAGE    = "HTTP_ACCEPT_LANGUAGE";
	static constexpr KStringView HTTP_CONNECTION         = "HTTP_CONNECTION";
	static constexpr KStringView HTTP_COOKIE             = "HTTP_COOKIE";
	static constexpr KStringView HTTP_HOST               = "HTTP_HOST";
	static constexpr KStringView HTTP_REFERER            = "HTTP_REFERER";
	static constexpr KStringView HTTP_USER_AGENT         = "HTTP_USER_AGENT";
	static constexpr KStringView QUERY_STRING            = "QUERY_STRING";
	static constexpr KStringView REMOTE_ADDR             = "REMOTE_ADDR";
	static constexpr KStringView REMOTE_HOST             = "REMOTE_HOST";
	static constexpr KStringView REMOTE_USER             = "REMOTE_USER";
    static constexpr KStringView REQUEST_METHOD          = "REQUEST_METHOD";
    static constexpr KStringView REQUEST_URI             = "REQUEST_URI";
    static constexpr KStringView SCRIPT_NAME             = "SCRIPT_NAME";
	static constexpr KStringView SERVER_NAME             = "SERVER_NAME";
	static constexpr KStringView SERVER_PORT             = "SERVER_PORT";
	static constexpr KStringView SERVER_PORT_SECURE      = "SERVER_PORT_SECURE";
	static constexpr KStringView SERVER_PROTOCOL         = "SERVER_PROTOCOL";
	static constexpr KStringView SERVER_SOFTWARE         = "SERVER_SOFTWARE";
	static constexpr KStringView WEB_SERVER_API          = "WEB_SERVER_API";

	static constexpr KStringView GET                     = "GET";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView HEAD                    = "HEAD";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView POST                    = "POST";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView PUT                     = "PUT";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView DELETE                  = "DELETE";            // legal RHS of REQUEST_METHOD
	static constexpr KStringView CONNECT                 = "CONNECT";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView OPTIONS                 = "OPTIONS";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView TRACE                   = "TRACE";             // legal RHS of REQUEST_METHOD
	static constexpr KStringView PATCH                   = "PATCH";             // legal RHS of REQUEST_METHOD

	static constexpr KStringView FCGI_WEB_SERVER_ADDRS   = "FCGI_WEB_SERVER_ADDRS";

	using HeadersT    = KProps <KString, KString, /*order-matters=*/false, /*unique-keys=*/false>;

	using QueryParmsT = KProps <KString, KString, /*order-matters=*/false, /*unique-keys=*/false>;

	// static bool IsWebRequest(); -- not sure this will work

	KCGI();

	~KCGI();

	KString GetVar (KStringView sEnvironmentVariable, const char* sDefaultValue="");

	/// Get next CGI (or FCGI) reqeuest.  Defaults to STDIN for CGI.
	/// Supplying a filename is useful for test harnesses that are not
	/// running inside a web server.
	bool GetNextRequest (KStringView sFilename = KStringView{}, KStringView sCommentDelim = KStringView{});

	/// read request headers
	bool ReadHeaders ();

	/// read request body
	bool ReadPostData ();

	/// returns a reference to the output writer
	KOutStream& Writer() { return *m_Writer; }

	/// incoming http request method: GET, POST, etc.
	const KString& RequestMethod() const
	{
		return m_sRequestMethod;
	}

	/// incoming URL including the query string
	const KString& RequestURI() const
	{
		return m_sRequestURI;
	}

	/// incoming URL with query string trimmed
	const KString& RequestPath() const
	{
		return m_sRequestPath;
	}

	/// incoming http protocol and version as defined in status header
	const KString& HTTPProtocol() const
	{
		return m_sHttpProtocol;
	}

	/// query string (name=value&...)
	const KString& QueryString() const
	{
		return m_sQueryString;
	}

	/// raw, unprocessed incomiong POST data
	const KString& PostData() const
	{
		return m_sPostData;
	}

	/// incoming request headers
	const HeadersT& RequestHeaders() const
	{
		return m_Headers;
	}

	/// incoming query parms off request URI
	const QueryParmsT& QueryParms() const
	{
		return m_QueryParms;
	}

	/// returns last error message
	const KString& GetLastError() const
	{
		return m_sError;
	}

//----------
protected:
//----------

	/// incoming http request method: GET, POST, etc., non-const version for children
	KString& SetRequestMethod()
	{
		return m_sRequestMethod;
	}

	/// incoming URL including the query string, non-const version for children
	KString& SetRequestURI()
	{
		return m_sRequestURI;
	}

	/// raw, unprocessed incomiong POST data, non-const version for children
	KString& SetPostData()
	{
		return m_sPostData;
	}

	/// incoming request headers, non-const version for children
	HeadersT& SetRequestHeaders()
	{
		return m_Headers;
	}

	/// incoming query parms off request URI, non-const version for children
	QueryParmsT& SetQueryParms()
	{
		return m_QueryParms;
	}

	/// reset all class members for next request
	void init (bool bResetStreams = true);

//----------
private:
//----------

	/// returns true if this is an FCGI connection
	bool IsFCGI() const
	{
		return (m_bIsFCGI);
	}

	KString      m_sRequestMethod;
	KString      m_sRequestURI;
	KString      m_sRequestPath;
	KString      m_sHttpProtocol;
	KString      m_sQueryString;
	KString      m_sPostData; // aka body
    HeadersT     m_Headers;
    QueryParmsT  m_QueryParms;

	KString                     m_sError;
	KString                     m_sCommentDelim;
	unsigned int                m_iNumRequests{0};
	bool                        m_bIsFCGI{false};
	std::unique_ptr<KInStream>  m_Reader;
	std::unique_ptr<KOutStream> m_Writer;
#ifdef DEKAF2_WITH_FCGI
	FCGX_Request                m_FcgiRequest;
#endif

}; // class KCGI

} // end of namespace dekaf2

