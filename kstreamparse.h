/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include <istream>
#include "kstringview.h"
#include "kreader.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper class for RAII on file descriptors
class FDFile
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// Open an unbuffered file
	FDFile(KStringViewZ sFilename, int mode = O_RDWR);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~FDFile();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	operator int() const { return m_fd; }
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	int m_fd;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A fast sequential reader from any standard istream or any string or file
/// descriptor
class KStreamParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// default construct
	KStreamParser() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// construct from any istream
	KStreamParser(KInStream& istream, size_t iBufferSize = 2048);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// construct from a string view
	KStreamParser(KStringView sInput);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// construct from a file descriptor
	KStreamParser(int fd, size_t iBufferSize = 16384);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// read one char
	std::istream::int_type Read()
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_pos != m_end))
		{
			return *m_pos++;
		}
		else
		{
			return Fill();
 		}
	}

	//-----------------------------------------------------------------------------
	/// if iSize is <= iBufferSize, return a string view on the next iSize bytes
	KStringView Read(size_t iSize)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_end - m_pos >= static_cast<ssize_t>(iSize)))
		{
			KStringView sv(m_pos, iSize);
			m_pos += iSize;
			return sv;
		}
		else
		{
			return ReadMore(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// Return a string view with the next line. Trim as requested.
	KStringView ReadLine(KStringView::value_type delimiter = '\n', KStringView sRightTrim = "\r\n");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a string with the next line. Trim as requested. Returns false on eof
	bool ReadLine(KString& sBuffer, KStringView::value_type delimiter = '\n', KStringView sRightTrim = "\r\n");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// put back iSize number of characters
	bool UnRead(size_t iSize = 1)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_pos - iSize >= m_start))
		{
			m_pos -= iSize;
			return true;
		}
		else
		{
			return UnReadStreamBuf(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// no more data?
	bool EndOfStream() const
	//-----------------------------------------------------------------------------
	{
		return m_pos == m_end && m_bEOF;
	}

	//-----------------------------------------------------------------------------
	/// returns maximum possible string view read size
	size_t MaxRead() const
	//-----------------------------------------------------------------------------
	{
		return m_istream ? m_iBufferSize : npos;
	}

//-------
protected:
//-------

	bool UnReadStreamBuf(size_t iSize);
	KStringView ReadMore(size_t iSize);
	std::istream::int_type Fill();

//-------
private:
//-------

	std::istream* m_istream { nullptr };
	std::unique_ptr<char[]> m_buffer;
	const char* m_start { nullptr };
	const char* m_pos { nullptr };
	const char* m_end { nullptr };
	size_t m_iBufferSize { 0 };
	int m_fd { -1 };
	bool m_bEOF { false };

}; // KStreamParser

} // end of namespace dekaf2
