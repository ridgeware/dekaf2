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

#include <cstdio>
#include <streambuf>
#include <istream>
#include <fstream>
#include <sstream>
#include <type_traits>
#include "kstring.h"

namespace dekaf2
{

/// Read a line of text until EOF or delimiter from a std::istream. Right trim values of sTrimRight.
/// Reads directly in the underlying streambuf
bool kReadLine(std::istream& Stream, KString& sLine, KStringView sTrimRight = "", KString::value_type delimiter = '\n');

/// Read all content of a std::istream device into a string. Fails on non-seekable istreams.
/// Reads directly in the underlying streambuf
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart = true);

/// Get the total size of a std::istream device. Returns -1 on Failure. Fails on non-seekable istreams.
ssize_t kGetSize(std::istream& Stream, bool bFromStart = true);

/// Reposition the device of a std::istream to the beginning. Fails on non-seekable istreams.
bool kRewind(std::istream& Stream);


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The general reader abstraction for dekaf2. Can be constructed around any
/// std::istream, and has iterators and read accessors that attach to the
/// std::streambuf of the istream. Provides a line iterator on the file
/// content, and can right trim returned lines.
template<class IStream>
class KReader : public IStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KReader<IStream>;
	using base_type = IStream;

//-------
public:
//-------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a istream iterator that returns strings, line by line, by reading directly
	/// from the underlying streambuf
	class const_istream_line_iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	//-------
	public:
	//-------
		typedef const_istream_line_iterator self_type;
		typedef KString value_type;
		typedef value_type& reference;
		typedef value_type* pointer;
		typedef KReader<IStream> base_iterator;
		typedef std::input_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

		const_istream_line_iterator()
		{
			// beware, m_it is a nullptr now
		}

		const_istream_line_iterator(base_iterator& it, bool bToEnd)
		    : m_it(&it)
		{
			if (m_it != nullptr && !bToEnd)
			{
				if (kReadLine(*m_it, m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
				{
					++m_iCount;
				}
			}
		}

		const_istream_line_iterator(const self_type& other)
		    : m_it(other.m_it)
		    , m_iCount(other.m_iCount)
		    , m_sBuffer(other.m_sBuffer)
		{
		}

		const_istream_line_iterator(self_type&& other)
		    : m_it(std::move(other.m_it))
		    , m_iCount(std::move(other.m_iCount))
		    , m_sBuffer(std::move(other.m_sBuffer))
		{
		}

		self_type& operator=(const self_type& other)
		{
			m_it = other.m_it;
			m_iCount = other.m_iCount;
			m_sBuffer = other.m_sBuffer;
			return *this;
		}

		self_type& operator=(self_type&& other)
		{
			m_it = std::move(other.m_it);
			m_iCount = std::move(other.m_iCount);
			m_sBuffer = std::move(other.m_sBuffer);
			return *this;
		}

		self_type& operator++()
		{
			if (m_it != nullptr && kReadLine(*m_it, m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
			{
				++m_iCount;
			}
			else
			{
				m_iCount = 0;
			}

			return *this;
		} // prefix

		self_type operator++(int)
		{
			self_type i = *this;

			if (m_it != nullptr && kReadLine(*m_it, m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
			{
				++m_iCount;
			}
			else
			{
				m_iCount = 0;
			}

			return i;
		} // postfix

		/// returns the current string
		inline reference operator*()
		{
			return m_sBuffer;
		}

		/// returns the current string
		inline pointer operator->()
		{
			return &m_sBuffer;
		}

		inline bool operator==(const self_type& rhs)
		{
			return m_it == rhs.m_it && m_iCount == rhs.m_iCount;
		}

		bool operator!=(const self_type& rhs)
		{
			return !operator==(rhs);
		}

	//-------
	protected:
	//-------
		base_iterator* m_it{nullptr};
		size_t m_iCount{0};
		KString m_sBuffer;

	}; // const_istream_line_iterator

	using const_iterator = const_istream_line_iterator;
	using iterator = const_iterator;

	//-----------------------------------------------------------------------------
	/// default ctor
	KReader()
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::istream, IStream>::value,
		              "KReader can only be instantiated with std::istream derivates as the template argument");
	}

	//-----------------------------------------------------------------------------
	/// construct a KReader on a passed std::istream (use std::move()!)
	KReader(IStream&& is,
	        KStringView sTrimRight = "", KString::value_type chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	    : base_type(std::move(is))
	    , m_sTrimRight(sTrimRight)
	    , m_chDelimiter(chDelimiter)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a KReader on a file-like input source.
	/// Be prepared to get compiler warnings when you call this method on an
	/// istream that does not have this constructor (i.e. all non-ifstreams..)
	KReader(const char* sName, std::ios::openmode mode = std::ios::in,
	        KStringView sTrimRight = "", KString::value_type chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	    : base_type(sName, mode)
	    , m_sTrimRight(sTrimRight)
	    , m_chDelimiter(chDelimiter)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a KReader on a file-like input source.
	/// be prepared to get compiler warnings when you call this method on an
	/// istream that does not have this constructor (i.e. all non-ifstreams..)
	KReader(const std::string& sName, std::ios::openmode mode = std::ios::in,
	        KStringView sTrimRight = "", KString::value_type chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	    : base_type(sName, mode)
	    , m_sTrimRight(sTrimRight)
	    , m_chDelimiter(chDelimiter)
	{}

	//-----------------------------------------------------------------------------
	KReader(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Move-construct a KReader
	KReader(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , m_sTrimRight(std::move(other.m_sTrimRight))
	    , m_chDelimiter(other.m_chDelimiter)
	{}

	//-----------------------------------------------------------------------------
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move-assign a KReader
	self_type& operator=(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		m_sTrimRight = std::move(other.m_sTrimRight);
		m_chDelimiter = other.m_chDelimiter;
	}

	//-----------------------------------------------------------------------------
	virtual ~KReader() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
	typename base_type::int_type Read()
	//-----------------------------------------------------------------------------
	{
		std::streambuf* sb = this->rdbuf();
		if (sb)
		{
			typename base_type::int_type iCh = sb->sbumpc();
			if (base_type::traits_type::eq_int_type(iCh, base_type::traits_type::eof()))
			{
				this->setstate(std::ios::eofbit);
			}
			return iCh;
		}
		return base_type::traits_type::eof();
	}

	//-----------------------------------------------------------------------------
	/// Read a character. Returns stream reference that resolves to false if no input available
	inline self_type& Read(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		ch = IStream::traits_type::to_char_type(Read());
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Read a range of characters. Returns count of successfully read charcters.
	size_t Read(const typename base_type::char_type* pAddress, size_t iCount)
	//-----------------------------------------------------------------------------
	{
		std::streambuf* sb = this->rdbuf();
		if (sb)
		{
			size_t iRead = static_cast<size_t>(sb->sgetn(pAddress, iCount));
			if (iRead != iCount)
			{
				this->setstate(std::ios::eofbit);
			}
			return iRead;
		}
		return 0;
	}

	//-----------------------------------------------------------------------------
	/// Read a type. Returns stream reference that resolves to false if no input available.
	/// Type must be trivially copyable.
	template<typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type* = nullptr>
	inline self_type& Read(T& value)
	//-----------------------------------------------------------------------------
	{
		Read(&value, sizeof(T));
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Read a line of text. Returns false if no input available. Stops at delimiter
	/// character defined with the constructor and optionally right trims the string
	/// from the trim definition given to the constructor. Per default contains the
	/// end-of-line character in the returned string.
	/// Please note that this method does _not_ return the stream reference,
	/// but a boolean. std::istreams would not read a file with a missing newline
	/// at the end successfully, but report an error. This function would succeed.
	inline bool ReadLine(KString& sLine)
	//-----------------------------------------------------------------------------
	{
		return kReadLine(*this, sLine, m_sTrimRight, m_chDelimiter);
	}

	//-----------------------------------------------------------------------------
	/// Returns the complete content of a file in a string. Returns false if no input
	/// available. Fails on non-seekable inputs, e.g. streams.
	inline bool ReadAll(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(*this, sBuffer, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining content of a file in a string. Returns false if no input
	/// available. Fails on non-seekable inputs, e.g. streams.
	inline bool ReadRemaining(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return kReadAll(*this, sBuffer, false);
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
		return kGetSize(*this, true);
	}

	//-----------------------------------------------------------------------------
	/// Returns the remaining size of a file. Returns 0 if no input available.
	/// Fails on non-seekable inputs, e.g. streams.
	inline size_t GetRemainingSize()
	//-----------------------------------------------------------------------------
	{
		return kGetSize(*this, false);
	}

	//-----------------------------------------------------------------------------
	/// Returns a line of text as if read by ReadLine(). If trimming as specified
	/// in the constructor includes the end-of-line delimiter, returns an empty
	/// string for both an empty line, and unavailable input. In that case call
	/// eof() to distinguish between both cases.
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
		return kRewind(*this);
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

//-------
protected:
//-------

	KString m_sTrimRight;
	KString::value_type m_chDelimiter{'\n'};

}; // KReader

/// File reader based on std::ifstream
using KFileReader   = KReader<std::ifstream>;
/// String reader based on std::istringstream
using KStringReader = KReader<std::istringstream>;

} // end of namespace dekaf2

