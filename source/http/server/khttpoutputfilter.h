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

#pragma once

// for chunked transfer and compression
#include <boost/iostreams/filtering_stream.hpp>

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/protocol/khttpcompression.h>
#include <dekaf2/io/streams/kstringstream.h>


/// @file khttpoutputfilter.h
/// HTTP streaming output filter implementation


DEKAF2_NAMESPACE_BEGIN

/// @addtogroup http_server
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOutHTTPFilter : public KHTTPCompression
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// construct a HTTP output filter without an output stream
	KOutHTTPFilter() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// construct a HTTP output filter around an output stream
	KOutHTTPFilter(KOutStream& OutStream)
	: m_OutStream(&OutStream)
	//-----------------------------------------------------------------------------
	{
	}

	KOutHTTPFilter(const KOutHTTPFilter&) = delete;
	KOutHTTPFilter(KOutHTTPFilter&&) = default;
	KOutHTTPFilter& operator=(const KOutHTTPFilter&) = delete;
	KOutHTTPFilter& operator=(KOutHTTPFilter&&) = default;

	//-----------------------------------------------------------------------------
	/// Set a new output stream for the filter
	void SetOutputStream(KOutStream& OutStream)
	//-----------------------------------------------------------------------------
	{
		m_OutStream = &OutStream;
	}

	//-----------------------------------------------------------------------------
	/// read input configuration from existing set of headers, but do not
	/// yet build the filter.
	bool Parse(const KHTTPHeaders& headers);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// build the filter if it is not yet created, and return a KOutStream reference to it
	KOutStream& FilteredStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOutStream& UnfilteredStream()
	//-----------------------------------------------------------------------------
	{
		return *m_OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Stream from InStream into OutStream
	/// @param InStream the stream to read from
	/// @param len the number of bytes to write, or npos to write until InStream ends
	/// @return the count of written bytes, which is less than len if InStream ends
	/// before, or 0 if the output failed
	size_t Write(KInStream& InStream, size_t len = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write sBuffer into OutStream
	size_t Write(KStringView sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write one line into OutStream, including EOL
	size_t WriteLine(KStringView sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void AllowCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bAllowCompression = bYesNo;
	}

	//-----------------------------------------------------------------------------
	/// returns false if compression was switched off with AllowCompression(false)
	bool IsCompressionAllowed() const
	//-----------------------------------------------------------------------------
	{
		return m_bAllowCompression;
	}

	//-----------------------------------------------------------------------------
	/// Sends all content that was written so far, and keeps the message open for further writes.
	/// @return false if the output is compressed with bzip2, which cannot be flushed and
	/// sends its content when the message gets closed
	bool Flush();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ends the message: sends all content, the end of a compressed stream and the last chunk
	/// of a chunked transfer.
	void Close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// removes the output pipeline of a message that shall not be completed, without
	/// writing what the pipeline still holds and without the last chunk
	void Abort();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// resets to initial state
	void Reset();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns true if the output stream failed before
	bool Fail()
	//-----------------------------------------------------------------------------
	{
		return !m_OutStream || m_OutStream->ostream().fail();
	}

	//-----------------------------------------------------------------------------
	/// get count of written bytes so far - with a compressed message the count is only known after Close(),
	/// as reading it earlier would need a flush of the compressor. Until then it is 0.
	std::streamsize Count() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reset count of written bytes - this would need a flush of the compressor while a compressed
	/// message is open. Do not use this function when compression is in the pipeline (in which case
	/// it will return with false). It will though work after Close() ..
	bool ResetCount();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void SetupOutputFilter();
	//-----------------------------------------------------------------------------

	KOutStream*     m_OutStream         { &kGetNullOutStream() };
	KOutStream      m_FilteredOutStream { kGetNullOStream() };
	std::unique_ptr<boost::iostreams::filtering_ostream>
	                m_Filter;
	std::streamsize m_iCount            { 0         };
	bool            m_bChunked          { false     };
	bool            m_bAllowCompression { true      };

}; // KOutHTTPFilter


/// @}

DEKAF2_NAMESPACE_END


