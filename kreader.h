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
#include "kstring.h"

namespace dekaf2
{

/// Read a line of text until EOF or delimiter from a std::istream. Right trim values of sTrimRight.
/// Reads directly in the underlying streambuf
bool kReadLine(std::istream& Stream,
               KString& sLine,
               KStringView sTrimRight = "\n",
               KStringView sTrimLeft = "",
               KString::value_type delimiter = '\n');

/// Appends all content of a std::istream device to a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise tries to read as much as possible.
/// Reads directly in the underlying streambuf
bool kAppendAll(std::istream& Stream, KString& sContent, bool bFromStart = true);

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise tries to read as much as possible.
/// Reads directly in the underlying streambuf
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart = true);

/// Read all content of a file with name sFileName into a string
bool kReadAll(KStringViewZ sFileName, KString& sContent);

/// Get the total size of a std::istream device. Returns -1 on Failure. Fails on non-seekable istreams.
ssize_t kGetSize(std::istream& Stream, bool bFromStart = true);

/// Get the total size of a file with name sFileName. Returns -1 on Failure.
ssize_t kGetSize(KStringViewZ sFileName);

/// Reposition the device of a std::istream to the beginning. Fails on non-seekable istreams.
bool kRewind(std::istream& Stream);

// forward declaration for Read(KOutStream&)
class KOutStream;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The standalone reader abstraction for dekaf2. Can be constructed around any
/// std::istream, and has iterators and read accessors that attach to the
/// std::streambuf of the istream. Provides a line iterator on the file
/// content, and can trim returned lines. Is used as a component for KReader.
class KInStream
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
		using pointer = value_type*;
		using difference_type = std::ptrdiff_t;
		using self_type = const_kreader_line_iterator;
		using base_iterator = KInStream;

		//-----------------------------------------------------------------------------
		/// standalone ctor
		inline const_kreader_line_iterator()
		//-----------------------------------------------------------------------------
		{
			// beware, m_it is a nullptr now
		}

		//-----------------------------------------------------------------------------
		/// ctor for KReaders
		const_kreader_line_iterator(base_iterator& it, bool bToEnd);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// copy constructor
		inline const_kreader_line_iterator(const self_type& other)
		//-----------------------------------------------------------------------------
		    : m_it(other.m_it)
		    , m_sBuffer(other.m_sBuffer)
		{
		}

		//-----------------------------------------------------------------------------
		/// move constructor
		inline const_kreader_line_iterator(self_type&& other)
		//-----------------------------------------------------------------------------
		    : m_it(std::move(other.m_it))
		    , m_sBuffer(std::move(other.m_sBuffer))
		{
		}

		//-----------------------------------------------------------------------------
		/// copy assignment
		inline self_type& operator=(const self_type& other)
		//-----------------------------------------------------------------------------
		{
			m_it = other.m_it;
			m_sBuffer = other.m_sBuffer;
			return *this;
		}

		//-----------------------------------------------------------------------------
		/// move assignment
		inline self_type& operator=(self_type&& other)
		//-----------------------------------------------------------------------------
		{
			m_it = std::move(other.m_it);
			m_sBuffer = std::move(other.m_sBuffer);
			return *this;
		}

		//-----------------------------------------------------------------------------
		/// postfix increment
		self_type& operator++();
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// prefix increment
		self_type operator++(int);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// returns the current string
		inline reference operator*()
		//-----------------------------------------------------------------------------
		{
			return m_sBuffer;
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
		inline bool operator==(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return m_it == rhs.m_it;
		}

		//-----------------------------------------------------------------------------
		/// inequality operator
		inline bool operator!=(const self_type& rhs)
		//-----------------------------------------------------------------------------
		{
			return !operator==(rhs);
		}

	//-------
	protected:
	//-------

		base_iterator* m_it{nullptr};
		KString m_sBuffer;

	}; // const_kreader_line_iterator

	using const_iterator = const_kreader_line_iterator;
	using iterator = const_iterator;
	using char_iterator = std::istreambuf_iterator<char>;

	//-----------------------------------------------------------------------------
	/// value constructor
	KInStream(std::istream& InStream)
	//-----------------------------------------------------------------------------
	    : m_InStream(&InStream)
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
	/// move assign a KInStream
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KInStream();
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
	size_t Read(KString& sBuffer, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to Stream. Returns count of successfully read charcters.
	size_t Read(KOutStream& Stream, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a line of text. Returns false if no input available. Stops at delimiter
	/// character defined and optionally right trims the string from the trim
	/// definition. Per default does not contain the end-of-line character in the
	/// returned string.
	///
	/// Please note that this method does _not_ return the stream reference,
	/// but a boolean. std::istreams would not read a file with a missing newline
	/// at the end successfully, but report an error. This function succeeds.
	bool ReadLine(KString& sLine)
	//-----------------------------------------------------------------------------
	{
		return kReadLine(InStream(), sLine, m_sTrimRight, m_sTrimLeft, m_chDelimiter);
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
	/// Returns the remaining content of a file in a string. Returns false if no input
	/// available. Does not fail on non-seekable inputs, but tries to read the utmost.
	bool ReadRemaining(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(InStream(), sBuffer, false);
	}

	//-----------------------------------------------------------------------------
	/// Alias for ReadAll
	bool GetContent(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return ReadAll(sBuffer);
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
	void SetReaderRightTrim(KStringView sTrimRight = "\n")
	//-----------------------------------------------------------------------------
	{
		m_sTrimRight = sTrimRight;
	}

	//-----------------------------------------------------------------------------
	/// Set the right and left trim characters for line based reading (default to LF for right, none for left)
	void SetReaderTrim(KStringView sTrimRight = "\n", KStringView sTrimLeft = "")
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
		return *m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	std::istream& InStream()
	//-----------------------------------------------------------------------------
	{
		return *m_InStream;
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

	// m_sRef always has to be valid after construction
	// - do not assign a nullptr per default
	std::istream* m_InStream;
	KString m_sTrimRight{"\n"};
	KString m_sTrimLeft;
	KString::value_type m_chDelimiter{'\n'};

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
	    , KInStream(reinterpret_cast<base_type&>(*this))
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

/// File reader based on std::ifstream
// std::ifstream does not understand KString and KStringView, so let's help it
class KInFile : public KReader<std::ifstream>
{
public:

	using base_type = KReader<std::ifstream>;

	//-----------------------------------------------------------------------------
	KInFile(KString str, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(str), mode)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInFile(KStringViewZ sz, ios_base::openmode mode = ios_base::in)
	: base_type(kToFilesystemPath(sz), mode)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KInFile(KStringView sv, ios_base::openmode mode = ios_base::in)
	: KInFile(KString(sv), mode)
	//-----------------------------------------------------------------------------
	{
	}

	using base_type::base_type;

	//-----------------------------------------------------------------------------
	void open(const KString& str, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(str), mode);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringViewZ sz, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sz), mode);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringView sv, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		open(KString(sv), mode);
	}

	using base_type::open;

};

} // end of namespace dekaf2

