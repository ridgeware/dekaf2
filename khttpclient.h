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

	using KMethod    = detail::http::KMethod;
	using KHeader    = detail::http::KHeader;
	using KUserAgent = detail::http::KUserAgent;

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
	KHTTPClient(const KURL& url, KMethod method = KMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server
	KHTTPClient(KStringView sUrl, KMethod method = KMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server
	KHTTPClient(std::unique_ptr<KConnection> stream, const KURL& url = KURL{}, KMethod method = KMethod::GET);
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
	bool Connect(KStringView sUrl, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetTimeout(long iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the resource to be requested
	KHTTPClient& Resource(const KURL& url, KMethod method = KMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Adds a request header for the next request
	KHTTPClient& RequestHeader(KStringView svName, KStringView svValue);
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

	std::unique_ptr<KConnection> m_Stream;
	KMethod  m_Method;
	KHeader  m_ResponseHeader;
	size_t   m_iRemainingContentSize{0};
	State    m_State{State::CLOSED};
	long     m_Timeout{ 300 };
	bool     m_bTEChunked;

}; // KHTTPClient


} // end of namespace dekaf2
