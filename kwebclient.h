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
#include "kjson.h"
#include "kcookie.h"

/// @file kwebclient.h
/// HTTP client implementation - high level

namespace dekaf2 {

/// Simplified helper method that uses KWebClient to "wget" a file from a remote server
/// to a local filesystem.  Eventually we will support all wget's cli options but currently
/// the Options argument is ignored.
bool KWget (KStringView sURL, const KString& sOutfile, const KJSON& Options=KJSON{});

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KWebClient : public KHTTPClient
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self = KWebClient;
	using TimingCallback_t = std::function<void(const KWebClient&, uint64_t, const KString&)>;

	//-----------------------------------------------------------------------------
	KWebClient(bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response to an output stream - this variant is needed for Unix socket requests, which need a separate URL for the connection target
	bool HttpRequest (KOutStream& OutStream, KURL HostURL, KURL RequestURL, KHTTPMethod RequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, KMIME MIME = KMIME::JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response to an output stream
	bool HttpRequest (KOutStream& OutStream, KURL URL, KHTTPMethod RequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, const KMIME& MIME = KMIME::JSON)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (OutStream, KURL{}, std::move(URL), RequestMethod, svRequestBody, MIME);
	}

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response as a string
	KString HttpRequest (KURL URL, KHTTPMethod RequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, const KMIME& MIME = KMIME::JSON);
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
	KString Get(KURL URL, KStringView svRequestBody, const KMIME& MIME)
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
	KString Post(KURL URL, KStringView svRequestBody, const KMIME& MIME)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::POST, svRequestBody, MIME);
	}

	//-----------------------------------------------------------------------------
	/// Deletes URL, store response body in return value KString
	KString Delete(KURL URL, KStringView svRequestBody = KStringView{}, const KMIME& MIME = KMIME{})
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::DELETE, svRequestBody, MIME);
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
	bool Put(KURL URL, KStringView svRequestBody, const KMIME& MIME)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::PUT, svRequestBody, MIME);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Patch URL - store response body in return value KString
	KString Patch(KURL URL, KStringView svRequestBody, const KMIME& MIME)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::PATCH, svRequestBody, MIME);
	}

	//-----------------------------------------------------------------------------
	/// Set count of allowed redirects (default 3, 0 disables)
	self& AllowRedirects(uint16_t iMaxRedirects = 3)
	//-----------------------------------------------------------------------------
	{
		m_iMaxRedirects = iMaxRedirects;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Allow or forbid automatic conversion of query parms into a form body when there is no
	/// other body - allowed per default
	self& AllowQueryToWWWFormConversion(bool bYesNo = true)
	//-----------------------------------------------------------------------------
	{
		m_bQueryToWWWFormConversion = bYesNo;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Accept or reject Set-Cookie requests - accepted per default
	self& AcceptCookies(bool bYesNo = true)
	//-----------------------------------------------------------------------------
	{
		m_bAcceptCookies = bYesNo;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// call back everytime a web request exceeds the given duration
	self& SetTimingCallback (int64_t iWarnIfOverMilliseconds, TimingCallback_t TimingCallback = nullptr)
	//-----------------------------------------------------------------------------
	{
		m_iWarnIfOverMilliseconds = iWarnIfOverMilliseconds;
		m_TimingCallback = TimingCallback;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// some applications want to keep running details about external service calls
	self& SetServiceSummary (KJSON* pServiceSummary)
	//-----------------------------------------------------------------------------
	{
		m_pServiceSummary = pServiceSummary;
		return *this;
	}

//------
protected:
//------

	KCookies         m_Cookies;
	TimingCallback_t m_TimingCallback { nullptr };
	int64_t          m_iWarnIfOverMilliseconds { 0 };
	KJSON*           m_pServiceSummary { nullptr };   // running details about external service calls
	uint16_t         m_iMaxRedirects { 3 };
	bool             m_bAcceptCookies { true };
	bool             m_bQueryToWWWFormConversion { true };

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
KString kHTTPPost(KURL URL, KStringView svPostData, const KMIME& Mime);
//-----------------------------------------------------------------------------

} // end of namespace dekaf2
