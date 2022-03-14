/*
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
class DEKAF2_PUBLIC KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct default HTTP server. Call Accept() to associate a stream.
	KHTTPServer() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct HTTP server around a stream
	/// @param Stream the IO stream
	/// @param sRemoteEndpoint IP address of the direct connection
	KHTTPServer(KStream& Stream, KStringView sRemoteEndpoint, url::KProtocol Proto, uint16_t iPort);
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
	KHTTPServer& operator=(KHTTPServer&&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Associate a stream with the HTTP server
	/// @param Stream the IO stream
	/// @param sRemoteEndpoint IP address of the direct connection
	/// @return true if Stream is ready for IO, false otherwise
	bool Accept(KStream& Stream, KStringView sRemoteEndpoint, url::KProtocol Proto, uint16_t iPort);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect from client
	void Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// receive request and headers (and setup the filtered input stream)
	/// @return true if request could be parsed, false otherwise
	bool Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write reponse headers (and setup the filtered output stream)
	/// @return true if response could be serialized, false otherwise
	bool Serialize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the input stream object
	/// @return the (filtered/uncompressed) input stream
	KInStream& InStream()
	//-----------------------------------------------------------------------------
	{
		return Request.FilteredStream();
	}

	//-----------------------------------------------------------------------------
	/// get the output stream object
	/// @return the output stream (that will eventually be fltered/compressed)
	KOutStream& OutStream()
	//-----------------------------------------------------------------------------
	{
		return Response.FilteredStream();
	}

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content into stream
	/// @param Stream the stream to read into
	/// @param len the number of bytes to read, or npos to read until EOF
	/// @return the count of read bytes
	size_t Read(KOutStream& Stream, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Read(Stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content into string
	/// @param sBuffer the string to read into
	/// @param len the number of bytes to read, or npos to read until EOF
	/// @return the count of read bytes
	size_t Read(KStringRef& sBuffer, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Read(sBuffer, len);
	}

	//-----------------------------------------------------------------------------
	/// Return entire POST/PUT content in a string
	/// @return a string with the read content
	KString Read();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read POST/PUT content line by line into string
	/// @param sBuffer the string to read into
	/// @return true if a line was available, false otherwise
	bool ReadLine(KStringRef& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Request.ReadLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Stream from instream
	/// @param Stream the stream to read from
	/// @param len the number of bytes to read, or npos to read until EOF
	/// @return the count of written bytes
	size_t Write(KInStream& Stream, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Write(Stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Write sBuffer
	/// @param sBuffer the string view to read from
	/// @return the count of written bytes
	size_t Write(KStringView sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.Write(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Write one line, including EOL (which is CR/LF)
	/// @param sBuffer the string view to read from
	/// @return the count of written bytes
	size_t WriteLine(KStringView sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.WriteLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Returns the last error string, if any
	/// @return a const ref on the error string
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	//-----------------------------------------------------------------------------
	/// Automatically check if the output in this connection can be compressed, and
	/// enable compression. This depends on the Accept-Encoding request header and
	/// the client HTTP version. This option is active per default after construction of this class.
	/// @param bYesNo if true, check if output can be compressed
	void ConfigureCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bConfigureCompression = bYesNo;
	}

	//-----------------------------------------------------------------------------
	/// Disable uncompress of incoming request, even if the respective request
	/// headers are set.
	/// @param bYesNo if false, do not uncompress input
	void AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Request.AllowUncompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Disable compression of outgoing response, even if the respective response
	/// headers are set.
	/// @param bYesNo if false, do not compress output
	void AllowCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.AllowCompression(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Clear all headers, resource, and error. Keep connection
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the IP address of the immediate client connection, not of any headers
	/// as for GetRemoteIP()
	/// @return a string with the IP address of the connection
	KString GetConnectedClientIP() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the port of the immediate client connection, not of any headers
	/// @return the peer port of the connection
	uint16_t GetConnectedClientPort() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we repeat the method from KHTTPRequest here as we want to look into
	// the remote endpoint data of the tcp connection if we do not find the IP
	// in the headers - and the connection details are only known here..
	/// Searches for the original requester's IP address in the Forwarded,
	/// X-Forwarded-For and X-ProxyUser-IP headers (in that order, first found wins),
	/// and if that remains without success returns the IP address of the immediate
	/// client connection as for GetConnectedClientIP()
	/// @return a string with the IP address of the original requester
	KString GetRemoteIP() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Alias for GetRemoteIP(), now deprecated
	KString GetBrowserIP() const
	//-----------------------------------------------------------------------------
	{
		return GetRemoteIP();
	}

	//-----------------------------------------------------------------------------
	// we repeat the method from KHTTPRequest here as we want to look into
	// the remote endpoint data of the tcp connection if we do not find the protocol
	// in the headers - and the connection details are only known here..
	/// Searches for the original requester's protocol in the Forwarded and X-Forwarded-Proto header
	url::KProtocol GetRemoteProto() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we repeat the method from KHTTPRequest here as we want to look into
	// the remote endpoint data of the tcp connection if we do not find the port
	// in the headers - and the connection details are only known here..
	/// Searches for the original requester's port in the Forwarded header
	uint16_t GetRemotePort() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get request method as a KHTTPMethod
	/// @return the HTTP request method
	KHTTPMethod GetRequestMethod() const
	//-----------------------------------------------------------------------------
	{
        return Request.Method;
	}

	//-----------------------------------------------------------------------------
	/// get request path as a const string ref
	const KString& GetRequestPath() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get one query parm value as a const string ref
	/// @param sKey the name of the requested query parm
	/// @return the value for the requested query parm
	const KString& GetQueryParm(KStringView sKey) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get one query parm value with default value if missing
	/// @param sKey the name of the requested query parm
	/// @param sDefault the default return value if there is no value for the key
	/// @return the value for the requested query parm
	KString GetQueryParm(KStringView sKey, KStringView sDefault) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get query parms as a map
	/// @return const ref on the map object with all query parms
	const url::KQueryParms& GetQueryParms() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set one query key/value parm
	/// @param sKey the name for the query parm
	/// @param sValue the value for the query parm
	template<typename Key, typename Value>
	void SetQueryParm(Key&& sKey, Value&& sValue)
	//-----------------------------------------------------------------------------
	{
		Request.Resource.Query.get().Add(std::forward<Key>(sKey), std::forward<Value>(sValue));
	}

	//-----------------------------------------------------------------------------
	/// returns the name of a user who was successfully authenticated (by another module which sets this user name)
	const KString& GetAuthenticatedUser() const
	//-----------------------------------------------------------------------------
	{
		return m_sAuthenticatedUser;
	}

	//-----------------------------------------------------------------------------
	/// confirm a user name as being authenticated
	void SetAuthenticatedUser(KString sAuthenticatedUser);
	//-----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	/// Set the error string, also outputs to debug log
	/// @param sError the error string
	/// @return always false
	bool SetError(KString sError) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void EnableCompressionIfPossible();
	//-----------------------------------------------------------------------------

	KString          m_sAuthenticatedUser;
	mutable KString  m_sError;
	bool             m_bConfigureCompression { true };

//------
public:
//------

	KInHTTPRequest   Request;
	KOutHTTPResponse Response;
	KString          RemoteEndpoint;
	url::KProtocol   Protocol { url::KProtocol::HTTP };
	uint16_t         Port     { 0 };

}; // KHTTPServer


} // end of namespace dekaf2
