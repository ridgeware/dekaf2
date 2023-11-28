/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kdefinitions.h"
#include <streambuf>
#include <array>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a streambuf that reads from /dev/null and writes to /dev/null (and is fast at it)
class DEKAF2_PUBLIC KNullStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	virtual ~KNullStreamBuf();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static KNullStreamBuf& Create();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	char_type m_chBuf;

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customizable input stream buffer
class DEKAF2_PUBLIC KInStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Reader's function's signature:
	/// std::streamsize Reader(void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns read bytes. CustomPointer can be used for anything, to the discretion of the
	/// Reader.
	using Reader = std::streamsize (*)(void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KInStreamBuf(Reader rcb, void* CustomPointerR = nullptr)
	//-----------------------------------------------------------------------------
	    : m_CallbackR(rcb), m_CustomPointerR(CustomPointerR)
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KInStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	static constexpr std::streamsize STREAMBUFSIZE { 256 };

	Reader m_CallbackR     { nullptr };
	void* m_CustomPointerR { nullptr };

	std::array<char_type, STREAMBUFSIZE> m_buf;

}; // KInStreamBuf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customized output stream buffer
class DEKAF2_PUBLIC KOutStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	using Writer = std::streamsize (*)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Writer function, it will be called by std::streambuf on buffer flushes
	KOutStreamBuf(Writer cb, void* CustomPointer = nullptr)
	//-----------------------------------------------------------------------------
	    : m_Callback(cb), m_CustomPointer(CustomPointer)
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KOutStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	Writer m_Callback{nullptr};
	void* m_CustomPointer{nullptr};

}; // KOutStreamBuf


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customizable input / output stream buffer
class DEKAF2_PUBLIC KStreamBuf : public KInStreamBuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	using Writer = std::streamsize (*)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KStreamBuf(Reader rcb, Writer wcb, void* CustomPointerR = nullptr, void* CustomPointerW = nullptr)
	//-----------------------------------------------------------------------------
	    : KInStreamBuf(rcb, CustomPointerR)
	    , m_CallbackW(wcb), m_CustomPointerW(CustomPointerW)
	{
	}

	//-----------------------------------------------------------------------------
	virtual ~KStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	Writer m_CallbackW{nullptr};
	void* m_CustomPointerW{nullptr};

}; // KStreamBuf


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customized buffered output stream buffer
class DEKAF2_PUBLIC KBufferedOutStreamBuf : public KOutStreamBuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	using Writer = std::streamsize (*)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Writer function, it will be called by std::streambuf on buffer flushes
	KBufferedOutStreamBuf(Writer cb, void* CustomPointer = nullptr)
	//-----------------------------------------------------------------------------
	: KOutStreamBuf(cb, CustomPointer)
	{
		setp(m_buf.data(), m_buf.data()+STREAMBUFSIZE);
	}

	//-----------------------------------------------------------------------------
	virtual ~KBufferedOutStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	using base_type = KOutStreamBuf;

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int sync() override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	//-----------------------------------------------------------------------------
	std::streamsize RemainingSize() const
	//-----------------------------------------------------------------------------
	{
		return epptr() - pptr();
	}

	//-----------------------------------------------------------------------------
	std::streamsize FlushableSize() const
	//-----------------------------------------------------------------------------
	{
		return pptr() - m_buf.data();
	}

	static constexpr std::streamsize STREAMBUFSIZE { 4096 };
	static constexpr std::streamsize DIRECTWRITE   { 2048 };

	std::array<char_type, STREAMBUFSIZE> m_buf;

	static_assert(STREAMBUFSIZE - DIRECTWRITE >= DIRECTWRITE, "buffer size has to be at least twice as large as the direct write threshold");

}; // KBufferedOutStreamBuf


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customizable buffered input / output stream buffer
class DEKAF2_PUBLIC KBufferedStreamBuf : public KStreamBuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	using Writer = std::streamsize (*)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KBufferedStreamBuf(Reader rcb, Writer wcb, void* CustomPointerR = nullptr, void* CustomPointerW = nullptr)
	//-----------------------------------------------------------------------------
	: KStreamBuf(rcb, wcb, CustomPointerR, CustomPointerW)
	{
		setp(m_buf.data(), m_buf.data()+STREAMBUFSIZE);
	}

	//-----------------------------------------------------------------------------
	virtual ~KBufferedStreamBuf();
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	using base_type = KStreamBuf;

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int sync() override;
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	//-----------------------------------------------------------------------------
	std::streamsize RemainingSize() const
	//-----------------------------------------------------------------------------
	{
		return epptr() - pptr();
	}

	//-----------------------------------------------------------------------------
	std::streamsize FlushableSize() const
	//-----------------------------------------------------------------------------
	{
		return pptr() - m_buf.data();
	}

	static constexpr std::streamsize STREAMBUFSIZE { 4096 };
	static constexpr std::streamsize DIRECTWRITE   { 2048 };

	std::array<char_type, STREAMBUFSIZE> m_buf;

	static_assert(STREAMBUFSIZE - DIRECTWRITE >= DIRECTWRITE, "buffer size has to be at least twice as large as the direct write threshold");

}; // KBufferedStreamBuf

DEKAF2_NAMESPACE_END
