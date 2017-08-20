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
#include <algorithm>
#include <type_traits>
#include "kstring.h"
#include "kformat.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customized output stream buffer
struct KOutStreamBuf : public std::streambuf
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
	typedef std::streamsize (*Writer)(const void*, std::streamsize, void*);
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
/// The standalone writer abstraction for dekaf2. Can be constructed around any
/// std::ostream. Provides localization friendly formatting methods and a fast
/// bypass of std::ostream's formatting functions by directly writing to the
/// std::streambuf. Is used as a component for KReader.
class KOutStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KOutStream;
	using ios_type  = std::ostream;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// value constructor
	KOutStream(std::ostream& OutStream)
	//-----------------------------------------------------------------------------
	    : m_OutStream(&OutStream)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy construction is deleted (to avoid ambiguities with the value constructor)
	KOutStream(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move construct a KOutStream
	KOutStream(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : m_OutStream(std::move(other.m_OutStream))
	    , m_sDelimiter(std::move(other.m_sDelimiter))
	{}

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assign a KOutStream
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		m_OutStream = std::move(other.m_OutStream);
		m_sDelimiter = std::move(other.m_sDelimiter);
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual ~KOutStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a character. Returns stream reference that resolves to false on failure
	self_type& Write(KString::value_type& ch);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a range of characters. Returns stream reference that resolves to false on failure
	self_type& Write(const typename std::ostream::char_type* pAddress, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a string. Returns stream reference that resolves to false on failure
	inline self_type& Write(KStringView str)
	//-----------------------------------------------------------------------------
	{
		Write(str.data(), str.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a type. Returns stream reference that resolves to false on failure.
	/// Type must be trivially copyable.
#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
	template<typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type* = nullptr>
#else
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
#endif
	inline self_type& Write(T& value)
	//-----------------------------------------------------------------------------
	{
		Write(&value, sizeof(T));
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a string and a line delimiter. Returns stream reference that resolves
	/// to false on failure.
	inline self_type& WriteLine(KStringView line)
	//-----------------------------------------------------------------------------
	{
		Write(line.data(), line.size());
		Write(m_sDelimiter.data(), m_sDelimiter.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a fmt::format() formatted argument list. Returns stream reference that
	/// resolves to false on failure.
	template<class... Args>
	inline self_type& Format(Args&&... args)
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

	//-----------------------------------------------------------------------------
	/// Set the end-of-line sequence (defaults to "LF", may differ depending on platform)
	inline self_type& SetWriterEndOfLine(KStringView sDelimiter = "\n")
	//-----------------------------------------------------------------------------
	{
		m_sDelimiter = sDelimiter;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	const std::ostream& OutStream() const
	//-----------------------------------------------------------------------------
	{
		return *m_OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	std::ostream& OutStream()
	//-----------------------------------------------------------------------------
	{
		return *m_OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	operator const std::ostream& () const
	//-----------------------------------------------------------------------------
	{
		return OutStream();
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	operator std::ostream& ()
	//-----------------------------------------------------------------------------
	{
		return OutStream();
	}

//-------
protected:
//-------

	// m_sRef always has to be valid after construction
	// - do not assign a nullptr per default
	std::ostream* m_OutStream;
	KString m_sDelimiter{"\n"};

}; // KOutStream

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The general Writer abstraction for dekaf2. Can be constructed around any
/// std::ostream.
template<class OStream>
class KWriter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
        : public OStream
        , public KOutStream
{
	using base_type = OStream;
	using self_type = KWriter<OStream>;

	static_assert(std::is_base_of<std::ostream, OStream>::value,
	              "KWriter cannot be derived from a non-std::ostream class");

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	// semi-perfect forwarding - currently needed as std::ostream does not yet
	// support string_views as arguments
	template<class... Args>
	KWriter(KStringView sv, Args&&... args)
	    : base_type(std::string(sv), std::forward<Args>(args)...)
	    , KOutStream(static_cast<std::ostream&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	template<class... Args>
	KWriter(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , KOutStream(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// move construct a KWriter
	KWriter(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , KOutStream(std::move(other))
	{}

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted, as with std::ostream
	KWriter(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted, as with std::ostream
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		KOutStream::operator=(std::move(other));
	}

}; // KWriter


/// File writer based on std::ofstream
using KOutFile         = KWriter<std::ofstream>;

/// String writer based on std::ostringstream
using KOutStringStream = KWriter<std::ostringstream>;

} // end of namespace dekaf2

