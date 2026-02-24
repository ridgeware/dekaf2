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

#include "khttp_method.h"
#include "khttp_header.h"
#include "khttpoutputfilter.h"
#include "khttpinputfilter.h"
#include "kstring.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds information about the original request line of an incoming request
class DEKAF2_PUBLIC KInHTTPRequestLine
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KInHTTPRequestLine() = default;

	/// construct from request line string
	KInHTTPRequestLine(KString sRequestLine)
	{
		Parse(std::move(sRequestLine));
	}

	/// parse the request line and check meticulously its format
	std::vector<KStringView> Parse(KString sRequestLine);
	/// is this a valid request line?
	bool                     IsValid()     const { return m_iMethodLen > 0; }
	/// get  the full string of the request line, even if it was not valid
	const KString&           Get()         const { return m_sFull;          }
	/// get the method string, empty if not valid
	KStringView              GetMethod()   const;
	/// get the resource string (path+query), empty if not valid
	KStringView              GetResource() const;
	/// get the path string, empty if not valid
	KStringView              GetPath()     const;
	/// get the query string, empty if not valid
	KStringView              GetQuery()    const;
	/// get the http version string, empty if not valid
	KStringView              GetVersion()  const;
	/// clear all content
	void                     clear();

//------
private:
//------

	KString      m_sFull;
	uint16_t     m_iPathLen    { 0 };
	uint8_t      m_iMethodLen  { 0 };
	uint8_t      m_iVersionLen { 0 };

//------
public:
//------

	static constexpr auto MAX_REQUESTLINELENGTH = std::numeric_limits<decltype(m_iPathLen)>::max();

}; // KInHTTPRequestLine

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPRequestHeaders : public KHTTPHeaders, public KHTTPTrustedRemoteEndpoint
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KHTTPRequestHeaders() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPRequestHeaders(const KHTTPHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// serialize request line and headers into Stream
	/// @param Stream output stream to write to
	bool Serialize(KOutStream& Stream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// serialize sent or received request line into Stream (included by Serialize())
	/// @param Stream output stream to write to
	bool SerializeRequestLine(KOutStream& Stream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Checks if chunked transfer is supported by the client
	bool HasChunking() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns either zstd, xz, lzma, gzip, deflate or bzip2 if either compression is supported by
	/// the client and the HTTP protocol is at least 1.1
	KStringView SupportedCompression() const;
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	KHTTPMethod        Method;
	KResource          Resource;
	/// for logging purposes on incoming requests we store a copy of the original
	/// request line
	KInHTTPRequestLine RequestLine;

}; // KHTTPRequestHeaders


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOutHTTPRequest : public KHTTPRequestHeaders, public KOutHTTPFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs a KOutHTTPRequest without output stream - set one with
	/// SetOutputStream() before actually writing through the class.
	KOutHTTPRequest() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a KOutHTTPRequest around any output stream
	KOutHTTPRequest(KOutStream& OutStream)
	//-----------------------------------------------------------------------------
	: KOutHTTPFilter(OutStream)
	{}

	//-----------------------------------------------------------------------------
	KOutHTTPRequest(const KHTTPRequestHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPRequestHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	KOutHTTPRequest& operator=(const KHTTPRequestHeaders& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPRequestHeaders::operator=(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KOutHTTPRequest& operator=(KHTTPRequestHeaders&& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPRequestHeaders::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	KOutHTTPRequest(const KHTTPHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPRequestHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	/// Serialize / write the request and headers to the output stream.
	bool Serialize();
	//-----------------------------------------------------------------------------

protected:

	//-----------------------------------------------------------------------------
	/// make parent class Parse() inaccessible
	bool Parse(KInStream& Stream)
	//-----------------------------------------------------------------------------
	{
		return KHTTPRequestHeaders::Parse(Stream);
	}

	//-----------------------------------------------------------------------------
	/// make parent class Serialize() inaccessible
	bool Serialize(KOutStream& Stream) const
	//-----------------------------------------------------------------------------
	{
		return KHTTPRequestHeaders::Serialize(Stream);
	}

}; // KOutHTTPRequest

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KInHTTPRequest : public KHTTPRequestHeaders, public KInHTTPFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs a KInHTTPRequest without input stream - set one with
	/// SetInputStream() before actually reading through the class.
	KInHTTPRequest() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a KInHTTPRequest around any input stream
	KInHTTPRequest(KInStream& InStream)
	//-----------------------------------------------------------------------------
		: KInHTTPFilter(InStream)
	{}

	//-----------------------------------------------------------------------------
	KInHTTPRequest(const KHTTPRequestHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPRequestHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	KInHTTPRequest& operator=(const KHTTPRequestHeaders& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPRequestHeaders::operator=(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KInHTTPRequest& operator=(KHTTPRequestHeaders&& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPRequestHeaders::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	KInHTTPRequest(const KHTTPHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPRequestHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	/// Parse / receive headers from input stream. Returns after the header is completed
	/// or stream read timeout occured.
	bool Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Retrieve the given cookie value from the COOKIE header.
	/// Does not throw, nor error: it just returns the empty string if
	/// there is no COOKIE header or if the given cookie is missing.
	/// Note: cookie names are case-sensitive.
	KStringView GetCookie (KStringView sCookieName) const;
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// make parent class Parse() inaccessible
	bool Parse(KInStream& Stream)
	//-----------------------------------------------------------------------------
	{
		return KHTTPRequestHeaders::Parse(Stream);
	}

	//-----------------------------------------------------------------------------
	/// make parent class Serialize() inaccessible
	bool Serialize(KOutStream& Stream) const
	//-----------------------------------------------------------------------------
	{
		return KHTTPRequestHeaders::Serialize(Stream);
	}

}; // KInHTTPRequest

DEKAF2_NAMESPACE_END
