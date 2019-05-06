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

#pragma once

// for chunked transfer and compression
#include <boost/iostreams/filtering_stream.hpp>

#include "kstringview.h"
#include "khttp_header.h"
#include "kinstringstream.h"


/// @file khttpinputfilter.h
/// HTTP streaming input filter implementation


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KInHTTPFilter
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
	KInHTTPFilter& operator=(KInHTTPFilter&&) = default;

	//-----------------------------------------------------------------------------
	/// Set a new input stream for the filter
	void SetInputStream(KInStream& InStream)
	//-----------------------------------------------------------------------------
	{
		m_InStream = &InStream;
	}

	//-----------------------------------------------------------------------------
	/// read input configuration from existing set of headers, but do not
	/// yet build the filter.
	bool Parse(const KHTTPHeaders& headers);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// build the filter if it is not yet created, and return a KOutStream reference to it
	KInStream& FilteredStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInStream& UnfilteredStream()
	//-----------------------------------------------------------------------------
	{
		return *m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Stream into outstream
	size_t Read(KOutStream& OutStream, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KString& sBuffer, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KString& sBuffer);
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
		return m_Filter.eof();
	}

	//-----------------------------------------------------------------------------
	void close();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	bool SetupInputFilter();
	//-----------------------------------------------------------------------------

	enum COMP
	{
		NONE,
		GZIP,
		ZLIB
	};

	static KInStringStream s_Empty;

	KInStream* m_InStream { &s_Empty };
	boost::iostreams::filtering_istream m_Filter;
	KInStream m_FilteredInStream { m_Filter };
	COMP m_Compression { NONE };
	bool m_bChunked { false };
	bool m_bAllowUncompression { true };
	std::streamsize m_iContentSize { -1 };

};

} // of namespace dekaf2


