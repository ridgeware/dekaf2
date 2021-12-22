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

/// @file kreader.h
/// holds the basic reader abstraction

#include <streambuf>
#include <istream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <type_traits>
#include "bits/kfilesystem.h"
#include "kfilesystem.h"
#include "kstring.h"

namespace dekaf2
{

/// Read a line of text until EOF or delimiter from a std::istream. Right trim values of sTrimRight.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sLine the string to read into
/// @param sTrimRight right trim characters, default \n
/// @param sTrimLeft left trim characters, default none
/// @param delimiter the char until which to read to, default \n
/// @param iMaxRead the maximum count of characters to be read
DEKAF2_PUBLIC
bool kReadLine(std::istream& Stream,
               KString& sLine,
               KStringView sTrimRight = "\n",
               KStringView sTrimLeft = "",
               KString::value_type delimiter = '\n',
			   std::size_t iMaxRead = npos);

/// Appends all content of a std::istream device to a string. Reads from current
/// position until end of stream and therefore works on unseekable streams.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kAppendAllUnseekable(std::istream& Stream, KString& sContent, std::size_t iMaxRead = npos);

/// Appends all content of a std::istream device to a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads until end of stream.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kAppendAll(std::istream& Stream, KString& sContent, bool bFromStart = true, std::size_t iMaxRead = npos);

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads until end of stream.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart = true, std::size_t iMaxRead = npos);

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads until end of stream.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
KString kReadAll(std::istream& Stream, bool bFromStart = true, std::size_t iMaxRead = npos);

/// Read all content of a file with name sFileName into a string, append to sContent
/// @param sFileName the input file's name
/// @param sContent the string to fill with the content of the file
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kAppendAll(KStringViewZ sFileName, KString& sContent, std::size_t iMaxRead = npos);

/// Read all content of a file with name sFileName into a string
/// @param sFileName the input file's name
/// @param sContent the string to fill with the content of the file
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kReadAll(KStringViewZ sFileName, KString& sContent, std::size_t iMaxRead = npos);

/// Returns all content of a file with name sFileName as a string
/// @param sFileName the input file's name
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
KString kReadAll(KStringViewZ sFileName, std::size_t iMaxRead = npos);

/// Get the total size of a std::istream device. Returns -1 on Failure. Fails on non-seekable istreams.
DEKAF2_PUBLIC
ssize_t kGetSize(std::istream& Stream, bool bFromStart = true);

/// Get the total size of a file with name sFileName. Returns -1 on Failure.
DEKAF2_PUBLIC
ssize_t kGetSize(KStringViewZ sFileName);

/// Reposition the device of a std::istream to the beginning. Fails on non-seekable istreams.
DEKAF2_PUBLIC
bool kRewind(std::istream& Stream);

/// Reads iCount chars from file descriptor into sBuffer, even on growing pipes or other unseekable inputs
DEKAF2_PUBLIC
std::size_t kReadFromFileDesc(int fd, void* sBuffer, std::size_t iCount);

// forward declaration for Read(KOutStream&)
class KOutStream;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The standalone reader abstraction for dekaf2. Can be constructed around any
/// std::istream, and has iterators and read accessors that attach to the
/// std::streambuf of the istream. Provides a line iterator on the file
/// content, and can trim returned lines. Is used as a component for KReader.
class DEKAF2_PUBLIC KInStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KInStream;

//-------
public:
//-------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a KReader iterator that returns strings, line by line, by reading directly
	/// from the underlying streambuf
	class const_kreader_line_iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	//-------
	public:
	//-------
		using iterator_category = std::input_iterator_tag;
		using value_type = KString;

		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using difference_type = std::ptrdiff_t;
		using self_type = const_kreader_line_iterator;
		using base_iterator = KInStream;

		//-----------------------------------------------------------------------------
		/// standalone ctor
		const_kreader_line_iterator() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// ctor for KReaders
		const_kreader_line_iterator(base_iterator& it, bool bToEnd);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// copy constructor
		const_kreader_line_iterator(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// move constructor
		const_kreader_line_iterator(self_type&& other) noexcept = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// copy assignment
		self_type& operator=(const self_type& other) = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// move assignment
		self_type& operator=(self_type&& other) noexcept = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// prefix increment
		self_type& operator++();
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// postfix increment
		const self_type operator++(int);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// returns the current string
		inline const_reference operator*() const
		//-----------------------------------------------------------------------------
		{
			return m_sBuffer;
		}

		//-----------------------------------------------------------------------------
		/// returns the current string
		inline reference operator*()
		//-----------------------------------------------------------------------------
		{
			return m_sBuffer;
		}

		//-----------------------------------------------------------------------------
		/// returns the current string
		inline const_pointer operator->() const
		//-----------------------------------------------------------------------------
		{
			return &m_sBuffer;
		}

		//-----------------------------------------------------------------------------
		/// returns the current string
		inline pointer operator->()
		//-----------------------------------------------------------------------------
		{
			return &m_sBuffer;
		}

		//-----------------------------------------------------------------------------
		/// equality operator
		inline bool operator==(const self_type& rhs) const
		//-----------------------------------------------------------------------------
		{
			return m_it == rhs.m_it;
		}

		//-----------------------------------------------------------------------------
		/// inequality operator
		inline bool operator!=(const self_type& rhs) const
		//-----------------------------------------------------------------------------
		{
			return !operator==(rhs);
		}

	//-------
	protected:
	//-------

		base_iterator* m_it { nullptr };
		KString m_sBuffer;

	}; // const_kreader_line_iterator

	using const_iterator = const_kreader_line_iterator;
	using iterator = const_iterator;
	using char_iterator = std::istreambuf_iterator<char>;

	//-----------------------------------------------------------------------------
	/// value constructor
	KInStream(std::istream& InStream)
	//-----------------------------------------------------------------------------
	    : m_InStream(InStream)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy construction is deleted (to avoid ambiguities with the value constructor)
	KInStream(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move construct a KInStream
	KInStream(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment is deleted, as with std::istream (use the move constructor instead)
	self_type& operator=(self_type&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KInStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// UnRead / putback a character. Returns false if character cannot be put back.
	bool UnRead();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
	std::istream::int_type Read();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a character. Returns stream reference that resolves to false if no input available
	self_type& Read(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		ch = std::istream::traits_type::to_char_type(Read());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters. Returns count of successfully read charcters.
	size_t Read(void* pAddress, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to sBuffer. Returns count of successfully read charcters.
	size_t Read(KString& sBuffer, size_t iCount = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to Stream. Returns count of successfully read charcters.
	size_t Read(KOutStream& Stream, size_t iCount = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Reads a line of text. Returns false if no input available. Stops at delimiter
	/// character defined and optionally right trims the string from the trim
	/// definition. Per default does not contain the end-of-line character in the
	/// returned string.
	///
	/// Please note that this method does _not_ return the stream reference,
	/// but a boolean. std::istreams would not read a file with a missing newline
	/// at the end successfully, but report an error. This function succeeds.
	bool ReadLine(KString& sLine, size_t iMaxRead = npos)
	//-----------------------------------------------------------------------------
	{
		return kReadLine(InStream(), sLine, m_sTrimRight, m_sTrimLeft, m_chDelimiter, iMaxRead);
	}

	//-----------------------------------------------------------------------------
	/// Reads a line of text and returns it. Stops at delimiter
	/// character defined and optionally right trims the string from the trim
	/// definition. Per default does not contain the end-of-line character in the
	/// returned string.
	KString ReadLine(std::size_t iMaxRead = npos)
	//-----------------------------------------------------------------------------
	{
		KString sLine;
		kReadLine(InStream(), sLine, m_sTrimRight, m_sTrimLeft, m_chDelimiter, iMaxRead);
		return sLine;
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in a string. Returns false if no input
	/// available. Fails on non-seekable inputs, e.g. streams.
	bool ReadAll(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(InStream(), sBuffer, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in the returned string. Fails on non-seekable inputs, e.g. streams.
	KString ReadAll()
	//-----------------------------------------------------------------------------
	{
		return kReadAll(InStream(), true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string. Returns false if no input
	/// available. Does not fail on non-seekable inputs, but tries to read the utmost.
	bool ReadRemaining(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(InStream(), sBuffer, false);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string. Does not fail on non-seekable
	/// inputs, but tries to read the utmost.
	KString ReadRemaining()
	//-----------------------------------------------------------------------------
	{
		return kReadAll(InStream(), false);
	}

	//-----------------------------------------------------------------------------
	/// Alias for ReadAll
	bool GetContent(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return ReadAll(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Alias for ReadAll
	KString GetContent()
	//-----------------------------------------------------------------------------
	{
		return ReadAll();
	}

	//-----------------------------------------------------------------------------
	/// Returns the size of a file. Returns 0 if no input available. Fails on
	/// non-seekable inputs, e.g. streams.
	size_t GetSize()
	//-----------------------------------------------------------------------------
	{
		auto iSize = kGetSize(InStream(), true);

		if (iSize < 0)
		{
			return 0;
		}
		else
		{
			return static_cast<size_t>(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining size of a file. Returns 0 if no input available.
	/// Fails on non-seekable inputs, e.g. streams.
	size_t GetRemainingSize()
	//-----------------------------------------------------------------------------
	{
		auto iSize = kGetSize(InStream(), false);
		
		if (iSize < 0)
		{
			return 0;
		}
		else
		{
			return static_cast<size_t>(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// Returns a line of text as if read by ReadLine(). If the (separately)
	/// specified trimming includes the end-of-line delimiter, returns an empty
	/// string for both an empty line and unavailable input. In that case call
	/// eof() on the istream to distinguish between both cases.
	operator KString()
	//-----------------------------------------------------------------------------
	{
		KString sStr;
		ReadLine(sStr);
		return sStr;
	}

	//-----------------------------------------------------------------------------
	/// Reposition the input device of the std::istream to the beginning. Fails on streams.
	bool Rewind()
	//-----------------------------------------------------------------------------
	{
		return kRewind(InStream());
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator to the current read position in a stream
	const_iterator cbegin()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(*this, false);
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator that is equal to an iterator that has reached the
	/// end of a stream
	const_iterator cend()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(*this, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator to the current read position in a stream
	const_iterator begin()
	//-----------------------------------------------------------------------------
	{
		return cbegin();
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator that is equal to an iterator that has reached the
	/// end of a stream
	const_iterator end()
	//-----------------------------------------------------------------------------
	{
		return cend();
	}

	//-----------------------------------------------------------------------------
	/// Returns a char iterator to the current read position in a stream
	char_iterator char_begin()
	//-----------------------------------------------------------------------------
	{
		return char_iterator(InStream());
	}

	//-----------------------------------------------------------------------------
	/// Returns a char iterator that is equal to an iterator that has reached the
	/// end of a stream
	char_iterator char_end()
	//-----------------------------------------------------------------------------
	{
		return char_iterator();
	}

	//-----------------------------------------------------------------------------
	/// Set the end-of-line character (defaults to LF)
	void SetReaderEndOfLine(char chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	{
		m_chDelimiter = chDelimiter;
	}

	//-----------------------------------------------------------------------------
	/// Set the left trim characters for line based reading (default to none)
	void SetReaderLeftTrim(KStringView sTrimLeft = "")
	//-----------------------------------------------------------------------------
	{
		m_sTrimLeft  = sTrimLeft;
	}

	//-----------------------------------------------------------------------------
	/// Set the right trim characters for line based reading (default to LF)
	void SetReaderRightTrim(KStringView sTrimRight = detail::kLineRightTrims)
	//-----------------------------------------------------------------------------
	{
		m_sTrimRight = sTrimRight;
	}

	//-----------------------------------------------------------------------------
	/// Set the right and left trim characters for line based reading (default to LF for right, none for left)
	void SetReaderTrim(KStringView sTrimRight = detail::kLineRightTrims, KStringView sTrimLeft = "")
	//-----------------------------------------------------------------------------
	{
		SetReaderRightTrim(sTrimRight);
		SetReaderLeftTrim(sTrimLeft);
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	const std::istream& InStream() const
	//-----------------------------------------------------------------------------
	{
		return m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	std::istream& InStream()
	//-----------------------------------------------------------------------------
	{
		return m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	operator const std::istream& () const
	//-----------------------------------------------------------------------------
	{
		return InStream();
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	operator std::istream& ()
	//-----------------------------------------------------------------------------
	{
		return InStream();
	}

	//-----------------------------------------------------------------------------
	/// Check if stream is good for input
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return InStream().good();
	}

//-------
protected:
//-------

	std::istream& m_InStream;
	KString m_sTrimRight { detail::kLineRightTrims };
	KString m_sTrimLeft;
	KString::value_type m_chDelimiter { '\n' };

}; // KInStream

extern KInStream KIn;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The templatized reader abstraction for dekaf2. Can be constructed around any
/// std::istream.
template<class IStream>
class KReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
        : public IStream
        , public KInStream
{
	using base_type = IStream;
	using self_type = KReader<IStream>;

	static_assert(std::is_base_of<std::istream, IStream>::value,
	              "KReader cannot be derived from a non-std::istream class");

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	// perfect forwarding
	template<class... Args>
	KReader(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , KInStream(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// move construct a KReader
	KReader(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted, as with std::istream
	KReader(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KReader()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted, as with std::istream
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// this one is necessary because ios_base has a symbol named end .. (for seeking)
	const_iterator end()
	//-----------------------------------------------------------------------------
	 {
		return KInStream::end();
	 }

}; // KReader

extern template class KReader<std::ifstream>;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// File reader based on std::ifstream
// std::ifstream does not understand KString and KStringView, so let's help it
class DEKAF2_PUBLIC KInFile : public KReader<std::ifstream>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	using base_type = KReader<std::ifstream>;

	//-----------------------------------------------------------------------------
	KInFile(KString str, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(str), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInFile(KStringViewZ sv, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(sv), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInFile(KStringView sv, ios_base::openmode mode = ios_base::in)
	: KInFile(KString(sv), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

#ifndef _MSC_VER
	using base_type::base_type;
#else
	// MSC has issues with perfect forwarding of KReader and does not catch the
	// KStringView ctor above if we forward all base class constructors
	// therefore we need to declare a few more constructors here
	KInFile() = default;
	KInFile(KInFile&&) = default;
	KInFile(const std::string& s, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(s), mode | ios_base::binary) {}
	KInFile(const char* sz, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary) {}
	void open(const std::string& s, ios_base::openmode mode = ios_base::in)
	{ base_type::open(kToFilesystemPath(s), mode | ios_base::binary); }
	// that open is the most bizarre.. MS added an open with an additional int parm,
	// but did not document what it does - we just drop it, but have to support it
	// for Windows compatibility
	void open(const char* sz, ios_base::openmode mode = ios_base::in, int = 0)
	{ base_type::open(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary); }
#endif

	//-----------------------------------------------------------------------------
	void open(const KString& str, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(str), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringViewZ sz, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sz), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringView sv, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		open(KString(sv), mode | ios_base::binary);
	}

	using base_type::open;

};

} // end of namespace dekaf2

