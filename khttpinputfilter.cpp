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

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>

#include "khttpinputfilter.h"
#include "kchunkedtransfer.h"
#include "kstringstream.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KHTTPInputFilter::Parse(const KHTTPHeader& headers)
//-----------------------------------------------------------------------------
{
	clear();

	// find the content length
	KStringView sRemainingContentSize = headers.Headers.Get(KHTTPHeader::content_length);
	if (!sRemainingContentSize.empty())
	{
		m_iContentSize = sRemainingContentSize.UInt64();
	}

	m_bChunked = headers.Headers.Get(KHTTPHeader::transfer_encoding) == "chunked";

	KStringView sCompression = headers.Headers.Get(KHTTPHeader::content_encoding);
	if (sCompression == "gzip" || sCompression == "x-gzip")
	{
		m_Compression = GZIP;
	}
	else if (sCompression == "deflate")
	{
		m_Compression = ZLIB;
	}

	kDebug(2, "content transfer: {}", m_bChunked ? "chunked" : "plain");
	kDebug(2, "content length:   {}", m_iContentSize);

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPInputFilter::SetupInputFilter(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers

	if (m_bPerformUncompression)
	{
		if (m_Compression == GZIP)
		{
			kDebug(2, "using gzip decompression");
			m_Filter.push(boost::iostreams::gzip_decompressor());
		}
		else if (m_Compression == ZLIB)
		{
			kDebug(2, "using zlib decompression");
			m_Filter.push(boost::iostreams::zlib_decompressor());
		}
	}

	// we use the chunked reader also in the unchunked case -
	// it protects us from reading more than content length bytes
	// into the buffered iostreams
	KChunkedSource Source(InStream,
						  m_bChunked,
						  m_iContentSize);

	// and finally add our source stream to the filtering_istream
	m_Filter.push(Source);

	return true;

} // SetupInputFilter

//-----------------------------------------------------------------------------
size_t KHTTPInputFilter::Read(KInStream& InStream, KOutStream& OutStream, size_t len)
//-----------------------------------------------------------------------------
{
	auto& In(Stream(InStream));

	if (len == KString::npos)
	{
		// read until eof
		// ignore len, copy full stream
		OutStream.OutStream() << In.InStream().rdbuf();
	}
	else
	{
		OutStream.Write(In, len);
	}

	return len;

} // Read

//-----------------------------------------------------------------------------
size_t KHTTPInputFilter::Read(KInStream& InStream, KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	auto& In(Stream(InStream));

	if (len == KString::npos)
	{
		// read until eof
		// ignore len, copy full stream
		KOStringStream stream(sBuffer);
		stream << In.InStream().rdbuf();
	}
	else
	{
		In.Read(sBuffer, len);
	}

	return sBuffer.size();

} // Read

//-----------------------------------------------------------------------------
bool KHTTPInputFilter::ReadLine(KInStream& InStream, KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	auto& In(Stream(InStream));

	if (!In.ReadLine(sBuffer))
	{
		return false;
	}

	return true;

} // ReadLine



//-----------------------------------------------------------------------------
void KHTTPInputFilter::clear()
//-----------------------------------------------------------------------------
{
	m_Filter.reset();
	m_Compression = NONE;
	m_bChunked = false;
	m_bPerformUncompression = true;
	m_iContentSize = -1;

} // clear


} // of namespace dekaf2


