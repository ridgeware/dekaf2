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

#include <dekaf2/io/compression/bits/kiostreams_filters.h>
#include <dekaf2/io/compression/bits/kflushablefilter.h>

#include <dekaf2/http/server/khttpoutputfilter.h>
#include <dekaf2/http/protocol/kchunkedtransfer.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/web/url/kmime.h>


DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
// flushes a filter chain that starts with a compressor of the type Compressor
template<class Compressor>
bool FlushCompressor(boost::iostreams::filtering_ostream& Chain)
//-----------------------------------------------------------------------------
{
	auto Filter = Chain.component<Compressor>(0);

	if (!Filter)
	{
		kDebug(1, "cannot get the compressor component from output pipeline");
		return false;
	}

	// the compressor takes part in a flush only here, as the chain also
	// flushes right before it gets closed
	Filter->enable_flush(true);
	auto bFlushed = Chain.strict_sync();
	Filter->enable_flush(false);

	return bFlushed;

} // FlushCompressor

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool KOutHTTPFilter::Parse(const KHTTPHeaders& headers)
//-----------------------------------------------------------------------------
{
	Close();

	// the filter stored its count when it got closed - the count of the next
	// message starts at 0, also when that message has no body and creates no filter
	m_iCount = 0;

	m_bChunked = headers.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked";

	KHTTPCompression::Parse(headers);

	return true;

} // Parse

//-----------------------------------------------------------------------------
void KOutHTTPFilter::SetupOutputFilter()
//-----------------------------------------------------------------------------
{
	// we lazy-create the input filter chain because we want to give
	// the user the chance to switch off compression AFTER reading
	// the headers
	m_Filter = std::make_unique<boost::iostreams::filtering_ostream>();

	if (m_bAllowCompression)
	{
		switch (KHTTPCompression::GetCompression())
		{
			case NONE:
			case ALL:
				kDebug(2, "no {}compression", "");
				break;
				
			case GZIP:
			{
				kDebug(2, "using {} {}compression", "gzip", "");
				boost::iostreams::zlib_params Params;
				// makes zlib write the gzip format
				Params.window_bits += 16;
				m_Filter->push(dekaf2::iostreams::flushable_zlib_compressor(Params));
				break;
			}

			case ZLIB:
				kDebug(2, "using {} {}compression", "zlib", "");
				m_Filter->push(dekaf2::iostreams::flushable_zlib_compressor());
				break;

			case BZIP2:
				kDebug(2, "using {} {}compression", "bzip2", "");
				m_Filter->push(boost::iostreams::bzip2_compressor());
				break;

#ifdef DEKAF2_HAS_LIBZSTD
			case ZSTD:
				kDebug(2, "using {} {}compression", "zstd", "");
				m_Filter->push(dekaf2::iostreams::flushable_zstd_compressor());
				break;

#endif
#ifdef DEKAF2_HAS_LIBLZMA
			case XZ:
				kDebug(2, "using {} {}compression", "xz", "");
				m_Filter->push(dekaf2::iostreams::flushable_lzma_compressor());
				break;

			case LZMA:
				kDebug(1, "lzma tx compression not supported!");
				break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
			case BROTLI:
				kDebug(2, "using {} {}compression", "brotli", "");
				m_Filter->push(dekaf2::iostreams::flushable_brotli_compressor());
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
	// without writing chunks (we use it to count sent bytes)
	KChunkedSink Sink(UnfilteredStream(), m_bChunked, &m_iCount);

	// and finally add our source stream to the filtering_ostream
	m_Filter->push(Sink);

	// save the pointer in a KOutStream object for return by reference
	m_FilteredOutStream = KOutStream(*m_Filter);

} // SetupOutputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KOutStream reference to it
KOutStream& KOutHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (!m_Filter)
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
		// we do not read the count when we compress - that would need a flush of the compressor
		if (m_bAllowCompression && KHTTPCompression::GetCompression() == NONE)
		{
			auto chunker = m_Filter->component<KChunkedSink>(static_cast<int>(m_Filter->size()-1));

			if (chunker)
			{
				// this sync does not flush a compressor
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
			kDebug(1, "cannot read count with a compressing pipeline before Close()");
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
		// we do not reset the count when we compress - that would need a flush of the compressor
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

	// copies in blocks until len bytes are written or InStream ends - an
	// ostream << streambuf would set the failbit for an empty InStream
	auto iWrote = InStream.Read(Out, len);

	if (Out.Good())
	{
		return iWrote;
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
bool KOutHTTPFilter::Flush()
//-----------------------------------------------------------------------------
{
	bool bFlushed { true };

	if (m_Filter && !m_Filter->empty())
	{
		if (m_bAllowCompression)
		{
			switch (KHTTPCompression::GetCompression())
			{
				case NONE:
				case ALL:
					bFlushed = m_Filter->strict_sync();
					break;

				case GZIP:
				case ZLIB:
					bFlushed = FlushCompressor<dekaf2::iostreams::flushable_zlib_compressor>(*m_Filter);
					break;

				case BZIP2:
					// libbz2 does not fill up the last byte of a block at a flush,
					// the receiver could not read the block
					bFlushed = false;
					break;

#ifdef DEKAF2_HAS_LIBZSTD
				case ZSTD:
					bFlushed = FlushCompressor<dekaf2::iostreams::flushable_zstd_compressor>(*m_Filter);
					break;

#endif
#ifdef DEKAF2_HAS_LIBLZMA
				case XZ:
					bFlushed = FlushCompressor<dekaf2::iostreams::flushable_lzma_compressor>(*m_Filter);
					break;

				case LZMA:
					// there is no compressor for it in the filter chain
					bFlushed = m_Filter->strict_sync();
					break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
				case BROTLI:
					bFlushed = FlushCompressor<dekaf2::iostreams::flushable_brotli_compressor>(*m_Filter);
					break;
#endif
			}
		}
		else
		{
			bFlushed = m_Filter->strict_sync();
		}
	}

	if (m_OutStream)
	{
		// flush the outermost stream
		m_OutStream->Flush();
	}

	return bFlushed;

} // Flush

//-----------------------------------------------------------------------------
void KOutHTTPFilter::Close()
//-----------------------------------------------------------------------------
{
	if (m_Filter)
	{
		if (!m_Filter->empty())
		{
			// this resets (empties) the filter chain, but not the unique ptr m_Filter:
			// the compressor writes its end, and the chunked sink the last chunk
			m_Filter->reset();
		}

		m_Filter.reset();
	}

	if (m_OutStream)
	{
		// flush the outermost stream
		m_OutStream->Flush();
	}

} // Close

//-----------------------------------------------------------------------------
void KOutHTTPFilter::Abort()
//-----------------------------------------------------------------------------
{
	if (m_Filter)
	{
		if (!m_Filter->empty())
		{
			auto chunker = m_Filter->component<KChunkedSink>(static_cast<int>(m_Filter->size()-1));

			if (chunker)
			{
				// closing the filter chain writes what the filters still hold, and
				// the last chunk
				chunker->Discard();
			}
			else
			{
				kDebug(1, "cannot get KChunkedSink component from output pipeline");
			}

			m_Filter->reset();
		}

		m_Filter.reset();
	}

} // Abort

//-----------------------------------------------------------------------------
void KOutHTTPFilter::Reset()
//-----------------------------------------------------------------------------
{
	m_OutStream         = &kGetNullOutStream();
	m_Filter.reset();
	m_bAllowCompression = true;
	m_bChunked          = false;

} // Reset

DEKAF2_NAMESPACE_END


