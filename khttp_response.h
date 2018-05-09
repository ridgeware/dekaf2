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
#include "khttp_header.h"
#include "khttpinputfilter.h"
#include "khttpoutputfilter.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPResponseHeaders : public KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTTPResponseHeaders() = default;
	KHTTPResponseHeaders(const KHTTPResponseHeaders&) = default;
	KHTTPResponseHeaders(KHTTPResponseHeaders&&) = default;
	KHTTPResponseHeaders& operator=(const KHTTPResponseHeaders&) = default;
	KHTTPResponseHeaders& operator=(KHTTPResponseHeaders&&) = default;
	KHTTPResponseHeaders(const KHTTPHeaders& other)
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
	void SetStatus(uint16_t iCode, KStringView sMessage)
	//-----------------------------------------------------------------------------
	{
		m_iStatusCode = iCode;
		m_sStatusString = sMessage;
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_iStatusCode <= 299 && m_iStatusCode >= 200;
	}

	//-----------------------------------------------------------------------------
	bool HasChunking() const;
	//-----------------------------------------------------------------------------

//------
public:
//------

	KString  m_sStatusString;
	uint16_t m_iStatusCode;

}; // KHTTPResponseHeaders

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KOutHTTPResponse : public KHTTPResponseHeaders, public KOutHTTPFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs a KOutHTTPResponse without output stream - set one with
	/// SetOutputStream() before actually writing through the class.
	KOutHTTPResponse() = default;
	//-----------------------------------------------------------------------------

	KOutHTTPResponse(KOutHTTPResponse&&) = default;
	KOutHTTPResponse& operator=(KOutHTTPResponse&&) = default;

	//-----------------------------------------------------------------------------
	/// Constructs a KOutHTTPResponse around any output stream
	KOutHTTPResponse(KOutStream& OutStream)
	//-----------------------------------------------------------------------------
	: KOutHTTPFilter(OutStream)
	{}

	//-----------------------------------------------------------------------------
	KOutHTTPResponse(const KHTTPResponseHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPResponseHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	KOutHTTPResponse& operator=(KHTTPResponseHeaders& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPResponseHeaders::operator=(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KOutHTTPResponse& operator=(KHTTPResponseHeaders&& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPResponseHeaders::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	KOutHTTPResponse(const KHTTPHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPResponseHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	bool Serialize();
	//-----------------------------------------------------------------------------

protected:

	//-----------------------------------------------------------------------------
	/// make parent class Parse() inaccessible
	bool Parse(KInStream& Stream)
	//-----------------------------------------------------------------------------
	{
		return KHTTPResponseHeaders::Parse(Stream);
	}

	//-----------------------------------------------------------------------------
	/// make parent class Serialize() inaccessible
	bool Serialize(KOutStream& Stream) const
	//-----------------------------------------------------------------------------
	{
		return KHTTPResponseHeaders::Serialize(Stream);
	}

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KInHTTPResponse : public KHTTPResponseHeaders, public KInHTTPFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs a KInHTTPResponse without input stream - set one with
	/// SetInputStream() before actually reading through the class.
	KInHTTPResponse() = default;
	//-----------------------------------------------------------------------------

	KInHTTPResponse(KInHTTPResponse&&) = default;
	KInHTTPResponse& operator=(KInHTTPResponse&&) = default;

	//-----------------------------------------------------------------------------
	/// Constructs a KInHTTPResponse around any input stream
	KInHTTPResponse(KInStream& InStream)
	//-----------------------------------------------------------------------------
	: KInHTTPFilter(InStream)
	{}

	//-----------------------------------------------------------------------------
	KInHTTPResponse& operator=(const KHTTPResponseHeaders& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPResponseHeaders::operator=(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KInHTTPResponse& operator=(KHTTPResponseHeaders&& other)
	//-----------------------------------------------------------------------------
	{
		KHTTPResponseHeaders::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	KInHTTPResponse(const KHTTPResponseHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPResponseHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	KInHTTPResponse(const KHTTPHeaders& other)
	//-----------------------------------------------------------------------------
	: KHTTPResponseHeaders(other)
	{
	}

	//-----------------------------------------------------------------------------
	bool Parse();
	//-----------------------------------------------------------------------------

protected:

	//-----------------------------------------------------------------------------
	/// make parent class Parse() inaccessible
	bool Parse(KInStream& Stream)
	//-----------------------------------------------------------------------------
	{
		return KHTTPResponseHeaders::Parse(Stream);
	}

	//-----------------------------------------------------------------------------
	/// make parent class Serialize() inaccessible
	bool Serialize(KOutStream& Stream) const
	//-----------------------------------------------------------------------------
	{
		return KHTTPResponseHeaders::Serialize(Stream);
	}

};


} // end of namespace dekaf2
