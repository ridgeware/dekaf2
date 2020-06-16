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

/// @file kfilesystem.h
/// remaining standalone functions and classes around files and the file system

#include <cinttypes>
#include <vector>
#include "kstringview.h"

namespace dekaf2
{

enum DefaultFileCeateFlags
{
	DEKAF2_MODE_CREATE_FILE = 0666,
	DEKAF2_MODE_CREATE_DIR  = 0777
};

#ifdef _WIN32
constexpr char kDirSep { '\\' };
namespace detail {
constexpr KStringViewZ kLineRightTrims { "\r\n" };
constexpr KStringViewZ kAllowedDirSep { "/\\:" };
constexpr KStringViewZ kCurrentDir { "." };
constexpr KStringViewZ kCurrentDirWithSep { ".\\" };
}
#else
constexpr char kDirSep { '/' };
namespace detail {
constexpr KStringViewZ kLineRightTrims { "\n" };
constexpr KStringViewZ kAllowedDirSep { "/" };
constexpr KStringViewZ kCurrentDir { "." };
constexpr KStringViewZ kCurrentDirWithSep { "./" };
}
#endif

namespace detail {
constexpr KStringView kUnsafeFilenameChars { "#%&{}<>*? $!'\":@+`|=\\/" };
#ifdef DEKAF2_HAS_CPP_14
constexpr KStringView kUnsafePathnameChars = kUnsafeFilenameChars.ToView(0, kUnsafeFilenameChars.size() - 2);
#else
constexpr KStringView kUnsafeFilenameChars { "#%&{}<>*? $!'\":@+`|=" };
#endif
constexpr KStringView kUnsafeLimiterChars  { " .-_" };
}

//-----------------------------------------------------------------------------
/// Change file or directory permissions to value iMode. iMode can be set both by
/// POSIX permission bits or std::filesystem::perms permission bits (as they use
/// the same numerical constants)
bool kChangeMode(KStringViewZ sPath, int iMode);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Query file or directory permissions. In case of failure 0 is returned (to
/// avoid accidentially detecting permission bits set if not checked for negative
/// value).
int kGetMode(KStringViewZ sPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a file system entity exists
bool kExists (KStringViewZ sPath, bool bAsFile, bool bAsDirectory, bool bTestForEmptyFile = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a file exists.
/// @param bTestForEmptyFile If true treats a file as non-existing if its size is 0
inline bool kFileExists (KStringViewZ sPath, bool bTestForEmptyFile = false)
//-----------------------------------------------------------------------------
{
	return kExists(sPath, true, false, bTestForEmptyFile);
}

//-----------------------------------------------------------------------------
/// Checks if a directory exists
inline bool kDirExists (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kExists(sPath, false, true, false);
}

//-----------------------------------------------------------------------------
/// rename a file or directory
bool kRename (KStringViewZ sOldPath, KStringViewZ sNewPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove (unlink) a file or directory tree
bool kRemove (KStringViewZ sPath, bool bDir);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove (unlink) a file
inline bool kRemoveFile (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, false);
}

//-----------------------------------------------------------------------------
/// Remove (unlink) a directory (folder) hierarchically
inline bool kRemoveDir (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, true);
}

//-----------------------------------------------------------------------------
/// Read entire text file into a single string and convert DOS newlines if
/// bToUnixLineFeeds is true. The base function (that is also called by this
/// variant) is kReadAll(). This function does not allow to read "special"
/// files like those in /proc , because it tries to determine the file size
/// in advance (which is 0 for virtual files). Please resort to KInFile.ReadRemaining()
/// for such files.
bool kReadFile (KStringViewZ sPath, KString& sContents, bool bToUnixLineFeeds);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create (or truncate) a file and write the given contents and then chmod the file if
/// perms are different than 0666 (the default)
bool kWriteFile (KStringViewZ sPath, KStringView sContents, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// If not existing, create a file. Append contents and close again.
bool kAppendFile (KStringViewZ sPath, KStringView sContents, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a directory (folder) hierarchically
bool kCreateDir (KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_DIR);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Alias for kCreateDir
inline bool kMakeDir (KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_DIR)
//-----------------------------------------------------------------------------
{
	return kCreateDir (sPath, iMode);
}

//-----------------------------------------------------------------------------
/// Create a file if it does not exist, including the directory component.
/// If the file exists, advance its last mod timestamp.
bool kTouchFile(KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the extension of a path (filename extension after a dot)
KStringView kExtension(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove the extension from a path (filename extension after a dot)
KStringView kRemoveExtension(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the basename of a path (filename without directory)
KStringView kBasename(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the dirname of a path (directory name without the fileame)
KStringView kDirname(KStringView sFilePath, bool bWithTrailingSlash = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get last modification time of a file, returns -1 if file not found
time_t kGetLastMod(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found
size_t kFileSize(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found
inline size_t kGetNumBytes(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
	return kFileSize(sFilePath);
}

//-----------------------------------------------------------------------------
/// Returns true if a file name is safe to use, means it cannot escape from a
/// directory or uses problematic characters
bool kIsSafeFilename(KStringView sName);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns a file name that is safe to use, means it cannot escape from a
/// directory or uses problematic characters. The input string is the base for
/// the file name
KString kMakeSafeFilename(KStringView sName, bool bToLowercase = true, KStringView sEmptyName = "none");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if a path name is safe to use, means it cannot escape from a
/// directory or uses problematic characters
bool kIsSafePathname(KStringView sName);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns a path name that is safe to use, means it cannot escape from a
/// directory or uses problematic characters. The input string is the base for
/// the path name
KString kMakeSafePathname(KStringView sName, bool bToLowercase = true, KStringView sEmptyName = "none");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// resolve .. and . parts of the input path, and make it an absolute path
KString kNormalizePath(KStringView sPath);
//-----------------------------------------------------------------------------

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
	const_iterator begin() const { return m_DirEntries.begin(); }
	const_iterator end() const { return m_DirEntries.end(); }
	iterator begin() { return m_DirEntries.begin(); }
	iterator end() { return m_DirEntries.end(); }
	size_t size() const { return m_DirEntries.size(); }
	bool empty() const { return m_DirEntries.empty(); }
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

	/// match or remove all files that have EntryType Type from the list, returns count of matched entries
	size_t Match(EntryType Type, bool bRemoveMatches = false);

	/// match or remove all files that match the regular expression sRegex from the list, returns count of matched entries
	size_t Match(KStringView sRegex, bool bRemoveMatches = false);

	/// match or remove all files that match the basic regular expression sWildCard from the list, returns count of matched entries
	size_t WildCardMatch(KStringView sWildCard, bool bRemoveMatches = false);

	/// returns true if the directory list contains sWildCard, wildcard matching is supported
	bool Find(KStringView sWildCard) const;

	/// returns true if the directory list contains sWildCard, wildcard matching is supported
	bool Contains(KStringView sWildCard) const
	{
		return Find(sWildCard);
	}

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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// generate a temp directory, and remove it with the destructor if requested to
class KTempDir
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Create temp directory. The creation is not safe against race conditions.
	/// @param bDeleteOnDestruction delete directory and contents on destruction?
	/// @param bCreateNow create now, or later on request
	KTempDir (bool bDeleteOnDestruction = true, bool bCreateNow = true);
	~KTempDir ();

	/// in case the folder was not created during construction (because bCreateNow was false), create it now
	bool MakeDir ();

	/// get the name of the temp directory
	const KString& Name() const
	{
		return m_sTempDirName;
	}

	/// do we have a temp directory?
	operator bool() const
	{
		return !m_sTempDirName.empty();
	}

	operator const KString&() const
	{
		return m_sTempDirName;
	}

//----------
private:
//----------

	KString m_sTempDirName;
	bool m_bDeleteOnDestruction;

}; // KTempDir

} // end of namespace dekaf2

