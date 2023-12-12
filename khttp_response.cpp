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

#include "khttp_response.h"
#include "khttperror.h"

DEKAF2_NAMESPACE_BEGIN


//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	if (!Stream.ReadLine(sLine))
	{
		// this is simply a read timeout, probably on a keep-alive
		// connection. Unset the error string and return false;
		return SetError("");
	}

	if (sLine.empty())
	{
		return SetError("empty status line");
	}

	// analyze protocol and status
	// HTTP/1.1 200 Message with arbitrary words

	auto Words = sLine.Split(" ");

	if (Words.size() < 2)
	{
		// garbage, bail out
		return SetError("cannot read HTTP response status");
	}

	sHTTPVersion = Words[0];
	iStatusCode = Words[1].UInt16();

	if (Words.size() > 2)
	{
		// this actually copies the reminder of the sLine
		// into m_sMessage. It looks dangerous but is absolutely
		// clean, as data() returns a pointer into sLine, which
		// itself is 0-terminated
		sStatusString.assign(Words[2].data());
	}

	return KHTTPHeaders::Parse(Stream);

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (sHTTPVersion.empty())
	{
		return SetError("missing http version");
	}
	
	if (!Stream.FormatLine("{} {} {}", sHTTPVersion, iStatusCode, sStatusString))
	{
		return SetError("Cannot write headers");
	}

	return KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	return Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked";

} // HasChunking

//-----------------------------------------------------------------------------
void KHTTPResponseHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	sHTTPVersion.clear();
	sStatusString.clear();
	iStatusCode = 0;

} // clear

//-----------------------------------------------------------------------------
void KHTTPResponseHeaders::SetStatus(uint16_t iCode, KStringView sMessage)
//-----------------------------------------------------------------------------
{
	iStatusCode = iCode;

	if (sMessage.empty())
	{
		sStatusString = KHTTPError::GetStatusString(iStatusCode);
	}
	else
	{
		sStatusString = sMessage;
	}

} // SetStatus

//-----------------------------------------------------------------------------
bool KOutHTTPResponse::Serialize()
//-----------------------------------------------------------------------------
{
	// set up the chunked writer
	return KOutHTTPFilter::Parse(*this) && KHTTPResponseHeaders::Serialize(UnfilteredStream());

} // Serialize

//-----------------------------------------------------------------------------
bool KInHTTPResponse::Parse()
//-----------------------------------------------------------------------------
{
	// set up the chunked reader
	return KHTTPResponseHeaders::Parse(UnfilteredStream()) && KInHTTPFilter::Parse(*this, iStatusCode);

} // Parse

//-----------------------------------------------------------------------------
bool KInHTTPResponse::Fail() const
//-----------------------------------------------------------------------------
{
	if (KInHTTPFilter::Fail())
	{
		// check if we have to set an appropriate error code, maybe
		// we had a timeout after already receiving a bad status code
		if (KHTTPResponseHeaders::GetStatusCode() == 0 || KHTTPResponseHeaders::Good())
		{
			// set a read error - we cast the const away..
			const_cast<KInHTTPResponse*>(this)->KHTTPResponseHeaders::SetStatus(KHTTPError::H5xx_READTIMEOUT, "NETWORK READ ERROR");
		}
	}

	return !KHTTPResponseHeaders::Good();

} // Fail

DEKAF2_NAMESPACE_END
