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

/// @file kstreambufadaptor.h
/// a streambuf baseclass that can be customized to watch or modify a streambuf it is attached to

#include "kwriter.h"
#include "kreader.h"
#include <ostream>
#include <istream>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to watch or modify the characters written through a streambuf
class DEKAF2_PUBLIC KOutStreamBufAdaptor : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	KOutStreamBufAdaptor() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOutStreamBufAdaptor(std::ostream& ostream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOutStreamBufAdaptor(KOutStream& outstream)
	//-----------------------------------------------------------------------------
	: KOutStreamBufAdaptor(outstream.OutStream())
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KOutStreamBufAdaptor();
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

	//-----------------------------------------------------------------------------
	/// handle a single character - this should at least include a call to Write(c)
	virtual bool Output(char ch) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// handle a string - this should at least include a call to Write(sOut)
	virtual bool Output(KStringView sOut);
	//-----------------------------------------------------------------------------

	// helpers for derived classes
	//-----------------------------------------------------------------------------
	/// write a single char to the output
	int_type Write(char ch)
	//-----------------------------------------------------------------------------
	{
		return m_ParentStreamBuf->sputc(ch);
	}

	//-----------------------------------------------------------------------------
	/// write a string to the output
	std::size_t Write(KStringView sWrite);
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	std::ostream*   m_ostream         { nullptr };
	std::streambuf* m_ParentStreamBuf { nullptr };

}; // KOutStreamBufAdaptor


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to watch or modify the characters read through a streambuf
class DEKAF2_PUBLIC KInStreamBufAdaptor : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	KInStreamBufAdaptor() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInStreamBufAdaptor(std::istream& istream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInStreamBufAdaptor(KInStream& instream)
	//-----------------------------------------------------------------------------
	: KInStreamBufAdaptor(instream.InStream())
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KInStreamBufAdaptor();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	/// inspect an input char - return original or modified input char, input never is eof, but output may be (will
	/// abort reading)
	virtual int_type Inspect(char_type ch) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Inspect an input string. The buffer contents may be changed and shortened, but not extended,
	/// and the new buffer size has to be returned. If the new buffer size is shorter than the input buffer,
	/// reading is aborted.
	virtual std::streamsize Inspect(char* sBuffer, std::streamsize iSize);
	//-----------------------------------------------------------------------------

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

	std::istream*   m_istream         { nullptr };
	std::streambuf* m_ParentStreamBuf { nullptr };
	char_type       m_chBuf;

}; // KInStreamBufAdaptor

} // end of namespace dekaf2

