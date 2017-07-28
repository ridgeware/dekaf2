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

#include "kstring.h"

namespace dekaf2
{

class KReader
{
public:

	KReader() {}
	KReader(const KStringView& sPathName) {}
	KReader(const KReader& other) = delete;
	KReader(KReader&& other) noexcept;
	KReader& operator=(const KReader& other) = delete;
	KReader& operator=(KReader&& other) noexcept;

	virtual ~KReader() {}// = 0;

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

	virtual bool Open(KStringView name) = 0;

	virtual void Close() = 0;

	virtual bool IsOpen() const = 0;

	virtual bool IsEOF() const = 0;

	virtual size_t GetSize() const = 0;

	virtual bool GetContent(KString& sContent, bool bIsText = false);

	virtual bool Read(KString::value_type& ch) = 0;

	virtual size_t Read(void* pAddress, size_t iCount) = 0;

	virtual bool ReadLine(KString& line, bool bOnlyText = true);

	operator KString()
	{
		KString sStr;
		ReadLine(sStr);
		return sStr;
	}

	const_iterator cbegin()
	{
		return const_iterator(this, false);
	}

	const_iterator cend()
	{
		return const_iterator(this, true);
	}

	const_iterator begin()
	{
		return cbegin();
	}

	const_iterator end()
	{
		return cend();
	}

};

class KFILEReader : public KReader
{
public:
	KFILEReader() {}
	explicit KFILEReader(int filedesc);
	explicit KFILEReader(FILE* fileptr);
	KFILEReader(const KStringView& sPathName)
	{
		Open(sPathName);
	}
	KFILEReader(const KFILEReader& other) = delete;
	KFILEReader(KFILEReader&& other) noexcept;
	KFILEReader& operator=(const KFILEReader& other) = delete;
	KFILEReader& operator=(KFILEReader&& other) noexcept;
	virtual ~KFILEReader();

	virtual bool Open(KStringView name);
	bool Open(FILE* fileptr);
	bool Open(int filedesc);
	virtual void Close();
	virtual bool IsOpen() const;
	virtual bool IsEOF() const;
	virtual size_t GetSize() const;
	virtual bool Read(KString::value_type& ch);
	virtual size_t Read(void* pAddress, size_t iCount);

private:

	FILE* m_File{nullptr};

}; // KFILEReader

} // end of namespace dekaf2

