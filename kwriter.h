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
template<class OStream, typename std::enable_if<std::is_base_of<std::ostream, OStream>::value>::type* = nullptr>
class KWriter : public OStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = OStream;
	using self_type = KWriter<OStream>;

public:

	//-----------------------------------------------------------------------------
	KWriter() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KWriter(OStream&& os)
	//-----------------------------------------------------------------------------
		: base_type(std::move(os))
	{
	}

	//-----------------------------------------------------------------------------
	KWriter(const char* sName, std::ios::openmode mode = std::ios::out)
	    : base_type(sName, mode) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KWriter(const KString& sName, std::ios::openmode mode = std::ios::out)
	    : base_type(sName, mode) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KWriter(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KWriter(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	{
	}

	//-----------------------------------------------------------------------------
	KWriter(int iFileDesc) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(iFileDesc)
	{
	}

	//-----------------------------------------------------------------------------
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
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
	inline self_type& Write(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		base_type::put();
		return *this;
	}

	//-----------------------------------------------------------------------------
	inline self_type& Write(const typename base_type::char_type* pAddress, size_t iCount)
	//-----------------------------------------------------------------------------
	{
		base_type::write(pAddress, iCount);
		return *this;
	}

	//-----------------------------------------------------------------------------
	inline KWriter& Write(KStringView str)
	//-----------------------------------------------------------------------------
	{
		Write(str.data(), str.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	template<typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type* = nullptr>
	inline self_type& Write(T& value)
	//-----------------------------------------------------------------------------
	{
		Write(&value, sizeof(T));
		return *this;
	}

	//-----------------------------------------------------------------------------
	inline self_type& WriteLine(KStringView line, KStringView delimiter = "\n")
	//-----------------------------------------------------------------------------
	{
		Write(line.data(), line.size());
		Write(delimiter.data(), delimiter.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	template<class... Args>
	KWriter& Format(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kfFormat(*this, std::forward<Args>(args)...);
	}

};

using KFileWriter     = KWriter<std::ofstream>;
using KStringWriter   = KWriter<std::ostringstream>;
using KFileDescWriter = KWriter<KOutputFDStream>;

} // end of namespace dekaf2

