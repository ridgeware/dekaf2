/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2023, Ridgeware, Inc.
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

/// @file kmodifyingstreambuf.h
/// a streambuf that modifies the bytes flowing through it

#include "kstreambufadaptor.h"
#include "kstringview.h"
#include "kstring.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to modify the characters written through a streambuf
class DEKAF2_PUBLIC KModifyingOutputStreamBuf : public KOutStreamBufAdaptor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using KOutStreamBufAdaptor::KOutStreamBufAdaptor;

	~KModifyingOutputStreamBuf();

	/// Replace sequence sSearch with sequence sReplace during streaming.
	/// If sSearch is empty insert sReplace at every start of line!
	void Replace(KStringView sSearch, KStringView sReplace);

//-------
protected:
//-------

	virtual bool Output(char ch) override;

	virtual bool Output(KStringView sOut) override;

//-------
private:
//-------

	bool OutputSingleChar(char ch);

	KString     m_sSearch;
	KString     m_sReplace;
	std::size_t m_iFound { 0 };
	bool        m_bAtStartOfLine { true };

}; // KModifyingOutputStreamBuf

#if 0
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to modify the characters read through a streambuf
class DEKAF2_PUBLIC KModifyingInputStreamBuf : std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	KModifyingInputStreamBuf() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KModifyingInputStreamBuf(std::istream& istream);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_KREADER
	//-----------------------------------------------------------------------------
	KModifyingInputStreamBuf(dekaf2::KInStream& instream)
	//-----------------------------------------------------------------------------
	: KModifyingInputStreamBuf(instream.InStream())
	{
	}
#endif

	//-----------------------------------------------------------------------------
	virtual ~KModifyingInputStreamBuf();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Attach(std::istream& istream);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_KREADER
	//-----------------------------------------------------------------------------
	void Attach(dekaf2::KInStream& instream)
	//-----------------------------------------------------------------------------
	{
		Attach(instream.InStream());
	}
#endif

	//-----------------------------------------------------------------------------
	void Detach();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get count of read bytes so far
	std::streamsize Count() const { return m_iCount; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reset count of read bytes
	void ResetCount() { m_iCount = 0; }
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

	std::istream*   m_istream    { nullptr };
	std::streambuf* m_SBuf       { nullptr };
	std::streamsize m_iCount     { 0 };
	char_type       m_chBuf;

}; // KModifyingInputStreamBuf

#endif

DEKAF2_NAMESPACE_END
