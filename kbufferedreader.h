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
#include <fcntl.h>
#include "kstringview.h"
#include "kreader.h"
#include "kfilesystem.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBufferedReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	virtual ~KBufferedReader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// read one char
	std::istream::int_type Read()
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_Arena.pos != m_Arena.end))
		{
			return *m_Arena.pos++;
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
		if (DEKAF2_LIKELY(m_Arena.end - m_Arena.pos >= static_cast<ssize_t>(iSize)))
		{
			KStringView sv(m_Arena.pos, iSize);
			m_Arena.pos += iSize;
			return sv;
		}
		else
		{
			return ReadMore(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// Return a string view with the next line. Trim as requested.
	bool ReadLine(KStringView& sLine, KStringView::value_type delimiter = '\n', KStringView sRightTrim = detail::kLineRightTrims);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a string with the next line. Trim as requested. Returns false on eof
	bool ReadLine(KString& sBuffer, KStringView::value_type delimiter = '\n', KStringView sRightTrim = detail::kLineRightTrims);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// put back iSize number of characters (default == 1)
	bool UnRead(size_t iSize = 1)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_Arena.pos - iSize >= m_Arena.start))
		{
			m_Arena.pos -= iSize;
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
		return m_Arena.pos == m_Arena.end && m_bEOF;
	}

	//-----------------------------------------------------------------------------
	/// returns maximum possible string view read size
	virtual size_t MaxRead() const = 0;
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	struct Arena
	{
		Arena(const char* _start = nullptr, size_t _iBufferSize = 0)
		: start(_start)
		, pos(_start)
		, end(_start)
		, iBufferSize(_iBufferSize)
		{
		}

		Arena(KStringView sInput)
		: start(sInput.data())
		, pos(start)
		, end(start + sInput.size())
		{
		}

		Arena(const char* _start, const char* _pos, const char* _end, size_t _iBufferSize)
		: start(_start)
		, pos(_pos)
		, end(_end)
		, iBufferSize(_iBufferSize)
		{
		}

		const char* start { nullptr };
		const char* pos { nullptr };
		const char* end { nullptr };
		size_t iBufferSize { 0 };
	};

	virtual bool UnReadStreamBuf(size_t iSize) { return false; };
	virtual KStringView ReadMore(size_t iSize) = 0;
	virtual std::istream::int_type Fill() = 0;

	Arena m_Arena;
	bool m_bEOF { false };

}; // KBufferedReader

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBufferedStreamReader : public KBufferedReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// construct from any istream
	KBufferedStreamReader(KInStream& istream, size_t iBufferSize = 2048);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns maximum possible string view read size (string read size is unlimited)
	virtual size_t MaxRead() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Arena.iBufferSize;
	}

//-------
protected:
//-------

	virtual bool UnReadStreamBuf(size_t iSize) override final;
	virtual KStringView ReadMore(size_t iSize) override final;
	virtual std::istream::int_type Fill() override final;

//-------
private:
//-------

	std::istream* m_istream { nullptr };
	std::unique_ptr<char[]> m_buffer;

}; // KBufferedStreamReader

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBufferedStringReader : public KBufferedReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// construct from any string view
	KBufferedStringReader(KStringView sInput);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns maximum possible string view read size (string read size is unlimited)
	virtual size_t MaxRead() const override final
	//-----------------------------------------------------------------------------
	{
		return npos;
	}

//-------
protected:
//-------

	virtual bool UnReadStreamBuf(size_t iSize) override final { return false; }
	virtual KStringView ReadMore(size_t iSize) override final { return KStringView { m_Arena.pos, static_cast<size_t>(m_Arena.end - m_Arena.pos) }; };
	virtual std::istream::int_type Fill() override final { return std::istream::traits_type::eof(); }

//-------
private:
//-------

}; // KBufferedStreamReader

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBufferedFileReader : public KBufferedReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// Construct from file descriptor fd. Does not close the fd on destruction.
	KBufferedFileReader(int fd, size_t iBufferSize = 16384);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct from sFilename. Will close the file on destruction.
	KBufferedFileReader(KStringViewZ sFilename, size_t iBufferSize = 16384);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KBufferedFileReader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns maximum possible string view read size (string read size is unlimited)
	virtual size_t MaxRead() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Arena.iBufferSize;
	}

//-------
protected:
//-------

	virtual bool UnReadStreamBuf(size_t iSize) override final;
	virtual KStringView ReadMore(size_t iSize) override final;
	virtual std::istream::int_type Fill() override final;

//-------
private:
//-------

	std::unique_ptr<char[]> m_buffer;
	int m_fd { -1 };
	bool m_bOwnsFileDescriptor { false };

}; // KBufferedFileReader

} // end of namespace dekaf2
