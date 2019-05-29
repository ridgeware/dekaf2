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
#include "khttp_response.h"
#include "khttp_request.h"
#include "khttp_method.h"
#include "kmime.h"
#include "kurl.h"

/// @file khttpclient.h
/// HTTP client implementation

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPClient
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KHTTPClient() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server and sets method and resource
	KHTTPClient(const KURL& url, KHTTPMethod method = KHTTPMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server via proxy and sets method and resource
	KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method = KHTTPMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, takes an existing connection to a server
	KHTTPClient(std::unique_ptr<KConnection> stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient(KHTTPClient&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient& operator=(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient& operator=(KHTTPClient&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set proxy server for all subsequent connects
	void SetProxy(KURL Proxy)
	//-----------------------------------------------------------------------------
	{
		m_Proxy = std::move(Proxy);
	}

	//-----------------------------------------------------------------------------
	bool Connect(std::unique_ptr<KConnection> Connection);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& url, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& url, const KURL& Proxy, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetTimeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the resource to be requested
	bool Resource(const KURL& url, KHTTPMethod method = KHTTPMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Adds a request header for the next request
	bool SetRequestHeader(KStringView svName, KStringView svValue, bool bOverwrite = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SendRequest(KStringView svPostData = KStringView{}, KMIME Mime = KMIME::TEXT_PLAIN);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write request headers (and setup the filtered output stream)
	bool Serialize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// receive response and headers (and setup the filtered input stream)
	bool Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// POST/PUT from stream
	size_t Write(KInStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Write(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Stream into outstream
	size_t Read(KOutStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KString& sBuffer, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(sBuffer, len);
	}

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.ReadLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Connection && m_Connection->Good();
	}

	//-----------------------------------------------------------------------------
	/// evaluates the http status code after a request and returns true 200 >= code <= 299
	bool HttpSuccess() const
	//-----------------------------------------------------------------------------
	{
		return Response.Good();
	}

	//-----------------------------------------------------------------------------
	/// evaluates the http status code after a request and returns true if !HttpSuccess()
	bool HttpFailure() const
	//-----------------------------------------------------------------------------
	{
		return !(Response.Good());
	}

	//-----------------------------------------------------------------------------
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	//-----------------------------------------------------------------------------
	void RequestCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bRequestCompression = bYesNo;
	}

	//-----------------------------------------------------------------------------
	/// uncompress incoming response?
	void AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.AllowUncompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// compress outgoing request?
	void AllowCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Request.AllowCompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Clear all headers, resource, and error. Keep connection
	void clear();
	//-----------------------------------------------------------------------------

	// alternative interface

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Get(KURL URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::GET, {}, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Options(KURL URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::OPTIONS, {}, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Post to URL, store response body in return value KString
	KString Post(KURL URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::POST, svRequestBody, MIME, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Deletes URL, store response body in return value KString
	KString Delete(KURL URL, KStringView svRequestBody, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (std::move(URL), KHTTPMethod::DELETE, svRequestBody, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Head from URL - returns true if response is in the 2xx range
	bool Head(KURL URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::HEAD, {}, {}, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Put to URL - returns true if response is in the 2xx range
	bool Put(KURL URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::PUT, svRequestBody, MIME, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Patch URL - returns true if response is in the 2xx range
	bool Patch(KURL URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (std::move(URL), KHTTPMethod::PATCH, svRequestBody, MIME, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response as a string
	KString HttpRequest (KURL URL, KStringView sRequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, KMIME MIME = KMIME::JSON, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Return HTTP status code from last request
	uint16_t GetStatusCode() const
	//-----------------------------------------------------------------------------
	{
		return Response.iStatusCode;
	}

	//-----------------------------------------------------------------------------
	/// Allow auto configuration of proxy server from environment variables?
	void AutoConfigureProxy(bool bYes = true)
	//-----------------------------------------------------------------------------
	{
		m_bAutoProxy = bYes;
	}

	//-----------------------------------------------------------------------------
	/// Set count of allowed redirects (default 3, 0 disables)
	void AllowRedirects(uint16_t iMaxRedirects = 3)
	//-----------------------------------------------------------------------------
	{
		m_iMaxRedirects = iMaxRedirects;
	}

	//-----------------------------------------------------------------------------
	/// Allows to manually configure a host header that is not derived from the
	/// connected URL
	void ForceHostHeader(KStringView sHost)
	//-----------------------------------------------------------------------------
	{
		m_sForcedHost = sHost;
	}

//------
protected:
//------
 
	//-----------------------------------------------------------------------------
	/// Set an error string
	bool SetError(KStringView sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if we are already connected to the endpoint in URL
	bool AlreadyConnected(const KURL& URL) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if url is not exempt from proxying through the comma delimited
	/// sNoProxy list. A leading dot means that only the end of the strings are
	/// compared.
	static bool FilterByNoProxyList(const KURL& url, KStringView sNoProxy);
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	bool ReadHeaders();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetHostHeader(const KURL& url, bool bForcePort = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool CheckForRedirect(KURL& URL, KStringView& sRequestMethod);
	//-----------------------------------------------------------------------------

	std::unique_ptr<KConnection> m_Connection;
	mutable KString m_sError;
	KString m_sForcedHost;
	KURL m_Proxy;
	int  m_Timeout { 30 };
	uint16_t m_iMaxRedirects { 3 };
	bool m_bRequestCompression { true };
	bool m_bAutoProxy { false };
	bool m_bUseHTTPProxyProtocol { false };

//------
public:
//------

	KOutHTTPRequest Request;
	KInHTTPResponse Response;

}; // KHTTPClient

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
