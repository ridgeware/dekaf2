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

/// @file kfile.h
/// remaining standalone functions around files and the file system

#include <cinttypes>
#include <vector>
#include "kstringview.h"
#include "kstream.h"

namespace dekaf2
{

typedef uint16_t KFileFlags;

//-----------------------------------------------------------------------------
/// Checks if a file system entity exists
bool kExists (KStringViewZ sPath, bool bAsFile, bool bAsDirectory, bool bTestForEmptyFile = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a file exists.
/// @param bTestForEmptyFile If true treats a file as non-existing if its size is 0
inline
bool kFileExists (KStringViewZ sPath, bool bTestForEmptyFile = false)
//-----------------------------------------------------------------------------
{
	return kExists(sPath, true, false, bTestForEmptyFile);
}

//-----------------------------------------------------------------------------
/// Checks if a directory exists
inline
bool kDirExists (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kExists(sPath, false, true, false);
}

//-----------------------------------------------------------------------------
/// Remove (unlink) a file or directory tree
bool kRemove (KStringViewZ sPath, bool bDir);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove (unlink) a file
inline
bool kRemoveFile (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, false);
}

//-----------------------------------------------------------------------------
/// Remove (unlink) a directory (folder) hierarchically
inline
bool kRemoveDir (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, true);
}

//-----------------------------------------------------------------------------
/// Create a directory (folder) hierarchically
bool kCreateDir (KStringViewZ sPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the extension of a path (filename extension after a dot)
KStringView kExtension(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the basename of a path (filename without directory)
KStringView kBasename(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the dirname of a path (directory name without the fileame)
KStringView kDirname(KStringView sFilePath, bool bWithSlash = true);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get last modification time of a file, returns -1 if file not found
time_t kGetLastMod(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found
size_t kGetNumBytes(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found
inline
size_t kFileSize(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
	return kGetNumBytes(sFilePath);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Retrieve and filter directory listings
class KDirectory
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum class EntryType
	{
		ALL,
		BLOCK,
		CHARACTER,
		DIRECTORY,
		FIFO,
		LINK,
		REGULAR,
		SOCKET,
		OTHER
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper type that keeps one directory entry, with its name, full path and type
	class DirEntry
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		DirEntry() : m_Type(EntryType::ALL) {}

		DirEntry(KStringView BasePath, KStringView Name, EntryType Type);

		bool operator<(const DirEntry& other)
		{
			return m_Path < other.m_Path;
		}

		bool operator==(const DirEntry& other)
		{
			return m_Path == other.m_Path && m_Type == other.m_Type;
		}

		/// returns full pathname (path + filename)
		operator KStringViewZ() const
		{
			return Path();
		}

		/// returns full pathname (path + filename)
		KStringViewZ Path() const
		{
			return m_Path;
		}

		/// returns filename only, no path component
		KStringViewZ Filename() const
		{
			return m_Filename;
		}

		/// returns directory entry type
		EntryType Type() const
		{
			return m_Type;
		}

	//----------
	private:
	//----------

		KString m_Path;
		KStringViewZ m_Filename;
		EntryType m_Type;

	}; // DirEntry

	using DirEntries = std::vector<DirEntry>;
	using iterator = DirEntries::iterator;
	using const_iterator = DirEntries::const_iterator;

	/// default ctor
	KDirectory() = default;

	/// ctor that will open a directory and store all entries that are of EntryType Type
	KDirectory(KStringViewZ sDirectory, EntryType Type = EntryType::ALL)
	{
		Open(sDirectory, Type);
	}

	const_iterator cbegin() const { return m_DirEntries.begin(); }
	const_iterator cend() const { return m_DirEntries.end(); }
	iterator begin() { return m_DirEntries.begin(); }
	iterator end() { return m_DirEntries.end(); }
	size_t size() const { return m_DirEntries.size(); }
	void clear();

	/// open a directory and store all entries that are of EntryType Type
	size_t Open(KStringViewZ sDirectory, EntryType Type = EntryType::ALL);

	/// open a directory and store all entries that are of EntryType Type
	size_t operator()(KStringViewZ sDirectory, EntryType Type = EntryType::ALL)
	{
		return Open(sDirectory, Type);
	}

	/// remove all hidden files, that is, files that start with a dot
	void RemoveHidden();

	/// match or remove all files that have EntryType Type from the list
	size_t Match(EntryType Type, bool bRemoveMatches = false);

	/// match or remove all files that match the regular expression sRegex from the list
	size_t Match(KStringView sRegex, bool bRemoveMatches = false);

	/// returns true if the directory list contains sName
	bool Find(KStringView sName, bool bRemoveMatch = false);

	/// sort the directory list
	void Sort();

//----------
protected:
//----------

	DirEntries m_DirEntries;

}; // KDirectory

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Get disk capacity
class KDiskStat
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default ctor
	KDiskStat() = default;

	/// Read disk stats for partition at sPath
	KDiskStat(KStringViewZ sPath)
	{
		Check(sPath);
	}

	/// Read disk stats for partition at sPath
	KDiskStat& Check(KStringViewZ sPath);

	/// Read disk stats for partition at sPath
	KDiskStat& operator()(KStringViewZ sPath)
	{
		return Check(sPath);
	}

	/// Return count of total bytes on file system
	uint64_t Total() const { return m_Total; }

	/// Return count of used bytes on file system
	uint64_t Used() const { return m_Used; }

	/// Return count of free bytes on file system for non-priviledged user
	uint64_t Free() const { return m_Free; }

	/// Return count of free bytes on file system for priviledged user
	uint64_t SystemFree() const { return m_SystemFree; }

	/// Returns false if an error occured
	operator bool() const { return !m_sError.empty(); }

	/// Return last error
	const KString& Error() const { return m_sError; }

	/// Reset to default
	void clear();

//----------
private:
//----------

	bool SetError(KStringView sError)
	{
		m_sError = sError;
		return false;
	}

	uint64_t m_Free       { 0 };
	uint64_t m_Total      { 0 };
	uint64_t m_Used       { 0 };
	uint64_t m_SystemFree { 0 };

	KString m_sError;

}; // KDiskStat

} // end of namespace dekaf2

