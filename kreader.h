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
#include "kstring.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KReader() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KReader(KStringView sName) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KReader(const KReader& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KReader(KReader&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KReader& operator=(const KReader& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KReader& operator=(KReader&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KReader() {}
	//-----------------------------------------------------------------------------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class const_iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	//-------
	public:
	//-------
		typedef const_iterator self_type;
		typedef KString value_type;
		typedef value_type& reference;
		typedef value_type* pointer;
		typedef KReader* base_iterator;
		typedef std::input_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

		const_iterator() {}
		const_iterator(KReader* it, bool bToEnd);
		const_iterator(const self_type&);
		const_iterator(self_type&& other);
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
		KReader* m_it{nullptr};
		size_t m_iCount{0};
		KString m_sBuffer;
	};

	typedef const_iterator iterator;

	//-----------------------------------------------------------------------------
	virtual bool Open(KStringView sName) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual void Close() = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool IsOpen() const = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool IsEOF() const = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual size_t GetSize() const = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool GetContent(KString& sContent, bool bIsText = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool Read(KString::value_type& ch) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual size_t Read(void* pAddress, size_t iCount) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template<typename T>
	inline bool Read(T& value)
	//-----------------------------------------------------------------------------
	{
		return Read(&value, sizeof(T)) == sizeof(T);
	}

	//-----------------------------------------------------------------------------
	virtual bool ReadLine(KString& line, KString::value_type delim = '\n');
	//-----------------------------------------------------------------------------

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
		return const_iterator(this, false);
	}

	//-----------------------------------------------------------------------------
	inline const_iterator cend()
	//-----------------------------------------------------------------------------
	{
		return const_iterator(this, true);
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

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KStringReader : public KReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KStringReader() {}
	KStringReader(KStringView sSource)
	{
		Open(sSource);
	}
	KStringReader(const KStringReader& other) = delete;
	KStringReader(KStringReader&& other) noexcept;
	KStringReader& operator=(const KStringReader& other) = delete;
	KStringReader& operator=(KStringReader&& other) noexcept;
	virtual ~KStringReader();

	virtual bool Open(KStringView sSource);
	virtual void Close();
	virtual bool IsOpen() const;
	virtual bool IsEOF() const;
	virtual size_t GetSize() const;
	virtual bool Read(KString::value_type& ch);
	virtual size_t Read(void* pAddress, size_t iCount);

//----------
protected:
//----------

	KStringView m_Source;
	KStringView::const_iterator m_it{m_Source.cend()};

}; // KFileReader

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KFileReader : public KReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KFileReader() {}

	explicit KFileReader(int filedesc)
	{
		Open(filedesc);
	}

	explicit KFileReader(FILE* fileptr)
	{
		Open(fileptr);
	}

	KFileReader(KStringView sName)
	{
		Open(sName);
	}

	KFileReader(const KFileReader& other) = delete;
	KFileReader(KFileReader&& other) noexcept;
	KFileReader& operator=(const KFileReader& other) = delete;
	KFileReader& operator=(KFileReader&& other) noexcept;
	virtual ~KFileReader();

	virtual bool Open(KStringView sName);
	bool Open(FILE* fileptr);
	bool Open(int filedesc);
	virtual void Close();
	virtual bool IsOpen() const;
	virtual bool IsEOF() const;
	virtual size_t GetSize() const;
	virtual bool Read(KString::value_type& ch);
	virtual size_t Read(void* pAddress, size_t iCount);

//----------
protected:
//----------

	FILE* m_File{nullptr};

}; // KFileReader

} // end of namespace dekaf2

