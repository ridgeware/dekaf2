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

#include <algorithm>
#include "dekaf2.h"
#include "bits/kfilesystem.h"
#include "bits/kcppcompat.h"
#include "kfilesystem.h"
#include "ksystem.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kstringview.h"
#include "klog.h"
#include "kregex.h"
#include "ksplit.h"
#include "kinshell.h"
#include "kwriter.h"
#include "kctype.h"
#include "kutf8.h"

#if (DEKAF2_IS_GCC && DEKAF2_GCC_MAJOR_VERSION < 10)
#include <sys/stat.h>
#endif



namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kChangeMode(KStringViewZ sPath, int iMode)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	fs::permissions(kToFilesystemPath(sPath), static_cast<fs::perms>(iMode), ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sPath, ec.message());
		return false;
	}

#else
	if (chmod(sPath.c_str(), iMode))
	{
		kDebug (1, "{}: {}", sPath, strerror (errno));
		return false;
	}
#endif

	return true;

}

//-----------------------------------------------------------------------------
int kGetMode(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	int mode = static_cast<int>(fs::status(kToFilesystemPath(sPath), ec).permissions());

	if (ec)
	{
		kDebug(1, "{}: {}", sPath, ec.message());
		mode = 0;
	}

	return mode;

#else

	struct stat StatStruct;

	if (stat (sPath.c_str(), &StatStruct) < 0)
	{
		kDebug(1, "{}: {}", sPath, strerror(errno));
		return 0;
	}

	return StatStruct.st_mode;

#endif

} // kGetMode

//-----------------------------------------------------------------------------
bool kExists (KStringViewZ sPath, bool bAsFile, bool bAsDirectory, bool bTestForEmptyFile)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	fs::file_status status = fs::status(kToFilesystemPath(sPath), ec);

	if (ec)
	{
		kDebug (3, "{}: {}", sPath, ec.message());
		return false;
	}

	if (!fs::exists(status))
	{
		return false;
	}

	if (bAsFile)
	{
		if (fs::is_directory(status))
		{
			kDebug (3, "entry exists, but is not a file: {}", sPath);
			return false;
		}
	}
	else if (bAsDirectory)
	{
		if (!fs::is_directory(status))
		{
			kDebug (3, "entry exists, but is not a directory: {}", sPath);
			return false;
		}
		else
		{
			return true;
		}
	}

	if (!bTestForEmptyFile)
	{
		// exists and is a file
		return true;
	}

	if (fs::is_empty(kToFilesystemPath(sPath), ec))
	{
		kDebug (3, "entry exists, but is empty: {}", sPath);
		return false;
	}

	if (ec)
	{
		kDebug (3, ec.message());
		return false;
	}

	return true;

#else

	struct stat StatStruct;
	if (stat (sPath.c_str(), &StatStruct) < 0)
	{
		return false;  // <-- file/dir doesn't exist
	}

	if (bAsFile)
	{
		if ((StatStruct.st_mode & S_IFREG) == 0)
		{
			kDebug (3, "entry exists, but is not a file: {}", sPath);
			return false;   // <-- exists, but is not a file
		}
	}
	else if (bAsDirectory)
	{
		if ((StatStruct.st_mode & S_IFDIR) == 0)
		{
			kDebug (3, "entry exists, but is not a directory: {}", sPath);
			return false;   // <-- exists, but is not a directory
		}
		return true;
	}

	if (!bTestForEmptyFile)
	{
		return true;    // <-- exists and is a file
	}

	if (StatStruct.st_size <= 0)
	{
		kDebug (3, "entry exists, but is empty: {}", sPath);
		return false;    // <-- exists, is a file but is zero length
	}

	return true;     // <-- exists, is a file and is non-zero length

#endif

} // kExists

//-----------------------------------------------------------------------------
KStringView kExtension(KStringView sFilePath)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, return the file extension
	if (!sFilePath.empty())
	{
#ifdef _WIN32
		auto pos = sFilePath.find_last_of("/\\:.");
#else
		auto pos = sFilePath.find_last_of("/.");
#endif

		if (pos != KStringView::npos && sFilePath[pos] == '.')
		{
			// a file like ".hidden" or "tmp/.hidden" is not an extension
			if (pos > 0 && detail::kAllowedDirSep.find(sFilePath[pos-1]) == KStringView::npos)
			{
				return sFilePath.substr(pos + 1);
			}
		}
	}

	return KStringView{};

} // kExtension

//-----------------------------------------------------------------------------
KStringView kRemoveExtension(KStringView sFilePath)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, remove the file extension
	if (!sFilePath.empty())
	{
#ifdef _WIN32
		auto pos = sFilePath.find_last_of("/\\:.");
#else
		auto pos = sFilePath.find_last_of("/.");
#endif

		if (pos != KStringView::npos && sFilePath[pos] == '.')
		{
			// a file like ".hidden" or "tmp/.hidden" is not an extension
			if (pos > 0 && detail::kAllowedDirSep.find(sFilePath[pos-1]) == KStringView::npos)
			{
				return sFilePath.substr(0, pos);
			}
		}
	}

	return sFilePath;

} // kRemoveExtension

//-----------------------------------------------------------------------------
KStringView kBasename(KStringView sFilePath)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, return the "basename":
	// that is, just the filename itself without any directory elements
	if (!sFilePath.empty())
	{
		auto pos = sFilePath.find_last_of(detail::kAllowedDirSep);

		if (pos != KStringView::npos)
		{
			return sFilePath.substr(pos + 1);
		}
	}

	return sFilePath;

} // kBasename

//-----------------------------------------------------------------------------
KStringView kDirname(KStringView sFilePath, bool bWithTrailingSlash)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, return the "dirname":
	// that is, the directory name component of the file path
	if (!sFilePath.empty())
	{
		auto pos = sFilePath.find_last_of(detail::kAllowedDirSep);

		if (pos != KStringView::npos)
		{
			return sFilePath.substr(0, (bWithTrailingSlash) ? pos + 1 : pos);
		}
	}

	return (bWithTrailingSlash) ? detail::kCurrentDirWithSep : detail::kCurrentDir;

}  // kDirname()

//-----------------------------------------------------------------------------
/// rename a file or directory
bool kRename (KStringViewZ sOldPath, KStringViewZ sNewPath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	fs::rename(kToFilesystemPath(sOldPath), kToFilesystemPath(sNewPath), ec);

	if (ec)
	{
		kDebug(2, ec.message());
		return false;
	}

	return true;

#else

	if (std::rename(sOldPath.c_str(), sNewPath.c_str()))
	{
		kDebug (1, "failed: {} > {}: {}", sOldPath, sNewPath, strerror (errno));
		return false;
	}

	return true;

#endif

} // kRename

//-----------------------------------------------------------------------------
bool kRemove (KStringViewZ sPath, bool bDir)
//-----------------------------------------------------------------------------
{
	if (bDir)
	{
		if (kFileExists (sPath))
		{
			kDebug (1, "cannot remove file: {}", sPath);
			return false;
		}

		if (!kDirExists (sPath))
		{
			return true;
		}
	}
	else
	{
		if (!kFileExists (sPath))
		{
			return true;
		}
	}

#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;
	fs::path Path(kToFilesystemPath(sPath));
	fs::permissions (Path, fs::perms::all, ec); // chmod (ignore failures)
	ec.clear();

	if (bDir)
	{
		fs::remove_all (Path, ec);
	}
	else
	{
		fs::remove (Path, ec);
	}
	if (ec)
	{
		kDebug (1, "failed: {}: {}", sPath, ec.message());
		return false;
	}

#else

	if (unlink (sPath.c_str()) != 0)
	{
		kChangeMode (sPath, S_IRUSR|S_IWUSR|S_IXUSR | S_IRGRP|S_IWGRP|S_IXGRP | S_IROTH|S_IWOTH|S_IXOTH);

		if (unlink (sPath.c_str()) != 0)
		{
			if (bDir && (rmdir (sPath.c_str()) != 0))
			{
				KString sCmd;
				sCmd.Format ("rm -rf \"{}\"", sPath);
				if (system (sCmd.c_str()) == 0)
				{
					return (true);
				}
			}

			kDebug (1, "failed: {}: {}", sPath, strerror (errno));
			return false;
		}
	}

#endif

	return (true);

} // kRemove

//-----------------------------------------------------------------------------
bool kCreateDir(KStringViewZ sPath, int iMode /* = DEKAF2_MODE_CREATE_DIR */)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	if (!sPath.empty() && (sPath.back() == '/'
#ifdef DEKAF2_IS_WINDOWS
						   || sPath.back() == '\\'
#endif
		))
	{
		// unfortunately fs::create_directories chokes on a
		// trailing slash, so we copy the KStringViewZ if it
		// has one and remove it from the copy
		KStringView sTmp = sPath;
		sTmp.remove_suffix(1);
		if (fs::create_directories(kToFilesystemPath(sTmp), ec))
		{
			// TODO set permissions if iMode != 0777, check ec instead of
			// fs::create_directories() return value (is false on existing
			// dir)
			return true;
		}
	}
	else
	{
		if (fs::create_directories(kToFilesystemPath(sPath), ec))
		{
			// TODO see above
			return true;
		}
	}

	if (ec)
	{
		kDebug(2, "{}: {}", sPath, ec.message());
	}
	else
	{
		// this directory was already existing..
		return true;
	}

	return false;

#else

	if (sPath.empty())
	{
		return false;
	}

	if (kDirExists(sPath))
	{
		return true;
	}

	KString sNewPath;

	// else test each part of the directory chain
	for (const auto& it : sPath.Split(detail::kAllowedDirSep, "", 0, true, false))
	{
		if (!it.empty())
		{
			if (sNewPath.empty())
			{
				if (sPath.front() == '/'
#ifdef DEKAF2_IS_WINDOWS
					|| sPath.front() == '\\'
#endif
					)
				{
					sNewPath += kDirSep;
				}
			}
			else
			{
				sNewPath += kDirSep;
			}
			sNewPath += it;

			if (!kDirExists(sNewPath))
			{
				if (::mkdir(sNewPath.c_str(), iMode))
				{
					kDebug(2, "{}: {}", sPath, strerror(errno));
					return false;
				}
			}
		}
	}

	return true;

#endif

} // kCreateDir

//-----------------------------------------------------------------------------
bool kTouchFile(KStringViewZ sPath, int iMode /* = DEKAF2_MODE_CREATE_FILE */)
//-----------------------------------------------------------------------------
{
	// we need to use a KOutFile here, as FILE* on Windows does not understand
	// UTF8 file names
	KOutFile File(sPath, std::ios::app);

	if (!File.is_open())
	{
		// else we may miss a path component
		KString sDir = kDirname(sPath);
		if (kCreateDir(sDir))
		{
			File.open(sPath, std::ios::app);
			if (!File.is_open())
			{
				// give up
				return false;
			}

			File.close();

			if (iMode != DEKAF2_MODE_CREATE_FILE)
			{
				return kChangeMode(sPath, iMode);
			}
		}
		else
		{
			return false;
		}
	}

	return true;

} // kTouchFile

//-----------------------------------------------------------------------------
time_t kGetLastMod(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_HAS_STD_FILESYSTEM) && (DEKAF2_NO_GCC || DEKAF2_GCC_MAJOR_VERSION > 9)

	std::error_code ec;

	auto ftime = fs::last_write_time(kToFilesystemPath(sFilePath), ec);

	if (ec)
	{
		kDebug(2, "{}: {}", sFilePath, ec.message());
		return -1;
	}

#if defined(DEKAF2_IS_WINDOWS) && !defined(DEKAF2_IS_CPP_20)

	// unfortunately windows uses its own filetime ticks (100 nanoseconds since 1.1.1601)
	// this will change with C++20!
	static constexpr uint64_t WINDOWS_TICK = 10000000;
	static constexpr uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;
	return (ftime.time_since_epoch().count() / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);

#else

	return decltype(ftime)::clock::to_time_t(ftime);

#endif

#else

	struct stat StatStruct;

	if (stat (sFilePath.c_str(), &StatStruct) < 0)
	{
		return -1;  // <-- file doesn't exist
	}

	return StatStruct.st_mtime;

#endif

} // kGetLastMod

//-----------------------------------------------------------------------------
size_t kFileSize(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	auto fsPath = kToFilesystemPath(sFilePath);

	auto size = fs::file_size(fsPath, ec);

	if (ec)
	{
		kDebug(2, "{}: {}", sFilePath, ec.message());
		return npos;
	}

	auto status = fs::status(fsPath, ec);

	if (ec)
	{
		kDebug (2, "{}: {}", sFilePath, ec.message());
		return npos;
	}

	if (fs::is_directory(status))
	{
		return npos;
	}

	return size;

#else

	struct stat StatStruct;

	if (stat (sFilePath.c_str(), &StatStruct) < 0
		|| (StatStruct.st_mode & S_IFREG) == 0)
	{
		return npos;  // <-- file doesn't exist or is not a regular file
	}

	return StatStruct.st_size;
	
#endif

} // kFileSize


#ifdef DEKAF2_IS_UNIX
	#define DEKAF2_FILESTAT_USE_STAT
#elif DEKAF2_HAS_STD_FILESYSTEM
	#define DEKAF2_FILESTAT_USE_STD_FILESYSTEM
	#ifndef S_IFMT
		#define S_IFMT      0170000
		#define S_IFIFO     0010000
		#define S_IFCHR     0020000
		#define S_IFDIR     0040000
		#define S_IFBLK     0060000
		#define S_IFREG     0100000
		#define S_IFLNK     0120000
		#define S_IFSOCK    0140000
	#endif
	#ifndef S_ISREG
		#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
		#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
		#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
		#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
		#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
		#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
		#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
	#endif
#endif

//-----------------------------------------------------------------------------
KFileStat::KFileStat(const KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_FILESTAT_USE_STAT

	// use the good ole stat() system call, it fetches all information with one call

	struct stat StatStruct;

	if (!stat(sFilename.c_str(), &StatStruct))
	{
		m_mode  = StatStruct.st_mode;
		m_uid   = StatStruct.st_uid;
		m_gid   = StatStruct.st_gid;
		m_atime = StatStruct.st_atime;
		m_mtime = StatStruct.st_mtime;
		m_ctime = StatStruct.st_ctime;
		m_size  = StatStruct.st_size;
	}

#elif DEKAF2_FILESTAT_USE_STD_FILESYSTEM

	// windows would have issues with utf8 file names, therefore use the
	// std::filesystem interface (which however needs multiple calls)

	std::error_code ec;
	auto fsPath = kToFilesystemPath(sPath);

	auto status = fs::status(fsPath, ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sPath, ec.message());
		m_mode = 0;
	}
	else
	{
		auto ftype = status.type();

		switch (ftype)
		{
			fs::file_type::regular:
				m_mode = S_IFREG;
				break;

			fs::file_type::directory:
				m_mode = S_IFDIR;
				break;

			fs::file_type::symlink:
				m_mode = S_IFLNK;
				break;

	#ifdef DEKAF2_HAS_CPP_17
			fs::file_type::block:
				m_mode = S_IFREG;
				break;
	#endif
			fs::file_type::character:
				m_mode = S_IFCHR;
				break;

			fs::file_type::fifo:
				m_mode = S_IFIFO;
				break;

			fs::file_type::socket:
				m_mode = S_IFSOCK;
				break;

			default:
				m_mode = 0;
				break;
		}

		m_mode |= static_cast<int>(status.permissions());
	}

	m_size = fs::file_size(fsPath, ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sPath, ec.message());
		m_size = 0;
	}

	auto ftime = fs::last_write_time(fsPath, ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sPath, ec.message());
	}
	else
	{
#if defined(DEKAF2_IS_WINDOWS) && !defined(DEKAF2_IS_CPP_20)

		// unfortunately windows uses its own filetime ticks (100 nanoseconds since 1.1.1601)
		// this will change with C++20!
		static constexpr uint64_t WINDOWS_TICK = 10000000;
		static constexpr uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;
		time_t stime = (ftime.time_since_epoch().count() / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);

#else
		time_t stime = decltype(ftime)::clock::to_time_t(ftime);
#endif
		m_atime = stime;
		m_mtime = stime;
		m_ctime = stime;
	}

	// we have no means to query user or group ID..

#else

	kDebug(1, "no implementation available for this environment");

#endif

} // KFileStat

//-----------------------------------------------------------------------------
int KFileStat::GetAccessMode() const
//-----------------------------------------------------------------------------
{
	return (m_mode & ~S_IFMT);

} // GetAccessMode

//-----------------------------------------------------------------------------
KFileType KFileStat::GetType() const
//-----------------------------------------------------------------------------
{
	if (S_ISREG(m_mode))
	{
		return KFileType::REGULAR;
	}
	else if (S_ISDIR(m_mode))
	{
		return KFileType::DIRECTORY;
	}
	else if (S_ISLNK(m_mode))
	{
		return KFileType::LINK;
	}
	else if (S_ISSOCK(m_mode))
	{
		return KFileType::SOCKET;
	}
	else if (S_ISFIFO(m_mode))
	{
		return KFileType::FIFO;
	}
	else if (S_ISBLK(m_mode))
	{
		return KFileType::BLOCK;
	}
	else if (S_ISCHR(m_mode))
	{
		return KFileType::CHARACTER;
	}
	else
	{
		return KFileType::OTHER;
	}

} // GetFileType

//-----------------------------------------------------------------------------
bool KFileStat::IsDirectory() const
//-----------------------------------------------------------------------------
{
	return S_ISDIR(m_mode);

} // IsDirectory

//-----------------------------------------------------------------------------
bool KFileStat::IsFile() const
//-----------------------------------------------------------------------------
{
	return S_ISREG(m_mode);

} // IsFile

//-----------------------------------------------------------------------------
bool KFileStat::IsSymlink() const
//-----------------------------------------------------------------------------
{
	return S_ISLNK(m_mode);

} // IsSymlink

//-----------------------------------------------------------------------------
bool KFileStat::Exists() const
//-----------------------------------------------------------------------------
{
	return (m_mode & S_IFMT) != 0;

} // Exists


static_assert(std::is_nothrow_move_constructible<KFileStat>::value,
			  "KFileStat is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
KFileStat kFileStat(KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	return KFileStat(sFilename);
}

//-----------------------------------------------------------------------------
KDirectory::DirEntry::DirEntry(KStringView BasePath, KStringView Name, KFileType Type)
//-----------------------------------------------------------------------------
: m_Type(Type)
{
	if (!BasePath.empty())
	{
		BasePath.TrimRight(detail::kAllowedDirSep);
		m_Path = BasePath;
		m_Path += kDirSep;
	}
	auto iPath = m_Path.size();
	m_Path += Name;
	m_Filename = m_Path.ToView(iPath);

} // DirEntry ctor

//-----------------------------------------------------------------------------
const KFileStat& KDirectory::DirEntry::FileStat() const
//-----------------------------------------------------------------------------
{
	if (!m_Stat)
	{
		m_Stat = std::make_unique<KFileStat>(m_Path);
	}

	return *m_Stat;

} // FileStat

static_assert(std::is_nothrow_move_constructible<KDirectory::DirEntry>::value,
			  "KDirectory::DirEntry is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
void KDirectory::clear()
//-----------------------------------------------------------------------------
{
	m_DirEntries.clear();

} // clear

//-----------------------------------------------------------------------------
size_t KDirectory::Open(KStringViewZ sDirectory, KFileType Type, bool bRecursive, bool bClear)
//-----------------------------------------------------------------------------
{
	if (bClear)
	{
		clear();
	}

	if (sDirectory.empty())
	{
		kDebug(1, "directory name is empty");
		return 0;
	}

#if DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	for (const auto& Entry : fs::directory_iterator(kToFilesystemPath(sDirectory), ec))
	{
		if (ec)
		{
			kDebug(2, "{}: {}", sDirectory, ec.message());
			break;
		}

		fs::file_type dtype;
		switch (Type)
		{
			case KFileType::BLOCK:
				dtype = fs::file_type::block;
				break;
			case KFileType::CHARACTER:
				dtype = fs::file_type::character;
				break;
			case KFileType::DIRECTORY:
				dtype = fs::file_type::directory;
				break;
			case KFileType::FIFO:
				dtype = fs::file_type::fifo;
				break;
			case KFileType::LINK:
				dtype = fs::file_type::symlink;
				break;
			case KFileType::ALL:
			case KFileType::REGULAR:
				dtype = fs::file_type::regular;
				break;
			case KFileType::SOCKET:
				dtype = fs::file_type::socket;
				break;
			default:
			case KFileType::OTHER:
				dtype = fs::file_type::not_found;
				break;
		}

		if (Type == KFileType::ALL)
		{
			fs::file_type ftype = Entry.symlink_status(ec).type();

			if (ec)
			{
				kDebug(2, "{}: {}", sDirectory, ec.message());
				break;
			}

			KFileType FT;

			switch (ftype)
			{
				case fs::file_type::block:
					FT = KFileType::BLOCK;
					break;
				case fs::file_type::character:
					FT = KFileType::CHARACTER;
					break;
				case fs::file_type::directory:
					FT = KFileType::DIRECTORY;
					break;
				case fs::file_type::fifo:
					FT = KFileType::FIFO;
					break;
				case fs::file_type::symlink:
					FT = KFileType::LINK;
					break;
				case fs::file_type::regular:
					FT = KFileType::REGULAR;
					break;
				case fs::file_type::socket:
					FT = KFileType::SOCKET;
					break;
				default:
				case fs::file_type::not_found:
					FT = KFileType::OTHER;
					break;
			}
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().u8string(), FT);
		}
		else if (Entry.symlink_status().type() == dtype)
		{
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().u8string(), Type);
		}

		if (bRecursive && Entry.symlink_status().type() == fs::file_type::directory)
		{
			// remove trailing dir separators
			KStringView sDir = sDirectory;
			sDir.TrimRight(detail::kAllowedDirSep);
			// recurse through the subdirectories
			Open(kFormat("{}{}{}", sDir, kDirSep, Entry.path().filename().u8string()), Type, true, false);
		}
	}

#else

	unsigned char dtype;
	switch (Type)
	{
		case KFileType::BLOCK:
			dtype = DT_BLK;
			break;
		case KFileType::CHARACTER:
			dtype = DT_CHR;
			break;
		case KFileType::DIRECTORY:
			dtype = DT_DIR;
			break;
		case KFileType::FIFO:
			dtype = DT_FIFO;
			break;
		case KFileType::LINK:
			dtype = DT_LNK;
			break;
		case KFileType::ALL:
		case KFileType::REGULAR:
			dtype = DT_REG;
			break;
		case KFileType::SOCKET:
			dtype = DT_SOCK;
			break;
		case KFileType::OTHER:
			dtype = DT_UNKNOWN;
			break;
	}

	auto d = ::opendir(sDirectory.c_str());
	if (d)
	{
		struct dirent* dir;
		while ((dir = ::readdir(d)) != nullptr)
		{
			// exclude . and .. as std::filesystem excludes them, too
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
			{
				if (Type == KFileType::ALL)
				{
					KFileType FT;
					switch (dir->d_type)
					{
						case DT_BLK:
							FT = KFileType::BLOCK;
							break;
						case DT_CHR:
							FT = KFileType::CHARACTER;
							break;
						case DT_DIR:
							FT = KFileType::DIRECTORY;
							break;
						case DT_FIFO:
							FT = KFileType::FIFO;
							break;
						case DT_LNK:
							FT = KFileType::LINK;
							break;
						case DT_REG:
							FT = KFileType::REGULAR;
							break;
						case DT_SOCK:
							FT = KFileType::SOCKET;
							break;
						default:
						case DT_UNKNOWN:
							FT = KFileType::OTHER;
							break;
					}
					m_DirEntries.emplace_back(sDirectory, dir->d_name, FT);
				}
				else if (dir->d_type == dtype)
				{
					m_DirEntries.emplace_back(sDirectory, dir->d_name, Type);
				}

				if (bRecursive && dir->d_type == DT_DIR)
				{
					// remove trailing dir separators
					KStringView sDir = sDirectory;
					sDir.TrimRight(detail::kAllowedDirSep);
					// recurse through the subdirectories
					Open(kFormat("{}{}{}", sDir, kDirSep, dir->d_name), Type, true, false);
				}
			}
		}
		::closedir(d);
	}
	else
	{
		kDebug(2, "could not open directory: {}", sDirectory);
	}

#endif

	return size();

} // Open

//-----------------------------------------------------------------------------
void KDirectory::RemoveHidden()
//-----------------------------------------------------------------------------
{
	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
									  m_DirEntries.end(),
									  [](const DirEntries::value_type& elem)
                                      {
                                          return elem.Filename().empty() || elem.Filename().front() == '.';
									  }),
                       m_DirEntries.end());

} // RemoveHidden

//-----------------------------------------------------------------------------
size_t KDirectory::Match(KFileType Type, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	size_t iMatched { 0 };

	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
									  m_DirEntries.end(),
									  [&iMatched, Type, bRemoveMatches](const DirEntries::value_type& elem)
									  {
										  bool bMatches = (elem.Type() == Type);
										  if (bMatches)
										  {
											  ++iMatched;
										  }
										  return bRemoveMatches == bMatches;
									  }),
					   m_DirEntries.end());

	return iMatched;

} // Match

//-----------------------------------------------------------------------------
size_t KDirectory::Match(KStringView sRegex, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	KRegex Regex(sRegex);

	size_t iMatched { 0 };

	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
                                      m_DirEntries.end(),
                                      [&Regex, &iMatched, bRemoveMatches](const DirEntries::value_type& elem)
                                      {
										  bool bMatches = Regex.Matches(KStringView(elem.Filename()));
										  if (bMatches)
										  {
											  ++iMatched;
										  }
										  return bRemoveMatches == bMatches;
                                      }),
                       m_DirEntries.end());

	return iMatched;

} // Match

//-----------------------------------------------------------------------------
size_t KDirectory::WildCardMatch(KStringView sWildCard, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	return Match(kWildCard2Regex(sWildCard), bRemoveMatches);

} // WildCardMatch

//-----------------------------------------------------------------------------
KDirectory::const_iterator KDirectory::Find(KStringView sWildCard) const
//-----------------------------------------------------------------------------
{
	KRegex Regex(kWildCard2Regex(sWildCard));

	for (auto it = cbegin(); it != cend(); ++it)
	{
		if (Regex.Matches(it->Filename()))
		{
			return it;
		}
	}

	return end();

} // Find

//-----------------------------------------------------------------------------
void KDirectory::Sort(SortBy SortBy, bool bReverse)
//-----------------------------------------------------------------------------
{
	switch (SortBy)
	{
		case SortBy::NAME:
			std::sort(m_DirEntries.begin(), m_DirEntries.end());
			break;

		case SortBy::SIZE:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [](const DirEntry& left, const DirEntry& right)
			{
				return left.Size() > right.Size();
			});
			break;

		case SortBy::DATE:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [](const DirEntry& left, const DirEntry& right)
			{
					return left.ModificationTime() > right.ModificationTime();
			});
			break;

		case SortBy::UID:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [](const DirEntry& left, const DirEntry& right)
			{
					return left.UID() > right.UID();
			});
			break;

		case SortBy::GID:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [](const DirEntry& left, const DirEntry& right)
			{
					return left.GID() > right.GID();
			});
			break;
	}

	if (bReverse)
	{
		std::reverse(m_DirEntries.begin(), m_DirEntries.end());
	}

} // Sort

//-----------------------------------------------------------------------------
KStringViewZ KDirectory::TypeAsString(KFileType Type)
//-----------------------------------------------------------------------------
{
	switch (Type)
	{
		case KFileType::ALL:
			return "ALL";
		case KFileType::BLOCK:
			return "BLOCK";
		case KFileType::CHARACTER:
			return "CHARACTER";
		case KFileType::DIRECTORY:
			return "DIRECTORY";
		case KFileType::FIFO:
			return "FIFO";
		case KFileType::LINK:
			return "LINK";
		case KFileType::REGULAR:
			return "REGULAR";
		case KFileType::SOCKET:
			return "SOCKET";
		case KFileType::OTHER:
			return "OTHER";
	}

	return "OTHER"; // gcc wants this. we do not want default: above

} // TypeAsString

static_assert(std::is_nothrow_move_constructible<KDirectory>::value,
			  "KDirectory is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
void KDiskStat::clear()
//-----------------------------------------------------------------------------
{
	m_Free = 0;
	m_Total = 0;
	m_Used = 0;
	m_SystemFree = 0;
	m_sError.clear();

} // clear

//-----------------------------------------------------------------------------
KDiskStat& KDiskStat::Check(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	clear();

#if DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	auto fsinfo = fs::space(kToFilesystemPath(sPath), ec);

	if (ec)
	{
		SetError(kFormat("cannot read file system status: {}", ec.message()));
		return *this;
	}

	m_Total       = fsinfo.capacity;
	m_Free        = fsinfo.available;
	m_SystemFree  = fsinfo.free;
	m_Used        = m_Total - m_SystemFree;

#else

	struct statvfs fsinfo;

	if (::statvfs(sPath.c_str(), &fsinfo))
	{
		SetError(kFormat("cannot read file system status: {}", strerror(errno)));
		return *this;
	}

	auto bsize     = static_cast<uint64_t>(fsinfo.f_frsize);
	m_Total        = bsize * fsinfo.f_blocks;
	m_Free         = bsize * fsinfo.f_bavail;
	m_SystemFree   = bsize * fsinfo.f_bfree;
	m_Used         = m_Total - m_SystemFree;

#endif // HAS STD FILESYSTEM

#ifdef DEKAF2_IS_UNIX

	if (m_Total == 0 || m_Free == 0)
	{
		// use a call to df, this is probably a file system that is not supported by
		// statvfs() like FAT..
		KString sCmd;
#ifdef DEKAF2_IS_OSX
		// OSX df outputs in 512 byte blocks, and we cannot change that
		sCmd.Format("df \"{}\"", sPath);
#else
		// Linux has it at 1k, and we set it also explicitly
		sCmd.Format("df -B 1024 \"{}\"", sPath);
#endif
		KInShell Shell(sCmd);

		// we expect two lines:
		// a) OSX
		// Filesystem   512-blocks      Used Available Capacity iused               ifree %iused  Mounted on
		// /dev/disk1s1  976490568 113271256 852397240    12%  891979 9223372036853883828    0%   /
		// b) Linux
		// Filesystem     1K-blocks    Used Available Use% Mounted on
		// /dev/sda1         82043328 8179584  69636776  11% /

		KString sLine;
		for (int i = 0; i < 2; ++i)
		{
			if (!Shell.ReadLine(sLine))
			{
				SetError("cannot read df output");
				return *this;
			}
		}

		auto Words = sLine.Split(" \t");

		static constexpr size_t MIN_DF_WORDS = 6;

		if (Words.size() < MIN_DF_WORDS)
		{
			SetError(kFormat("unexpected output format: {}", sLine));
			return *this;
		}

#ifdef DEKAF2_IS_OSX
		enum { iBlock = 512 };
#else
		enum { iBlock = 1024 };
#endif

		m_Total      = iBlock * Words[1].UInt64();
		m_Used       = iBlock * Words[2].UInt64();
		m_Free       = iBlock * Words[3].UInt64();
		m_SystemFree = m_Free;
	}

#endif // IS UNIX

	return *this;

} // Check

//-----------------------------------------------------------------------------
bool IntWriteFile(KStringViewZ sPath, std::ios_base::openmode OpenMode, KStringView sContents, int iMode)
//-----------------------------------------------------------------------------
{
	{
		KOutFile file(sPath, OpenMode);

		if (!file.is_open())
		{
			kWarning ("cannot open file: {}", sPath);
			return (false);
		}

		file.Write(sContents);
	}

	if (iMode != DEKAF2_MODE_CREATE_FILE)
	{
		kChangeMode (sPath, iMode);
	}

	return (true);

} // IntWriteFile

//-----------------------------------------------------------------------------
bool kWriteFile (KStringViewZ sPath, KStringView sContents, int iMode /* = DEKAF2_MODE_CREATE_FILE */)
//-----------------------------------------------------------------------------
{
	return IntWriteFile(sPath, std::ios_base::trunc, sContents, iMode);

} // kWriteFile

//-----------------------------------------------------------------------------
bool kAppendFile (KStringViewZ sPath, KStringView sContents, int iMode /* = DEKAF2_MODE_CREATE_FILE */)
//-----------------------------------------------------------------------------
{
	return IntWriteFile(sPath, std::ios_base::app, sContents, iMode);

} // kAppendFile

//-----------------------------------------------------------------------------
bool kReadFile (KStringViewZ sPath, KString& sContents, bool bToUnixLineFeeds)
//-----------------------------------------------------------------------------
{
	kDebug (2, sPath);

	if (!kReadAll(sPath, sContents))
	{
		return false;
	}

	if (bToUnixLineFeeds)
	{
		sContents.Replace("\r\n","\n"); // DOS -> UNIX
	}
	
	return true;

} // kReadFile

//-----------------------------------------------------------------------------
KString kNormalizePath(KStringView sPath)
//-----------------------------------------------------------------------------
{
	// "/user/./test/../peter/myfile" -> "/user/peter/myfile"
	// "../peter/myfile" -> "/user/peter/myfile"

	// remove any whitespace left and right
	sPath.Trim();

#ifdef DEKAF2_HAS_STD_FILESYSTEM_NONONO
	// for the time being fs::weakly_canonical() is not stable enough
	// - it is missing on early gcc implementations of std::filesystem
	// - it does not work with relative paths on VS 2017
	// therefore we always use our discrete implementation

	std::error_code ec;

	return fs::weakly_canonical(kToFilesystemPath(sPath), ec).u8string();

#else

#ifdef DEKAF2_IS_WINDOWS
	char chHasDrive { 0 };
	if (sPath.size() > 1 && sPath[1] == ':' && KASCII::kIsAlpha(sPath[0]))
	{
		chHasDrive = KASCII::kToUpper(sPath[0]);
		sPath.remove_prefix(2);
	}
#endif

	std::vector<KStringView> Normalized;

	KString sCWD;

	if (!sPath.starts_with('/')
#ifdef DEKAF2_IS_WINDOWS
		&& !sPath.starts_with('\\')
#endif
		)
	{
#ifdef DEKAF2_IS_WINDOWS
		if (!chHasDrive)
#endif
		{
			// This is a relative path. Get current working directory
			std::vector<KStringView> CWD;
			sCWD = kGetCWD();
#ifdef DEKAF2_IS_WINDOWS
			if (sCWD.size() > 1 && sCWD[1] == ':' && KASCII::kIsAlpha(sCWD[0]))
			{
				chHasDrive = KASCII::kToUpper(sCWD[0]);
				sCWD.remove_prefix(2);
			}
#endif
			// and add it to the normalized path
			for (auto it : sCWD.Split(detail::kAllowedDirSep, ""))
			{
				if (!it.empty())
				{
					Normalized.push_back(it);
				}
			}
		}
	}
#ifdef DEKAF2_IS_WINDOWS
	else
	{
		// sPath starts with a directory separator
		sPath.remove_prefix(1);
	}
#endif

	bool bWarned { false };

	// split into path components
	for (auto it : sPath.Split(detail::kAllowedDirSep, ""))
	{
		if (it.empty())
		{
#ifdef DEKAF2_IS_WINDOWS
			if (Normalized.empty() && !chHasDrive)
			{
				// on Windows, we accept a path starting with "//" or "\\"
				Normalized.push_back(it);
			}
#endif
		}
		else if (it == ".")
		{
			// this is just junk
			continue;
		}
		else if (it == "..")
		{
			if (!Normalized.empty())
			{
				Normalized.pop_back();
			}
			else
			{
				if (!bWarned)
				{
					// emit this warning only once
					kDebug(1, "invalid normalization path: {}", sPath);
					bWarned = true;
				}
			}
		}
		else
		{
			// ordinary directory
			Normalized.push_back(it);
		}
	}

	KString sNormalized;
	sNormalized.reserve(sPath.size());

#ifdef DEKAF2_IS_WINDOWS
	if (chHasDrive)
	{
		sNormalized = chHasDrive;
		sNormalized += ':';
	}
#endif

	for (auto it : Normalized)
	{
		sNormalized += kDirSep;
		sNormalized += it;
	}

	return sNormalized;

#endif
	
} // kNormalizePath

//-----------------------------------------------------------------------------
bool kIsSafeFilename(KStringView sName)
//-----------------------------------------------------------------------------
{
	return kMakeSafeFilename(sName, false) == sName;

} // kIsSafeFilename

//-----------------------------------------------------------------------------
KString kMakeSafeFilename(KStringView sName, bool bToLowercase, KStringView sEmptyName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	// try to get rid of the drive prefix, if any
	auto iPos = sName.find_first_of(detail::kAllowedDirSep);

	if (iPos != KStringView::npos && sName[iPos] == ':')
	{
		sName.remove_prefix(iPos + 1);
	}
#endif

	KString sSafe;
	sSafe.reserve(sName.size());

	for (auto Dotted : sName.Split(".", detail::kUnsafeLimiterChars))
	{
		if (Dotted.empty())
		{
			// drop empty fragments
			continue;
		}

		KCodePoint lastCp { 0 };

		Unicode::TransformUTF8(Dotted, sSafe, [bToLowercase, &lastCp](Unicode::codepoint_t uch, KString& sOut)
		{
			KCodePoint Cp(uch);

			if (uch != '_' && !Cp.IsAlNum())
			{
				if (lastCp == 0)
				{
					// drop it..
					return true;
				}
				if (lastCp == '-')
				{
					// drop it
					return true;
				}
				// replace it (once)
				Cp = '-';
			}

			lastCp = Cp;

			if (bToLowercase)
			{
				return Unicode::ToUTF8(Cp.ToLower().value(), sOut);
			}
			else
			{
				return Unicode::ToUTF8(Cp.value(), sOut);
			}
		});

		// make sure there is no trailing dash
		sSafe.TrimRight('-');

		if (!sSafe.empty() && sSafe.back() != '.')
		{
			sSafe += '.';
		}
	}

	sSafe.TrimRight('.');

	if (sSafe.empty())
	{
		sSafe = sEmptyName;
	}

	return sSafe;

} // kMakeSafeFilename

//-----------------------------------------------------------------------------
bool kIsSafePathname(KStringView sName)
//-----------------------------------------------------------------------------
{
	return kMakeSafePathname(sName, false) == sName;

} // kIsSafePathname

//-----------------------------------------------------------------------------
KString kMakeSafePathname(KStringView sName, bool bToLowercase, KStringView sEmptyName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	// try to get rid of the drive prefix, if any
	auto iPos = sName.find_first_of(detail::kAllowedDirSep);

	if (iPos != KStringView::npos && sName[iPos] == ':')
	{
		sName.remove_prefix(iPos + 1);
	}
#endif

	KString sSafe;
	sSafe.reserve(sName.size());

	for (auto Part : sName.Split(detail::kAllowedDirSep, detail::kUnsafeLimiterChars))
	{
		if (Part.empty())
		{
			// drop empty fragments
			continue;
		}

		sSafe += kMakeSafeFilename(Part, bToLowercase, KStringView{});

		if (!sSafe.empty() && sSafe.back() != kDirSep)
		{
			sSafe += kDirSep;
		}
	}

	sSafe.TrimRight(kDirSep);

	if (sSafe.empty())
	{
		sSafe = sEmptyName;
	}

	return sSafe;

} // kMakeSafePathname

//-----------------------------------------------------------------------------
const KString& KTempDir::Name()
//-----------------------------------------------------------------------------
{
	if (m_sTempDirName.empty())
	{
		m_sTempDirName = MakeDir();
	}

	return m_sTempDirName;

} // Name

//-----------------------------------------------------------------------------
KString KTempDir::MakeDir ()
//-----------------------------------------------------------------------------
{
	KString sDirName;

	for (int i = 0; i < 100; ++i)
	{
		sDirName = kFormat ("{}{}{}-{}-{}",
							kGetTemp(),
							kDirSep,
							kFirstNonEmpty(Dekaf::getInstance().GetProgName(), "dekaf"),
							kGetTid(),
							kRandom (10000, 99999));

		if (kDirExists(sDirName))
		{
			continue;
		}
		else
		{
			if (kCreateDir (sDirName))
			{
				kDebug(2, "created temp directory: {}", sDirName);

				return sDirName;
			}
		}
	}

	kDebug (1, "failed to create temp directory");

	sDirName.clear();

	return sDirName;

} // MakeDir

//-----------------------------------------------------------------------------
KTempDir::~KTempDir()
//-----------------------------------------------------------------------------
{
	clear();

} // KTempDir dtor

//-----------------------------------------------------------------------------
void KTempDir::clear()
//-----------------------------------------------------------------------------
{
	if (!m_sTempDirName.empty())
	{
		if (m_bDeleteOnDestruction)
		{
			if (kRemoveDir(m_sTempDirName))
			{
				kDebug (2, "removed temp directory: {}", m_sTempDirName);
			}
			else
			{
				kDebug (1, "failed to remove temp directory: {}", m_sTempDirName);
			}
		}

		m_sTempDirName.clear();
	}

} // clear

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARS
namespace detail {
constexpr KStringViewZ kLineRightTrims;
constexpr KStringViewZ kAllowedDirSep;
constexpr KStringViewZ kCurrentDir;
constexpr KStringViewZ kCurrentDirWithSep;
}
#endif

} // end of namespace dekaf2

