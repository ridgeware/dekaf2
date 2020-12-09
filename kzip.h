/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include <iterator>
#include "kstring.h"
#include "kexception.h"
#include "kwriter.h"
#include "kreader.h"
#include "kfilesystem.h"

struct zip_stat;

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Wrapper class around libzip to give easy access from C++ to the files in
/// a zip archive or to create or append to one
class KZip
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// class that holds all information about one ZIP archive entry
	struct DirEntry
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		KStringViewZ sName;              ///< name of the file
		std::size_t  iIndex;             ///< index within archive
		uint64_t     iSize;              ///< size of file (uncompressed)
		uint64_t     iCompSize;          ///< size of file (compressed)
		time_t       mtime;              ///< modification time

		/// clear the DirEntry struct
		void         clear();

		/// return true if the entry is a directory
		bool         IsDirectory() const;

		/// return a sanitized file name (no path, no escaping, no special characters)
		KString      SafeName() const
		{
			return kMakeSafeFilename(kBasename(sName), false);
		}

		/// return a sanitized path name (no escaping, no special characters)
		KString      SafePath() const
		{
			return kMakeSafePathname(sName, false);
		}

		/// return compression ratio in percent
		uint16_t     PercentCompressed() const;

	//------
	private:
	//------

		friend class KZip;

		bool from_zip_stat(const struct zip_stat* stat);

	}; // DirEntry

	using Directory = std::vector<DirEntry>;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class iterator
	{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	//------
	public:
	//------

		using iterator_category = std::random_access_iterator_tag;
		using value_type        = const DirEntry;
		using pointer           = const value_type*;
		using reference         = const value_type&;
		using difference_type   = int64_t;
		using self_type         = iterator;

		iterator(KZip& Zip, uint64_t iIndex = 0) noexcept;

		reference operator*() const;

		pointer operator->() const
		{
			return & operator*();
		}

		// post increment
		iterator& operator++() noexcept;

		// pre increment
		iterator operator++(int) noexcept;

		// post decrement
		iterator& operator--() noexcept;

		// pre decrement
		iterator operator--(int) noexcept;

		// increment
		iterator operator+(difference_type iIncrement) const noexcept
		{
			return iterator(*m_Zip, m_iIndex + iIncrement);
		}

		// decrement
		iterator operator-(difference_type iDecrement) const noexcept
		{
			return iterator(*m_Zip, m_iIndex + iDecrement);
		}

		bool operator==(const iterator& other) const noexcept
		{
			return m_Zip == other.m_Zip && m_iIndex == other.m_iIndex;
		}

		bool operator!=(const iterator& other) const noexcept
		{
			return !operator==(other);
		}

		DirEntry operator[](difference_type iIndex)
		{
			return m_Zip->Get(m_iIndex + iIndex);
		}

	//------
	private:
	//------

		KZip*    m_Zip    { nullptr };
		uint64_t m_iIndex { 0 };
		mutable  DirEntry m_DirEntry;

	}; // iterator

	using const_iterator = iterator;

	KZip(bool bThrow = false)
	: m_bThrow(bThrow)
	{
	}

	KZip(KStringViewZ sFilename, bool bWrite = false, bool bThrow = false)
	: m_bThrow(bThrow)
	{
		Open(sFilename, bWrite);
	}

	KZip(const KZip&) = delete;
	KZip(KZip&&) = default;
	KZip& operator=(const KZip&) = delete;
	KZip& operator=(KZip&&) = default;

	~KZip();

	/// returns true if (strong, AES 256) encryption is available
	bool HaveStrongEncryption() const;

	/// set password for encrypted archive entries
	KZip& SetPassword(KString sPassword)
	{
		m_sPassword = std::move(sPassword);
		return *this;
	}

	/// open a zip archive, either for reading or for reading and writing
	bool Open(KStringViewZ sFilename, bool bWrite = false);

	/// returns true if archive is opened
	bool is_open() const
	{
		return D.get();
	}

	/// close a zip archive - not needed, will be done by dtor, ctor, or
	/// opening another one as well
	void Close();

	/// returns count of entries in zip
	std::size_t size() const noexcept;

	iterator begin() noexcept
	{
		return iterator(*this);
	}

	const_iterator begin() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this));
	}

	const_iterator cbegin() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this));
	}

	iterator end() noexcept
	{
		return iterator(*this, size());
	}

	const_iterator end() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this), size());
	}

	const_iterator cend() const noexcept
	{
		return const_iterator(const_cast<KZip&>(*this), size());
	}

	/// returns a DirEntry at iIndex
	/// @param iIndex the index position in the archive directory
	DirEntry Get(std::size_t iIndex) const;

	/// returns a DirEntry for file sName
	/// @param sName the file name searched for
	/// @param bNoPathCompare if true, only the file name part of the path
	/// is searched for
	DirEntry Get(KStringViewZ sName, bool bNoPathCompare = false) const;

	/// returns a DirEntry, alias of Get()
	/// @param sName the file name searched for
	/// @param bNoPathCompare if true, only the file name part of the path
	/// is searched for
	DirEntry Find(KStringViewZ sName, bool bNoPathCompare = false) const
	{
		return Get(sName, bNoPathCompare);
	}

	/// returns true if file sName exists
	/// @param sName the file name searched for
	/// @param bNoPathCompare if true, only the file name part of the path
	/// is searched for
	bool Contains(KStringViewZ sName, bool bNoPathCompare = false) const noexcept;

	/// returns a vector with all DirEntries of files
	Directory Files() const;

	/// returns a vector with all DirEntries of directories
	Directory Directories() const;

	/// returns a vector with all DirEntries of files and directories
	Directory FilesAndDirectories() const;

	/// reads a DirEntry's file into a KOutStream
	/// @param OutStream the stream to add to from the archive
	/// @param DirEntry the archive directory entry for the file to read
	bool Read(KOutStream& OutStream, const DirEntry& DirEntry);

	/// reads a DirEntry's file into a file sFileName
	/// @param sFileName the file to read into from the archive
	/// @param DirEntry the archive directory entry for the file to read
	bool Read(KStringViewZ sFileName, const DirEntry& DirEntry);

	/// reads a DirEntry's file into a string to return
	/// @param DirEntry the archive directory entry for the file to read
	KString Read(const DirEntry& DirEntry);

	/// reads all files listed in Directory into sTargetDirectory
	/// @param Directory list of files to extract.
	/// @param sTargetDirectory directory into which the archive will be expanded.
	/// If it does not exist it will be created.
	/// @param bWithSubdirectories create subdirectories on extraction, or write
	/// all files into a flat hierarchy. Default true.
	bool Read(const Directory& Directory, KStringViewZ sTargetDirectory, bool bWithSubdirectories = true);

	/// reads all files and directories in an archive
	/// @param sTargetDirectory directory into which the archive will be expanded.
	/// If it does not exist it will be created.
	/// @param bWithSubdirectories create subdirectories on extraction, or write
	/// all files into a flat hierarchy. Default true.
	bool ReadAll(KStringViewZ sTargetDirectory, bool bWithSubdirectories = true);

	/// add a stream to the archive
	/// @param InStream the stream to add
	/// @param sDispname the name for the stream in the archive (including path)
	bool Write(KInStream& InStream, KStringViewZ sDispname);

	/// add a string buffer to the archive
	/// @param sBuffer the data buffer to add
	/// @param sDispname the name for the buffer in the archive (including path)
	bool WriteBuffer(KStringView sBuffer, KStringViewZ sDispname);

	/// add a file to the archive
	/// @param sFilename the file to add
	/// @param sDispname the name for the file in the archive (including path)
	bool WriteFile(KStringViewZ sFilename, KStringViewZ sDispname = KStringViewZ{});

	/// adds a directory entry to the archive (does not read a directory from disk!)
	/// @param sDispname the directory name to add to the archive
	bool WriteDirectory(KStringViewZ sDispname);

	/// writes all files in a KDirectory list into archive
	/// @param Directory the directory list
	/// @param sDirectoryRoot the part of the pathnames that should be removed when storing in the archive
	/// @param sNewRoot name to use as the root directory, default none
	bool WriteFiles(const KDirectory& Directory, KStringView sDirectoryRoot = KStringView{}, KStringView sNewRoot = KStringView{});

	/// returns last error if class is not constructed to throw (default)
	const KString& Error() const
	{
		return m_sError;
	}

//------
private:
//------

	bool SetError(KString sError) const;
	bool SetError(int iError) const;
	bool SetError() const;
	bool SetEncryptionForFile(uint64_t iIndex);

	using Buffer = std::unique_ptr<char[]>;
	std::vector<Buffer> m_WriteBuffers;
	KString m_sPassword;
	mutable KString m_sError;
	bool m_bThrow { false };

	// helper types to allow for a unique_ptr<void>, which lets us hide all
	// implementation headers from the interface and nonetheless keep exception safety
	using deleter_t = std::function<void(void *)>;
	using unique_void_ptr = std::unique_ptr<void, deleter_t>;

	unique_void_ptr D;

}; // KZip

} // end of namespace dekaf2
