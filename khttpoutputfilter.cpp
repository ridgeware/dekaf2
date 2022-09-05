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

#include "bits/kiostreams_filters.h"

#include "khttpoutputfilter.h"
#include "kchunkedtransfer.h"
#include "kstringstream.h"
#include "kmime.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::Parse(const KHTTPHeaders& headers)
//-----------------------------------------------------------------------------
{
	reset();

	m_bChunked = headers.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked";

	KHTTPCompression::Parse(headers);

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::SetupOutputFilter()
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers

	if (!m_Filter)
	{
		return false;
	}

	if (m_bAllowCompression)
	{
		switch (KHTTPCompression::GetCompression())
		{
			case NONE:
			case ALL:
				kDebug(2, "no {}compression", "");
				break;
				
			case GZIP:
				kDebug(2, "using {} {}compression", "gzip", "");
				m_Filter->push(boost::iostreams::gzip_compressor());
				break;

			case ZLIB:
				kDebug(2, "using {} {}compression", "zlib", "");
				m_Filter->push(boost::iostreams::zlib_compressor());
				break;

			case BZIP2:
				kDebug(2, "using {} {}compression", "bzip2", "");
				m_Filter->push(boost::iostreams::bzip2_compressor());
				break;

#ifdef DEKAF2_HAS_LIBZSTD
			case ZSTD:
				kDebug(2, "using {} {}compression", "zstd", "");
				m_Filter->push(dekaf2::iostreams::zstd_compressor());
				break;

#endif
#ifdef DEKAF2_HAS_LIBLZMA
			case XZ:
				kDebug(2, "using {} {}compression", "xz", "");
				m_Filter->push(lzmaiostreams::lzma_compressor());
				break;

			case LZMA:
				kDebug(1, "lzma tx compression not supported!");
				break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
			case BROTLI:
				kDebug(2, "using {} {}compression", "brotli", "");
				m_Filter->push(dekaf2::iostreams::brotli_compressor());
				break;
#endif
		}
	}
	else
	{
		kDebug(2, "no {}compression", "");
	}

	if (m_bChunked)
	{
		kDebug(2, "chunked {}", "TX");
	}

	m_iCount = 0;

	// we use the chunked writer also in the unchunked case, but
	// without writing chunks
	KChunkedSink Sink(UnfilteredStream(), m_bChunked, &m_iCount);

	// and finally add our source stream to the filtering_ostream
	m_Filter->push(std::move(Sink));

	return true;

} // SetupOutputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KOutStream reference to it
KOutStream& KOutHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (m_Filter && m_Filter->empty())
	{
		SetupOutputFilter();
	}
	return m_FilteredOutStream;
}

//-----------------------------------------------------------------------------
std::streamsize KOutHTTPFilter::Count() const
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		// we cannot reliably read the count when we compress - no way to flush
		if (m_bAllowCompression && KHTTPCompression::GetCompression() == NONE)
		{
			auto chunker = m_Filter->component<KChunkedSink>(static_cast<int>(m_Filter->size()-1));

			if (chunker)
			{
				// this sync does not work with compression!
				m_Filter->strict_sync();
				return chunker->Count();
			}
			else
			{
				kDebug(1, "cannot get KChunkedSink component from output pipeline");
				return 0;
			}
		}
		else
		{
			kDebug(1, "cannot read count with a compressing pipeline before close()");
			return 0;
		}
	}
	else
	{
		return m_iCount;
	}

} // Count

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::ResetCount()
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		// we cannot reliably reset the count when we compress - no way to flush
		if (m_bAllowCompression && KHTTPCompression::GetCompression() == NONE)
		{
			auto chunker = m_Filter->component<KChunkedSink>(static_cast<int>(m_Filter->size()-1));

			if (chunker)
			{
				m_Filter->strict_sync();
				chunker->ResetCount();
			}
			else
			{
				kDebug(1, "cannot get KChunkedSink component from output pipeline");
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	m_iCount = 0;
	return true;

} // ResetCount

//-----------------------------------------------------------------------------
size_t KOutHTTPFilter::Write(KInStream& InStream, size_t len)
//-----------------------------------------------------------------------------
{
	auto& Out(FilteredStream());

	if (len == npos)
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

} // Write

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

} // Write

//-----------------------------------------------------------------------------
size_t KOutHTTPFilter::WriteLine(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	auto& Out(FilteredStream());

	Out.WriteLine(sBuffer);

	if (Out.Good())
	{
		return sBuffer.size() + Out.GetWriterEndOfLine().size();
	}
	else
	{
		return 0;
	}
	
} // WriteLine

//-----------------------------------------------------------------------------
void KOutHTTPFilter::reset()
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		// this resets (empties) the filter chain, but not the unique ptr m_Filter
		m_Filter->reset();

		if (m_OutStream)
		{
			m_OutStream->Flush();
		}
	}

	KHTTPCompression::SetCompression(NONE);
	m_bChunked          = false;
	m_bAllowCompression = true;

} // reset

//-----------------------------------------------------------------------------
void KOutHTTPFilter::close()
//-----------------------------------------------------------------------------
{
	reset();
	ResetOutputStream();

} // close

KOutStringStream KOutHTTPFilter::s_Empty;

} // of namespace dekaf2


