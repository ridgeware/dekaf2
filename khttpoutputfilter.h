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


/// @file khttpoutputfilter.h
/// HTTP streaming output filter implementation


namespace dekaf2 {

class KHTTPOutputFilter
{
public:

	//-----------------------------------------------------------------------------
	/// read input configuration from existing set of headers, but do not
	/// yet build the filter.
	bool Parse(const KHTTPHeaders& headers);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// build the filter if it is not yet created, and return a KOutStream reference to it
	KOutStream& Stream(KOutStream& OutStream)
	//-----------------------------------------------------------------------------
	{
		if (m_Filter.empty())
		{
			SetupOutputFilter(OutStream);
		}
		return m_OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Stream from InStream into OutStream
	size_t Write(KOutStream& OutStream, KInStream& InStream, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write sBuffer into OutStream
	size_t Write(KOutStream& OutStream, KStringView sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write one line into OutStream, including EOL
	bool WriteLine(KOutStream& OutStream, KStringView sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Compress(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bPerformCompression = bYesNo;
	}

private:

	//-----------------------------------------------------------------------------
	bool SetupOutputFilter(KOutStream& OutStream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void reset();
	//-----------------------------------------------------------------------------

	enum COMP
	{
		NONE,
		GZIP,
		ZLIB
	};

	boost::iostreams::filtering_ostream m_Filter;
	KOutStream m_OutStream { m_Filter };
	COMP m_Compression { NONE };
	bool m_bChunked { false };
	bool m_bPerformCompression { true };
	std::streamsize m_iContentSize { -1 };

}; // KHTTPOutputFilter

} // of namespace dekaf2


