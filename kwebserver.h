/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2024, Ridgeware, Inc.
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

#include "kfileserver.h"
#include "khttp_method.h"
#include "khttp_header.h"
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include <functional>

/// @file kwebserver.h
/// a web server implementation

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Simple web (file) server implementation. This class keeps state about ONE file at a time when starting a new
/// request with Serve(). If you want concurrent access from multiple threads on the same pool of files,
/// then use one instance of KWebServer per thread.
/// The main purpose of this implementation is to maximise security, detect invalid requests, and assure
/// correct HTTP protocol handling.
/// This is NOT a "web server" implementation - it handles single requests for static files in the context of
/// e.g. a KRESTServer instance, which itself is again a per-thread instance called inside KREST.
/// KREST is what comes closest to a "web server"
class DEKAF2_PUBLIC KWebServer : protected KFileServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using CheckMethod = std::function<bool(KHTTPMethod, KStringView)>;

	/// Simple web server implementation - errors lead to KHTTPErrors thrown
	/// @param sDirIndexFile file name to serve when resource is a directory, per default index.html
	KWebServer(KString sDirIndexFile = "index.html")
	: KFileServer(std::move(sDirIndexFile))
	{
	}

	/// serve static pages - throws a KHTTPError on various conditions, or returns either a 200 or 206
	/// state for success. Subsequently call GetStreamForReading() to read the selected file.
	/// @param sDocumentRoot The file system directory that contains all served files.
	/// @param sResourcePath The normalized resource request with the full external path,
	/// but stripped by the base route prefix and an eventual trailing slash.
	/// @param bHadTrailingSlash did the original request have a trailing slash, so that we can 
	/// search for the index file?
	/// @param sRoute The base path valid for this request. Will be substracted from sRequest.
	/// @param Method The HTTP method of the request
	/// @param RequestHeaders The request headers of the request
	/// @param ResponseHeaders A reference to the response headers for the response
	/// @param Check A function that returns true if the sResourcePath would have been a valid
	/// REST route with a different HTTP Method (defaults to nullptr) - is only important if you
	/// want to return a HTTP-405 instead of a HTTP-404 in case of wrong method for any of the
	/// other routes in your REST or HTTP server
	uint16_t Serve(KStringView         sDocumentRoot,
	               KStringView         sResourcePath,
	               bool                bHadTrailingSlash,
	               KStringView         sRoute,
	               KHTTPMethod         Method,
	               const KHTTPHeaders& RequestHeaders,
	               KHTTPHeaders&       ResponseHeaders,
	               const CheckMethod&  Check = nullptr);
	/// returns a HTTP 200 or 206 status to be used for the HTTP response
	uint16_t GetStatus()   const { return m_iStatus;   }
	/// returns file size, may be shorter than full file size in result of range requests
	uint64_t GetFileSize() const { return m_iFileSize; }
	/// returns file stream pointer to output file
	std::unique_ptr<KInStream> GetStreamForReading() { return KFileServer::GetStreamForReading(m_iFileStart); }

	using KFileServer::GetMIMEType;

//------
private:
//------

	uint64_t m_iFileStart { 0 };
	uint64_t m_iFileSize  { 0 };
	uint16_t m_iStatus    { 0 };

}; // KWebServer

DEKAF2_NAMESPACE_END
