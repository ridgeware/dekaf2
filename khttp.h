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

namespace dekaf2 {

namespace KHTTP {
/*
// Common headers (to not be harcoded in the code)
	static constexpr KStringView HostHeader          = "host";
	static constexpr KStringView CookieHeader        = "cookie";
	static constexpr KStringView UserAgentHeader     = "user-agent";
*/
namespace KCharSet {

	static constexpr KStringView ANY_ISO8859         = "ISO-8859"; /*-1...*/
	static constexpr KStringView DEFAULT_CHARSET     = "WINDOWS-1252";

} // end of namespace KCharSet

namespace KHeader {

	static constexpr KStringView AUTHORIZATION       = "Authorization";
	static constexpr KStringView HOST                = "Host";
	static constexpr KStringView HOST_OVERRIDE       = "HostOverride";
	static constexpr KStringView USER_AGENT          = "User-Agent";
	static constexpr KStringView CONTENT_TYPE        = "Content-Type";
	static constexpr KStringView CONTENT_LENGTH      = "Content-Length";
	static constexpr KStringView CONTENT_MD5         = "Content-MD5";
	static constexpr KStringView CONNECTION          = "Connection";
	static constexpr KStringView LOCATION            = "Location";
	static constexpr KStringView REQUEST_COOKIE      = "Cookie";
	static constexpr KStringView RESPONSE_COOKIE     = "Set-Cookie";
	static constexpr KStringView ACCEPT_ENCODING     = "Accept-Encoding";
	static constexpr KStringView TRANSFER_ENCODING   = "Transfer-Encoding";
	static constexpr KStringView VARY                = "Vary";
	static constexpr KStringView REFERER             = "Referer";
	static constexpr KStringView ACCEPT              = "Accept";
	static constexpr KStringView X_FORWARDED_FOR     = "X-Forwarded-For";

	static constexpr KStringView authorization       = "authorization";
	static constexpr KStringView host                = "host";
	static constexpr KStringView host_override       = "hostoverride";
	static constexpr KStringView user_agent          = "user-agent";
	static constexpr KStringView content_type        = "content-type";
	static constexpr KStringView content_length      = "content-length";
	static constexpr KStringView content_md5         = "content-md5";
	static constexpr KStringView connection          = "connection";
	static constexpr KStringView location            = "location";
	static constexpr KStringView request_cookie      = "cookie";
	static constexpr KStringView response_cookie     = "set-cookie";
	static constexpr KStringView accept_encoding     = "accept-encoding";
	static constexpr KStringView transfer_encoding   = "transfer-encoding";
	static constexpr KStringView vary                = "vary";
	static constexpr KStringView referer             = "referer";
	static constexpr KStringView accept              = "accept";
	static constexpr KStringView x_forwarded_for     = "x-forwarded-for";

} // end of namespace KHeader

} // end of namespace KHTTP

namespace KMIME {

	static constexpr KStringView JSON_UTF8           = "application/json; charset=UTF-8";
	static constexpr KStringView HTML_UTF8           = "text/html; charset=UTF-8";
	static constexpr KStringView XML_UTF8            = "text/xml; charset=UTF-8";
	static constexpr KStringView SWF                 = "application/x-shockwave-flash";

} // end of namespace KMIME

class KUserAgent
{

public:

	static KStringView Translate (KStringView sUserAgent);

	static constexpr KStringView IE6     = "Mozilla/4.0 (compatible; MSIE 6.01; Windows NT 6.0)";
	static constexpr KStringView IE7     = "Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.0; en-US)";
	static constexpr KStringView IE8     = "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; GTB6; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022; InfoPath.2)";
	static constexpr KStringView IE9     = "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)";
	static constexpr KStringView FF2     = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.13) Gecko/20080311 Firefox/2.0.0.13";
	static constexpr KStringView FF3     = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9pre) Gecko/2008040318 Firefox/3.0pre (Swiftfox)";
	static constexpr KStringView FF9     = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.6; rv:9.0) Gecko/20100101 Firefox/9.0";
	static constexpr KStringView FF25    = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:25.0) Gecko/20100101 Firefox/25.0";
	static constexpr KStringView SAFARI  = "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_8; en-us) AppleWebKit/530.19.2 (KHTML, like Gecko) Version/4.0.2 Safari/530.19";
	static constexpr KStringView CHROME  = "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/535.2 (KHTML, like Gecko) Chrome/15.0.874.121 Safari/535.2";
	static constexpr KStringView WGET    = "Wget/1.10.2 (Red Hat modified)";
	static constexpr KStringView GOOGLE  = "Googlebot/2.1 (+http://www.google.com/bot.html)";
	static constexpr KStringView ANDROID = "Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; ADR6300 Build/GRJ22) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1";
	static constexpr KStringView IPHONE  = "Mozilla/5.0 (iPhone; U; CPU like Mac OS X; en) AppleWebKit/420+ (KHTML, like Gecko) Version/3.0 Mobile/1A543 Safari/419.3";
	static constexpr KStringView IPAD    = "Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 Mobile/7B334b Safari/531.21.10";

};


} // end of namespace dekaf2
