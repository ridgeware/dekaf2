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
#include "kcasestring.h"
#include "kprops.h"

namespace dekaf2 {

extern template class KProps<KCaseTrimString, KString>;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// The key for the Header is trimmed and lower cased on comparisons, but stores the original string
	using KHeaderMap = KProps<KCaseTrimString, KString>; // case insensitive map for header info

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
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Field_names
	// awk '{u = toupper($1); gsub(/[-]/,"_",u); printf ("\tstatic constexpr KStringView %-32s = \"%s\";\n", u, $1)}'

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Header names in "official" Pascal case
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	static constexpr KStringView ACCEPT                           = "Accept";
	static constexpr KStringView ACCEPT_CHARSET                   = "Accept-Charset";
	static constexpr KStringView ACCEPT_DATETIME                  = "Accept-Datetime";
	static constexpr KStringView ACCEPT_ENCODING                  = "Accept-Encoding";
	static constexpr KStringView ACCEPT_LANGUAGE                  = "Accept-Language";
	static constexpr KStringView ACCEPT_PATCH                     = "Accept-Patch";
	static constexpr KStringView ACCEPT_RANGES                    = "Accept-Ranges";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_CREDENTIALS = "Access-Control-Allow-Credentials";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_HEADERS     = "Access-Control-Allow-Headers";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_METHODS     = "Access-Control-Allow-Methods";
	static constexpr KStringView ACCESS_CONTROL_ALLOW_ORIGIN      = "Access-Control-Allow-Origin";
	static constexpr KStringView ACCESS_CONTROL_EXPOSE_HEADERS    = "Access-Control-Expose-Headers";
	static constexpr KStringView ACCESS_CONTROL_MAX_AGE           = "Access-Control-Max-Age";
	static constexpr KStringView ACCESS_CONTROL_REQUEST_HEADERS   = "Access-Control-Request-Headers";
	static constexpr KStringView ACCESS_CONTROL_REQUEST_METHOD    = "Access-Control-Request-Method";
	static constexpr KStringView AGE                              = "Age";
	static constexpr KStringView ALLOW                            = "Allow";
	static constexpr KStringView ALT_SVC                          = "Alt-Svc";
	static constexpr KStringView AUTHORIZATION                    = "Authorization";
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
	static constexpr KStringView COOKIE                           = "Cookie";
	static constexpr KStringView DATE                             = "Date";
	static constexpr KStringView ETAG                             = "ETag";
	static constexpr KStringView EXPECT                           = "Expect";
	static constexpr KStringView EXPIRES                          = "Expires";
	static constexpr KStringView FORWARDED                        = "Forwarded";
	static constexpr KStringView FROM                             = "From";
	static constexpr KStringView HOST                             = "Host";
	static constexpr KStringView HOST_OVERRIDE                    = "HostOverride";
	static constexpr KStringView IF_MATCH                         = "If-Match";
	static constexpr KStringView IF_MODIFIED_SINCE                = "If-Modified-Since";
	static constexpr KStringView IF_NONE_MATCH                    = "If-None-Match";
	static constexpr KStringView IF_RANGE                         = "If-Range";
	static constexpr KStringView IF_UNMODIFIED_SINCE              = "If-Unmodified-Since";
	static constexpr KStringView KEEP_ALIVE                       = "Keep-Alive";
	static constexpr KStringView LAST_MODIFIED                    = "Last-Modified";
	static constexpr KStringView LINK                             = "Link";
	static constexpr KStringView LOCATION                         = "Location";
	static constexpr KStringView MAX_FORWARDS                     = "Max-Forwards";
	static constexpr KStringView ORIGIN                           = "Origin";
	static constexpr KStringView P3P                              = "P3P";
	static constexpr KStringView PRAGMA                           = "Pragma";
	static constexpr KStringView PROXY_AUTHENTICATE               = "Proxy-Authenticate";
	static constexpr KStringView PROXY_AUTHORIZATION              = "Proxy-Authorization";
	static constexpr KStringView PUBLIC_KEY_PINS                  = "Public-Key-Pins";
	static constexpr KStringView RANGE                            = "Range";
	static constexpr KStringView REFERER                          = "Referer";
	static constexpr KStringView REQUEST_COOKIE                   = "Cookie";
	static constexpr KStringView RESPONSE_COOKIE                  = "Set-Cookie";
	static constexpr KStringView RETRY_AFTER                      = "Retry-After";
	static constexpr KStringView SERVER                           = "Server";
	static constexpr KStringView SET_COOKIE                       = "Set-Cookie";
	static constexpr KStringView STRICT_TRANSPORT_SECURITY        = "Strict-Transport-Security";
	static constexpr KStringView TRAILER                          = "Trailer";
	static constexpr KStringView TRANSFER_ENCODING                = "Transfer-Encoding";
	static constexpr KStringView UPGRADE                          = "Upgrade";
	static constexpr KStringView USER_AGENT                       = "User-Agent";
	static constexpr KStringView VARY                             = "Vary";
	static constexpr KStringView VIA                              = "Via";
	static constexpr KStringView WARNING                          = "Warning";
	static constexpr KStringView WWW_AUTHENTICATE                 = "WWW-Authenticate";
	static constexpr KStringView X_FORWARDED_FOR                  = "X-Forwarded-For";
	static constexpr KStringView X_FORWARDED_HOST                 = "X-Forwarded-Host";
	static constexpr KStringView X_FORWARDED_PROTO                = "X-Forwarded-Proto";
	static constexpr KStringView X_FORWARDED_SERVER               = "X-Forwarded-Server";
	static constexpr KStringView X_FRAME_OPTIONS                  = "X-Frame-Options";
	static constexpr KStringView X_POWERED_BY                     = "X-Powered-By";

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Header names in lowercase for normalized compares
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	static constexpr KStringView accept                           = "accept";
	static constexpr KStringView accept_charset                   = "accept-charset";
	static constexpr KStringView accept_datetime                  = "accept-datetime";
	static constexpr KStringView accept_encoding                  = "accept-encoding";
	static constexpr KStringView accept_language                  = "accept-language";
	static constexpr KStringView accept_patch                     = "accept-patch";
	static constexpr KStringView accept_ranges                    = "accept-ranges";
	static constexpr KStringView access_control_allow_credentials = "access-control-allow-credentials";
	static constexpr KStringView access_control_allow_headers     = "access-control-allow-headers";
	static constexpr KStringView access_control_allow_methods     = "access-control-allow-methods";
	static constexpr KStringView access_control_allow_origin      = "access-control-allow-origin";
	static constexpr KStringView access_control_expose_headers    = "access-control-expose-headers";
	static constexpr KStringView access_control_max_age           = "access-control-max-age";
	static constexpr KStringView access_control_request_headers   = "access-control-request-headers";
	static constexpr KStringView access_control_request_method    = "access-control-request-method";
	static constexpr KStringView age                              = "age";
	static constexpr KStringView allow                            = "allow";
	static constexpr KStringView alt_svc                          = "alt-svc";
	static constexpr KStringView authorization                    = "authorization";
	static constexpr KStringView cache_control                    = "cache-control";
	static constexpr KStringView connection                       = "connection";
	static constexpr KStringView content_disposition              = "content-disposition";
	static constexpr KStringView content_encoding                 = "content-encoding";
	static constexpr KStringView content_language                 = "content-language";
	static constexpr KStringView content_length                   = "content-length";
	static constexpr KStringView content_location                 = "content-location";
	static constexpr KStringView content_md5                      = "content-md5";
	static constexpr KStringView content_range                    = "content-range";
	static constexpr KStringView content_type                     = "content-type";
	static constexpr KStringView cookie                           = "cookie";
	static constexpr KStringView date                             = "date";
	static constexpr KStringView etag                             = "etag";
	static constexpr KStringView expect                           = "expect";
	static constexpr KStringView expires                          = "expires";
	static constexpr KStringView forwarded                        = "forwarded";
	static constexpr KStringView from                             = "from";
	static constexpr KStringView host                             = "host";
	static constexpr KStringView host_override                    = "hostoverride";
	static constexpr KStringView if_match                         = "if-match";
	static constexpr KStringView if_modified_since                = "if-modified-since";
	static constexpr KStringView if_none_match                    = "if-none-match";
	static constexpr KStringView if_range                         = "if-range";
	static constexpr KStringView if_unmodified_since              = "if-unmodified-since";
	static constexpr KStringView keep_alive                       = "keep-alive";
	static constexpr KStringView last_modified                    = "last-modified";
	static constexpr KStringView link                             = "link";
	static constexpr KStringView location                         = "location";
	static constexpr KStringView max_forwards                     = "max-forwards";
	static constexpr KStringView origin                           = "origin";
	static constexpr KStringView p3p                              = "p3p";
	static constexpr KStringView pragma                           = "pragma";
	static constexpr KStringView proxy_authenticate               = "proxy-authenticate";
	static constexpr KStringView proxy_authorization              = "proxy-authorization";
	static constexpr KStringView public_key_pins                  = "public-key-pins";
	static constexpr KStringView range                            = "range";
	static constexpr KStringView referer                          = "referer";
	static constexpr KStringView request_cookie                   = "cookie";
	static constexpr KStringView response_cookie                  = "set-cookie";
	static constexpr KStringView retry_after                      = "retry-after";
	static constexpr KStringView server                           = "server";
	static constexpr KStringView set_cookie                       = "set-cookie";
	static constexpr KStringView strict_transport_security        = "strict-transport-security";
	static constexpr KStringView trailer                          = "trailer";
	static constexpr KStringView transfer_encoding                = "transfer-encoding";
	static constexpr KStringView upgrade                          = "upgrade";
	static constexpr KStringView user_agent                       = "user-agent";
	static constexpr KStringView vary                             = "vary";
	static constexpr KStringView via                              = "via";
	static constexpr KStringView warning                          = "warning";
	static constexpr KStringView www_authenticate                 = "www-authenticate";
	static constexpr KStringView x_forwarded_for                  = "x-forwarded-for";
	static constexpr KStringView x_forwarded_host                 = "x-forwarded-host";
	static constexpr KStringView x_forwarded_proto                = "x-forwarded-proto";
	static constexpr KStringView x_forwarded_server               = "x-forwarded-server";
	static constexpr KStringView x_frame_options                  = "x-frame-options";
	static constexpr KStringView x_powered_by                     = "x-powered-by";

//------
protected:
//------

	//-----------------------------------------------------------------------------
	bool SetError(KStringView sError) const
	//-----------------------------------------------------------------------------
	{
		m_sError = sError;
		return false;
	}

	//-----------------------------------------------------------------------------
	void SplitContentType() const;
	//-----------------------------------------------------------------------------

//------
public:
//------

	KHeaderMap Headers; // response headers read in

	// we store the http version here as it is a shared property
	// of request and response headers
	KString HTTPVersion;

//------
private:
//------

	mutable KString m_sCharset;
	mutable KString m_sContentType;
	mutable KString m_sError;

}; // KHTTPHeaders

} // end of namespace dekaf2
