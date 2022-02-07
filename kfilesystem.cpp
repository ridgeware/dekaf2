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
#include "kstringutils.h"
#include "kstringview.h"
#include "kstring.h"
#include "klog.h"
#include "kregex.h"
#include "ksplit.h"
#include "kinshell.h"
#include "kwriter.h"
#include "kctype.h"
#include "kutf8.h"


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

} // kChangeMode

//-----------------------------------------------------------------------------
int kGetMode(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sPath);

	return Stat.AccessMode();

} // kGetMode

//-----------------------------------------------------------------------------
bool kExists (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sPath);

	return Stat.Exists();

} // kExists

//-----------------------------------------------------------------------------
bool kFileExists (KStringViewZ sPath, bool bTestForEmptyFile)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sPath);

	if (Stat.IsFile())
	{
		if (bTestForEmptyFile && Stat.Size() == 0)
		{
			kDebug (3, "entry exists, but is empty: {}", sPath);
			return false;    // <-- exists, is a file but is zero length
		}

		kDebug (3, "file exists: {}", sPath);
		return true;    // <-- exists and is a file
	}

	if (Stat.Exists())
	{
		kDebug (3, "entry exists, but is a {}, not a file: {}", Stat.Type().Serialize(), sPath);
	}

	return false;

} // kFileExists

//-----------------------------------------------------------------------------
bool kDirExists (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sPath);

	if (Stat.IsDirectory())
	{
		kDebug (3, "directory exists: {}", sPath);
		return true;
	}

	if (Stat.Exists())
	{
		kDebug (3, "entry exists, but is a {}, not a directory: {}", Stat.Type().Serialize(), sPath);
	}

	return false;

} // kDirExists

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
bool kRemove (KStringViewZ sPath, KFileTypes Types)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sPath);

	if (Stat.Type() == KFileType::UNEXISTING)
	{
		kDebug(2, "file does not exist, return success: {}", sPath);
		return true;
	}

	if (!Types.contains(Stat.Type()))
	{
		kDebug(1, "cannot remove fs entity: bad type {} for {}, requested was {}",
			   Stat.Type().Serialize(),
			   sPath,
			   kJoined(Types.Serialize()));
		return false;
	}

#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;
	fs::path Path(kToFilesystemPath(sPath));
	fs::permissions (Path, fs::perms::all, ec); // chmod (ignore failures)
	ec.clear();

	if (Stat.Type() == KFileType::DIRECTORY)
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
			if (Stat.Type() == KFileType::DIRECTORY)
			{
				if (rmdir (sPath.c_str()) == 0)
				{
					return true;
				}

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
					kDebug(2, "{}: {}", sNewPath, strerror(errno));
					return false;
				}
			}
		}
	}

	return true;

#endif

} // kCreateDir

//-----------------------------------------------------------------------------
bool kCreateSymlink(KStringViewZ sOrigin, KStringViewZ sSymlink)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

#ifdef DEKAF2_IS_UNIX
	fs::create_symlink(kToFilesystemPath(sOrigin), kToFilesystemPath(sSymlink), ec);
#else
	// find out if the origin is a directory, and use a special API call if it is
	if (kDirExists(sOrigin))
	{
		fs::create_directory_symlink(kToFilesystemPath(sOrigin), kToFilesystemPath(sSymlink), ec);
	}
	else
	{
		fs::create_symlink(kToFilesystemPath(sOrigin), kToFilesystemPath(sSymlink), ec);
	}
#endif

	if (ec)
	{
		kDebug(2, ec.message());
		return false;
	}

	return true;

#else

	if (::symlink(sOrigin.c_str(), sSymlink.c_str()))
	{
		kDebug (1, "failed: {} > {}: {}", sOrigin, sSymlink, strerror (errno));
		return false;
	}

	return true;

#endif

} // kCreateSymlink

//-----------------------------------------------------------------------------
bool kCreateHardlink(KStringViewZ sOrigin, KStringViewZ sHardlink)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	fs::create_hard_link(kToFilesystemPath(sOrigin), kToFilesystemPath(sHardlink), ec);

	if (ec)
	{
		kDebug(2, ec.message());
		return false;
	}

	return true;

#else

	if (::link(sOrigin.c_str(), sHardlink.c_str()))
	{
		kDebug (1, "failed: {} > {}: {}", sOrigin, sHardlink, strerror (errno));
		return false;
	}

	return true;

#endif

} // kCreateHardlink

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
	KFileStat Stat(sFilePath);

	if (!Stat.Exists())
	{
		return -1;  // <-- file doesn't exist
	}

	return Stat.ModificationTime();

} // kGetLastMod

//-----------------------------------------------------------------------------
size_t kFileSize(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
	KFileStat Stat(sFilePath);

	if (!Stat.IsFile())
	{
		return npos;  // <-- file doesn't exist or is not a regular file
	}

	return Stat.Size();

} // kFileSize

//-----------------------------------------------------------------------------
std::unique_ptr<KOutFile> kCreateFileWithBackup(KStringView sOrigname, KStringView sBackupExtension)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KOutFile> OutFile;

	if (sBackupExtension.empty())
	{
		kDebug(1, "backup extension is empty")
	}
	else if(sBackupExtension.find_first_of(detail::kUnsafeFileExtensionChars) != npos)
	{
		kDebug(1, "backup extension '{}' contains one of {}", sBackupExtension, detail::kUnsafeFileExtensionChars);
	}
	else
	{
		KString sFilename = sOrigname;

		// only rename files, not directories..
		if (kFileExists(sFilename))
		{
			KString sBakExt(1, '.');
			sBakExt += sBackupExtension;
			sFilename += sBakExt;

			while (kFileExists(sFilename))
			{
				sFilename += sBakExt;
			}

			while (sFilename.size() > sOrigname.size())
			{
				KString sLastname = sFilename.Left(sFilename.size() - sBakExt.size());
				kDebug(2, "renaming '{}' to '{}'", sLastname, sFilename);

				if (!kRename(sLastname, sFilename))
				{
					// rename failed, bail out
					return OutFile;
				}

				sFilename.erase(sFilename.end() - sBakExt.size(), sFilename.end());
			}
		}

		if (!kExists(sFilename))
		{
			OutFile = std::make_unique<KOutFile>(sFilename, std::ios::trunc);
		}

		if (OutFile && !OutFile->is_open())
		{
			kDebug(1, "cannot create file '{}'", sFilename);
			OutFile.reset();
		}
	}

	return OutFile;

} // kCreateFileWithBackup

namespace {

#ifdef DEKAF2_IS_UNIX

//-----------------------------------------------------------------------------
KFileType KFileTypeFromUnixMode(uint32_t mode)
//-----------------------------------------------------------------------------
{
	switch ((mode & S_IFMT))
	{
		case S_IFREG:
			return KFileType::FILE;

		case S_IFDIR:
			return KFileType::DIRECTORY;

		case S_IFLNK:
			return KFileType::SYMLINK;

		case S_IFSOCK:
			return KFileType::SOCKET;

		case S_IFIFO:
			return KFileType::PIPE;

		case S_IFBLK:
			return KFileType::BLOCK;

		case S_IFCHR:
			return KFileType::CHARACTER;

		default:
			return KFileType::UNKNOWN;
	}

	return KFileType::UNKNOWN;

} // KFileTypeFromUnixMode

#endif // of DEKAF2_IS_UNIX

#ifdef DEKAF2_HAS_STD_FILESYSTEM

//-----------------------------------------------------------------------------
KFileType KFileTypeFromStdFilesystem(fs::file_type ftype)
//-----------------------------------------------------------------------------
{
	switch (ftype)
	{
#ifdef DEKAF2_HAS_CPP_17
		case fs::file_type::block:
			return KFileType::BLOCK;
#endif
		case fs::file_type::none:
		case fs::file_type::not_found:
			return KFileType::UNEXISTING;

		case fs::file_type::character:
			return KFileType::CHARACTER;

		case fs::file_type::directory:
			return KFileType::DIRECTORY;

		case fs::file_type::fifo:
			return KFileType::PIPE;

		case fs::file_type::symlink:
			return KFileType::SYMLINK;

		case fs::file_type::regular:
			return KFileType::FILE;

		case fs::file_type::socket:
			return KFileType::SOCKET;

		default:
		case fs::file_type::unknown:
			return KFileType::UNKNOWN;
	}

	return KFileType::UNKNOWN;

} // KFileTypeFromStdFilesystem

#else // of DEKAF2_HAS_STD_FILESYSTEM

//-----------------------------------------------------------------------------
KFileType KFileTypeFromDirentry(uint8_t d_type)
//-----------------------------------------------------------------------------
{
	switch (d_type)
	{
		case DT_BLK:
			return KFileType::BLOCK;

		case DT_CHR:
			return KFileType::CHARACTER;

		case DT_DIR:
			return KFileType::DIRECTORY;

		case DT_FIFO:
			return KFileType::PIPE;

		case DT_LNK:
			return KFileType::SYMLINK;

		case DT_REG:
			return KFileType::FILE;

		case DT_SOCK:
			return KFileType::SOCKET;

		default:
		case DT_UNKNOWN:
			return KFileType::UNKNOWN;
	}

	return KFileType::UNKNOWN;

} // KFileTypeFromDirentry

#endif // of DEKAF2_HAS_STD_FILESYSTEM

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KStringViewZ KFileType::Serialize() const
//-----------------------------------------------------------------------------
{
	switch (m_FType)
	{
		case UNEXISTING:
			return "unexisting";
		case FILE:
			return "file";
		case DIRECTORY:
			return "directory";
		case SYMLINK:
			return "symlink";
		case PIPE:
			return "pipe";
		case BLOCK:
			return "block";
		case CHARACTER:
			return "character";
		case SOCKET:
			return "socket";
		case UNKNOWN:
			return "unknown";
	}

	return "error";

} // Serialize

//-----------------------------------------------------------------------------
std::vector<KStringViewZ> KFileTypes::Serialize() const
//-----------------------------------------------------------------------------
{
	std::vector<KStringViewZ> Result;

	if (m_Types == 255)
	{
		Result.push_back("all");
	}
	else
	{
		uint8_t mask = 1;

		for (;;)
		{
			auto iMasked = m_Types & mask;

			if (iMasked != 0)
			{
				Result.push_back(KFileType(KFileType::FileType(iMasked)).Serialize());
			}

			if (mask >= KFileType::MAX)
			{
				break;
			}

			mask <<= 1;
		}
	}
	
	return Result;

} // Serialize

const KFileTypes KFileTypes::ALL = static_cast<KFileType::FileType>(255);

#ifdef DEKAF2_FILESTAT_USE_STAT
//-----------------------------------------------------------------------------
void KFileStat::FromStat(struct stat& StatStruct)
//-----------------------------------------------------------------------------
{
	m_inode = StatStruct.st_ino;
	m_atime = StatStruct.st_atime;
	m_mtime = StatStruct.st_mtime;
	m_ctime = StatStruct.st_ctime;
	m_mode  = StatStruct.st_mode & ~S_IFMT;
	m_uid   = StatStruct.st_uid;
	m_gid   = StatStruct.st_gid;
	m_links = StatStruct.st_nlink;
	m_ftype = KFileTypeFromUnixMode(StatStruct.st_mode);

	if (!IsDirectory())
	{
		m_size = StatStruct.st_size;
	}

} // FromStat

//-----------------------------------------------------------------------------
KFileStat::KFileStat(int iFileDescriptor)
//-----------------------------------------------------------------------------
{
	// use the good ole fstat() system call, it fetches all information with one call

	struct stat StatStruct;

	if (!fstat(iFileDescriptor, &StatStruct))
	{
		FromStat(StatStruct);
	}

} // ctor
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
		FromStat(StatStruct);
	}

#elif defined(DEKAF2_FILESTAT_USE_STD_FILESYSTEM)

	// windows would have issues with utf8 file names, therefore use the
	// std::filesystem interface (which however needs multiple calls)

	std::error_code ec;
	auto fsPath = kToFilesystemPath(sFilename);

	auto status = fs::status(fsPath, ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sFilename, ec.message());
		m_mode = 0;
	}
	else
	{
		m_ftype = KFileTypeFromStdFilesystem(status.type());
		m_mode  = static_cast<int>(status.permissions());
	}

	if (!IsDirectory())
	{
		m_size = fs::file_size(fsPath, ec);

		if (ec)
		{
			kDebug(1, "{}: {}", sFilename, ec.message());
			m_size = 0;
		}
	}

	auto ftime = fs::last_write_time(fsPath, ec);

	if (ec)
	{
		kDebug(1, "{}: {}", sFilename, ec.message());
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
KFileStat& KFileStat::SetDefaults()
//-----------------------------------------------------------------------------
{
	time_t tNow { 0 };

	if (ModificationTime() == 0)
	{
		tNow = Dekaf::getInstance().GetCurrentTime();
		SetModificationTime(tNow);
	}
	else
	{
		tNow = ModificationTime();
	}

	if (AccessTime() == 0)
	{
		SetAccessTime(tNow);
	}

	if (ChangeTime() == 0)
	{
		SetChangeTime(tNow);
	}

	if (UID() == 0)
	{
		SetUID(kGetUid());
	}

	if (GID() == 0)
	{
		SetGID(kGetGid());
	}

	if (Type() == KFileType::UNEXISTING)
	{
		SetType(KFileType::FILE);
	}

	if (AccessMode() == 0)
	{
		if (IsDirectory())
		{
			SetAccessMode(DEKAF2_MODE_CREATE_DIR);
		}
		else
		{
			SetAccessMode(DEKAF2_MODE_CREATE_FILE);
		}
	}

	return *this;

} // SetDefaults

static_assert(std::is_nothrow_move_constructible<KFileStat>::value,
			  "KFileStat is intended to be nothrow move constructible, but is not!");

const KFileStat KDirectory::DirEntry::s_EmptyStat;


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
		if (!m_Path.empty())
		{
			m_Stat = std::make_unique<KFileStat>(m_Path);
		}
		else
		{
			return s_EmptyStat;
		}
	}

	return *m_Stat;

} // FileStat

static_assert(std::is_nothrow_move_constructible<KDirectory::DirEntry>::value,
			  "KDirectory::DirEntry is intended to be nothrow move constructible, but is not!");


const KDirectory::DirEntry KDirectory::s_Empty;

//-----------------------------------------------------------------------------
void KDirectory::clear()
//-----------------------------------------------------------------------------
{
	m_DirEntries.clear();

} // clear

//-----------------------------------------------------------------------------
size_t KDirectory::Open(KStringViewZ sDirectory, KFileTypes Types, bool bRecursive, bool bClear)
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

		fs::file_type ftype = Entry.symlink_status(ec).type();

		if (ec)
		{
			kDebug(2, "{}: {}", sDirectory, ec.message());
			break;
		}

		KFileType FT(KFileTypeFromStdFilesystem(ftype));

		if (Types.contains(FT))
		{
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().string(), FT);
		}

		if (bRecursive && Entry.symlink_status().type() == fs::file_type::directory)
		{
			// remove trailing dir separators
			KStringView sDir = sDirectory;
			sDir.TrimRight(detail::kAllowedDirSep);
			// recurse through the subdirectories
			Open(kFormat("{}{}{}", sDir, kDirSep, Entry.path().filename().string()), Types, true, false);
		}
	}

#else

	auto d = ::opendir(sDirectory.c_str());
	if (d)
	{
		struct dirent* dir;
		while ((dir = ::readdir(d)) != nullptr)
		{
			// exclude . and .. as std::filesystem excludes them, too
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
			{
				KFileType FT = KFileTypeFromDirentry(dir->d_type);

				if (Types.contains(FT))
				{
					m_DirEntries.emplace_back(sDirectory, dir->d_name, FT);
				}

				if (bRecursive && dir->d_type == DT_DIR)
				{
					// remove trailing dir separators
					KStringView sDir = sDirectory;
					sDir.TrimRight(detail::kAllowedDirSep);
					// recurse through the subdirectories
					Open(kFormat("{}{}{}", sDir, kDirSep, dir->d_name), Types, true, false);
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
void KDirectory::RemoveAppleResourceFiles()
//-----------------------------------------------------------------------------
{
	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
									  m_DirEntries.end(),
									  [](const DirEntries::value_type& elem)
									  {
										  return elem.Filename().starts_with("._");
									  }),
					   m_DirEntries.end());

} // RemoveAppleResourceFiles

//-----------------------------------------------------------------------------
size_t KDirectory::Match(KFileTypes Types, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	size_t iMatched { 0 };

	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
									  m_DirEntries.end(),
									  [&iMatched, Types, bRemoveMatches](const DirEntries::value_type& elem)
									  {
										  bool bMatches = (Types.contains(elem.Type()));
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
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [bReverse](const DirEntry& left, const DirEntry& right)
			{
				return (left.Path() > right.Path()) == bReverse;
			});
			break;

		case SortBy::SIZE:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [bReverse](const DirEntry& left, const DirEntry& right)
			{
				return (left.Size() > right.Size()) == bReverse;
			});
			break;

		case SortBy::DATE:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [bReverse](const DirEntry& left, const DirEntry& right)
			{
					return (left.ModificationTime() > right.ModificationTime()) == bReverse;
			});
			break;

		case SortBy::UID:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [bReverse](const DirEntry& left, const DirEntry& right)
			{
					return (left.UID() > right.UID()) == bReverse;
			});
			break;

		case SortBy::GID:
			std::sort(m_DirEntries.begin(), m_DirEntries.end(), [bReverse](const DirEntry& left, const DirEntry& right)
			{
					return (left.GID() > right.GID()) == bReverse;
			});
			break;
	}

} // Sort

static_assert(std::is_nothrow_move_constructible<KDirectory>::value,
			  "KDirectory is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
void KDiskStat::clear()
//-----------------------------------------------------------------------------
{
	m_Free       = 0;
	m_Total      = 0;
	m_Used       = 0;
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

namespace {

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

} // end of anonymous namespace

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
bool kReadFile (KStringViewZ sPath, KString& sContents, bool bToUnixLineFeeds, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	kDebug (2, sPath);

	if (!kReadAll(sPath, sContents, iMaxRead))
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

	return fs::weakly_canonical(kToFilesystemPath(sPath), ec).string();

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
bool kIsSafePathname(KStringView sName, bool bAllowAbsolutePath, bool bAllowTrailingSlash)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	if (bAllowAbsolutePath)
	{
		// try to get rid of the drive prefix, if any
		auto iPos = sName.find_first_of(detail::kAllowedDirSep);

		if (iPos != KStringView::npos && sName[iPos] == ':')
		{
			sName.remove_prefix(iPos + 1);
		}
	}
#endif

	if (bAllowAbsolutePath && !sName.empty())
	{
		if (detail::kAllowedDirSep.find(sName.front()) != KStringView::npos)
		{
			// we allow to remove only one slash, not repeated ones
			sName.remove_prefix(1);
		}
	}

	if (bAllowTrailingSlash && !sName.empty())
	{
		if (detail::kAllowedDirSep.find(sName.back()) != KStringView::npos)
		{
			// we allow to remove only one slash, not repeated ones
			sName.remove_suffix(1);
		}
	}

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
constexpr KStringViewZ kUnsafeFilenameChars;
constexpr KStringViewZ kUnsafePathnameChars;
constexpr KStringViewZ kUnsafeLimiterChars;
}
#endif

} // end of namespace dekaf2

