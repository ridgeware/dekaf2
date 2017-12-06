
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

#include "kstringview.h"
#include "kconnection.h"
#include "khttp_header.h"
#include "khttp_method.h"
#include "kuseragent.h"


namespace dekaf2 {

namespace detail {
namespace http {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCharSet
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringView ANY_ISO8859         = "ISO-8859"; /*-1...*/
	static constexpr KStringView DEFAULT_CHARSET     = "WINDOWS-1252";

}; // end of namespace KCharSet

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIME
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	constexpr
	KMIME()
	//-----------------------------------------------------------------------------
	    : m_mime()
	{}

	//-----------------------------------------------------------------------------
	constexpr
	KMIME(KStringView sv)
	//-----------------------------------------------------------------------------
	    : m_mime(sv)
	{}

	//-----------------------------------------------------------------------------
	constexpr
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return m_mime;
	}

	static constexpr KStringView JSON_UTF8           = "application/json; charset=UTF-8";
	static constexpr KStringView HTML_UTF8           = "text/html; charset=UTF-8";
	static constexpr KStringView XML_UTF8            = "text/xml; charset=UTF-8";
	static constexpr KStringView SWF                 = "application/x-shockwave-flash";
	static constexpr KStringView WWW_FORM_URLENCODED = "application/x-www-form-urlencoded";
	static constexpr KStringView MULTIPART_FORM_DATA = "multipart/form-data";
	static constexpr KStringView TEXT_PLAIN          = "text/plain";
	static constexpr KStringView APPLICATION_BINARY  = "application/octet-stream";

//------
private:
//------

	KStringView m_mime{TEXT_PLAIN};

}; // KMIME

} // end of namespace http
} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class CGI
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
	static constexpr KStringView GET                     = "GET";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView HEAD                    = "HEAD";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView POST                    = "POST";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView PUT                     = "PUT";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView DELETE                  = "DELETE";            // legal RHS of REQUEST_METHOD
	static constexpr KStringView CONNECT                 = "CONNECT";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView OPTIONS                 = "OPTIONS";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView TRACE                   = "TRACE";             // legal RHS of REQUEST_METHOD
	static constexpr KStringView PATCH                   = "PATCH";             // legal RHS of REQUEST_METHOD
	static constexpr KStringView SCRIPT_NAME             = "SCRIPT_NAME";
	static constexpr KStringView SERVER_NAME             = "SERVER_NAME";
	static constexpr KStringView SERVER_PORT             = "SERVER_PORT";
	static constexpr KStringView SERVER_PORT_SECURE      = "SERVER_PORT_SECURE";
	static constexpr KStringView SERVER_PROTOCOL         = "SERVER_PROTOCOL";
	static constexpr KStringView SERVER_SOFTWARE         = "SERVER_SOFTWARE";
	static constexpr KStringView WEB_SERVER_API          = "WEB_SERVER_AP";

}; // CGI variables

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class HTTPREQ
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Field_names
	// awk '{u = toupper($1); gsub(/[-]/,"_",u); printf ("\tstatic constexpr KStringView %-32s = \"%s\";\n", u, $1)}'

//------
public:
//------
	static constexpr KStringView ACCEPT                           = "Accept";
	static constexpr KStringView ACCEPT_CHARSET                   = "Accept-Charset";
	static constexpr KStringView ACCEPT_ENCODING                  = "Accept-Encoding";
	static constexpr KStringView ACCEPT_LANGUAGE                  = "Accept-Language";
	static constexpr KStringView ACCEPT_DATETIME                  = "Accept-Datetime";
	static constexpr KStringView ACCESS_CONTROL_REQUEST_METHOD    = "Access-Control-Request-Method";
	static constexpr KStringView ACCESS_CONTROL_REQUEST_HEADERS   = "Access-Control-Request-Headers";
	static constexpr KStringView AUTHORIZATION                    = "Authorization";
	static constexpr KStringView CACHE_CONTROL                    = "Cache-Control";
	static constexpr KStringView CONNECTION                       = "Connection";
	static constexpr KStringView COOKIE                           = "Cookie";
	static constexpr KStringView CONTENT_LENGTH                   = "Content-Length";
	static constexpr KStringView CONTENT_MD5                      = "Content-MD5";
	static constexpr KStringView CONTENT_TYPE                     = "Content-Type";
	static constexpr KStringView DATE                             = "Date";
	static constexpr KStringView EXPECT                           = "Expect";
	static constexpr KStringView FORWARDED                        = "Forwarded";
	static constexpr KStringView FROM                             = "From";
	static constexpr KStringView HOST                             = "Host";
	static constexpr KStringView IF_MATCH                         = "If-Match";
	static constexpr KStringView IF_MODIFIED_SINCE                = "If-Modified-Since";
	static constexpr KStringView IF_NONE_MATCH                    = "If-None-Match";
	static constexpr KStringView IF_RANGE                         = "If-Range";
	static constexpr KStringView IF_UNMODIFIED_SINCE              = "If-Unmodified-Since";
	static constexpr KStringView MAX_FORWARDS                     = "Max-Forwards";
	static constexpr KStringView ORIGIN                           = "Origin";
	static constexpr KStringView PRAGMA                           = "Pragma";
	static constexpr KStringView PROXY_AUTHORIZATION              = "Proxy-Authorization";
	static constexpr KStringView RANGE                            = "Range";
	static constexpr KStringView REFERER                          = "Referer";
	static constexpr KStringView UPGRADE                          = "Upgrade";
	static constexpr KStringView VIA                              = "Via";
	static constexpr KStringView WARNING                          = "Warning";
	static constexpr KStringView X_FORWARDED_FOR                  = "X-Forwarded-For";
	static constexpr KStringView X_FORWARDED_HOST                 = "X-Forwarded-Host";
	static constexpr KStringView X_FORWARDED_PROTO                = "X-Forwarded-Proto";

}; // HTTPREQ

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class HTTPRESP
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Field_names
	// awk '{u = toupper($1); gsub(/[-]/,"_",u); printf ("\tstatic constexpr KStringView %-32s = \"%s\";\n", u, $1)}'

//------
public:
//------
	static constexpr KStringView ACCESS_CONTROL_ALLOW_ORIGIN      = "Access-Control-Allow-Origin";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_CREDENTIALS = "Access-Control-Allow-Credentials";
	static constexpr KStringView ACCESS_CONTROL_EXPOSE_HEADERS    = "Access-Control-Expose-Headers";
	static constexpr KStringView ACCESS_CONTROL_MAX_AGE           = "Access-Control-Max-Age";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_METHODS     = "Access-Control-Allow-Methods";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_HEADERS     = "Access-Control-Allow-Headers";
	static constexpr KStringView ACCEPT_PATCH                     = "Accept-Patch";
	static constexpr KStringView ACCEPT_RANGES                    = "Accept-Ranges";
	static constexpr KStringView AGE                              = "Age";
	static constexpr KStringView ALLOW                            = "Allow";
	static constexpr KStringView ALT_SVC                          = "Alt-Svc";
	static constexpr KStringView CACHE_CONTROL                    = "Cache-Control";
	static constexpr KStringView CONNECTION                       = "Connection";
	static constexpr KStringView CONTENT_DISPOSITION              = "Content-Disposition";
	static constexpr KStringView CONTENT_ENCODING                 = "Content-Encoding";
	static constexpr KStringView CONTENT_LANGUAGE                 = "Content-Language";
	static constexpr KStringView CONTENT_LENGTH                   = "Content-Length";
	static constexpr KStringView CONTENT_LOCATION                 = "Content-Location";
	static constexpr KStringView CONTENT_MD5                      = "Content-MD5";
	static constexpr KStringView CONTENT_RANGE                    = "Content-Range";
	static constexpr KStringView CONTENT_TYPE                     = "Content-Type";
	static constexpr KStringView DATE                             = "Date";
	static constexpr KStringView ETAG                             = "ETag";
	static constexpr KStringView EXPIRES                          = "Expires";
	static constexpr KStringView LAST_MODIFIED                    = "Last-Modified";
	static constexpr KStringView LINK                             = "Link";
	static constexpr KStringView LOCATION                         = "Location";
	static constexpr KStringView P3P                              = "P3P";
	static constexpr KStringView PRAGMA                           = "Pragma";
	static constexpr KStringView PROXY_AUTHENTICATE               = "Proxy-Authenticate";
	static constexpr KStringView PUBLIC_KEY_PINS                  = "Public-Key-Pins";
	static constexpr KStringView RETRY_AFTER                      = "Retry-After";
	static constexpr KStringView SERVER                           = "Server";
	static constexpr KStringView SET_COOKIE                       = "Set-Cookie";
	static constexpr KStringView STRICT_TRANSPORT_SECURITY        = "Strict-Transport-Security";
	static constexpr KStringView TRAILER                          = "Trailer";
	static constexpr KStringView TRANSFER_ENCODING                = "Transfer-Encoding";
	static constexpr KStringView UPGRADE                          = "Upgrade";
	static constexpr KStringView VARY                             = "Vary";
	static constexpr KStringView WWW_AUTHENTICATE                 = "WWW-Authenticate";
	static constexpr KStringView X_FRAME_OPTIONS                  = "X-Frame-Options";
	static constexpr KStringView X_POWERED_BY                     = "X-Powered-By";

}; // HTTPRESP

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTP
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KMethod    = detail::http::KMethod;
	using KHeader    = detail::http::KHeader;
	using KUserAgent = detail::http::KUserAgent;
	using KMIME      = detail::http::KMIME;
	using KCharSet   = detail::http::KCharSet;

	enum class State
	{
		CONNECTED,
		RESOURCE_SET,
		HEADER_SET,
		REQUEST_SENT,
		HEADER_PARSED,
		CLOSED
	};

	//-----------------------------------------------------------------------------
	KHTTP(KConnection& stream, const KURL& url = KURL{}, KMethod method = KMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTP& Resource(const KURL& url, KMethod method = KMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTP& RequestHeader(KStringView svName, KStringView svValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Request(KStringView svPostData = KStringView{}, KStringView svMime = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stream into outstream
	size_t Read(KOutStream& stream, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KString& sBuffer, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KString& sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_iRemainingContentSize;
	}

	//-----------------------------------------------------------------------------
	State GetState() const
	//-----------------------------------------------------------------------------
	{
		return m_State;
	}

	//-----------------------------------------------------------------------------
	const KHeader& GetResponseHeader() const
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

	//-----------------------------------------------------------------------------
	KHeader& GetResponseHeader()
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	bool GetNextChunkSize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void CheckForChunkEnd();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool ReadHeader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_t Post(KStringView sv);
	//-----------------------------------------------------------------------------

//------
private:
//------

	KConnection& m_Stream;
	KMethod  m_Method;
	KHeader  m_ResponseHeader;
	size_t   m_iRemainingContentSize{0};
	State    m_State{State::CLOSED};
	bool     m_bTEChunked;

}; // KHTTP


} // end of namespace dekaf2
