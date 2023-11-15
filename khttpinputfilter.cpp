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

#include "khttpinputfilter.h"
#include "kchunkedtransfer.h"
#include "kstringstream.h"
#include "kcountingstreambuf.h"
#include "bits/kiostreams_filters.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KInHTTPFilter::Parse(const KHTTPHeaders& headers, uint16_t iStatusCode)
//-----------------------------------------------------------------------------
{
	NextRound();

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

	KHTTPCompression::Parse(headers);

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KInHTTPFilter::SetupInputFilter()
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers
	m_Filter = std::make_unique<boost::iostreams::filtering_istream>();

	if (m_bAllowUncompression)
	{
		switch (KHTTPCompression::GetCompression())
		{
			case NONE:
			case ALL:
				kDebug(2, "no {}compression", "un");
				break;

			case GZIP:
				kDebug(2, "using {} {}compression", "gzip", "un");
				m_Filter->push(boost::iostreams::gzip_decompressor());
				break;

			case ZLIB:
				kDebug(2, "using {} {}compression", "zlib", "un");
				m_Filter->push(boost::iostreams::zlib_decompressor());
				break;

			case BZIP2:
				kDebug(2, "using {} {}compression", "bzip2", "un");
				m_Filter->push(boost::iostreams::bzip2_decompressor());
				break;

#ifdef DEKAF2_HAS_LIBZSTD
			case ZSTD:
				kDebug(2, "using {} {}compression", "zstd", "un");
				m_Filter->push(dekaf2::iostreams::zstd_decompressor());
				break;
#endif
#ifdef DEKAF2_HAS_LIBLZMA
			case XZ: // lzma v2
				kDebug(2, "using {} {}compression", "xz", "un");
				m_Filter->push(lzmaiostreams::lzma_decompressor());
				break;

			case LZMA: // LZMA means v1, it should be supported by the lzma decompressor
				kDebug(2, "using {} {}compression", "lzma", "un");
				m_Filter->push(lzmaiostreams::lzma_decompressor());
				break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
			case BROTLI:
				kDebug(2, "using {} {}compression", "brotli", "un");
				m_Filter->push(dekaf2::iostreams::brotli_decompressor());
				break;
#endif
		}
	}
	else
	{
		kDebug(2, "no {}compression", "un");
	}

	if (m_bChunked)
	{
		kDebug(2, "chunked {}", "RX");
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

	// save the pointer in a KInStream object for return by reference
	m_FilteredInStream = KInStream(*m_Filter.get());

	return true;

} // SetupInputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KInStream reference to it
KInStream& KInHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (!m_Filter)
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
		OutStream.ostream() << In.istream().rdbuf();

		return Count.Count();
	}
	else
	{
		OutStream.Write(In, len);

		return len;
	}

} // Read

//-----------------------------------------------------------------------------
size_t KInHTTPFilter::Read(KStringRef& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	KOutStringStream stream(sBuffer);
	return Read(stream, len);

} // Read

//-----------------------------------------------------------------------------
bool KInHTTPFilter::ReadLine(KStringRef& sBuffer)
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
void KInHTTPFilter::NextRound()
//-----------------------------------------------------------------------------
{
	if (m_Filter)
	{
		m_Filter.reset();
	}

	KHTTPCompression::SetCompression(NONE);
	m_bAllowUncompression = true;
	m_bChunked            = false;
	m_iContentSize        = -1;

} // NextRound

//-----------------------------------------------------------------------------
void KInHTTPFilter::Reset()
//-----------------------------------------------------------------------------
{
	NextRound();
	m_InStream = &kGetNullInStream();

} // Reset

} // of namespace dekaf2


