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

#include <dekaf2/http/server/khttpinputfilter.h>
#include <dekaf2/http/protocol/kchunkedtransfer.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/streams/kcountingstreambuf.h>
#include <dekaf2/io/compression/bits/kiostreams_filters.h>
#include <dekaf2/http/server/khttperror.h>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// boost::iostreams multichar input filter that limits the number of bytes
/// passing through. Designed to be pushed as the first filter in the chain,
/// so it counts decompressed (post-decompression) bytes. Throws
/// KHTTPError::H4xx_PAYLOAD_TOO_LARGE when the limit is exceeded.
class KInputLimiter : public boost::iostreams::multichar_input_filter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	KInputLimiter(std::size_t iMaxSize, bool* pExceeded = nullptr)
	: m_iMaxSize(iMaxSize)
	, m_pExceeded(pExceeded)
	{}

	template<typename Source>
	std::streamsize read(Source& src, char* s, std::streamsize n)
	{
		if (m_iCount >= m_iMaxSize)
		{
			// set the flag before throwing - the throw may be swallowed
			// by the std::istream layer (badbit set, exception lost),
			// so the caller must also check IsBodySizeLimitExceeded()
			if (m_pExceeded) *m_pExceeded = true;

			throw KHTTPError { KHTTPError::H4xx_PAYLOAD_TOO_LARGE,
				kFormat("decompressed request body exceeds size limit of {} bytes", m_iMaxSize) };
		}

		auto iRemaining = static_cast<std::streamsize>(m_iMaxSize - m_iCount);
		auto iToRead    = std::min(n, iRemaining);
		auto iRead      = boost::iostreams::read(src, s, iToRead);

		if (iRead > 0)
		{
			m_iCount += static_cast<std::size_t>(iRead);
		}

		return iRead;
	}

private:

	std::size_t m_iMaxSize;
	std::size_t m_iCount { 0 };
	bool*       m_pExceeded { nullptr };

}; // KInputLimiter

//-----------------------------------------------------------------------------
bool KInHTTPFilter::Parse(const KHTTPHeaders& headers, uint16_t iStatusCode, KHTTPVersion HTTPVersion)
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

	m_bChunked = (HTTPVersion & (KHTTPVersion::http2 | KHTTPVersion::http3))
	              ? false
	              : headers.Headers.Get(KHTTPHeader::TRANSFER_ENCODING).ToLowerASCII() == "chunked";

	// reject Content-Length + Transfer-Encoding conflict (RFC 7230 §3.3.3)
	if (m_bChunked && m_iContentSize >= 0)
	{
		if (iStatusCode == 0)
		{
			// server side: reject the request to prevent request smuggling
			kDebug(1, "rejecting request with both Content-Length and Transfer-Encoding headers");
			return false;
		}
		else
		{
			// client side: Transfer-Encoding takes precedence, ignore Content-Length
			kDebug(1, "ignoring Content-Length in response with Transfer-Encoding: chunked");
			m_iContentSize = -1;
		}
	}

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

	if (m_iMaxBodySize > 0)
	{
		kDebug (3, "limiting decompressed input to {} bytes", m_iMaxBodySize);
		m_Filter->push(KInputLimiter(m_iMaxBodySize, &m_bBodySizeLimitExceeded));
	}

	if (m_bAllowUncompression)
	{
		switch (KHTTPCompression::GetCompression())
		{
			case NONE:
			case ALL:
				kDebug (3, "no {}compression", "un");
				break;

			case GZIP:
				kDebug (3, "using {} {}compression", "gzip", "un");
				m_Filter->push(boost::iostreams::gzip_decompressor());
				break;

			case ZLIB:
				kDebug (3, "using {} {}compression", "zlib", "un");
				m_Filter->push(boost::iostreams::zlib_decompressor());
				break;

			case BZIP2:
				kDebug (3, "using {} {}compression", "bzip2", "un");
				m_Filter->push(boost::iostreams::bzip2_decompressor());
				break;

#ifdef DEKAF2_HAS_LIBZSTD
			case ZSTD:
				kDebug (3, "using {} {}compression", "zstd", "un");
				m_Filter->push(dekaf2::iostreams::zstd_decompressor());
				break;
#endif
#ifdef DEKAF2_HAS_LIBLZMA
			case XZ: // lzma v2
				kDebug (3, "using {} {}compression", "xz", "un");
				m_Filter->push(lzmaiostreams::lzma_decompressor());
				break;

			case LZMA: // LZMA means v1, it should be supported by the lzma decompressor
				kDebug (3, "using {} {}compression", "lzma", "un");
				m_Filter->push(lzmaiostreams::lzma_decompressor());
				break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
			case BROTLI:
				kDebug (3, "using {} {}compression", "brotli", "un");
				m_Filter->push(dekaf2::iostreams::brotli_decompressor());
				break;
#endif
		}
	}
	else
	{
		kDebug (3, "no {}compression", "un");
	}

	if (m_bChunked)
	{
		kDebug (3, "chunked {}", "RX");
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
	m_FilteredInStream = KInStream(*m_Filter);

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
bool KInHTTPFilter::IsInputConsumed() const
//-----------------------------------------------------------------------------
{
	if (m_Filter && !m_Filter->empty())
	{
		auto chunker = m_Filter->component<KChunkedSource>(static_cast<int>(m_Filter->size()-1));
		return chunker && chunker->IsFinished();
	}

	// filter was never created - input is consumed if no content was expected
	return m_iContentSize <= 0 && !m_bChunked;

} // IsInputConsumed

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
	else if (len)
	{
		OutStream.Write(In, len);
	}

	// this may be wrong for OutStream.Write() if the input stream did not
	// have len bytes
	return len;

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

DEKAF2_NAMESPACE_END


