/*
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
#include "kcasestring.h"
#include "kprops.h"

namespace dekaf2 {


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum { MAX_LINELENGTH = 8 * 1024 };

	// The key for the Header is no trimming and lower cased on comparisons, and stores the original string
	using KHeaderMap = KProps<KCaseString, KString>; // case insensitive map for header info

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& Stream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& ContentType() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& Charset() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasKeepAlive() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns -1 for chunked content, 0 for no content, > 0 = size of content
	std::streamsize ContentLength() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasContent() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Field_names
	// awk '{u = toupper($1); gsub(/[-]/,"_",u); printf ("\tstatic constexpr KStringViewZ %-32s = \"%s\";\n", u, $1)}'

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Header names in "official" Pascal case
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	static constexpr KStringViewZ ACCEPT                           = "Accept";
	static constexpr KStringViewZ ACCEPT_CHARSET                   = "Accept-Charset";
	static constexpr KStringViewZ ACCEPT_DATETIME                  = "Accept-Datetime";
	static constexpr KStringViewZ ACCEPT_ENCODING                  = "Accept-Encoding";
	static constexpr KStringViewZ ACCEPT_LANGUAGE                  = "Accept-Language";
	static constexpr KStringViewZ ACCEPT_PATCH                     = "Accept-Patch";
	static constexpr KStringViewZ ACCEPT_RANGES                    = "Accept-Ranges";
	static constexpr KStringViewZ ACCESS_CONTROL_ALLOW_CREDENTIALS = "Access-Control-Allow-Credentials";
	static constexpr KStringViewZ ACCESS_CONTROL_ALLOW_HEADERS     = "Access-Control-Allow-Headers";
	static constexpr KStringViewZ ACCESS_CONTROL_ALLOW_METHODS     = "Access-Control-Allow-Methods";
	static constexpr KStringViewZ ACCESS_CONTROL_ALLOW_ORIGIN      = "Access-Control-Allow-Origin";
	static constexpr KStringViewZ ACCESS_CONTROL_EXPOSE_HEADERS    = "Access-Control-Expose-Headers";
	static constexpr KStringViewZ ACCESS_CONTROL_MAX_AGE           = "Access-Control-Max-Age";
	static constexpr KStringViewZ ACCESS_CONTROL_REQUEST_HEADERS   = "Access-Control-Request-Headers";
	static constexpr KStringViewZ ACCESS_CONTROL_REQUEST_METHOD    = "Access-Control-Request-Method";
	static constexpr KStringViewZ AGE                              = "Age";
	static constexpr KStringViewZ ALLOW                            = "Allow";
	static constexpr KStringViewZ ALT_SVC                          = "Alt-Svc";
	static constexpr KStringViewZ AUTHORIZATION                    = "Authorization";
	static constexpr KStringViewZ CACHE_CONTROL                    = "Cache-Control";
	static constexpr KStringViewZ CONNECTION                       = "Connection";
	static constexpr KStringViewZ CONTENT_DISPOSITION              = "Content-Disposition";
	static constexpr KStringViewZ CONTENT_ENCODING                 = "Content-Encoding";
	static constexpr KStringViewZ CONTENT_LANGUAGE                 = "Content-Language";
	static constexpr KStringViewZ CONTENT_LENGTH                   = "Content-Length";
	static constexpr KStringViewZ CONTENT_LOCATION                 = "Content-Location";
	static constexpr KStringViewZ CONTENT_MD5                      = "Content-MD5";
	static constexpr KStringViewZ CONTENT_RANGE                    = "Content-Range";
	static constexpr KStringViewZ CONTENT_TYPE                     = "Content-Type";
	static constexpr KStringViewZ COOKIE                           = "Cookie";
	static constexpr KStringViewZ DATE                             = "Date";
	static constexpr KStringViewZ ETAG                             = "ETag";
	static constexpr KStringViewZ EXPECT                           = "Expect";
	static constexpr KStringViewZ EXPIRES                          = "Expires";
	static constexpr KStringViewZ FORWARDED                        = "Forwarded";
	static constexpr KStringViewZ FROM                             = "From";
	static constexpr KStringViewZ HOST                             = "Host";
	static constexpr KStringViewZ HOST_OVERRIDE                    = "HostOverride";
	static constexpr KStringViewZ IF_MATCH                         = "If-Match";
	static constexpr KStringViewZ IF_MODIFIED_SINCE                = "If-Modified-Since";
	static constexpr KStringViewZ IF_NONE_MATCH                    = "If-None-Match";
	static constexpr KStringViewZ IF_RANGE                         = "If-Range";
	static constexpr KStringViewZ IF_UNMODIFIED_SINCE              = "If-Unmodified-Since";
	static constexpr KStringViewZ KEEP_ALIVE                       = "Keep-Alive";
	static constexpr KStringViewZ LAST_MODIFIED                    = "Last-Modified";
	static constexpr KStringViewZ LINK                             = "Link";
	static constexpr KStringViewZ LOCATION                         = "Location";
	static constexpr KStringViewZ MAX_FORWARDS                     = "Max-Forwards";
	static constexpr KStringViewZ ORIGIN                           = "Origin";
	static constexpr KStringViewZ P3P                              = "P3P";
	static constexpr KStringViewZ PRAGMA                           = "Pragma";
	static constexpr KStringViewZ PROXY_AUTHENTICATE               = "Proxy-Authenticate";
	static constexpr KStringViewZ PROXY_AUTHORIZATION              = "Proxy-Authorization";
	static constexpr KStringViewZ PROXY_CONNECTION                 = "Proxy-Connection";
	static constexpr KStringViewZ PUBLIC_KEY_PINS                  = "Public-Key-Pins";
	static constexpr KStringViewZ RANGE                            = "Range";
	static constexpr KStringViewZ REFERER                          = "Referer";
	static constexpr KStringViewZ REQUEST_COOKIE                   = "Cookie";
	static constexpr KStringViewZ RESPONSE_COOKIE                  = "Set-Cookie";
	static constexpr KStringViewZ RETRY_AFTER                      = "Retry-After";
	static constexpr KStringViewZ SERVER                           = "Server";
	static constexpr KStringViewZ SET_COOKIE                       = "Set-Cookie";
	static constexpr KStringViewZ STRICT_TRANSPORT_SECURITY        = "Strict-Transport-Security";
	static constexpr KStringViewZ TRAILER                          = "Trailer";
	static constexpr KStringViewZ TRANSFER_ENCODING                = "Transfer-Encoding";
	static constexpr KStringViewZ UPGRADE                          = "Upgrade";
	static constexpr KStringViewZ USER_AGENT                       = "User-Agent";
	static constexpr KStringViewZ VARY                             = "Vary";
	static constexpr KStringViewZ VIA                              = "Via";
	static constexpr KStringViewZ WARNING                          = "Warning";
	static constexpr KStringViewZ WWW_AUTHENTICATE                 = "WWW-Authenticate";
	static constexpr KStringViewZ X_FORWARDED_FOR                  = "X-Forwarded-For";
	static constexpr KStringViewZ X_FORWARDED_HOST                 = "X-Forwarded-Host";
	static constexpr KStringViewZ X_FORWARDED_PROTO                = "X-Forwarded-Proto";
	static constexpr KStringViewZ X_FORWARDED_SERVER               = "X-Forwarded-Server";
	static constexpr KStringViewZ X_FRAME_OPTIONS                  = "X-Frame-Options";
	static constexpr KStringViewZ X_POWERED_BY                     = "X-Powered-By";

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Header names in lowercase for normalized compares
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	static constexpr KStringViewZ accept                           = "accept";
	static constexpr KStringViewZ accept_charset                   = "accept-charset";
	static constexpr KStringViewZ accept_datetime                  = "accept-datetime";
	static constexpr KStringViewZ accept_encoding                  = "accept-encoding";
	static constexpr KStringViewZ accept_language                  = "accept-language";
	static constexpr KStringViewZ accept_patch                     = "accept-patch";
	static constexpr KStringViewZ accept_ranges                    = "accept-ranges";
	static constexpr KStringViewZ access_control_allow_credentials = "access-control-allow-credentials";
	static constexpr KStringViewZ access_control_allow_headers     = "access-control-allow-headers";
	static constexpr KStringViewZ access_control_allow_methods     = "access-control-allow-methods";
	static constexpr KStringViewZ access_control_allow_origin      = "access-control-allow-origin";
	static constexpr KStringViewZ access_control_expose_headers    = "access-control-expose-headers";
	static constexpr KStringViewZ access_control_max_age           = "access-control-max-age";
	static constexpr KStringViewZ access_control_request_headers   = "access-control-request-headers";
	static constexpr KStringViewZ access_control_request_method    = "access-control-request-method";
	static constexpr KStringViewZ age                              = "age";
	static constexpr KStringViewZ allow                            = "allow";
	static constexpr KStringViewZ alt_svc                          = "alt-svc";
	static constexpr KStringViewZ authorization                    = "authorization";
	static constexpr KStringViewZ cache_control                    = "cache-control";
	static constexpr KStringViewZ connection                       = "connection";
	static constexpr KStringViewZ content_disposition              = "content-disposition";
	static constexpr KStringViewZ content_encoding                 = "content-encoding";
	static constexpr KStringViewZ content_language                 = "content-language";
	static constexpr KStringViewZ content_length                   = "content-length";
	static constexpr KStringViewZ content_location                 = "content-location";
	static constexpr KStringViewZ content_md5                      = "content-md5";
	static constexpr KStringViewZ content_range                    = "content-range";
	static constexpr KStringViewZ content_type                     = "content-type";
	static constexpr KStringViewZ cookie                           = "cookie";
	static constexpr KStringViewZ date                             = "date";
	static constexpr KStringViewZ etag                             = "etag";
	static constexpr KStringViewZ expect                           = "expect";
	static constexpr KStringViewZ expires                          = "expires";
	static constexpr KStringViewZ forwarded                        = "forwarded";
	static constexpr KStringViewZ from                             = "from";
	static constexpr KStringViewZ host                             = "host";
	static constexpr KStringViewZ host_override                    = "hostoverride";
	static constexpr KStringViewZ if_match                         = "if-match";
	static constexpr KStringViewZ if_modified_since                = "if-modified-since";
	static constexpr KStringViewZ if_none_match                    = "if-none-match";
	static constexpr KStringViewZ if_range                         = "if-range";
	static constexpr KStringViewZ if_unmodified_since              = "if-unmodified-since";
	static constexpr KStringViewZ keep_alive                       = "keep-alive";
	static constexpr KStringViewZ last_modified                    = "last-modified";
	static constexpr KStringViewZ link                             = "link";
	static constexpr KStringViewZ location                         = "location";
	static constexpr KStringViewZ max_forwards                     = "max-forwards";
	static constexpr KStringViewZ origin                           = "origin";
	static constexpr KStringViewZ p3p                              = "p3p";
	static constexpr KStringViewZ pragma                           = "pragma";
	static constexpr KStringViewZ proxy_authenticate               = "proxy-authenticate";
	static constexpr KStringViewZ proxy_authorization              = "proxy-authorization";
	static constexpr KStringViewZ proxy_connection                 = "proxy-connection";
	static constexpr KStringViewZ public_key_pins                  = "public-key-pins";
	static constexpr KStringViewZ range                            = "range";
	static constexpr KStringViewZ referer                          = "referer";
	static constexpr KStringViewZ request_cookie                   = "cookie";
	static constexpr KStringViewZ response_cookie                  = "set-cookie";
	static constexpr KStringViewZ retry_after                      = "retry-after";
	static constexpr KStringViewZ server                           = "server";
	static constexpr KStringViewZ set_cookie                       = "set-cookie";
	static constexpr KStringViewZ strict_transport_security        = "strict-transport-security";
	static constexpr KStringViewZ trailer                          = "trailer";
	static constexpr KStringViewZ transfer_encoding                = "transfer-encoding";
	static constexpr KStringViewZ upgrade                          = "upgrade";
	static constexpr KStringViewZ user_agent                       = "user-agent";
	static constexpr KStringViewZ vary                             = "vary";
	static constexpr KStringViewZ via                              = "via";
	static constexpr KStringViewZ warning                          = "warning";
	static constexpr KStringViewZ www_authenticate                 = "www-authenticate";
	static constexpr KStringViewZ x_forwarded_for                  = "x-forwarded-for";
	static constexpr KStringViewZ x_forwarded_host                 = "x-forwarded-host";
	static constexpr KStringViewZ x_forwarded_proto                = "x-forwarded-proto";
	static constexpr KStringViewZ x_forwarded_server               = "x-forwarded-server";
	static constexpr KStringViewZ x_frame_options                  = "x-frame-options";
	static constexpr KStringViewZ x_powered_by                     = "x-powered-by";

//------
protected:
//------

	//-----------------------------------------------------------------------------
	bool SetError(KString sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SplitContentType() const;
	//-----------------------------------------------------------------------------

//------
public:
//------

	KHeaderMap Headers; // response headers read in

	// we store the http version here as it is a shared property
	// of request and response headers
	KString sHTTPVersion;

//------
private:
//------

	mutable KString m_sCharset;
	mutable KString m_sContentType;
	mutable KString m_sError;

}; // KHTTPHeaders

} // end of namespace dekaf2
