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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to count the characters written through a streambuf
class CountingOutputStreamBuf : std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	CountingOutputStreamBuf(dekaf2::KOutStream& outstream)
	: m_ostream(outstream.OutStream())
	, m_SBuf(m_ostream.rdbuf(this))
	{
	}

	CountingOutputStreamBuf(std::ostream& ostream)
	: m_ostream(ostream)
	, m_SBuf(m_ostream.rdbuf(this))
	{
	}

	virtual ~CountingOutputStreamBuf()
	{
		m_ostream.rdbuf(m_SBuf);
	}

    virtual int_type overflow(int_type c) override
	{
        if (!traits_type::eq_int_type(traits_type::eof(), c))
		{
			auto ch = m_SBuf->sputc(c);

			if (!traits_type::eq_int_type(traits_type::eof(), ch))
			{
				++m_iCount;
			}

			return ch;
		}
		return c;
	}

	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override
	{
		auto iWrote = m_SBuf->sputn(s, n);
		m_iCount += iWrote;
		return iWrote;
    }

    virtual int sync() override
	{
		return m_SBuf->pubsync();
	}

    std::streamsize count() const
	{
		return m_iCount;
	}

//-------
private:
//-------

	std::ostream& m_ostream;
	std::streambuf* m_SBuf { nullptr };
	std::streamsize m_iCount { 0 };

}; // CountingOutputStreamBuf

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KInHTTPFilter::Parse(const KHTTPHeaders& headers, uint16_t iStatusCode)
//-----------------------------------------------------------------------------
{
	close();

	// find the content length
	KStringView sRemainingContentSize = headers.Headers.Get(KHTTPHeaders::content_length);
	if (!sRemainingContentSize.empty())
	{
		m_iContentSize = sRemainingContentSize.UInt64();
	}
	else if (iStatusCode == 204)
	{
		m_iContentSize = 0;
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

	// we use the chunked reader also in the unchunked case -
	// it protects us from reading more than content length bytes
	// into the buffered iostreams
	KChunkedSource Source(UnfilteredStream(),
						  m_bChunked,
						  m_iContentSize);

	// and finally add our source stream to the filtering_istream
	m_Filter->push(Source);

	return true;

} // SetupInputFilter

//-----------------------------------------------------------------------------
// build the filter if it is not yet created, and return a KInStream reference to it
KInStream& KInHTTPFilter::FilteredStream()
//-----------------------------------------------------------------------------
{
	if (m_Filter->empty())
	{
		SetupInputFilter();
	}
	return m_FilteredInStream;

} // Stream

//-----------------------------------------------------------------------------
size_t KInHTTPFilter::Read(KOutStream& OutStream, size_t len)
//-----------------------------------------------------------------------------
{
	auto& In(FilteredStream());

	CountingOutputStreamBuf Count(OutStream);

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

	return Count.count();

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
void KInHTTPFilter::close()
//-----------------------------------------------------------------------------
{
	if (!m_Filter->empty())
	{
		m_Filter->reset();
		m_Compression = NONE;
		m_bChunked = false;
		m_bAllowUncompression = true;
		m_iContentSize = -1;
	}

} // reset

KInStringStream KInHTTPFilter::s_Empty;

} // of namespace dekaf2


