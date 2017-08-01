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

namespace KReader_detail
{

bool ReadLine(std::streambuf* Stream, KString& sLine, KStringView sTrimRight = "", KString::value_type delimiter = '\n');
bool ReadAll(std::streambuf* Stream, KString& sContent);
ssize_t GetSize(std::streambuf* Stream, bool bReposition = true);
bool ReadRemaining(std::streambuf* Stream, KString& sContent);
ssize_t GetRemainingSize(std::streambuf* Stream);
bool Rewind(std::streambuf* Stream);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a streambuf iterator that returns strings, line by line
class const_streambuf_iterator
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//-------
public:
//-------
	typedef const_streambuf_iterator self_type;
	typedef KString value_type;
	typedef value_type& reference;
	typedef value_type* pointer;
	typedef std::streambuf* base_iterator;
	typedef std::input_iterator_tag iterator_category;
	typedef std::ptrdiff_t difference_type;

	const_streambuf_iterator() {}
	const_streambuf_iterator(base_iterator it, bool bToEnd, KStringView sTrimRight = "", KString::value_type chDelimiter = '\n');
	const_streambuf_iterator(const self_type&);
	const_streambuf_iterator(self_type&& other);
	self_type& operator=(const self_type&);
	self_type& operator=(self_type&& other);
	self_type& operator++();
	self_type operator++(int dummy);
	reference operator*();
	pointer operator->();
	bool operator==(const self_type& rhs);
	bool operator!=(const self_type& rhs);

//-------
private:
//-------
	base_iterator m_it{nullptr};
	size_t m_iCount{0};
	KString m_sBuffer;
	KString m_sTrimRight;
	KString::value_type m_chDelimiter{'\n'};
};

} // end of namespace KReader_detail


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class IStream>
class KReader : public IStream, public KReader_detail::const_streambuf_iterator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type = KReader<IStream>;
	using base_type = IStream;

//-------
public:
//-------

	using const_iterator = const_streambuf_iterator;
	using iterator = const_iterator;

	//-----------------------------------------------------------------------------
	KReader()
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::istream, IStream>::value,
		              "KReader can only be instantiated with std::istream derivates as the template argument");
	}

	//-----------------------------------------------------------------------------
	KReader(IStream&& is,
	        KStringView sTrimRight = "", KString::value_type chDelimiter = '\n')
	//-----------------------------------------------------------------------------
	    : base_type(std::move(is))
	    , m_sTrimRight(sTrimRight)
	    , m_chDelimiter(chDelimiter)
	{
	}

	//-----------------------------------------------------------------------------
	/// be prepared to get compiler warnings when you call this method on an
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
	self_type& operator=(self_type&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KReader() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline typename base_type::int_type Read()
	//-----------------------------------------------------------------------------
	{
		return base_type::sbumpc();
	}

	//-----------------------------------------------------------------------------
	inline bool Read(KString::value_type& ch)
	//-----------------------------------------------------------------------------
	{
		auto iCh = Read();
		ch = IStream::traits_type::to_char_type(iCh);
		return !base_type::traits_type::eq_int_type(iCh, base_type::traits_type::eof());
	}

	//-----------------------------------------------------------------------------
	inline KReader& Read(const typename base_type::char_type* pAddress, size_t iCount)
	//-----------------------------------------------------------------------------
	{
		base_type::sgetn(pAddress, iCount);
		return *this;
	}

	//-----------------------------------------------------------------------------
	template<typename T>
	inline self_type& Read(T& value)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_trivially_copyable<T>::value,
		              "KReader::Read() needs a trivially copyable type to succeed");
		Read(&value, sizeof(T));
		return *this;
	}

	//-----------------------------------------------------------------------------
	inline self_type& ReadLine(KString& sLine)
	//-----------------------------------------------------------------------------
	{
		KReader_detail::ReadLine(this->rdbuf(), sLine, m_sTrimRight, m_chDelimiter);
		return *this;
	}

	//-----------------------------------------------------------------------------
	inline bool ReadAll(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return KReader_detail::ReadAll(this->rdbuf(), sBuffer);
	}

	//-----------------------------------------------------------------------------
	inline bool ReadRemaining(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return KReader_detail::ReadRemaining(this->rdbuf(), sBuffer);
	}

	//-----------------------------------------------------------------------------
	inline bool GetContent(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return ReadAll(sBuffer);
	}

	//-----------------------------------------------------------------------------
	size_t GetSize()
	//-----------------------------------------------------------------------------
	{
		return KReader_detail::GetSize(this->rdbuf());
	}

	//-----------------------------------------------------------------------------
	size_t GetRemainingSize()
	//-----------------------------------------------------------------------------
	{
		return KReader_detail::GetRemainingSize(this->rdbuf());
	}

	//-----------------------------------------------------------------------------
	inline operator KString()
	//-----------------------------------------------------------------------------
	{
		KString sStr;
		ReadLine(sStr);
		return sStr;
	}

	//-----------------------------------------------------------------------------
	inline const_iterator cbegin()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(this->rdbuf(), false, m_sTrimRight, m_chDelimiter);
	}

	//-----------------------------------------------------------------------------
	inline const_iterator cend()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(this->rdbuf(), true);
	}

	//-----------------------------------------------------------------------------
	inline const_iterator begin()
	//-----------------------------------------------------------------------------
	{
		return cbegin();
	}

	//-----------------------------------------------------------------------------
	inline const_iterator end()
	//-----------------------------------------------------------------------------
	{
		return cend();
	}

	//-----------------------------------------------------------------------------
	inline bool IsEOF()
	//-----------------------------------------------------------------------------
	{
		return base_type::traits_type::eq_int_type(base_type::rdbuf()->sgetc(), base_type::traits_type::eof());
	}

//-------
protected:
//-------

	KString m_sTrimRight;
	KString::value_type m_chDelimiter{'\n'};

}; // KReader

using KFileReader   = KReader<std::ifstream>;
using KStringReader = KReader<std::istringstream>;

} // end of namespace dekaf2

