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

#include "kdefinitions.h"
#include "bits/kfilesystem.h"
#include "kfilesystem.h"
#include "kstring.h"
#include "kread.h"
#include <istream>
#include <fstream>

#define DEKAF2_HAS_KREADER_H 1

DEKAF2_NAMESPACE_BEGIN

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
               KStringRef& sLine,
               KStringView sTrimRight = "\n",
               KStringView sTrimLeft = "",
               KString::value_type delimiter = '\n',
               std::size_t iMaxRead = npos);

/// Appends all content of a std::istream device to a string. Reads from current
/// position until end of stream and therefore works on unseekable streams.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kAppendAllUnseekable(std::istream& Stream, KStringRef& sContent, std::size_t iMaxRead = npos);

/// Appends all content of a std::istream device to a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads from current position until end of stream.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kAppendAll(std::istream& Stream, KStringRef& sContent, bool bFromStart = true, std::size_t iMaxRead = npos);

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads from current position until end of stream.
/// Reads directly in the underlying streambuf
/// @param Stream the input stream
/// @param sContent the string to fill with the content of the file
/// @param bFromStart if true will seek to start before reading
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kReadAll(std::istream& Stream, KStringRef& sContent, bool bFromStart = true, std::size_t iMaxRead = npos);

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise reads from current position until end of stream.
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
bool kAppendAll(KStringViewZ sFileName, KStringRef& sContent, std::size_t iMaxRead = npos);

/// Read all content of a file with name sFileName into a string
/// @param sFileName the input file's name
/// @param sContent the string to fill with the content of the file
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
bool kReadAll(KStringViewZ sFileName, KStringRef& sContent, std::size_t iMaxRead = npos);

/// Returns all content of a file with name sFileName as a string
/// @param sFileName the input file's name
/// @param iMaxRead the maximum number of bytes read from the file, default unlimited
DEKAF2_PUBLIC
KString kReadAll(KStringViewZ sFileName, std::size_t iMaxRead = npos);

/// Get the total size of a file with name sFileName. Returns -1 on Failure.
DEKAF2_PUBLIC
ssize_t kGetSize(KStringViewZ sFileName);

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
#if defined(DEKAF2_HAS_FULL_CPP_17) && (!defined(DEKAF2_IS_CLANG) || DEKAF2_CLANG_VERSION >= 90000)
		self_type& operator=(self_type&& other) noexcept = default;
#else
		self_type& operator=(self_type&& other) = default;
#endif
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
	KInStream(std::istream&           InStream,
			  KStringView             sTrimRight  = detail::kLineRightTrims,
			  KStringView             sTrimLeft   = KStringView{},
			  KStringView::value_type chDelimiter = '\n',
			  bool                    bImmutable  = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy construction is deleted (to avoid ambiguities with the value constructor)
	KInStream(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move construct a KInStream
	KInStream(self_type&& other) = default;
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
	virtual ~KInStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// UnRead / putback multiple characters
	/// @param iCount count of characters to unread
	/// @returns count of characters that could not get unread (so, 0 on success)
	std::size_t UnRead(std::size_t iCount)
	//-----------------------------------------------------------------------------
	{
		return kUnRead(istream(), iCount);
	}

	//-----------------------------------------------------------------------------
	/// UnRead / putback a character
	/// @returns false if character cannot be put back
	bool UnRead()
	//-----------------------------------------------------------------------------
	{
		return UnRead(1) == 0;
	}

	//-----------------------------------------------------------------------------
	/// Read a character
	/// @returns std::istream::traits_type::eof() (== -1) if no input available
	std::istream::int_type Read()
	//-----------------------------------------------------------------------------
	{
		return kRead(istream());
	}

	//-----------------------------------------------------------------------------
	/// Read a character
	/// @param ch reference to a read char
	/// @returns stream reference that resolves to false if no input available
	self_type& Read(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		kRead(istream(), ch);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters
	/// @param pAddress address of a memory buffer to be read into
	/// @param iCount count of bytes to be read into the memory buffer
	/// @returns count of successfully read charcters
	std::size_t Read(void* pAddress, std::size_t iCount)
	//-----------------------------------------------------------------------------
	{
		return kRead(istream(), pAddress, iCount);
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to sBuffer
	/// @returns count of successfully read charcters
	std::size_t Read(KStringRef& sBuffer, std::size_t iCount = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a range of characters and append to Stream
	/// @returns count of successfully read charcters
	std::size_t Read(KOutStream& Stream, std::size_t iCount = npos);
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
	bool ReadLine(KStringRef& sLine, std::size_t iMaxRead = npos)
	//-----------------------------------------------------------------------------
	{
		return kReadLine(istream(), sLine, m_sTrimRight, m_sTrimLeft, m_chDelimiter, iMaxRead);
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
		kReadLine(istream(), sLine, m_sTrimRight, m_sTrimLeft, m_chDelimiter, iMaxRead);
		return sLine;
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in a string
	/// @returns false if no input available. Fails on non-seekable inputs, e.g. streams.
	bool ReadAll(KStringRef& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(istream(), sBuffer, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in the returned string. Fails on non-seekable inputs, e.g. streams.
	KString ReadAll()
	//-----------------------------------------------------------------------------
	{
		return kReadAll(istream(), true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string
	/// @returns false if no input available. Does not fail on non-seekable inputs, but tries to read the utmost.
	bool ReadRemaining(KStringRef& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(istream(), sBuffer, false);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string. Does not fail on non-seekable
	/// inputs, but tries to read the utmost.
	KString ReadRemaining()
	//-----------------------------------------------------------------------------
	{
		return kReadAll(istream(), false);
	}

	//-----------------------------------------------------------------------------
	/// Alias for ReadAll
	bool GetContent(KStringRef& sBuffer)
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
	std::size_t GetSize()
	//-----------------------------------------------------------------------------
	{
		auto iSize = kGetSize(istream(), true);

		if (iSize < 0)
		{
			return 0;
		}
		else
		{
			return static_cast<std::size_t>(iSize);
		}
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining size of a file. Returns 0 if no input available.
	/// Fails on non-seekable inputs, e.g. streams.
	std::size_t GetRemainingSize()
	//-----------------------------------------------------------------------------
	{
		auto iSize = kGetSize(istream(), false);
		
		if (iSize < 0)
		{
			return 0;
		}
		else
		{
			return static_cast<std::size_t>(iSize);
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
	/// Returns the read position of the input device. Fails on non-seekable streams.
	ssize_t GetReadPosition() const
	//-----------------------------------------------------------------------------
	{
		return kGetReadPosition(istream());
	}

	//-----------------------------------------------------------------------------
	/// Reposition the input device of the std::istream to the given position. Fails on non-seekable streams.
	bool SetReadPosition(std::size_t iToPos)
	//-----------------------------------------------------------------------------
	{
		return kSetReadPosition(istream(), iToPos);
	}

	//-----------------------------------------------------------------------------
	/// Reposition the input device of the std::istream to the end position. Fails on non-seekable streams.
	bool Forward()
	//-----------------------------------------------------------------------------
	{
		return kForward(istream());
	}

	//-----------------------------------------------------------------------------
	/// Reposition the input device of the std::istream to the beginning. Fails on non-seekable streams.
	bool Rewind()
	//-----------------------------------------------------------------------------
	{
		return SetReadPosition(0);
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
		return char_iterator(istream());
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
	/// Fixates all trim and eol settings
	void SetReaderImmutable()
	//-----------------------------------------------------------------------------
	{
		m_bImmutable = true;
	}

	//-----------------------------------------------------------------------------
	/// Set the end-of-line character (defaults to LF)
	bool SetReaderEndOfLine(char chDelimiter = '\n');
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the left trim characters for line based reading (default to none)
	bool SetReaderLeftTrim(KStringView sTrimLeft = "");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the right trim characters for line based reading (default to LF)
	bool SetReaderRightTrim(KStringView sTrimRight = detail::kLineRightTrims);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the right and left trim characters for line based reading (default to LF for right, none for left)
	bool SetReaderTrim(KStringView sTrimRight = detail::kLineRightTrims, KStringView sTrimLeft = "");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a const ref on the KInStream component
	const self_type& InStream() const
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Get a ref on the KInStream component
	self_type& InStream()
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	const std::istream& istream() const
	//-----------------------------------------------------------------------------
	{
		return *m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	std::istream& istream()
	//-----------------------------------------------------------------------------
	{
		return *m_InStream;
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	operator const std::istream& () const
	//-----------------------------------------------------------------------------
	{
		return istream();
	}

	//-----------------------------------------------------------------------------
	/// Get the std::istream
	operator std::istream& ()
	//-----------------------------------------------------------------------------
	{
		return istream();
	}

	//-----------------------------------------------------------------------------
	/// Check if stream is good for input
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return istream().good();
	}

//-------
private:
//-------

	std::istream*       m_InStream;
	KString             m_sTrimRight;
	KString             m_sTrimLeft;
	KString::value_type m_chDelimiter;
	bool                m_bImmutable;

}; // KInStream

extern KInStream KIn;

//-----------------------------------------------------------------------------
/// return a std::istream object that reads from nothing, but is valid
std::istream& kGetNullIStream();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// return a KInStream object that reads from nothing, but is valid
KInStream& kGetNullInStream();
//-----------------------------------------------------------------------------

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The general reader abstraction for dekaf2. Can be constructed around any
/// std::istream.
template<class IStream>
class DEKAF2_PUBLIC KReader
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
	// make sure this does not cover the copy or move constructor by requesting an
	// args count of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KReader(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , KInStream(static_cast<base_type&>(*this))
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
	KReader(Arg&& arg)
	    : base_type(std::forward<Arg>(arg))
	    , KInStream(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// copy construction is not allowed
	KReader(const KReader&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// depending on the istream type, move construction is allowed
	template<typename T = IStream, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KReader(KReader&& other)
	    : base_type(std::move(other))
	    , KInStream(std::move(other))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the istream type, move construction is not allowed
	template<typename T = IStream, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KReader(KReader&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// copy assignment is not allowed
	KReader& operator=(const KReader&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// move assignment is not allowed
	KReader& operator=(KReader&& other) = delete;
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
	KInFile() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KInFile(KStringViewZ sv, ios_base::openmode mode = ios_base::in)
	    : base_type(kToFilesystemPath(sv), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KInFile(KInFile&& other)
	    : base_type(std::move(other))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is not allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KInFile(KInFile&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void open(KStringViewZ sFilename, ios_base::openmode mode = ios_base::in)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sFilename), mode | ios_base::binary);
	}

};

DEKAF2_NAMESPACE_END
