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

#include "kstringview.h"
#include "khttp_header.h"
#include "kinstringstream.h"
#include "khttpcompression.h"

/// @file khttpinputfilter.h
/// HTTP streaming input filter implementation


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KInHTTPFilter : public KHTTPCompression
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// construct a HTTP input filter without an input stream
	KInHTTPFilter() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// construct a HTTP input filter around an input stream
	KInHTTPFilter(KInStream& InStream)
	: m_InStream(&InStream)
	//-----------------------------------------------------------------------------
	{
	}

	KInHTTPFilter(const KInHTTPFilter&) = delete;
	KInHTTPFilter(KInHTTPFilter&&) = default;
	KInHTTPFilter& operator=(const KInHTTPFilter&) = delete;
	KInHTTPFilter& operator=(KInHTTPFilter&&) = delete;

	//-----------------------------------------------------------------------------
	/// Set a new input stream for the filter
	void SetInputStream(KInStream& InStream)
	//-----------------------------------------------------------------------------
	{
		m_InStream = &InStream;
	}

	//-----------------------------------------------------------------------------
	/// Reset the input stream for the filter to nil
	void ResetInputStream()
	//-----------------------------------------------------------------------------
	{
		m_InStream = &s_Empty;
	}

	//-----------------------------------------------------------------------------
	/// read input configuration from existing set of headers, but do not
	/// yet build the filter.
	bool Parse(const KHTTPHeaders& headers, uint16_t iStatusCode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// build the filter if it is not yet created, and return a KInStream reference to it
	KInStream& FilteredStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInStream& UnfilteredStream()
	//-----------------------------------------------------------------------------
	{
		return *m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Stream into OutStream
	size_t Read(KOutStream& OutStream, size_t len = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KStringRef& sBuffer, size_t len = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KStringRef& sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bAllowUncompression = bYesNo;
	}

	//-----------------------------------------------------------------------------
	/// result is only valid after first call to Read(), ReadLine() or Stream()
	bool eof()
	//-----------------------------------------------------------------------------
	{
		return m_Filter->eof();
	}

	//-----------------------------------------------------------------------------
	void reset();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns true if the input stream failed before (not on EOF)
	bool Fail() const
	//-----------------------------------------------------------------------------
	{
		return m_InStream && m_InStream->InStream().fail();
	}

	//-----------------------------------------------------------------------------
	/// get count of read bytes so far - this is not reliable in-flight, as pipeline buffers may already have been filled.
	/// It will though work reliably after close() ..
	std::streamsize Count() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reset count of read bytes - this is not reliable in-flight, as pipeline buffers may already have been filled.
	/// It will though work reliably after close() ..
	bool ResetCount();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	bool SetupInputFilter();
	//-----------------------------------------------------------------------------

	static KInStringStream s_Empty;

	KInStream*      m_InStream            { &s_Empty  };
	// We made the filter a unique_ptr because we want to be able to move
	// construct this class. We never reset it so it will never be null otherwise.
	std::unique_ptr<boost::iostreams::filtering_istream>
					m_Filter              { std::make_unique<boost::iostreams::filtering_istream>() };
	KInStream       m_FilteredInStream    { *m_Filter };
	std::streamsize m_iContentSize        { -1        };
	std::streamsize m_iCount              { 0         };
	bool            m_bChunked            { false     };
	bool            m_bAllowUncompression { true      };

};

} // of namespace dekaf2


