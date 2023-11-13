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

#include "bits/kfilesystem.h"
#include "kstring.h"
#include "kformat.h"
#include <cinttypes>
#include <ostream>
#include <fstream>

namespace dekaf2
{

/// Writes iCount chars from sBuffer into file descriptor, signal safe
DEKAF2_PUBLIC
std::size_t kWriteToFileDesc(int fd, const void* sBuffer, std::size_t iCount);

/// Writes iCount chars from sBuffer into FILE*, signal safe
DEKAF2_PUBLIC
std::size_t kWriteToFilePtr(FILE* fp, const void* sBuffer, std::size_t iCount);

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
	    : m_OutStream(&OutStream)
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
	/// copy assignment
	self_type& operator=(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other) = default;
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
		kPrint(*this, std::forward<Args>(args)...);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Write a fmt::format() formatted argument list. Returns stream reference that
	/// resolves to false on failure.
	template<class... Args>
	self_type& FormatLine(Args&&... args)
	//-----------------------------------------------------------------------------
	{
		kPrint(*this, std::forward<Args>(args)...);
		return WriteLine();
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
private:
//-------

	std::ostream* m_OutStream;
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
	// make sure this does not cover the copy or move constructor by requesting an
	// args count of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KWriter(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , KOutStream(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// make sure this does not cover the copy or move constructor by requesting the
	// single arg being of a different type than self_type
	template<class Arg,
		typename std::enable_if<
			!std::is_same<
				typename std::decay<Arg>::type, self_type
			>::value, int
		>::type = 0
	>
	KWriter(Arg&& arg)
	    : base_type(std::forward<Arg>(arg))
	    , KOutStream(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// copy construction is not allowed
	KWriter(const KWriter&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// depending on the ostream type, move construction is allowed
	template<typename T = OStream, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KWriter(KWriter&& other)
	    : base_type(std::move(other))
	    , KOutStream(std::move(other))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the ostream type, move construction is not allowed
	template<typename T = OStream, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KWriter(KWriter&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// copy assignment is not allowed
	KWriter& operator=(const KWriter&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// move assignment is not allowed
	KWriter& operator=(KWriter&& other) = delete;
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
	KOutFile() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOutFile(KStringViewZ sz, ios_base::openmode mode = ios_base::out)
	    : base_type(kToFilesystemPath(sz), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KOutFile(KOutFile&& other)
	    : base_type(std::move(other))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is not allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KOutFile(KOutFile&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void open(KStringViewZ sFilename, ios_base::openmode mode = ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sFilename), mode | ios_base::binary);
	}

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

