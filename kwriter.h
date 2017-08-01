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

#include <cinttypes>
#include <streambuf>
#include <ostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include "kstring.h"
#include "kformat.h"

namespace dekaf2
{


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customized output stream buffer
struct KOStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	/// the Writer function's signature:
	/// std::streamsize Writer(const void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns written bytes. CustomPointer can be used for anything, to the discretion of the
	/// Writer.
	typedef std::streamsize (*Writer)(const void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Writer function, it will be called by std::streambuf on buffer flushes
	KOStreamBuf(Writer cb, void* CustomPointer = nullptr)
	//-----------------------------------------------------------------------------
	    : m_Callback(cb), m_CustomPointer(CustomPointer)
	{
	}
	//-----------------------------------------------------------------------------
	virtual ~KOStreamBuf();
	//-----------------------------------------------------------------------------

protected:
	//-----------------------------------------------------------------------------
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type overflow(int_type ch) override;
	//-----------------------------------------------------------------------------

private:
	Writer m_Callback{nullptr};
	void* m_CustomPointer{nullptr};
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

	KOutputFDStream(const KOutputFDStream&) = delete;

	//-----------------------------------------------------------------------------
	KOutputFDStream(KOutputFDStream&& other);
	//-----------------------------------------------------------------------------

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

	//-----------------------------------------------------------------------------
	KOutputFDStream& operator=(KOutputFDStream&& other);
	//-----------------------------------------------------------------------------

	KOutputFDStream& operator=(const KOutputFDStream&) = delete;

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
	KOStreamBuf m_FPStreamBuf{&FileDescWriter, &m_FileDesc};
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

	KOutputFPStream(const KOutputFPStream&) = delete;

	//-----------------------------------------------------------------------------
	KOutputFPStream(KOutputFPStream&& other);
	//-----------------------------------------------------------------------------

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

	//-----------------------------------------------------------------------------
	KOutputFPStream& operator=(KOutputFPStream&& other);
	//-----------------------------------------------------------------------------

	KOutputFPStream& operator=(const KOutputFPStream&) = delete;

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

	KOStreamBuf m_FPStreamBuf{&FilePtrWriter, &m_FilePtr};
};


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class OStream>
class KWriter : public OStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = OStream;
	using self_type = KWriter<OStream>;

public:

	//-----------------------------------------------------------------------------
	/// default ctor
	KWriter()
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::ostream, OStream>::value,
		              "KWriter can only be instantiated with std::ostream derivates as the template argument");
	}

	//-----------------------------------------------------------------------------
	/// construct a KWriter on a passed std::ostream (use std::move()!)
	KWriter(OStream&& os)
	//-----------------------------------------------------------------------------
		: base_type(std::move(os))
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a KWriter on a file-like output target.
	/// Be prepared to get compiler warnings when you call this method on an
	/// ostream that does not have this constructor (i.e. all non-ofstreams)
	KWriter(const char* sName, std::ios::openmode mode = std::ios::out)
	    : base_type(sName, mode) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a KWriter on a file-like output target.
	/// be prepared to get compiler warnings when you call this method on an
	/// ostream that does not have this constructor (i.e. all non-ofstreams)
	KWriter(const std::string& sName, std::ios::openmode mode = std::ios::out)
	    : base_type(sName, mode) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a KWriter on a (possibly) file descriptor output target.
	/// Be prepared to get compiler warnings when you call this method on an
	/// ostream that does not have this constructor (i.e. all non-KOutputFDStreams)
	KWriter(int iFileDesc) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(iFileDesc)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a KWriter on a FILE* output target.
	/// Be prepared to get compiler warnings when you call this method on an
	/// ostream that does not have this constructor (i.e. all non-KOutputFPStreams)
	KWriter(FILE* iFilePtr) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(iFilePtr)
	{
	}

	//-----------------------------------------------------------------------------
	KWriter(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Move-construct a KWriter
	KWriter(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	{
	}

	//-----------------------------------------------------------------------------
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move-assign a KWriter
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual ~KWriter() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a character. Returns stream reference that resolves to false on failure
	inline self_type& Write(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		std::streambuf* sb = this->rdbuf();
		if (sb != nullptr)
		{
			typename base_type::int_type iCh = sb->sputc(ch);
			if (base_type::traits_type::eq_int_type(iCh, base_type::traits_type::eof()))
			{
				this->setstate(std::ios_base::badbit);
			}
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a range of characters. Returns stream reference that resolves to false on failure
	inline self_type& Write(const typename base_type::char_type* pAddress, size_t iCount)
	//-----------------------------------------------------------------------------
	{
		std::streambuf* sb = this->rdbuf();
		if (sb != nullptr)
		{
			size_t iWrote = static_cast<size_t>(sb->sputn(pAddress, iCount));
			if (iWrote != iCount)
			{
				this->setstate(std::ios_base::badbit);
			}
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a string. Returns stream reference that resolves to false on failure
	inline KWriter& Write(KStringView str)
	//-----------------------------------------------------------------------------
	{
		Write(str.data(), str.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a type. Returns stream reference that resolves to false on failure.
	/// Type must be trivially copyable.
	template<typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type* = nullptr>
	inline self_type& Write(T& value)
	//-----------------------------------------------------------------------------
	{
		Write(&value, sizeof(T));
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a string and a line delimiter. Returns stream reference that resolves
	/// to false on failure.
	inline self_type& WriteLine(KStringView line, KStringView delimiter = "\n")
	//-----------------------------------------------------------------------------
	{
		Write(line.data(), line.size());
		Write(delimiter.data(), delimiter.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a fmt::format() formatted argument list. Returns stream reference that
	/// resolves to false on failure.
	template<class... Args>
	self_type& Format(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kfFormat(*this, std::forward<Args>(args)...);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a fmt::printf() formatted argument list. Returns stream reference that
	/// resolves to false on failure.
	template<class... Args>
	self_type& Printf(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kfPrintf(*this, std::forward<Args>(args)...);
		return *this;
	}

};


/// File writer based on std::ofstream
using KFileWriter     = KWriter<std::ofstream>;

/// String writer based on std::ostringstream
using KStringWriter   = KWriter<std::ostringstream>;

/// File descriptor writer based on KOutputFDStream>
using KFDWriter = KWriter<KOutputFDStream>;

/// FILE ptr writer based on KOutputFPStream>
using KFPWriter = KWriter<KOutputFPStream>;

} // end of namespace dekaf2

