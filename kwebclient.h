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
#include "kmime.h"
#include "kurl.h"
#include "khttpclient.h"

/// @file kwebclient.h
/// HTTP client implementation - high level

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KWebClient : public KHTTPClient
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KWebClient(bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response as a string
	KString HttpRequest (KURL URL, KStringView sRequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, KMIME MIME = KMIME::JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Get(KURL URL)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::GET, {}, {});
	}

	//-----------------------------------------------------------------------------
	/// Get from URL, with request body, store response body in return value KString
	KString Get(KURL URL, KStringView svRequestBody, KMIME MIME)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::GET, svRequestBody, MIME);
	}

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Options(KURL URL)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::OPTIONS, {}, {});
	}

	//-----------------------------------------------------------------------------
	/// Post to URL, store response body in return value KString
	KString Post(KURL URL, KStringView svRequestBody, KMIME MIME)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::POST, svRequestBody, MIME);
	}

	//-----------------------------------------------------------------------------
	/// Deletes URL, store response body in return value KString
	KString Delete(KURL URL, KStringView svRequestBody)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::DELETE, svRequestBody, {});
	}

	//-----------------------------------------------------------------------------
	/// Head from URL - returns true if response is in the 2xx range
	bool Head(KURL URL)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::HEAD, {}, {});
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Put to URL - returns true if response is in the 2xx range
	bool Put(KURL URL, KStringView svRequestBody, KMIME MIME)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::PUT, svRequestBody, MIME);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Patch URL - returns true if response is in the 2xx range
	bool Patch(KURL URL, KStringView svRequestBody, KMIME MIME)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::PATCH, svRequestBody, MIME);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Set count of allowed redirects (default 3, 0 disables)
	void AllowRedirects(uint16_t iMaxRedirects = 3)
	//-----------------------------------------------------------------------------
	{
		m_iMaxRedirects = iMaxRedirects;
	}

//------
protected:
//------

	uint16_t m_iMaxRedirects { 3 };

}; // KWebClient

//-----------------------------------------------------------------------------
/// Get from URL, store body in return value KString
KString kHTTPGet(KURL URL);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Head from URL - returns true if response is in the 2xx range
bool kHTTPHead(KURL URL);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Post to URL, store body in return value KString
KString kHTTPPost(KURL URL, KStringView svPostData, KStringView svMime);
//-----------------------------------------------------------------------------

} // end of namespace dekaf2