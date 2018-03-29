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

#include "khttp_request.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
bool KHTTPRequest::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	if (!Stream.ReadLine(sLine))
	{
		return SetError("cannot read input stream");
	}

	if (sLine.empty())
	{
		return SetError("empty request line");
	}

	// analyze method and resource
	// GET /some/page?query=search#fragment HTTP/1.1

	std::vector<KStringView> Words;
	Words.reserve(3);
	kSplit(Words, sLine, " ");

	if (Words.size() != 3)
	{
		// garbage, bail out
		return SetError("invalid HTTP header");
	}

	m_Method = Words[0];
	m_Resource = Words[1];
	HTTPVersion() = Words[2];

	if (!HTTPVersion().StartsWith("HTTP/"))
	{
		return SetError("missing HTTP version in header");
	}

	if (!KHTTPHeader::Parse(Stream))
	{
		// never returns false actually, therefore no error to fetch
		return false;
	}

	// set up the chunked reader
	return KHTTPOutputFilter::Parse(*this);

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPRequest::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	Stream.FormatLine("{} {} {}", m_Method.Serialize(), m_Resource.Serialize(), HTTPVersion());
	return KHTTPHeader::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPRequest::HasChunking() const
//-----------------------------------------------------------------------------
{
	if (HTTPVersion() == "HTTP/1.0" || HTTPVersion() == "HTTP/0.9")
	{
		return false;
	}
	else
	{
		return true;
	}

} // HasChunking

//-----------------------------------------------------------------------------
std::streamsize KHTTPRequest::ContentLength() const
//-----------------------------------------------------------------------------
{
	std::streamsize iSize { -1 };

	KStringView sSize = Get(KHTTPHeader::content_length);

	if (!sSize.empty())
	{
		iSize = sSize.UInt64();
	}

	return iSize;

} // ContentLength

//-----------------------------------------------------------------------------
bool KHTTPRequest::HasContent() const
//-----------------------------------------------------------------------------
{
	auto iSize = ContentLength();

	if (iSize < 0)
	{
		// do not blindly trust in the transfer-encoding header, e.g.
		// for methods that can not have content
		return (Method() == "POST" || Method() == "PUT")
		     && Get(KHTTPHeader::transfer_encoding) == "chunked";
	}
	else
	{
		return iSize > 0;
	}

} // HasContent


//-----------------------------------------------------------------------------
void KHTTPRequest::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeader::clear();
	HTTPVersion().clear();
	m_Resource.clear();

}

} // end of namespace dekaf2
