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

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC DEKAF2_DEPRECATED("no more maintained") KHTTPUserAgent
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KHTTPUserAgent() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	KHTTPUserAgent(KStringView svUserAgent)
	//-----------------------------------------------------------------------------
	{
		m_svUserAgent = Translate(svUserAgent);
	}

	//-----------------------------------------------------------------------------
	constexpr
	KHTTPUserAgent& operator=(KStringView svUserAgent)
	//-----------------------------------------------------------------------------
	{
		m_svUserAgent = Translate(svUserAgent);
		return *this;
	}

	//-----------------------------------------------------------------------------
	constexpr
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return m_svUserAgent;
	}

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

//------
protected:
//------

	//-----------------------------------------------------------------------------
	static constexpr KStringView Translate (KStringView svUserAgent)
	//------------------------------------------------------------------------------
	{
		if (svUserAgent == "google") {
			svUserAgent = GOOGLE;
		}
		else if (svUserAgent == "wget") {
			svUserAgent = WGET;
		}
		else if (svUserAgent == "ie9") {
			svUserAgent = IE9;
		}
		else if (svUserAgent == "ie8") {
			svUserAgent = IE8;
		}
		else if (svUserAgent == "ie" || svUserAgent == "ie7") {
			svUserAgent = IE7;
		}
		else if (svUserAgent == "ie6") {
			svUserAgent = IE6;
		}
		else if (svUserAgent == "ff" || svUserAgent == "ff2") {
			svUserAgent = FF2;
		}
		else if (svUserAgent == "ff3") {
			svUserAgent = FF3;
		}
		else if (svUserAgent == "ff9") {
			svUserAgent = FF9;
		}
		else if (svUserAgent == "safari") {
			svUserAgent = SAFARI;
		}
		else if (svUserAgent == "chrome") {
			svUserAgent = CHROME;
		}
		else if (svUserAgent == "android") {
			svUserAgent = ANDROID;
		}
		else if (svUserAgent == "ipad") {
			svUserAgent = IPAD;
		}
		else if (svUserAgent == "iphone") {
			svUserAgent = IPHONE;
		}

		return (svUserAgent);

	} // TranslateUserAgent

//------
private:
//------

	KStringView m_svUserAgent{};

};

DEKAF2_NAMESPACE_END
