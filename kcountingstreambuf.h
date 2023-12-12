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

#include "kdefinitions.h"
#include "kstreambufadaptor.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to count the characters written through a streambuf
class DEKAF2_PUBLIC KCountingOutputStreamBuf : public KOutStreamBufAdaptor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using KOutStreamBufAdaptor::KOutStreamBufAdaptor;

	/// get count of written bytes so far
	std::streamsize Count() const { return m_iCount; }

	/// reset count of written bytes
	void ResetCount() { m_iCount = 0; }

//-------
protected:
//-------

	virtual bool Output(char ch) override;

	virtual bool Output(KStringView sOut) override;

//-------
private:
//-------

	std::streamsize m_iCount  { 0 };

}; // KCountingOutputStreamBuf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to count the characters read through a streambuf
class DEKAF2_PUBLIC KCountingInputStreamBuf : public KInStreamBufAdaptor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using KInStreamBufAdaptor::KInStreamBufAdaptor;

	/// get count of read bytes so far
	std::streamsize Count() const { return m_iCount; }

	/// reset count of read bytes
	void ResetCount() { m_iCount = 0; }

//-------
protected:
//-------

	virtual int_type Inspect(char_type ch) override;

	virtual std::streamsize Inspect(char* sBuffer, std::streamsize iSize) override;

//-------
private:
//-------

	std::streamsize m_iCount     { 0 };

}; // KCountingInputStreamBuf

DEKAF2_NAMESPACE_END

