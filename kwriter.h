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

/// @file kwriter.h
/// holds the basic writer abstraction

#include <cinttypes>
#include <ostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <type_traits>
#include "bits/kfilesystem.h"
#include "kstring.h"
#include "kformat.h"

namespace dekaf2
{

/// Writes iCount chars from sBuffer into file descriptor, signal safe
DEKAF2_PUBLIC
std::size_t kWriteToFileDesc(int fd, const void* sBuffer, std::size_t iCount);

class KInStream; // fwd decl

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The standalone writer abstraction for dekaf2. Can be constructed around any
/// std::ostream. Provides localization friendly formatting methods and a fast
/// bypass of std::ostream's formatting functions by directly writing to the
/// std::streambuf. Is used as a component for KWriter.
class DEKAF2_PUBLIC KOutStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KOutStream;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// value constructor
	KOutStream(std::ostream& OutStream)
	//-----------------------------------------------------------------------------
	    : m_OutStream(OutStream)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy construction is deleted (to avoid ambiguities with the value constructor)
	KOutStream(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move construct a KOutStream
	KOutStream(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment is deleted, as with std::ostream
	self_type& operator=(self_type&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KOutStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a character. Returns stream reference that resolves to false on failure.
	self_type& Write(KStringView::value_type ch);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a range of characters. Returns stream reference that resolves to false
	/// on failure.
	self_type& Write(const void* pAddress, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Write a string. Returns stream reference that resolves to false on failure.
	self_type& Write(KStringView str)
	//-----------------------------------------------------------------------------
	{
		Write(str.data(), str.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to Stream. Returns stream reference
	/// that resolves to false on failure.
	self_type& Write(KInStream& Stream, size_t iCount = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator+=(KStringView::value_type ch)
	//-----------------------------------------------------------------------------
	{
		return Write(ch);
	}

	//-----------------------------------------------------------------------------
	self_type& operator+=(KStringView sv)
	//-----------------------------------------------------------------------------
	{
		return Write(sv);
	}

	//-----------------------------------------------------------------------------
	/// Write a line delimiter. Returns stream reference that resolves
	/// to false on failure.
	self_type& WriteLine()
	//-----------------------------------------------------------------------------
	{
		Write(m_sDelimiter.data(), m_sDelimiter.size());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a string and a line delimiter. Returns stream reference that resolves
	/// to false on failure.
	self_type& WriteLine(KStringView line)
	//-----------------------------------------------------------------------------
	{
		Write(line.data(), line.size());
		return WriteLine();
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
	/// Write a fmt::format() formatted argument list. Returns stream reference that
	/// resolves to false on failure.
	template<class... Args>
	self_type& FormatLine(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kfFormat(*this, std::forward<Args>(args)...);
		return WriteLine();
	}

	//-----------------------------------------------------------------------------
	/// Write a fmt::printf() formatted argument list. Returns stream reference that
	/// resolves to false on failure. Now DEPRECATED, please use Format()!
	template<class... Args>
	DEKAF2_DEPRECATED("only for compatibility with old code")
	self_type& Printf(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kfPrintf(*this, std::forward<Args>(args)...);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Set the end-of-line sequence (defaults to "LF", may differ depending on platform)
	self_type& SetWriterEndOfLine(KStringView sDelimiter = "\n")
	//-----------------------------------------------------------------------------
	{
		m_sDelimiter = sDelimiter;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Get the end-of-line sequence (defaults to "LF", may differ depending on platform)
	const KString& GetWriterEndOfLine() const
	//-----------------------------------------------------------------------------
	{
		return m_sDelimiter;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	const std::ostream& OutStream() const
	//-----------------------------------------------------------------------------
	{
		return m_OutStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::ostream
	std::ostream& OutStream()
	//-----------------------------------------------------------------------------
	{
		return m_OutStream;
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

	//-----------------------------------------------------------------------------
	/// flush the write buffer
	self_type& Flush()
	//-----------------------------------------------------------------------------
	{
		OutStream().flush();
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Check if stream is good for output
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return OutStream().good();
	}

	//-----------------------------------------------------------------------------
	/// Check if stream is good for output
	explicit operator bool() const
	//-----------------------------------------------------------------------------
	{
		return Good();
	}

//-------
protected:
//-------

	std::ostream& m_OutStream;
	KString m_sDelimiter{"\n"};

}; // KOutStream

extern KOutStream KErr;
extern KOutStream KOut;

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
	KWriter(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted, as with std::ostream
	KWriter(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KWriter() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted, as with std::ostream
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Check if stream is good for output
	explicit operator bool() const
	//-----------------------------------------------------------------------------
	{
		return base_type::good();
	}

}; // KWriter

extern template class KWriter<std::ofstream>;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// File writer based on std::ofstream
// std::ofstream does not understand KString and KStringView, so let's help it
class DEKAF2_PUBLIC KOutFile : public KWriter<std::ofstream>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using base_type = KWriter<std::ofstream>;

	//-----------------------------------------------------------------------------
	KOutFile(KString str, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(str), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KOutFile(KStringViewZ sz, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(sz), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KOutFile(KStringView sv, ios_base::openmode mode = ios_base::out)
	: KOutFile(KString(sv), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

#ifndef _MSC_VER
	using base_type::base_type;
#else
	// MSC has issues with perfect forwarding of KWriter and does not catch the
	// KStringView ctor above if we forward all base class constructors
	// therefore we need to declare a few more constructors here
	KOutFile() = default;
	KOutFile(KOutFile&&) = default;
	KOutFile(const std::string& s, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(s), mode | ios_base::binary) {}
	KOutFile(const char* sz, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary) {}
	void open(const std::string& s, ios_base::openmode mode = ios_base::out)
	{ base_type::open(kToFilesystemPath(s), mode | ios_base::binary); }
	// that open is the most bizarre.. MS added an open with an additional int parm,
	// but did not document what it does - we just drop it, but have to support it
	// for Windows compatibility
	void open(const char* sz, ios_base::openmode mode = ios_base::out, int = 0)
	{ base_type::open(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary); }
#endif

	//-----------------------------------------------------------------------------
	void open(const KString& str, ios_base::openmode mode = ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(str), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringViewZ sz, ios_base::openmode mode = ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sz), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringView sv, ios_base::openmode mode = ios_base::out)
	//-----------------------------------------------------------------------------
	{
		open(KString(sv), mode | ios_base::binary);
	}

	using base_type::open;

};

//-----------------------------------------------------------------------------
/// open a stream from any of the supported stream schemata, like file, stdout, stderr
extern DEKAF2_PUBLIC std::unique_ptr<KOutStream> kOpenOutStream(KStringViewZ sSchema, std::ios::openmode openmode = std::ios::app);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// universal logger implemented with an output queue - make sure the Stream is valid until the program ends
extern DEKAF2_PUBLIC void kLogger(KOutStream& Stream, KString sMessage, bool bFlush = true);
//-----------------------------------------------------------------------------

} // end of namespace dekaf2

