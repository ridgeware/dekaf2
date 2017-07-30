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
#include "kreader.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KFile : public KFileReader
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

	bool             Open         (const KString& sPathName, KFileFlags eFlags = TEXT | READ);
	bool             Open         (int filedesc, KFileFlags eFlags = TEXT | READ);
	bool             Open         (FILE* fileptr, KFileFlags eFlags = TEXT | READ);
	void             Close        ();

	bool             Write        (const KString& sLine);
	bool             WriteLine    (const KString& sLine);
	bool             Flush        ();

	bool             Exists       (KFileFlags iFlags = NONE) const;
	size_t           GetBytes     () const { return KFileReader::GetSize(); }

	bool             GetContent   (KString& sContent, bool bIsText = false) { return KFileReader::GetContent(sContent, bIsText); }

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

