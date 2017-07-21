/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

#include <cinttypes>
#include <experimental/filesystem>
#include <cstdio>
#include <iterator>
#include "kstring.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KFile
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//-------
public:
//-------
	enum // flags
	{
		NONE   = 0,
		TEXT   = 1 << 0,
		TRIM   = 1 << 1,
		TEST0  = 1 << 2,  // test for zero-length file
		READ   = 1 << 3,
		WRITE  = 1 << 4,
		APPEND = 1 << 5
	};

	typedef uint16_t KFileFlags;

	KFile() {}
	explicit KFile(int filedesc);
	explicit KFile(FILE* fileptr);
	KFile(const KString& sPathName, KFileFlags eFlags = TEXT)
	{
		Open(sPathName, eFlags);
	}
	KFile(const KFile& other) = delete;
	KFile(KFile&& other) noexcept;
	KFile& operator=(const KFile& other) = delete;
	KFile& operator=(KFile&& other) noexcept;

	virtual ~KFile()
	{
		Close();
	}

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
		typedef KFile* base_iterator;
		typedef std::input_iterator_tag iterator_category;
		typedef std::ptrdiff_t difference_type;

		const_iterator() {}
		const_iterator(KFile* it, bool bToEnd);
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
		KFile* m_it{nullptr};
		size_t m_iCount{0};
		KString m_sBuffer;
	};

	typedef const_iterator iterator;

	bool             Open         (const KString& sPathName, KFileFlags eFlags = TEXT | READ);
	bool             Open         (int filedesc, KFileFlags eFlags = TEXT | READ);
	bool             Open         (FILE* fileptr, KFileFlags eFlags = TEXT | READ);
	void             Close        ();
	bool             GetLine      (KString& sLine);
	operator         KString      () { KString sStr; GetLine(sStr); return sStr; }
	const_iterator   cbegin       () { return const_iterator(this, false); }
	const_iterator   cend         () { return const_iterator(this, true); }
	const_iterator   begin        () { return cbegin(); }
	const_iterator   end          () { return cend(); }

	bool             Write        (const KString& sLine);
	bool             WriteLine    (const KString& sLine);
	bool             Flush        ();

	bool             IsOpen       () const { return m_File; }
	bool             IsEOF        () const { return !m_File || feof(m_File); }

	bool             Exists       (KFileFlags iFlags = NONE) const;
	size_t           GetSize      () const;
	size_t           GetBytes     () const { return GetSize(); }
	bool             GetContent   (KString& sContent) const;

	// static interface
	static bool      Exists       (const KString& sPath, KFileFlags iFlags = NONE);
	static bool      GetContent   (KString& sContent, const KString& sPath, KFileFlags eFlags = TEXT);
	static size_t    GetSize      (const KString& sPath);
	static size_t    GetBytes     (const KString& sPath) { return GetSize(sPath); }
	static KString   CalcFOpenModeString(KFileFlags eFlags);
	static KString   GetCWD       ();

//-------
protected:
//-------

//-------
private:
//-------
	std::experimental::filesystem::path m_FilePath;
	KFileFlags m_eFlags{NONE};
	FILE* m_File{nullptr};

};

inline KString       kGetCWD      () { return KFile::GetCWD(); }

} // end of namespace dekaf2

