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

#include <streambuf>
#include <istream>
#include <fstream>
#include <sstream>
#include <type_traits>
#include "kstring.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a customizable input stream buffer
struct KIStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	/// the Reader's function's signature:
	/// std::streamsize Reader(void* sBuffer, std::streamsize iCount, void* CustomPointer)
	///  - returns read bytes. CustomPointer can be used for anything, to the discretion of the
	/// Reader.
	typedef std::streamsize (*Reader)(void*, std::streamsize, void*);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// provide a Reader function, it will be called by std::streambuf on buffer reads
	KIStreamBuf(Reader cb, void* CustomPointer = nullptr)
	//-----------------------------------------------------------------------------
	    : m_Callback(cb), m_CustomPointer(CustomPointer)
	{
	}
	//-----------------------------------------------------------------------------
	virtual ~KIStreamBuf();
	//-----------------------------------------------------------------------------

protected:
	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
//	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual int_type uflow() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
//	virtual std::streamsize showmanyc() override;
	//-----------------------------------------------------------------------------

private:
	Reader m_Callback{nullptr};
	void* m_CustomPointer{nullptr};
};


/// Read a line of text until EOF or delimiter from a std::istream. Right trim values of sTrimRight.
/// Reads directly in the underlying streambuf
bool kReadLine(std::istream& Stream, KString& sLine, KStringView sTrimRight = "", KString::value_type delimiter = '\n');

/// Read all content of a std::istream device into a string. Fails on non-seekable
/// istreams if bFromStart is true, otherwise tries to read as much as possible.
/// Reads directly in the underlying streambuf
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart = true);

/// Read all content of a file with name sFileName into a string
bool kReadAll(KStringView sFileName, KString& sContent);

/// Get the total size of a std::istream device. Returns -1 on Failure. Fails on non-seekable istreams.
ssize_t kGetSize(std::istream& Stream, bool bFromStart = true);

/// Get the total size of a file with name sFileName. Returns -1 on Failure.
ssize_t kGetSize(const char* sFileName);

/// Get the total size of a file with name sFileName. Returns -1 on Failure.
inline ssize_t kGetSize(const KString& sFileName)
{
	return kGetSize(sFileName.c_str());
}

/// Reposition the device of a std::istream to the beginning. Fails on non-seekable istreams.
bool kRewind(std::istream& Stream);


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The standalone reader abstraction for dekaf2. Can be constructed around any
/// std::istream, and has iterators and read accessors that attach to the
/// std::streambuf of the istream. Provides a line iterator on the file
/// content, and can trim returned lines. Is used as a component for KReader.
class KBasicReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KBasicReader;
	using ios_type  = std::istream;

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
		typedef const_kreader_line_iterator self_type;
		typedef KString value_type;
		typedef value_type& reference;
		typedef value_type* pointer;
		typedef KBasicReader base_iterator;
		typedef std::input_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

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

	//-----------------------------------------------------------------------------
	/// value constructor
	KBasicReader(std::istream& sRef,
	             KStringView sTrimRight = "",
	             KString::value_type chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	    : m_sRef(&sRef)
	    , m_sTrimRight(sTrimRight)
	    , m_chDelimiter(chDelimiter)
	{
	}

	//-----------------------------------------------------------------------------
	/// copy construct a KBasicReader
	KBasicReader(const self_type& other)
	//-----------------------------------------------------------------------------
	    : m_sRef(other.m_sRef)
	    , m_sTrimRight(other.m_sTrimRight)
	    , m_chDelimiter(other.m_chDelimiter)
	{}

	//-----------------------------------------------------------------------------
	/// move construct a KBasicReader
	KBasicReader(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : m_sRef(std::move(other.m_sRef))
	    , m_sTrimRight(std::move(other.m_sTrimRight))
	    , m_chDelimiter(other.m_chDelimiter)
	{}

	//-----------------------------------------------------------------------------
	/// copy assign a KBasicReader
	self_type& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		m_sRef = other.m_sRef;
		m_sTrimRight = other.m_sTrimRight;
		m_chDelimiter = other.m_chDelimiter;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move assign a KBasicReader
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		m_sRef = std::move(other.m_sRef);
		m_sTrimRight = std::move(other.m_sTrimRight);
		m_chDelimiter = other.m_chDelimiter;
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual ~KBasicReader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
	typename std::istream::int_type Read();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a character. Returns stream reference that resolves to false if no input available
	inline ios_type& Read(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		ch = std::istream::traits_type::to_char_type(Read());
		return *this->m_sRef;
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters. Returns count of successfully read charcters.
	size_t Read(typename std::istream::char_type* pAddress, size_t iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a type. Returns stream reference that resolves to false if no input available.
	/// Type must be trivially copyable.
	template<typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type* = nullptr>
	inline ios_type& Read(T& value)
	//-----------------------------------------------------------------------------
	{
		Read(&value, sizeof(T));
		return *this->m_sRef;
	}

	//-----------------------------------------------------------------------------
	/// Read a line of text. Returns false if no input available. Stops at delimiter
	/// character defined and optionally right trims the string from the trim
	/// definition. Per default contains the end-of-line character in the returned string.
	///
	/// Please note that this method does _not_ return the stream reference,
	/// but a boolean. std::istreams would not read a file with a missing newline
	/// at the end successfully, but report an error. This function succeeds.
	inline bool ReadLine(KString& sLine)
	//-----------------------------------------------------------------------------
	{
		return kReadLine(*m_sRef, sLine, m_sTrimRight, m_chDelimiter);
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in a string. Returns false if no input
	/// available. Fails on non-seekable inputs, e.g. streams.
	inline bool ReadAll(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(*m_sRef, sBuffer, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string. Returns false if no input
	/// available. Does not fail on non-seekable inputs, but tries to read the utmost.
	inline bool ReadRemaining(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(*m_sRef, sBuffer, false);
	}

	//-----------------------------------------------------------------------------
	/// Alias for ReadAll
	inline bool GetContent(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return ReadAll(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Returns the size of a file. Returns 0 if no input available. Fails on
	/// non-seekable inputs, e.g. streams.
	inline size_t GetSize()
	//-----------------------------------------------------------------------------
	{
		std::streamsize iSize = kGetSize(*m_sRef, true);
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
	inline size_t GetRemainingSize()
	//-----------------------------------------------------------------------------
	{
		std::streamsize iSize = kGetSize(*m_sRef, false);
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
	inline operator KString()
	//-----------------------------------------------------------------------------
	{
		KString sStr;
		ReadLine(sStr);
		return sStr;
	}

	//-----------------------------------------------------------------------------
	/// Reposition the input device of the std::istream to the beginning. Fails on streams.
	inline bool Rewind()
	//-----------------------------------------------------------------------------
	{
		return kRewind(*m_sRef);
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator to the current read position in a stream
	inline const_iterator cbegin()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(*this, false);
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator that is equal to an iterator that has reached the
	/// end of a stream
	inline const_iterator cend()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(*this, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator to the current read position in a stream
	inline const_iterator begin()
	//-----------------------------------------------------------------------------
	{
		return cbegin();
	}

	//-----------------------------------------------------------------------------
	/// Returns a const_iterator that is equal to an iterator that has reached the
	/// end of a stream
	inline const_iterator end()
	//-----------------------------------------------------------------------------
	{
		return cend();
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
	/// Set the right trim characters for line based reading (default to none)
	void SetReaderRightTrim(KStringView sTrimRight = "")
	//-----------------------------------------------------------------------------
	{
		m_sTrimRight = sTrimRight;
	}

	//-----------------------------------------------------------------------------
	/// Set the left and right trim characters for line based reading (default to none)
	void SetReaderTrim(KStringView sTrimLeft = "", KStringView sTrimRight = "")
	//-----------------------------------------------------------------------------
	{
		SetReaderLeftTrim(sTrimLeft);
		SetReaderRightTrim(sTrimRight);
	}

//-------
protected:
//-------
	// m_sRef always has to be valid after construction
	// - do not assign a nullptr per default
	std::istream* m_sRef;
	KString m_sTrimLeft;
	KString m_sTrimRight;
	KString::value_type m_chDelimiter{'\n'};

}; // KBasicReader


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The general reader abstraction for dekaf2. Can be constructed around any
/// std::istream.
template<class IStream>
class KReader : public IStream, public KBasicReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = IStream;
	using self_type = KReader<IStream>;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// move construct a KReader
	KReader(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , KBasicReader(std::move(other))
	{}

	//-----------------------------------------------------------------------------
	/// copy construct a KReader
	KReader(self_type& other)
	//-----------------------------------------------------------------------------
	    : base_type(other)
	    , KBasicReader(other)
	{}

	//-----------------------------------------------------------------------------
	// semi-perfect forwarding - currently needed as std::istream does not yet
	// support string_views as arguments
	template<class... Args>
	KReader(KStringView sv, Args&&... args)
	    : base_type(std::string(sv), std::forward<Args>(args)...)
	    , KBasicReader(static_cast<std::istream&>(*this))
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::istream, IStream>::value,
		              "KReader cannot be derived from a non-std::istream class");
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	template<class... Args>
	KReader(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , KBasicReader(static_cast<std::istream&>(*this))
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::istream, IStream>::value,
		              "KReader cannot be derived from a non-std::istream class");
	}

	//-----------------------------------------------------------------------------
	/// copy assignment
	self_type& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(other);
		KBasicReader::operator=(other);
	}

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		KBasicReader::operator=(std::move(other));
	}

	//-----------------------------------------------------------------------------
	// this one is necessary because ios_base has a symbol named end .. (for seeking)
	const_iterator end()
	//-----------------------------------------------------------------------------
	 {
		return KBasicReader::end();
	 }

};

/// File reader based on std::ifstream
using KInFile          = KReader<std::ifstream>;

/// String reader based on std::istreamstream
using KInStringStream  = KReader<std::istringstream>;

} // end of namespace dekaf2

