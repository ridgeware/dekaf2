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

#include "khttpoutputfilter.h"
#include "kchunkedtransfer.h"
#include "kstringstream.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::Parse(const KHTTPHeaders& headers)
//-----------------------------------------------------------------------------
{
	close();

	// find the content length
	KStringView sRemainingContentSize = headers.Headers.Get(KHTTPHeaders::content_length);
	if (!sRemainingContentSize.empty())
	{
		m_iContentSize = sRemainingContentSize.UInt64();
	}

	m_bChunked = headers.Headers.Get(KHTTPHeaders::transfer_encoding) == "chunked";

	KStringView sCompression = headers.Headers.Get(KHTTPHeaders::content_encoding);
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
bool KOutHTTPFilter::SetupOutputFilter()
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers

	if (m_bPerformCompression)
	{
		if (m_Compression == GZIP)
		{
			kDebug(2, "using gzip compression");
			m_Filter.push(boost::iostreams::gzip_compressor());
		}
		else if (m_Compression == ZLIB)
		{
			kDebug(2, "using zlib compression");
			m_Filter.push(boost::iostreams::zlib_compressor());
		}
	}

	// we use the chunked writer also in the unchunked case, but
	// without writing chunks
	KChunkedSink Sink(UnfilteredStream(), m_bChunked);

	// and finally add our source stream to the filtering_istream
	m_Filter.push(Sink);

	return true;

} // SetupOutputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KOutStream reference to it
KOutStream& KOutHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (m_Filter.empty())
	{
		SetupOutputFilter();
	}
	return m_FilteredOutStream;
}

//-----------------------------------------------------------------------------
size_t KOutHTTPFilter::Write(KInStream& InStream, size_t len)
//-----------------------------------------------------------------------------
{
	auto& Out(FilteredStream());

	if (len == KString::npos)
	{
		// write until eof
		Out.OutStream() << InStream.InStream().rdbuf();
	}
	else
	{
		Out.Write(InStream, len);
	}

	if (Out.Good())
	{
		return len;
	}
	else
	{
		return 0;
	}

} // Read

//-----------------------------------------------------------------------------
size_t KOutHTTPFilter::Write(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	auto& Out(FilteredStream());

	Out.Write(sBuffer);

	if (Out.Good())
	{
		return sBuffer.size();
	}
	else
	{
		return 0;
	}

} // Read

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::WriteLine(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	auto& Out(FilteredStream());

	Out.WriteLine(sBuffer);

	return Out.Good();

} // ReadLine

//-----------------------------------------------------------------------------
void KOutHTTPFilter::close()
//-----------------------------------------------------------------------------
{
	m_Filter.reset();
	m_Compression = NONE;
	m_bChunked = false;
	m_bPerformCompression = true;
	m_iContentSize = -1;

} // reset

KOutStringStream KOutHTTPFilter::s_Empty;

} // of namespace dekaf2


