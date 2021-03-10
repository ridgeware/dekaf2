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
/// standalone functions and classes around files and the file system

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
constexpr KStringView kUnsafePathnameChars { "#%&{}<>*? $!'\":@+`|=" };
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
/// variant) is kReadAll().
bool kReadFile (KStringViewZ sPath, KString& sContents, bool bToUnixLineFeeds, std::size_t iMaxRead = npos);
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
/// Get size in bytes of a file, returns npos if file not found or is not a regular file
size_t kFileSize(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found or is not a regular file
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


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds a file type, constructs from different inputs
class KFileType
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum FileType
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

	KFileType() = default;

	/// constructs from a FileType
	KFileType(FileType ftype)
	: m_FType(ftype)
	{
	}

	/// constructs from a Unix mode combination
	KFileType(uint32_t mode);

	/// returns a string serialization of the file type
	KStringViewZ Serialize() const;

	operator FileType() const { return m_FType; }

//------
private:
//------

	enum FileType m_FType { ALL };

}; // KFileType


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Retrieve information about a file hat is typically found in the stat struct
class KFileStat
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct default KFileStat object
	KFileStat() = default;

	/// Construct KFileStat object on a file
	/// @param sFilename the file for which the status should be read
	KFileStat(const KStringViewZ sFilename);

	/// Returns file access mode
	int AccessMode()          const;

	/// Returns file's owner UID
	uint32_t UID()            const { return m_uid;   }

	/// Returns file's owner GID
	uint32_t GID()            const { return m_gid;   }

	/// Returns file's last access time
	time_t AccessTime()       const { return m_atime; }

	/// Returns file's last modification time
	time_t ModificationTime() const { return m_mtime; }

	/// Returns file's status change time (writes, but also inode changes)
	time_t ChangeTime()       const { return m_ctime; }

	/// Returns file's size
	size_t Size()             const { return m_size;  }

	/// Returns file's type
	KFileType Type()          const;

	/// Is this a directory entry?
	bool IsDirectory()        const;

	/// Is this a file?
	bool IsFile()             const;

	/// Is this a symlink
	bool IsSymlink()          const;

	/// Does this object exist?
	bool Exists()             const;

//----------
private:
//----------

	uint32_t m_mode  { 0 };
	uint32_t m_uid   { 0 };
	uint32_t m_gid   { 0 };
	time_t   m_atime { 0 };
	time_t   m_mtime { 0 };
	time_t   m_ctime { 0 };
	size_t   m_size  { 0 };

}; // KFileStat


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Retrieve and filter directory listings
class KDirectory
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	// deprecated name
	using EntryType = KFileType;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Helper type that keeps one directory entry, with its name, full path and type.
	/// Expands on request to a KFileStat.
	class DirEntry
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		DirEntry() : m_Type(KFileType::ALL) {}

		DirEntry(KStringView BasePath, KStringView Name, KFileType Type);

		bool operator<(const DirEntry& other) const
		{
			return m_Path < other.m_Path;
		}

		bool operator==(const DirEntry& other) const
		{
			return m_Path == other.m_Path && m_Type == other.m_Type;
		}

		/// returns full pathname (path + filename)
		operator const KString&() const
		{
			return Path();
		}

		/// returns full pathname (path + filename)
		const KString& Path() const
		{
			return m_Path;
		}

		/// returns filename only, no path component
		KStringViewZ Filename() const
		{
			return m_Filename;
		}

		/// returns directory entry type
		KFileType Type() const
		{
			return m_Type;
		}

		/// returns directory entry type as name
		KStringViewZ Serialize() const
		{
			return m_Type.Serialize();
		}

		/// Returns file access mode
		int Mode()                const
		{
			return FileStat().AccessMode();
		}

		/// Returns file's owner UID
		uint32_t UID()            const
		{
			return FileStat().UID();
		}

		/// Returns file's owner GID
		uint32_t GID()            const
		{
			return FileStat().GID();
		}

		/// Returns file's last access time
		time_t AccessTime()       const
		{
			return FileStat().AccessTime();
		}

		/// Returns file's last modification time
		time_t ModificationTime() const
		{
			return FileStat().ModificationTime();
		}

		/// Returns file's status change time
		time_t ChangeTime()       const
		{
			return FileStat().ChangeTime();
		}

		/// Returns file's size
		size_t Size()             const
		{
			return FileStat().Size();
		}

		/// Return the KFileStat component as a const ref
		const KFileStat& FileStat() const;

	//----------
	private:
	//----------

		mutable std::unique_ptr<KFileStat> m_Stat;
		KString      m_Path;
		KStringViewZ m_Filename;
		KFileType    m_Type;

	}; // DirEntry

	using DirEntries     = std::vector<DirEntry>;
	using iterator       = DirEntries::iterator;
	using const_iterator = DirEntries::const_iterator;

	/// default ctor
	KDirectory() = default;

	/// ctor that will open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Type the FileType to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	KDirectory(KStringViewZ sDirectory, KFileType Type = KFileType::ALL, bool bRecursive = false)
	{
		Open(sDirectory, Type, bRecursive, false);
	}

	/// returns const_iterator to the start of the directory list
	const_iterator cbegin() const { return m_DirEntries.begin(); }
	/// returns const_iterator to the end of the directory list
	const_iterator cend() const { return m_DirEntries.end(); }
	/// returns const_iterator to the begin of the directory list
	const_iterator begin() const { return m_DirEntries.begin(); }
	/// returns const_iterator to the end of the directory list
	const_iterator end() const { return m_DirEntries.end(); }
	/// returns iterator to the start of the directory list
	iterator begin() { return m_DirEntries.begin(); }
	/// returns iterator to the end of the directory list
	iterator end() { return m_DirEntries.end(); }

	/// count of matched directory entries
	size_t size() const { return m_DirEntries.size(); }

	/// did no directory entry match?
	bool empty() const { return m_DirEntries.empty(); }

	/// clear list of directory entries
	void clear();

	/// open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Type the FileType to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	/// @param bClear remove existing (previously found) directory entries, default = true
	size_t Open(KStringViewZ sDirectory, KFileType Type = KFileType::ALL, bool bRecursive = false, bool bClear = true);

	/// open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Type the FileType to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	/// @param bClear remove existing (previously found) directory entries, default = true
	size_t operator()(KStringViewZ sDirectory, KFileType Type = KFileType::ALL, bool bRecursive = false, bool bClear = true)
	{
		return Open(sDirectory, Type, bRecursive, bClear);
	}

	/// remove all hidden files, that is, files that start with a dot
	void RemoveHidden();

	/// match or remove all files that have FileType Type from the list, returns count of matched entries
	/// @param Type the FileType to search for
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_t Match(KFileType Type, bool bRemoveMatches = false);

	/// match or remove all files that match the regular expression sRegex from the list, returns count of matched entries
	/// @param sRegex the regular expression to search for
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_t Match(KStringView sRegex, bool bRemoveMatches = false);

	/// match or remove all files that match the basic regular expression sWildCard from the list, returns count of matched entries
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_t WildCardMatch(KStringView sWildCard, bool bRemoveMatches = false);

	/// returns first found directory entry if the directory list matches sWildCard, wildcard matching is supported
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	const_iterator Find(KStringView sWildCard) const;

	/// returns true if the directory list contains sWildCard, wildcard matching is supported
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	bool Contains(KStringView sWildCard) const
	{
		return Find(sWildCard) != end();
	}

	enum class SortBy
	{
		NAME,
		SIZE,
		DATE,
		UID,
		GID
	};

	/// sort the directory list
	void Sort(SortBy = SortBy::NAME, bool bReverse = false);

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

	/// Create temp directory
	/// @param bDeleteOnDestruction delete directory and contents on destruction?
	KTempDir (bool bDeleteOnDestruction = true)
	: m_bDeleteOnDestruction(bDeleteOnDestruction)
	{
	}

	~KTempDir ();

	/// get the name of the temp directory
	const KString& Name();

	/// do we have a temp directory?
	operator bool()
	{
		return !Name().empty();
	}

	operator const KString&()
	{
		return Name();
	}

	/// reset status to the one directly after construction, so,
	/// removes all files if bDeleteOnDestruction was set, and
	/// will create a new directory once Name() is called
	void clear();

//----------
private:
//----------

	/// create the temp folder now
	KString MakeDir();

	KString m_sTempDirName;
	bool m_bDeleteOnDestruction;

}; // KTempDir

} // end of namespace dekaf2

