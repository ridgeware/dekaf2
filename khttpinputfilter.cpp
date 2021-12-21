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

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include "khttpinputfilter.h"
#include "kchunkedtransfer.h"
#include "kstringstream.h"
#include "kcountingstreambuf.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KInHTTPFilter::Parse(const KHTTPHeaders& headers, uint16_t iStatusCode)
//-----------------------------------------------------------------------------
{
	reset();

	// find the content length
	KStringView sRemainingContentSize = headers.Headers.Get(KHTTPHeader::CONTENT_LENGTH);
	if (!sRemainingContentSize.empty())
	{
		m_iContentSize = sRemainingContentSize.UInt64();
	}
	else if (iStatusCode == 204)
	{
		m_iContentSize = 0;
	}

	m_bChunked = headers.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked";

	KStringView sCompression = headers.Headers.Get(KHTTPHeader::CONTENT_ENCODING);
	if (sCompression == "gzip" || sCompression == "x-gzip")
	{
		m_Compression = GZIP;
	}
	else if (sCompression == "deflate")
	{
		m_Compression = ZLIB;
	}
	else if (sCompression == "bzip2")
	{
		m_Compression = BZIP2;
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KInHTTPFilter::SetupInputFilter()
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers

	if (!m_Filter)
	{
		return false;
	}

	if (m_bAllowUncompression)
	{
		if (m_Compression == GZIP)
		{
			m_Filter->push(boost::iostreams::gzip_decompressor());
		}
		else if (m_Compression == ZLIB)
		{
			m_Filter->push(boost::iostreams::zlib_decompressor());
		}
		else if (m_Compression == BZIP2)
		{
			m_Filter->push(boost::iostreams::bzip2_decompressor());
		}
	}

	m_iCount = 0;

	// we use the chunked reader also in the unchunked case -
	// it protects us from reading more than content length bytes
	// into the buffered iostreams
	KChunkedSource Source(UnfilteredStream(),
						  m_bChunked,
						  m_iContentSize,
						  &m_iCount);

	// and finally add our source stream to the filtering_istream
	m_Filter->push(Source);

	return true;

} // SetupInputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KInStream reference to it
KInStream& KInHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (m_Filter && m_Filter->empty())
	{
		SetupInputFilter();
	}
	return m_FilteredInStream;

} // Stream

//-----------------------------------------------------------------------------
std::streamsize KInHTTPFilter::Count() const
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		auto chunker = m_Filter->component<KChunkedSource>(static_cast<int>(m_Filter->size()-1));

		if (chunker)
		{
			// this sync does not work with uncompression!
			m_Filter->strict_sync();
			return chunker->Count();
		}
		else
		{
			kDebug(1, "cannot get KChunkedSource component from output pipeline");
			return 0;
		}
	}
	else
	{
		return m_iCount;
	}

} // Count

//-----------------------------------------------------------------------------
bool KInHTTPFilter::ResetCount()
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		auto chunker = m_Filter->component<KChunkedSource>(static_cast<int>(m_Filter->size()-1));

		if (chunker)
		{
			m_Filter->strict_sync();
			chunker->ResetCount();
		}
		else
		{
			kDebug(1, "cannot get KChunkedSource component from output pipeline");
			return false;
		}
	}

	m_iCount = 0;
	return true;

} // ResetCount

//-----------------------------------------------------------------------------
size_t KInHTTPFilter::Read(KOutStream& OutStream, size_t len)
//-----------------------------------------------------------------------------
{
	auto& In(FilteredStream());

	if (len == npos)
	{
		KCountingOutputStreamBuf Count(OutStream);

		// read until eof
		// ignore len, copy full stream
		OutStream.OutStream() << In.InStream().rdbuf();

		return Count.Count();
	}
	else
	{
		OutStream.Write(In, len);

		return len;
	}

} // Read

//-----------------------------------------------------------------------------
size_t KInHTTPFilter::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	KOutStringStream stream(sBuffer);
	return Read(stream, len);

} // Read

//-----------------------------------------------------------------------------
bool KInHTTPFilter::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	auto& In(FilteredStream());

	if (!In.ReadLine(sBuffer))
	{
		return false;
	}

	return true;

} // ReadLine

//-----------------------------------------------------------------------------
void KInHTTPFilter::reset()
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		m_Filter->reset();
	}
	m_Compression         = NONE;
	m_bChunked            = false;
	m_bAllowUncompression = true;
	m_iContentSize        = -1;

} // reset

//-----------------------------------------------------------------------------
void KInHTTPFilter::close()
//-----------------------------------------------------------------------------
{
	reset();
	ResetInputStream();

} // close

KInStringStream KInHTTPFilter::s_Empty;

} // of namespace dekaf2


