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

#include "kdefinitions.h"
#include "kcompatibility.h"
#include "kstringview.h"
#include "kstring.h"
#include "kwriter.h"
#include "ktime.h"
#include "kerror.h"
#include <cinttypes>
#include <vector>

#ifdef DEKAF2_IS_UNIX
	#define DEKAF2_FILESTAT_USE_STAT 1
	#include <sys/stat.h>
#elif defined(DEKAF2_HAS_STD_FILESYSTEM)
	#define DEKAF2_FILESTAT_USE_STD_FILESYSTEM 1
#endif

DEKAF2_NAMESPACE_BEGIN

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
constexpr KStringViewZ kLineRightTrims { "\r\n" };
constexpr KStringViewZ kAllowedDirSep { "/" };
constexpr KStringViewZ kCurrentDir { "." };
constexpr KStringViewZ kCurrentDirWithSep { "./" };
}
#endif

namespace detail {
constexpr KStringView kUnsafeFileExtensionChars { "#%&{}<>*? $!'\":@+`|=\\/.-_" };
#ifdef DEKAF2_HAS_CPP_14
constexpr KStringView kUnsafeFilenameChars = kUnsafeFileExtensionChars.ToView(0, kUnsafeFileExtensionChars.size() - 3);
#else
constexpr KStringView kUnsafeFilenameChars { "#%&{}<>*? $!'\":@+`|=\\/" };
#endif
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
DEKAF2_PUBLIC
bool kChangeMode(KStringViewZ sPath, int iMode);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Query file or directory permissions. In case of failure 0 is returned (to
/// avoid accidentially detecting permission bits set if not checked for negative
/// value).
DEKAF2_NODISCARD DEKAF2_PUBLIC
int kGetMode(KStringViewZ sPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Change owner and group of a file system entity. Note that only the superuser is permitted to set the owner
/// to a value other than its own user,  and that ordinary users can only set a group they are member of.
/// @param sPath the pathname to change
/// @param iOwner the new owner for the pathname
/// @param iGroup the new group for the pathname
/// @return false on error, else true
DEKAF2_PUBLIC
bool kChangeOwnerAndGroup(KStringViewZ sPath, uid_t iOwner, uid_t iGroup);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Change group of a file system entity. Note that ordinary users can only set a group they are member of.
/// @param sPath the pathname to change
/// @param iGroup the new group for the pathname
/// @return false on error, else true
DEKAF2_PUBLIC
bool kChangeGroup(KStringViewZ sPath, uid_t iGroup);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a file system entity exists
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kExists (KStringViewZ sPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a file exists.
/// @param bTestForEmptyFile If true treats a file as non-existing if its size is 0
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kFileExists (KStringViewZ sPath, bool bTestForEmptyFile = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Checks if a directory exists
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kDirExists (KStringViewZ sPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// rename a file or directory, fails across file system boundaries (use kMove instead)
DEKAF2_PUBLIC
bool kRename (KStringViewZ sOldPath, KStringViewZ sNewPath);
//-----------------------------------------------------------------------------

enum class KCopyOptions
{
	None                  = 0,       ///< copy files, report an error on existing target files, skip subdirectories, 
	                                 ///< follow/resolve symlinks, set access mode to defaults, set owner to current owner
	// files
	SkipExistingFile      = 1 <<  0, ///< skip existing target files without reporting an error
	OverwriteExistingFile = 1 <<  1, ///< replace existing target files
	UpdateExistingFile    = 1 <<  2, ///< replace existing target files if they are older than the source files
	CreateMissingPath     = 1 <<  3, ///< create missing path components for target file (for single file copy)
	// directories
	Recursive             = 1 <<  4, ///< recursively copy directories
	// symlinks
	CopySymLinks          = 1 <<  5, ///< copy symlinks as symlinks, not as their target files
	SkipSymLinks          = 1 <<  6, ///< skip symlinks
	// general
	DirectoriesOnly       = 1 <<  7, ///< copy only directories
	CreateSymLinks        = 1 <<  8, ///< copy only directories, but create symlinks instead of files
	CreateHardLinks       = 1 <<  9, ///< copy only directories, but create hardlinks instead of files
	DeleteAfterCopy       = 1 << 10, ///< delete the input files after successful completion
	// access
	KeepMode              = 1 << 11, ///< copy access mode
	KeepOwner             = 1 << 12, ///< copy owner (needs superuser permissions), includes KeepGroup
	KeepGroup             = 1 << 13, ///< copy group (normal users must be member of the group to set)
	/// a reasonable default for tree copying: recursive, overwrite existing files, copy symlinks as symlinks, keep access mode
	Default = OverwriteExistingFile + Recursive + CopySymLinks + KeepMode
};

DEKAF2_ENUM_IS_FLAG(KCopyOptions)

//-----------------------------------------------------------------------------
/// copy a file
/// @param sOldPath the origin for the copy
/// @param sNewPath the new location for the copy
/// @param Options see KCopyOptions for the copy options.. defaults to KCopyOptions::Default
/// @return false on failure, true on success.
DEKAF2_PUBLIC
bool kCopyFile (KStringViewZ sOldPath, KStringViewZ sNewPath, KCopyOptions Options = KCopyOptions::Default);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// copy a file or directory. If a directory tree is copied, in case of partial failures the copied portion is
/// not removed.
/// @param sOldPath the origin for the copy
/// @param sNewPath the new location for the copy
/// @param Options see KCopyOptions for the copy options.. defaults to KCopyOptions::Default
/// @return false on (partial) failure, true on success.
DEKAF2_PUBLIC
bool kCopy (KStringViewZ sOldPath, KStringViewZ sNewPath, KCopyOptions Options = KCopyOptions::Default);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// move a file or directory, also across file system boundaries, keeps access modes and symlinks
DEKAF2_PUBLIC
bool kMove (KStringViewZ sOldPath, KStringViewZ sNewPath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Read entire text file into a single string and convert DOS newlines if
/// bToUnixLineFeeds is true. The base function (that is also called by this
/// variant) is kReadAll().
DEKAF2_PUBLIC
bool kReadTextFile (KStringViewZ sPath, KStringRef& sContents, bool bToUnixLineFeeds=true, std::size_t iMaxRead = npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// DEPRECATED, use kReadTextFile() in new code.
/// Read entire text file into a single string and convert DOS newlines if
/// bToUnixLineFeeds is true. The base function (that is also called by this
/// variant) is kReadAll().
DEKAF2_PUBLIC inline
bool kReadFile (KStringViewZ sPath, KStringRef& sContents, bool bToUnixLineFeeds=true, std::size_t iMaxRead = npos)
//-----------------------------------------------------------------------------
{
	return kReadTextFile(sPath, sContents, bToUnixLineFeeds, iMaxRead);
}

//-----------------------------------------------------------------------------
/// Read entire binary file into a single string. The base function (that is also called by this
/// variant) is kReadAll().
DEKAF2_PUBLIC
bool kReadBinaryFile (KStringViewZ sPath, KStringRef& sContents, std::size_t iMaxRead = npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create (or truncate) a file and write the given contents and then chmod the file if
/// perms are different than 0666 (the default)
DEKAF2_PUBLIC
bool kWriteFile (KStringViewZ sPath, KStringView sContents, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// If not existing, create a file. Append contents and close again.
DEKAF2_PUBLIC
bool kAppendFile (KStringViewZ sPath, KStringView sContents, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a directory (folder) hierarchically
DEKAF2_PUBLIC
bool kCreateDir (KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_DIR);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Alias for kCreateDir
DEKAF2_PUBLIC
inline bool kMakeDir (KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_DIR)
//-----------------------------------------------------------------------------
{
	return kCreateDir (sPath, iMode);
}

//-----------------------------------------------------------------------------
/// returns the target for a symbolic link
/// @param sSymlink the pathname of the symbolic link
/// @param bRealPath if true, the pathname of the fully resolved chain of symlinks is returned,
/// if false (the default), only the first symlink will be resolved
DEKAF2_PUBLIC
KString kReadLink(KStringViewZ sSymlink, bool bRealPath = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a symbolic link
/// @param sOrigin the existing file
/// @param sSymlink the symbolic link to create (may cross file system boundaries)
DEKAF2_PUBLIC
bool kCreateSymlink(KStringViewZ sOrigin, KStringViewZ sSymlink);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a hard link
/// @param sOrigin the existing file
/// @param sHardlink the hard link to create (may not cross file system boundaries)
DEKAF2_PUBLIC
bool kCreateHardlink(KStringViewZ sOrigin, KStringViewZ sHardlink);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a file if it does not exist, including the directory component.
/// If the file exists, advance its last mod timestamp.
DEKAF2_PUBLIC
bool kTouchFile(KStringViewZ sPath, int iMode = DEKAF2_MODE_CREATE_FILE);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the extension of a path (filename extension after a dot)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kExtension(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove the extension from a path (filename extension after a dot)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kRemoveExtension(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the basename of a path (filename without directory)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kBasename(KStringView sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Isolate the dirname of a path (directory name without the fileame)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kDirname(KStringView sFilePath, bool bWithTrailingSlash = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get last modification time of a file, returns -1 if file not found
DEKAF2_NODISCARD DEKAF2_PUBLIC
KUnixTime kGetLastMod(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found or is not a regular file
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFileSize(KStringViewZ sFilePath);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get size in bytes of a file, returns npos if file not found or is not a regular file
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline size_t kGetNumBytes(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
	return kFileSize(sFilePath);
}

//-----------------------------------------------------------------------------
/// Returns true if a file name is safe to use, means it cannot escape from a
/// directory or uses problematic characters
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsSafeFilename(KStringView sName);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns a file name that is safe to use, means it cannot escape from a
/// directory or uses problematic characters. The input string is the base for
/// the file name
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kMakeSafeFilename(KStringView sName, bool bToLowercase = true, KStringView sEmptyName = "none");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if a path name is safe to use, means it cannot escape from a
/// directory or uses problematic characters
/// @param sName the path to check
/// @param bAllowAbsolutePath if false, only relative paths are allowed (= no leading slash)
/// @param bAllowTrailingSlash if true, a directory separator as last character is permitted
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsSafePathname(KStringView sName, bool bAllowAbsolutePath = false, bool bAllowTrailingSlash = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns a path name that is safe to use, means it cannot escape from a
/// directory or uses problematic characters. The input string is the base for
/// the path name
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kMakeSafePathname(KStringView sName, bool bToLowercase = true, KStringView sEmptyName = "none");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// resolve .. and . parts of the input path, and make it an absolute path - WARNING: relative paths get the
/// current directory inserted as a prefix BEFORE resolution of .. if bKeepRelative is false
/// @param bKeepRelative if true, a relative path will not be expanded to the current directory, excess
/// ../ will thus not permit to break out of the base location. Default is false though.
/// @param bThrowOnError if true, instead of silently skipping excess ../ throw an error
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kNormalizePath(KStringView sPath, bool bKeepRelative = false);
//-----------------------------------------------------------------------------

class KFileTypes;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds one file type, constructs from different inputs
class DEKAF2_PUBLIC KFileType
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum FileType : uint8_t
	{
		UNEXISTING = 0,
		FILE       = 1 << 0,
		DIRECTORY  = 1 << 1,
		SYMLINK    = 1 << 2,
		PIPE       = 1 << 3,
		BLOCK      = 1 << 4,
		CHARACTER  = 1 << 5,
		SOCKET     = 1 << 6,
		UNKNOWN    = 1 << 7
	};

	static constexpr uint8_t MAX = UNKNOWN;

	constexpr
	KFileType() = default;

	/// constructs from a FileType
	constexpr
	KFileType(FileType ftype)
	: m_FType(ftype)
	{
	}

	/// returns a string serialization of the file type
	DEKAF2_NODISCARD
	KStringViewZ Serialize() const;

	constexpr
	operator FileType() const { return m_FType; }

	friend constexpr KFileTypes operator|(const KFileType first, const KFileType second);
	friend constexpr KFileTypes operator|(const FileType  first, const FileType  second);

//------
private:
//------

	FileType m_FType { UNEXISTING };

}; // KFileType

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds a mask of file types, constructs from different inputs
class DEKAF2_PUBLIC KFileTypes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	constexpr
	KFileTypes() = default;

	/// constructs from a KFileType
	constexpr KFileTypes(KFileType ftype)
	: m_Types(ftype)
	{
	}

	/// constructs from a KFileType::FileType
	constexpr KFileTypes(KFileType::FileType ftype)
	: m_Types(ftype)
	{
	}

	constexpr
	void push_back(KFileType ftype)
	{
		m_Types = KFileType(static_cast<KFileType::FileType>(static_cast<uint8_t>(m_Types) | static_cast<uint8_t>(ftype)));
	}

	constexpr
	void operator|=(KFileType ftype)
	{
		push_back(ftype);
	}

	DEKAF2_NODISCARD constexpr
	bool contains(KFileType ftype) const
	{
		return (m_Types & ftype);
	}

	/// returns a vector with string serializations of the file types - join them for single output
	DEKAF2_NODISCARD
	std::vector<KStringViewZ> Serialize() const;

	static const KFileTypes ALL;

	friend constexpr KFileTypes operator|(KFileTypes first, const KFileTypes second);

//------
private:
//------

	KFileType m_Types { KFileType::UNEXISTING };

}; // KFileTypes

constexpr DEKAF2_PUBLIC
KFileTypes operator|(const KFileType::FileType first, const KFileType::FileType second)
{
	return KFileType(static_cast<KFileType::FileType>(static_cast<uint8_t>(first) | static_cast<uint8_t>(second)));
}

constexpr DEKAF2_PUBLIC
KFileTypes operator|(const KFileType first, const KFileType second)
{
	return first.m_FType | second.m_FType;
}

constexpr DEKAF2_PUBLIC
KFileTypes operator|(KFileTypes first, const KFileType second)
{
	first.push_back(second);
	return first;
}

constexpr DEKAF2_PUBLIC
KFileTypes operator|(KFileTypes first, const KFileType::FileType second)
{
	first.push_back(second);
	return first;
}

constexpr DEKAF2_PUBLIC
KFileTypes operator|(KFileTypes first, const KFileTypes second)
{
	first.push_back(second.m_Types);
	return first;
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Retrieve information about a file that is typically found in the stat struct
class DEKAF2_PUBLIC KFileStat : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct default KFileStat object
	KFileStat() = default;

	/// Construct KFileStat object on a file
	/// @param sFilename the file for which the status should be read
	/// @param bDetectSymlinks if true, symbolic links will be detected, if false (the default),
	/// the target type of a symbolic link will be detected
	KFileStat(const KStringViewZ sFilename, bool bDetectSymlinks = false);

#ifdef DEKAF2_FILESTAT_USE_STAT
	/// Construct KFileStat object on a file handle
	/// @param iFileDescriptor the file handle for which the status should be read
	KFileStat(int iFileDescriptor);

	/// Construct KFileStat object on a struct stat
	/// @param StatStruct the stat struct from which the status should be read
	KFileStat(struct stat& StatStruct) { FromStat(StatStruct, false); }
#endif

	/// Returns inode number (only on supported file systems, otherwise 0)
	DEKAF2_NODISCARD
	uint64_t Inode()             const { return m_inode; }

	/// Returns inode link count (only on supported file systems, otherwise 0)
	DEKAF2_NODISCARD
	uint16_t Links()             const { return m_links; }

	/// Returns file access mode
	DEKAF2_NODISCARD
	int AccessMode()             const { return m_mode;  }

	/// Returns file's owner UID
	DEKAF2_NODISCARD
	uint32_t UID()               const { return m_uid;   }

	/// Returns file's owner GID
	DEKAF2_NODISCARD
	uint32_t GID()               const { return m_gid;   }

	/// Returns file's last access time
	DEKAF2_NODISCARD
	KUnixTime AccessTime()       const { return m_atime; }

	/// Returns file's last modification time
	DEKAF2_NODISCARD
	KUnixTime ModificationTime() const { return m_mtime; }

	/// Returns file's status change time (writes, but also inode changes)
	DEKAF2_NODISCARD
	KUnixTime ChangeTime()       const { return m_ctime; }

	/// Returns file's size
	DEKAF2_NODISCARD
	size_t Size()                const { return m_size;  }

	/// Returns file's type
	DEKAF2_NODISCARD
	KFileType Type()             const { return m_ftype; }

	/// Is this a directory entry?
	DEKAF2_NODISCARD
	bool IsDirectory()           const { return m_ftype == KFileType::DIRECTORY;  }

	/// Is this a file?
	DEKAF2_NODISCARD
	bool IsFile()                const { return m_ftype == KFileType::FILE;       }

	/// Is this a symlink
	DEKAF2_NODISCARD
	bool IsSymlink()             const { return m_ftype == KFileType::SYMLINK;    }

	/// Is this a socket
	DEKAF2_NODISCARD
	bool IsSocket()              const { return m_ftype == KFileType::SOCKET;     }

	/// Does this object exist?
	DEKAF2_NODISCARD
	bool Exists()                const { return m_ftype != KFileType::UNEXISTING; }

	// setters to create a KFileStat object piece wise
	// (no change to any file system object):

	/// Sets all values to reasonable defaults for the current user if not already set
	KFileStat& SetDefaults();

	/// Set Inode number
	void SetInode(uint64_t inode)             { m_inode = inode; }

	/// Set Inode link count
	void SetLinks(uint16_t links)             { m_links = links; }

	/// Set file access mode
	void SetAccessMode(uint32_t mode)         { m_mode  = mode;  }

	/// Set file's owner UID
	void SetUID(uint32_t uid)                 { m_uid   = uid;   }

	/// Set file's owner GID
	void SetGID(uint32_t gid)                 { m_gid   = gid;   }

	/// Set file's last access time
	void SetAccessTime(KUnixTime atime)       { m_atime = atime; }

	/// Set file's last modification time
	void SetModificationTime(KUnixTime mtime) { m_mtime = mtime; }

	/// Set file's status change time (writes, but also inode changes)
	void SetChangeTime(KUnixTime ctime)       { m_ctime = ctime; }

	/// Set file's size
	void SetSize(std::size_t size)            { m_size  = size;  }

	/// Set file's type
	void SetType(KFileType ftype)             { m_ftype = ftype; }

//----------
private:
//----------

#ifdef DEKAF2_FILESTAT_USE_STAT
	DEKAF2_PRIVATE
	void FromStat(struct stat& StatStruct, bool bAddForSymlinks);
#endif

	uint64_t    m_inode { 0 };
	KUnixTime   m_atime { 0 };
	KUnixTime   m_mtime { 0 };
	KUnixTime   m_ctime { 0 };
	std::size_t m_size  { 0 };
	uint32_t    m_mode  { 0 };
	uint32_t    m_uid   { 0 };
	uint32_t    m_gid   { 0 };
	uint16_t    m_links { 0 };
	KFileType   m_ftype;

}; // KFileStat

//-----------------------------------------------------------------------------
/// Remove (unlink) a file or directory tree, matching Types (for the first file)
DEKAF2_PUBLIC
bool kRemove (KStringViewZ sPath, KFileTypes Types);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Remove (unlink) a file
DEKAF2_PUBLIC
inline bool kRemoveFile (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, KFileType::FILE | KFileType::SYMLINK);
}

//-----------------------------------------------------------------------------
/// Remove (unlink) a directory (folder) hierarchically
DEKAF2_PUBLIC
inline bool kRemoveDir (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, KFileType::DIRECTORY);
}

//-----------------------------------------------------------------------------
/// Remove (unlink) a unix socket
DEKAF2_PUBLIC
inline bool kRemoveSocket (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	return kRemove (sPath, KFileType::SOCKET);
}

//-----------------------------------------------------------------------------
/// open a file with the name sOrigName + . + sBackupExtension, so that all existing files with the same name are
/// renamed with an additional sBackupExtension. The open file will have the same flags as the original.
/// @return unique pointer with open file, or nullptr in case of error
DEKAF2_NODISCARD extern DEKAF2_PUBLIC
std::unique_ptr<KOutFile> kCreateFileWithBackup(KStringView sOrigname, KStringView sBackupExtension = "old");
//-----------------------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Retrieve and filter directory listings
class DEKAF2_PUBLIC KDirectory : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Helper type that keeps one directory entry, with its name, full path and type.
	/// Expands on request to a KFileStat.
	class DirEntry
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		DirEntry() = default;

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
		DEKAF2_NODISCARD
		const KString& Path() const
		{
			return m_Path;
		}

		/// returns filename only, no path component
		DEKAF2_NODISCARD
		KStringViewZ Filename() const
		{
			return m_Path.ToView(m_iFilenameStartsAt);
		}

		/// returns directory entry type
		DEKAF2_NODISCARD
		KFileType Type() const
		{
			return m_Type;
		}

		/// returns directory entry type as name
		DEKAF2_NODISCARD
		KStringViewZ Serialize() const
		{
			return m_Type.Serialize();
		}

		/// Returns file access mode
		DEKAF2_NODISCARD
		int Mode()                const
		{
			return FileStat().AccessMode();
		}

		/// Returns file's owner UID
		DEKAF2_NODISCARD
		uint32_t UID()            const
		{
			return FileStat().UID();
		}

		/// Returns file's owner GID
		DEKAF2_NODISCARD
		uint32_t GID()            const
		{
			return FileStat().GID();
		}

		/// Returns file's last access time
		DEKAF2_NODISCARD
		KUnixTime AccessTime()    const
		{
			return FileStat().AccessTime();
		}

		/// Returns file's last modification time
		DEKAF2_NODISCARD
		KUnixTime ModificationTime() const
		{
			return FileStat().ModificationTime();
		}

		/// Returns file's status change time
		DEKAF2_NODISCARD
		KUnixTime ChangeTime()    const
		{
			return FileStat().ChangeTime();
		}

		/// Returns file's size
		DEKAF2_NODISCARD
		size_t Size()             const
		{
			return FileStat().Size();
		}

		/// Returns file's inode number (only on supported file systems, otherwise 0)
		DEKAF2_NODISCARD
		uint64_t Inode()          const
		{
			return FileStat().Inode();
		}

		/// Returns file's inode link count (only on supported file systems, otherwise 0)
		DEKAF2_NODISCARD
		uint64_t Links()          const
		{
			return FileStat().Links();
		}

		/// Return the KFileStat component as a const ref
		DEKAF2_NODISCARD
		const KFileStat& FileStat() const;

	//----------
	private:
	//----------

		// needed to avoid MT races for KDirectory on operator[] when running
		// multiple KDirectory at the same time with empty path names for the
		// first time
		static const KFileStat s_EmptyStat;

		mutable std::unique_ptr<KFileStat> m_Stat;
		KString            m_Path;
		KString::size_type m_iFilenameStartsAt { 0 };
		KFileType          m_Type { KFileType::UNEXISTING };

	}; // DirEntry

	using DirEntries     = std::vector<DirEntry>;
	using const_iterator = DirEntries::const_iterator;
	using size_type      = DirEntries::size_type;

	/// default ctor
	KDirectory() = default;

	/// ctor that will open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Types the FileTypes to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	KDirectory(KStringViewZ sDirectory, KFileTypes Types = KFileTypes::ALL, bool bRecursive = false)
	{
		Open(sDirectory, Types, bRecursive, false);
	}

	/// returns const_iterator to the start of the directory list
	DEKAF2_NODISCARD
	const_iterator cbegin() const { return m_DirEntries.begin(); }
	/// returns const_iterator to the end of the directory list
	DEKAF2_NODISCARD
	const_iterator cend() const { return m_DirEntries.end(); }
	/// returns const_iterator to the begin of the directory list
	DEKAF2_NODISCARD
	const_iterator begin() const { return m_DirEntries.begin(); }
	/// returns const_iterator to the end of the directory list
	DEKAF2_NODISCARD
	const_iterator end() const { return m_DirEntries.end(); }

	/// count of matched directory entries
	DEKAF2_NODISCARD
	size_type size() const { return m_DirEntries.size(); }

	/// did no directory entry match?
	DEKAF2_NODISCARD
	bool empty() const { return m_DirEntries.empty(); }

	/// clear list of directory entries
	void clear();

	/// get the directory enty at position pos, never throws, returns empty entry if out of bounds
	DEKAF2_NODISCARD
	const DirEntry& at(size_type pos) const noexcept
	{
		return (pos < size()) ? m_DirEntries[pos] : s_Empty;
	}

	/// get the directory enty at position pos, never throws, returns empty entry if out of bounds
	DEKAF2_NODISCARD
	const DirEntry& operator[](size_type pos) const noexcept
	{
		return at(pos);
	}

	/// open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Types the FileTypes to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	/// @param bClear remove existing (previously found) directory entries, default = true
	size_type Open(KStringViewZ sDirectory, KFileTypes Types = KFileTypes::ALL, bool bRecursive = false, bool bClear = true);

	/// open a directory and store all entries that are of FileType Type
	/// @param sDirectory the direcory path to open
	/// @param Types the FileTypes to search for, default = ALL
	/// @param bRecursive traverse subdirectories recursively, default = false
	/// @param bClear remove existing (previously found) directory entries, default = true
	size_type operator()(KStringViewZ sDirectory, KFileTypes Types = KFileTypes::ALL, bool bRecursive = false, bool bClear = true)
	{
		return Open(sDirectory, Types, bRecursive, bClear);
	}

	/// remove all hidden files, that is, files that start with a dot
	void RemoveHidden();

	/// remove all Apple Resource Fork files, that is, files that start with a dot and an underscore
	void RemoveAppleResourceFiles();

	/// match or remove all files that have FileTypes Types from the list, returns count of matched entries
	/// @param Types the FileTypes to search for
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_type Match(KFileTypes Types, bool bRemoveMatches = false);

	/// match or remove all files that match the regular expression sRegex from the list, returns count of matched entries
	/// @param sRegex the regular expression to search for
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_type Match(KStringView sRegex, bool bRemoveMatches = false);

	/// match or remove all files that match the basic regular expression sWildCard from the list, returns count of matched entries
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	/// @param bRemoveMatches if true remove matches, else keep only those (default = false)
	size_type WildCardMatch(KStringView sWildCard, bool bRemoveMatches = false);

	/// returns first found directory entry if the directory list matches sWildCard, wildcard matching is supported
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	DEKAF2_NODISCARD
	const_iterator Find(KStringView sWildCard) const;

	/// returns true if the directory list contains sWildCard, wildcard matching is supported
	/// @param sWildCard the basic regular expression to search for (e.g. "*.tx?")
	DEKAF2_NODISCARD
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

//----------
private:
//----------

	static const DirEntry s_Empty;

}; // KDirectory


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Get disk capacity
class DEKAF2_PUBLIC KDiskStat : public KErrorBase
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
	DEKAF2_NODISCARD
	uint64_t Total() const { return m_Total; }

	/// Return count of used bytes on file system
	DEKAF2_NODISCARD
	uint64_t Used() const { return m_Used; }

	/// Return count of free bytes on file system for non-priviledged user
	DEKAF2_NODISCARD
	uint64_t Free() const { return m_Free; }

	/// Return count of free bytes on file system for priviledged user
	DEKAF2_NODISCARD
	uint64_t SystemFree() const { return m_SystemFree; }

	/// Returns false if an error occured
	operator bool() const { return HasError(); }

	/// Reset to default
	void clear();

//----------
private:
//----------

	uint64_t m_Free       { 0 };
	uint64_t m_Total      { 0 };
	uint64_t m_Used       { 0 };
	uint64_t m_SystemFree { 0 };

}; // KDiskStat


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// generate a temp directory, and remove it with the destructor if requested to
class DEKAF2_PUBLIC KTempDir
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
	DEKAF2_NODISCARD
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

	/// create the temp folder - static method
	DEKAF2_NODISCARD
	static KString MakeDir();

//----------
private:
//----------

	KString m_sTempDirName;
	bool m_bDeleteOnDestruction;

}; // KTempDir

DEKAF2_NAMESPACE_END
