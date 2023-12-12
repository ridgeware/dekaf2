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

/// @file klambdastream.h
/// provides an implementation of std::istreams for lambda interface inside a web server

#include "kstring.h"
#include "kstreambuf.h"

DEKAF2_NAMESPACE_BEGIN


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::istream implementation
class DEKAF2_PUBLIC KLambdaInStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::istream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construcs a CGI stream from an istream
	KLambdaInStream(std::istream& stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.istream->good();
	}

	//-----------------------------------------------------------------------------
	bool CreateHeader();
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	struct Stream
	{
		std::istream* istream;
		KStringView   sHeader;
	};

	Stream  m_Stream;
	KString m_sHeader;

	KInStreamBuf m_StreamBuf{&StreamReader, &m_Stream};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE static std::streamsize StreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

}; // KLambdaInStream

DEKAF2_NAMESPACE_END

