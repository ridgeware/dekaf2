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
#include "khttp_method.h"
#include "kuseragent.h"

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
	KHTTPClient() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server
	KHTTPClient(const KURL& url, KHTTPMethod method = KHTTPMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server
	KHTTPClient(std::unique_ptr<KConnection> stream, const KURL& url = KURL{}, KHTTPMethod method = KHTTPMethod::GET);
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
	bool RequestHeader(KStringView svName, KStringView svValue);
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
	const KHTTPResponse& GetResponseHeader() const
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

	//-----------------------------------------------------------------------------
	KHTTPResponse& GetResponseHeader()
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

	//-----------------------------------------------------------------------------
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	// alternative interface

	//-----------------------------------------------------------------------------
	/// Get from URL, store in return value KString
	KString Get(const KURL& URL);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Post to URL, store in return value KString
	KString Post(const KURL& URL, KStringView svPostData, KStringView svMime);
	//-----------------------------------------------------------------------------

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
	bool SetError(KStringView sError) const
	//-----------------------------------------------------------------------------
	{
		m_sError = sError;
		return false;
	}

	//-----------------------------------------------------------------------------
	/// Returns true if we are already connected to the endpoint in URL
	bool AlreadyConnected(const KURL& URL) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	std::unique_ptr<KConnection> m_Connection;
	KHTTPMethod m_Method;
	KHTTPResponse m_ResponseHeader;
	mutable KString m_sError;
	size_t m_iRemainingContentSize{0};
	State m_State{State::CLOSED};
	long m_Timeout{ 300 };
	bool m_bTEChunked;
	bool m_bReceivedFinalChunk;

}; // KHTTPClient

//-----------------------------------------------------------------------------
/// Get from URL, store body in return value KString
KString kHTTPGet(const KURL& URL);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Post to URL, store body in return value KString
KString kHTTPPost(const KURL& URL, KStringView svPostData, KStringView svMime);
//-----------------------------------------------------------------------------


} // end of namespace dekaf2
