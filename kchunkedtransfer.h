/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#pragma once

/// @file kchunkedtransfer.h
/// Implements chunked reader and writer as boost iostreams source and sink

#include <boost/iostreams/concepts.hpp>    // source
#include "kstream.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KChunkedSource : public boost::iostreams::source
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	enum STATE
	{
		StartingUp,
		ReadingSize,
		SkipUntilEmptyLine,
		HadNonEmptyLine,
		IsNotChunked,
		ReadingChunk,
		ReadingChunkEnd,
		Finished
	};

//------
public:
//------

	//-----------------------------------------------------------------------------
	KChunkedSource(KInStream& src, bool bIsChunked, std::streamsize iContentLen = -1);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize read(char* s, std::streamsize n);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// non-boost interface to read a whole chunked transfer into a KString
	KString read();
	//-----------------------------------------------------------------------------

//------
private:
//------

	KInStream& m_src;
	std::streamsize m_iRemainingInChunk { 0 };
	std::streamsize m_iContentLen;
	STATE m_State;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KChunkedSink : public boost::iostreams::sink
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KChunkedSink(KOutStream& sink, bool bIsChunked);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize write(const char* s, std::streamsize n);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// non-boost interface to write a KStringView in chunked mode
	std::streamsize write(KStringView sBuffer)
	//-----------------------------------------------------------------------------
	{
		return write(sBuffer.data(), sBuffer.size());
	}

	//-----------------------------------------------------------------------------
	void close();
	//-----------------------------------------------------------------------------

//------
private:
//------

	KOutStream& m_sink;
	bool m_bIsChunked;

};


} // end of namespace dekaf2

