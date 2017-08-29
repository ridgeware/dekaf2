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

/// @file kfdreader.h
/// provides std::istreams that can be constructed from open file descriptors and FILE*

#include <cstdio>
#include <streambuf>
#include <istream>
#include <type_traits>
#include "kreader.h"


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Unbuffered std::istream that is constructed around a unix file descriptor.
/// Mainly to allow its usage with pipes, for general file I/O use std::ifstream.
/// This one is really slow on small reads on files, on purpose, because pipes
/// should not be buffered. Therefore do _not_ use it for ordinary file I/O.
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

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
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

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KInputFDStream& operator=(KInputFDStream&& other);
	//-----------------------------------------------------------------------------
#endif

	KInputFDStream& operator=(const KInputFDStream&) = delete;

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

	// see comment in KWriter's KOStreamBuf about the legality
	// to only construct the KIStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf{&FileDescReader, &m_FileDesc};

};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffered std::istream that is constructed around a FILE*.
/// Mainly to allow its usage with pipes, for general file I/O use std::ifstream.
/// Do _not_ use it for ordinary file I/O, it does not implement the full
/// std::istream interface.
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
	/// copy constructor is deleted
	KInputFPStream(const KInputFPStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move constructor
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
	/// destructor
	virtual ~KInputFPStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	KInputFPStream& operator=(KInputFPStream&& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted
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

	// see comment in KWriter's KOStreamBuf about the legality
	// to only construct the KIStreamBuf here, but to use it in
	// the constructor before
	KInStreamBuf m_FPStreamBuf{&FilePtrReader, &m_FilePtr};

};


/// FOR PIPES AND SPECIAL DEVICES ONLY! File descriptor reader based on KInputFDStream
using KFDReader = KReader<KInputFDStream>;

/// FOR PIPES AND SPECIAL DEVICES ONLY! FILE* reader based on KInputFPStream
using KFPReader = KReader<KInputFPStream>;

} // end of namespace dekaf2

