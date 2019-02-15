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
	bool Connect(std::unique_ptr<KConnection> Connection);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& url, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetTimeout(long iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the resource to be requested
	bool Resource(const KURL& url, KHTTPMethod method = KHTTPMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Adds a request header for the next request
	bool RequestHeader(KStringView svName, KStringView svValue, bool bOverwrite = true);
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
		//return ((GetStatusCode() >= 200) && (GetStatusCode() <= 299));
		return Response.Good();
	}

	//-----------------------------------------------------------------------------
	/// evaluates the http status code after a request and returns true if !HttpSuccess()
	bool HttpFailure() const
	//-----------------------------------------------------------------------------
	{
		//return (!HttpSuccess());
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
	void Uncompress(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.Uncompress(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// compress outgoing request?
	void Compress(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Request.Compress(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Clear all headers, resource, and error. Keep connection
	void clear();
	//-----------------------------------------------------------------------------

	// alternative interface

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Get(const KURL& URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (URL, KHTTPMethod::GET, {}, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Get from URL, store response body in return value KString
	KString Options(const KURL& URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (URL, KHTTPMethod::OPTIONS, {}, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Post to URL, store response body in return value KString
	KString Post(const KURL& URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (URL, KHTTPMethod::POST, svRequestBody, MIME, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Deletes URL, store response body in return value KString
	KString Delete(const KURL& URL, KStringView svRequestBody, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return HttpRequest (URL, KHTTPMethod::DELETE, svRequestBody, {}, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	/// Head from URL - returns true if response is in the 2xx range
	bool Head(const KURL& URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (URL, KHTTPMethod::HEAD, {}, {}, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Put to URL - returns true if response is in the 2xx range
	bool Put(const KURL& URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (URL, KHTTPMethod::PUT, svRequestBody, MIME, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Patch URL - returns true if response is in the 2xx range
	bool Patch(const KURL& URL, KStringView svRequestBody, KMIME MIME, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		HttpRequest (URL, KHTTPMethod::PATCH, svRequestBody, MIME, bVerifyCerts);
		return HttpSuccess();
	}

	//-----------------------------------------------------------------------------
	/// Send given request method and return raw response as a string
	KString HttpRequest (const KURL& URL, KStringView sRequestMethod = KHTTPMethod::GET, KStringView svRequestBody = KStringView{}, KMIME MIME = KMIME::JSON, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Return HTTP status code from last request
	uint16_t GetStatusCode() const
	//-----------------------------------------------------------------------------
	{
		return Response.iStatusCode;
	}

//------
protected:
//------
 
	//-----------------------------------------------------------------------------
	bool ReadHeader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetError(KStringView sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if we are already connected to the endpoint in URL
	bool AlreadyConnected(const KURL& URL) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	std::unique_ptr<KConnection> m_Connection;
	mutable KString m_sError;
	long m_Timeout { 30 };
	bool m_bRequestCompression { true };

//------
public:
//------

	KOutHTTPRequest Request;
	KInHTTPResponse Response;

}; // KHTTPClient

//-----------------------------------------------------------------------------
/// Get from URL, store body in return value KString
KString kHTTPGet(const KURL& URL);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Head from URL - returns true if response is in the 2xx range
bool kHTTPHead(const KURL& URL);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Post to URL, store body in return value KString
KString kHTTPPost(const KURL& URL, KStringView svPostData, KStringView svMime);
//-----------------------------------------------------------------------------


} // end of namespace dekaf2
