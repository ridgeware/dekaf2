/*
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


namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
/// custom streambuf reader for file descriptors
std::streamsize FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// custom streambuf writer for file descriptors
std::streamsize FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// custom streambuf reader for file pointers
std::streamsize FilePtrReader(void* sBuffer, std::streamsize iCount, void* fileptr);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// custom streambuf writer for file pointers
std::streamsize FilePtrWriter(const void* sBuffer, std::streamsize iCount, void* fileptr);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an unbuffered std::istream that is constructed around a unix file descriptor
/// (mainly to allow its usage with pipes, for general file I/O use std::ifstream)
/// (really, do it - this one is really slow on small writes to files, on purpose,
/// because pipes should not be buffered!)
class KInputFDStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::istream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInputFDStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KInputFDStream(const KInputFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KInputFDStream(KInputFDStream&& other)
	//-----------------------------------------------------------------------------
	: KInputFDStream(other.m_FileDesc)
	{
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KInputFDStream(int iFileDesc)
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
		open(iFileDesc);
	}

	// do not call close on destruction. This class did not open the file
	// but just received a handle for it

	//-----------------------------------------------------------------------------
	KInputFDStream& operator=(KInputFDStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInputFDStream& operator=(const KInputFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: open from a standard unix
	/// file descriptor
	void open(int iFileDesc);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a file is associated to this stream
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_FileDesc >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// invalidate the file descriptor
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_FileDesc = -1;
	}

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

	int m_FileDesc { -1 };

	// see comment in KOutputStream about the legality
	// to only construct the KInStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf { &detail::FileDescReader, &m_FileDesc };

};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a buffered std::istream that is constructed around a FILE ptr
/// (mainly to allow its usage with pipes, for general file I/O use std::ifstream)
/// (really, do it - this one does not implement the full streambuf interface and
/// may therefore be slower, and it is not seekable)
class KInputFPStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------

	using base_type = std::istream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInputFPStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KInputFPStream(const KInputFPStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KInputFPStream(KInputFPStream&& other)
	//-----------------------------------------------------------------------------
	: KInputFPStream(other.m_FilePtr)
	{
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a FILE ptr
	KInputFPStream(FILE* iFilePtr)
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
		open(iFilePtr);
	}

	// do not call close on destruction. This class did not open the file
	// but just received a handle for it

	//-----------------------------------------------------------------------------
	KInputFPStream& operator=(KInputFPStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInputFPStream& operator=(const KInputFPStream&) = delete;
	//-----------------------------------------------------------------------------

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
	/// close the stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// invalidate the FILE ptr
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_FilePtr = nullptr;
	}

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
	FILE* m_FilePtr { nullptr };

	// see comment in KOutputFDStream about the legality
	// to only construct the KInStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf { &detail::FilePtrReader, &m_FilePtr };

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

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KOutputFDStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KOutputFDStream(const KOutputFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KOutputFDStream(KOutputFDStream&& other)
	//-----------------------------------------------------------------------------
	: KOutputFDStream(other.m_FileDesc)
	{
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KOutputFDStream(int iFileDesc)
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
		open(iFileDesc);
	}

	// do not call close on destruction. This class did not open the file
	// but just received a handle for it

	//-----------------------------------------------------------------------------
	KOutputFDStream& operator=(KOutputFDStream&& other) = default;
	//-----------------------------------------------------------------------------

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
	/// invalidate the file descriptor
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_FileDesc = -1;
	}

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

	int m_FileDesc { -1 };

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
	KOutStreamBuf m_FPStreamBuf { &detail::FileDescWriter, &m_FileDesc };

};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a buffered std::ostream that is constructed around a FILE ptr
/// (mainly to allow its usage with pipes, for general file I/O use std::ofstream)
/// (really, do it - this one does not implement the full streambuf interface and
/// may therefore be slower, and it is not seekable)
class KOutputFPStream : public std::ostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::ostream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KOutputFPStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KOutputFPStream(const KOutputFPStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KOutputFPStream(KOutputFPStream&& other)
	//-----------------------------------------------------------------------------
	: KOutputFPStream(other.m_FilePtr)
	{
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from a standard unix
	/// file descriptor
	KOutputFPStream(FILE* iFilePtr)
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
		open(iFilePtr);
	}

	// do not call close on destruction. This class did not open the file
	// but just received a handle for it

	//-----------------------------------------------------------------------------
	KOutputFPStream& operator=(KOutputFPStream&& other) = default;
	//-----------------------------------------------------------------------------

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
	/// invalidate the file descriptor
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_FilePtr = nullptr;
	}

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

	FILE* m_FilePtr { nullptr };

	KOutStreamBuf m_FPStreamBuf { &detail::FilePtrWriter, &m_FilePtr };

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

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KInOutFDStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KInOutFDStream(const KInOutFDStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KInOutFDStream(KInOutFDStream&& other)
	//-----------------------------------------------------------------------------
	: KInOutFDStream(other.m_FileDescR, other.m_FileDescW)
	{
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: allow construction from one or two standard
	/// unix file descriptors
	KInOutFDStream(int iFileDescR, int iFileDescW = -1)
	//-----------------------------------------------------------------------------
		: base_type(&m_FPStreamBuf)
	{
		open(iFileDescR, iFileDescW);
	}

	// do not call close on destruction. This class did not open the file
	// but just received a handle for it

	//-----------------------------------------------------------------------------
	KInOutFDStream& operator=(KInOutFDStream&& other) = default;
	//-----------------------------------------------------------------------------

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
	/// close the stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// invalidate the FILE ptrs
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_FileDescR = m_FileDescW = -1;
	}

//----------
protected:
//----------

	int m_FileDescR{-1};
	int m_FileDescW{-1};

	// see comment in KOutputFDStream about the legality
	// to only construct the KIStreamBuf here, but to use it in
	// the constructor before
	KStreamBuf m_FPStreamBuf { &detail::FileDescReader, &detail::FileDescWriter, &m_FileDescR, &m_FileDescW };

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

