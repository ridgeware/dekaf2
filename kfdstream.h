/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

#include <cstdio>
#include <streambuf>
#include <istream>
#include <type_traits>
#include "kstream.h"
#include "kstreambuf.h"


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an unbuffered std::istream that is constructed around a unix file descriptor
/// (mainly to allow its usage with pipes, for general file I/O use std::ofstream)
/// (really, do it - this one is really slow on small writes to files, on purpose,
/// because pipes should not be buffered!)
class KInputFDStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::istream;

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInputFDStream()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInputFDStream(const KInputFDStream&) = delete;
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KInputFDStream(KInputFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KInputFDStream(int iFileDesc)
	//-----------------------------------------------------------------------------
	{
		open(iFileDesc);
	}

	//-----------------------------------------------------------------------------
	virtual ~KInputFDStream();
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KInputFDStream& operator=(KInputFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KInputFDStream& operator=(const KInputFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from a standard unix
	/// file descriptor
	void open(int iFileDesc);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this output stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDesc >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the output stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the file descriptor
	int GetDescriptor() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDesc;
	}

//----------
protected:
//----------
	int m_FileDesc{-1};

	// see comment in KOutputStream about the legality
	// to only construct the KInStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf{&FileDescReader, &m_FileDesc};
};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a buffered std::istream that is constructed around a FILE ptr
/// (mainly to allow its usage with pipes, for general file I/O use std::ofstream)
/// (really, do it - this one does not implement the full istream interface)
class KInputFPStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------

	using base_type = std::istream;

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize FilePtrReader(void* sBuffer, std::streamsize iCount, void* filedesc);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInputFPStream()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInputFPStream(const KInputFPStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInputFPStream(KInputFPStream&& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a FILE ptr
	KInputFPStream(FILE* iFilePtr)
	//-----------------------------------------------------------------------------
	{
		open(iFilePtr);
	}

	//-----------------------------------------------------------------------------
	virtual ~KInputFPStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInputFPStream& operator=(KInputFPStream&& other);
	//-----------------------------------------------------------------------------

	KInputFPStream& operator=(const KInputFPStream&) = delete;

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from a FILE ptr
	void open(FILE* iFilePtr);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this input stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FilePtr;
	}

	//-----------------------------------------------------------------------------
	/// close the output stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the file ptr
	FILE* GetPtr() const
	//-----------------------------------------------------------------------------
	{
		return m_FilePtr;
	}

//----------
protected:
//----------
	FILE* m_FilePtr{nullptr};

	// see comment in KOutputFDStream about the legality
	// to only construct the KInStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf{&FilePtrReader, &m_FilePtr};
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an unbuffered std::ostream that is constructed around a unix file descriptor
/// (mainly to allow its usage with pipes, for general file I/O use std::ofstream)
/// (really, do it - this one is really slow on small writes to files, on purpose,
/// because pipes should not be buffered!)
class KOutputFDStream : public std::ostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::ostream;

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KOutputFDStream()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KOutputFDStream(const KOutputFDStream&) = delete;
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KOutputFDStream(KOutputFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KOutputFDStream(int iFileDesc)
	//-----------------------------------------------------------------------------
	{
		open(iFileDesc);
	}

	//-----------------------------------------------------------------------------
	virtual ~KOutputFDStream();
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KOutputFDStream& operator=(KOutputFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KOutputFDStream& operator=(const KOutputFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from a standard unix
	/// file descriptor
	void open(int iFileDesc);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this output stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDesc >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the output stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the file descriptor
	int GetDescriptor() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDesc;
	}

//----------
protected:
//----------

	int m_FileDesc{-1};

	// jschurig: The standard guarantees that the streambuf is neither used by
	// constructors nor by destructors of base classes of a streambuf, nor by
	// base classes that do not declare the streambuf themselves.
	// Therefore it is safe to only construct it here, but use it in the
	// constructor above. Angelika Langer had pointed out in 1995 that otherwise a
	// private virtual inheritance would have guaranteed a protection against side
	// effects of constructors accessing incomplete data types, but we do not need
	// this anymore after C++98. I mention this because I was wondering about the
	// constructor order when declaring this class, and did not know that this was
	// solved by the standard before reading her article:
	// http://www.angelikalanger.com/Articles/C++Report/IOStreamsDerivation/IOStreamsDerivation.html
	KOutStreamBuf m_FPStreamBuf{&FileDescWriter, &m_FileDesc};
};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a buffered std::ostream that is constructed around a FILE ptr
/// (mainly to allow its usage with pipes, for general file I/O use std::ofstream)
/// (really, do it - this one does not implement the full ostream interface)
class KOutputFPStream : public std::ostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::ostream;

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize FilePtrWriter(const void* sBuffer, std::streamsize iCount, void* fileptr);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KOutputFPStream()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KOutputFPStream(const KOutputFPStream&) = delete;
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KOutputFPStream(KOutputFPStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KOutputFPStream(FILE* iFilePtr)
	//-----------------------------------------------------------------------------
	{
		open(iFilePtr);
	}

	//-----------------------------------------------------------------------------
	virtual ~KOutputFPStream();
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KOutputFPStream& operator=(KOutputFPStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KOutputFPStream& operator=(const KOutputFPStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from a standard unix
	/// file descriptor
	void open(FILE* iFilePtr);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this output stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FilePtr;
	}

	//-----------------------------------------------------------------------------
	/// close the output stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the file ptr
	FILE* GetPtr() const
	//-----------------------------------------------------------------------------
	{
		return m_FilePtr;
	}

//----------
protected:
//----------

	FILE* m_FilePtr{nullptr};

	KOutStreamBuf m_FPStreamBuf{&FilePtrWriter, &m_FilePtr};
};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an unbuffered std::iostream that is constructed around one or two unix file
/// descriptors
/// (mainly to allow its usage with pipes, for general file I/O use std::fstream)
/// (really, do it - this one is really slow on small writes to files, on purpose,
/// because pipes should not be buffered!)
class KInOutFDStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::iostream;

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInOutFDStream()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInOutFDStream(const KInOutFDStream&) = delete;
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KInOutFDStream(KInOutFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from one or two standard
	/// unix file descriptors
	KInOutFDStream(int iFileDescR, int iFileDescW = -1)
	//-----------------------------------------------------------------------------
	{
		open(iFileDescR, iFileDescW);
	}

	//-----------------------------------------------------------------------------
	virtual ~KInOutFDStream();
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	//-----------------------------------------------------------------------------
	KInOutFDStream& operator=(KInOutFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KInOutFDStream& operator=(const KInOutFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from one or two standard unix
	/// file descriptors. If only one file descriptor is given, it is
	/// used for both reading and writing.
	void open(int iFileDescR, int iFileDescW = -1);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDescR >= 0 && m_FileDescW >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the output stream
	void close();
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	int m_FileDescR{-1};
	int m_FileDescW{-1};

	// see comment in KOutputFDStream about the legality
	// to only construct the KIStreamBuf here, but to use it in
	// the constructor before
	KStreamBuf m_FPStreamBuf{&FileDescReader, &FileDescWriter, &m_FileDescR, &m_FileDescW};

};

extern template class KWriter<KOutputFDStream>;
extern template class KWriter<KOutputFPStream>;
extern template class KReader<KInputFDStream>;
extern template class KReader<KInputFPStream>;
extern template class KReaderWriter<KInOutFDStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! File descriptor writer based on KOutputFDStream>
using KFDWriter = KWriter<KOutputFDStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! FILE ptr writer based on KOutputFPStream>
using KFPWriter = KWriter<KOutputFPStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! File descriptor reader based on KInputFDStream
using KFDReader = KReader<KInputFDStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! FILE* reader based on KInputFPStream
using KFPReader = KReader<KInputFPStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! File descriptor reader/writer based on KInOutFDStream
using KFDStream = KReaderWriter<KInOutFDStream>;

} // end of namespace dekaf2

