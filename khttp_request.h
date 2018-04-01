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
#include "kurl.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPRequestHeaders : public KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KHTTPRequestHeaders() = default;
	KHTTPRequestHeaders(const KHTTPRequestHeaders&) = default;
	KHTTPRequestHeaders(KHTTPRequestHeaders&&) = default;
	KHTTPRequestHeaders& operator=(const KHTTPRequestHeaders&) = default;
	KHTTPRequestHeaders& operator=(KHTTPRequestHeaders&&) = default;
	KHTTPRequestHeaders(const KHTTPHeaders& other)
	: KHTTPHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& Stream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasChunking() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns -1 for chunked content, 0 for no content, > 0 = size of content
	std::streamsize ContentLength() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasContent() const;
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	KHTTPMethod Method;
	KURL Resource;

}; // KHTTPRequestHeaders


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KOutHTTPRequest : public KHTTPRequestHeaders, public KOutHTTPFilter
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

	KOutHTTPRequest(KOutHTTPRequest&&) = default;
	KOutHTTPRequest& operator=(KOutHTTPRequest&&) = default;

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

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KInHTTPRequest : public KHTTPRequestHeaders, public KInHTTPFilter
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

	KInHTTPRequest(KInHTTPRequest&&) = default;
	KInHTTPRequest& operator=(KInHTTPRequest&&) = default;

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

};

} // end of namespace dekaf2
