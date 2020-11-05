/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file khttpserver.h
/// HTTP server implementation

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KHTTPServer() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPServer(KStream& Stream, KStringView sRemoteEndpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPServer(const KHTTPServer&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPServer(KHTTPServer&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPServer& operator=(const KHTTPServer&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPServer& operator=(KHTTPServer&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Accept(KStream& Stream, KStringView sRemoteEndpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// receive request and headers (and setup the filtered input stream)
	bool Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write reponse headers (and setup the filtered output stream)
	bool Serialize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the input stream object
	KInStream& InStream()
	//-----------------------------------------------------------------------------
	{
		return Request.FilteredStream();
	}

	//-----------------------------------------------------------------------------
	/// get the output stream object
	KOutStream& OutStream()
	//-----------------------------------------------------------------------------
	{
		return Response.FilteredStream();
	}

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content into stream
	size_t Read(KOutStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Read(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content into string
	size_t Read(KString& sBuffer, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Read(sBuffer, len);
	}

	//-----------------------------------------------------------------------------
	/// Return entire POST/PUT content in a string
	KString Read()
	//-----------------------------------------------------------------------------
	{
		KString sBuffer;
		Read(sBuffer);
		return sBuffer;
	}

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content line by line into string
	bool ReadLine(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Request.ReadLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Stream from instream
	size_t Write(KInStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Write(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Write sBuffer
	size_t Write(KStringView sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.Write(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Write one line, including EOL
	bool WriteLine(KStringView sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.WriteLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Returns the last error string, if any
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	//-----------------------------------------------------------------------------
	/// Automatically check if the output in this connection can be compressed, and
	/// enable compression. This depends on the Accept-Encoding request header and
	/// the client HTTP version. This option is active per default.
	void ConfigureCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bConfigureCompression = bYesNo;
	}

	//-----------------------------------------------------------------------------
	/// Disable uncompress of incoming request, even if the respective request
	/// headers are set.
	void AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Request.AllowUncompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Disable compression of outgoing response, even if the respective response
	/// headers are set.
	void AllowCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.AllowCompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Clear all headers, resource, and error. Keep connection
	void clear();
	//-----------------------------------------------------------------------------

	/// returns the IP address of the immediate client connection, not of any headers
	/// as for GetBrowserIP()
	//-----------------------------------------------------------------------------
	KString GetConnectedClientIP() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we repeat the method from KHTTPRequest here as we want to look into
	// the remote endpoint data of the tcp connection if we do not find the IP
	// in the headers - and the connection details are only known here..
	/// Searches for the original requester's IP address in the Forwarded,
	/// X-Forwarded-For and X-ProxyUser-IP headers (in that order, first found wins),
	/// and if that remains without success returns the IP address of the immediate
	/// client connection as for GetConnectedClientIP()
	KString GetBrowserIP() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get request method as a KHTTPMethod
	KHTTPMethod GetRequestMethod() const
	//-----------------------------------------------------------------------------
	{
        return Request.Method;
	}

	//-----------------------------------------------------------------------------
	/// get request path as a string
	const KString& GetRequestPath() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Path.get();
	}

	//-----------------------------------------------------------------------------
	/// get one query parm value as a const string ref
	const KString& GetQueryParm(KStringView sKey) const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Query.get().Get(sKey);
	}

	//-----------------------------------------------------------------------------
	/// get one query parm value with default value if missing
	KString GetQueryParm(KStringView sKey, KStringView sDefault) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get one query parm value and remove possible injection attempts: single, double, backtick and backslash
	KString GetQueryParmSafe (KStringView sKey, KStringView sDefault="") const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get query parms as a map
	const url::KQueryParms& GetQueryParms() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Query.get();
	}

	//-----------------------------------------------------------------------------
	/// set one query key/value parm
	void SetQueryParm(KStringView sKey, KStringView sValue)
	//-----------------------------------------------------------------------------
	{
		Request.Resource.Query.get().Add(sKey, sValue);
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

//------
private:
//------

	//-----------------------------------------------------------------------------
	void EnableCompressionIfPossible();
	//-----------------------------------------------------------------------------

	mutable KString m_sError;
	long m_Timeout { 30 };
	bool m_bConfigureCompression { true };

//------
public:
//------

	KInHTTPRequest Request;
	KOutHTTPResponse Response;
	KString RemoteEndpoint;

}; // KHTTPServer


} // end of namespace dekaf2
