/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

/// @file kcountingstreambuf.h
/// a streambuf that counts the bytes flowing through it

#include "kwriter.h"
#include "kreader.h"
#include <ostream>
#include <istream>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to count the characters written through a streambuf
class KCountingOutputStreamBuf : std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	KCountingOutputStreamBuf(dekaf2::KOutStream& outstream)
	//-----------------------------------------------------------------------------
	: m_ostream(outstream.OutStream())
	, m_SBuf(m_ostream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	explicit KCountingOutputStreamBuf(std::iostream& iostream)
	//-----------------------------------------------------------------------------
	: m_ostream(iostream)
	, m_SBuf(m_ostream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	explicit KCountingOutputStreamBuf(std::ostream& ostream)
	//-----------------------------------------------------------------------------
	: m_ostream(ostream)
	, m_SBuf(m_ostream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KCountingOutputStreamBuf();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get count of written bytes so far
	std::streamsize count() const { return m_iCount; }
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type c) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int sync() override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	std::ostream&   m_ostream;
	std::streambuf* m_SBuf    { nullptr };
	std::streamsize m_iCount  { 0 };

}; // KCountingOutputStreamBuf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to count the characters read through a streambuf
class KCountingInputStreamBuf : std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	explicit KCountingInputStreamBuf(dekaf2::KInStream& instream)
	//-----------------------------------------------------------------------------
	: m_istream(instream.InStream())
	, m_SBuf(m_istream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	explicit KCountingInputStreamBuf(std::iostream& iostream)
	//-----------------------------------------------------------------------------
	: m_istream(iostream)
	, m_SBuf(m_istream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	explicit KCountingInputStreamBuf(std::istream& istream)
	//-----------------------------------------------------------------------------
	: m_istream(istream)
	, m_SBuf(m_istream.rdbuf(this))
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KCountingInputStreamBuf();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get count of read bytes so far
	std::streamsize count() const { return m_iCount; }
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual pos_type seekoff(off_type off,
							 std::ios_base::seekdir dir,
							 std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	std::istream&   m_istream;
	std::streambuf* m_SBuf       { nullptr };
	std::streamsize m_iCount     { 0 };
	char_type       m_chBuf;

}; // KCountingInputStreamBuf

} // end of namespace dekaf2

