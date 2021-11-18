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
#include "kstringstream.h"


/// @file khttpoutputfilter.h
/// HTTP streaming output filter implementation


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOutHTTPFilter
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
		close();
		m_OutStream = &OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Reset the output stream for the filter to nil
	void ResetOutputStream()
	//-----------------------------------------------------------------------------
	{
		m_OutStream = &s_Empty;
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
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns true if the output stream failed before
	bool Fail()
	//-----------------------------------------------------------------------------
	{
		return m_OutStream && m_OutStream->OutStream().fail();
	}

	//-----------------------------------------------------------------------------
	/// get count of written bytes so far - this is not reliable in-flight, as boost::iostreams do not properly
	/// flush on compressed streams when requested to do so. This count is only reliable after close() ..
	std::streamsize Count() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reset count of written bytes - this is not reliable in-flight, as boost::iostreams do not properly
	/// flush on compressed streams when requested to do so. Do not use this function when compression
	/// is in the pipeline (in which case it will return with false). It will though work after close() ..
	bool ResetCount();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	bool SetupOutputFilter();
	//-----------------------------------------------------------------------------

	enum COMP
	{
		NONE,
		GZIP,
		ZLIB,
		BZIP2
	};

	static KOutStringStream s_Empty;

	KOutStream*     m_OutStream         { &s_Empty  };
	// We made the filter a unique_ptr because we want to be able to move
	// construct this class. We never reset it so it will never be null.
	std::unique_ptr<boost::iostreams::filtering_ostream>
	                m_Filter            { std::make_unique<boost::iostreams::filtering_ostream>() };
	KOutStream      m_FilteredOutStream { *m_Filter };
	std::streamsize m_iCount            { 0         };
	COMP            m_Compression       { NONE      };
	bool            m_bChunked          { false     };
	bool            m_bAllowCompression { true      };

}; // KHTTPOutputFilter

} // of namespace dekaf2


