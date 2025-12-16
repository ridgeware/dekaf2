/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

/// @file kwebsocketclient.h
/// websocket client implementation

#include "kstring.h"
#include "kstringview.h"
#include "kwebclient.h"
#include "khttperror.h"
#include "kwebsocket.h"

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// websocket client implementation that connects and upgrades to a http(s) or ws(s) server
class DEKAF2_PUBLIC KWebSocketClient : protected KWebClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self = KWebSocketClient;
	using base = KWebClient;

	/// Default ctor - call SetURL() before any request
	KWebSocketClient     (KHTTPStreamOptions = KHTTPStreamOptions{});

	/// Construct with URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	KWebSocketClient     (KURL URL, KHTTPStreamOptions);

	/// Set URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	self& SetURL    (KURL URL, KHTTPStreamOptions);

	/// Get the API URL, const version
	const KURL& GetURL() const      { return m_URL;                                              }

	/// Get the API URL, non-const version
	KURL& URL()                     { return m_URL;                                              }

	/// Send the upgrade request without body to the target
	/// Throws or sets error object for non-200 responses.
	/// @param sWebSocketProtocols requested web socket protocols, like "chat, superchat", comma separated, default empty
	bool Connect (KStringView sWebSocketProtocols = KStringView{});

	/// Set the Path for the upgrade request - will be appended to a base path set at the constructor
	self& Path      (KString sPath);
	/// Set the Query part for the next request
	self& SetQuery  (url::KQuery Query);
	/// Set binary mode, default is text mode
	self& SetBinary (bool bYesNo = true);
	/// Add (overwrite) a request header to existing headers
	self& AddHeader(KHTTPHeader Header, KStringView sValue);

	/// Return a header's content from the response
	const KString& GetResponseHeader(KHTTPHeader Header) const { return Response.Headers.Get(Header); }

	/// clear all state except the ctor parameters - will be called automatically after a
	/// Request() and before the next setup of a request
	void clear ();

	// the web socket abstraction for reading and writing

	/// write from stream, creates one or more sent frames with continuation
	bool Write(KInStream& stream, std::size_t len = npos);

	/// write from string, creates one sent frame
	bool Write(KString sBuffer);

	/// Read a character
	/// @returns std::istream::traits_type::eof() (== -1) if no input available
	std::istream::int_type Read();

	/// read into outstream, returns read character count - always only reads max one
	/// WebSocket frame
	std::size_t Read(KOutStream& stream, std::size_t len = npos);

	/// append to sBuffer, returns read character count - always only reads max one
	/// WebSocket frame
	std::size_t Read(KStringRef& sBuffer, std::size_t len = npos);

	/// read one line into sBuffer, including EOL - not yet implemented
	bool ReadLine(KStringRef& sBuffer, std::size_t maxlen = npos);

	// inherited protected methods lifted into public access

	using base::Good;
	using base::HttpSuccess;
	using base::HttpFailure;
	using base::GetStatusCode;
	using base::GetStatusString;
	using base::GetLastError;
	using base::HasError;
	using base::SetThrowOnError;
	using base::Authentication;
	using base::BasicAuthentication;
	using base::DigestAuthentication;
	using base::TokenAuthentication;
	using base::ClearAuthentication;
	using base::SetTimeout;
	using base::SetProxy;
	using base::AutoConfigureProxy;
	using base::AllowRedirects;
	using base::AllowConnectionRetry;
	using base::SetStreamOptions;
	using base::GetStreamOptions;
	using base::SetVerifyCerts;
	using base::GetVerifyCerts;
	using base::AcceptCookies;
	using base::GetCookies;
	using base::SetTimingCallback;
	using base::SetServiceSummary;
	using base::GetConnectedEndpoint;
	using base::GetEndpointAddress;
	using base::GetStream;

//----------
protected:
//----------

	bool GetNextFrameIfEmpty();

	KURL        m_URL;
	KString     m_sPath;
	url::KQuery m_Query;
	bool        m_bIsBinary { false };
	class KWebSocket::Frame m_RXFrame;
	KStringView m_sRXBuffer;

}; // KWebSocketClient

DEKAF2_NAMESPACE_END
