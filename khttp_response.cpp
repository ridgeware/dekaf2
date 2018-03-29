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

namespace dekaf2 {


//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	if (!Stream.ReadLine(sLine))
	{
		return SetError("cannot read from stream");
	}

	if (sLine.empty())
	{
		return SetError("empty status line");
	}

	// analyze protocol and status
	// HTTP/1.1 200 Message with arbitrary words

	std::vector<KStringView> Words;
	Words.reserve(2);
	kSplit(Words, sLine, " ");

	if (Words.size() < 2)
	{
		// garbage, bail out
		return SetError("cannot read HTTP response status");
	}

	HTTPVersion = Words[0];
	StatusCode = Words[1].UInt16();

	if (Words.size() > 2)
	{
		// this actually copies the reminder of the sLine
		// into m_sMessage. It looks dangerous but is absolutely
		// clean, as data() returns a pointer into sLine, which
		// itself is 0-terminated
		StatusString.assign(Words[2].data());
	}

	return KHTTPHeaders::Parse(Stream);

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (HTTPVersion.empty())
	{
		SetError("missing http version");
	}
	
	Stream.FormatLine("{} {} {}", HTTPVersion, StatusCode, StatusString);

	return KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	return Headers.Get(KHTTPHeaders::transfer_encoding) == "chunked";

} // HasChunking

//-----------------------------------------------------------------------------
void KHTTPResponseHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	HTTPVersion.clear();
	StatusString.clear();
	StatusCode = 0;

} // clear

//-----------------------------------------------------------------------------
bool KOutHTTPResponse::Serialize(KOutStream& Stream)
//-----------------------------------------------------------------------------
{
	// set up the chunked writer
	return KHTTPOutputFilter::Parse(*this) && KHTTPResponseHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KInHTTPResponse::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	// set up the chunked reader
	return KHTTPResponseHeaders::Parse(Stream) && KHTTPInputFilter::Parse(*this);

} // Parse



} // end of namespace dekaf2
