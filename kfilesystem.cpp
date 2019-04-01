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
#include "bits/kfilesystem.h"
#include "kfilesystem.h"
#include "ksystem.h"
#include "kstring.h"
#include "kstringview.h"
#include "klog.h"
#include "kregex.h"
#include "ksplit.h"
#include "kinshell.h"
#include "kwriter.h"
#include "kctype.h"


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
bool kExists (KStringViewZ sPath, bool bAsFile, bool bAsDirectory, bool bTestForEmptyFile)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	fs::file_status status = fs::status(kToFilesystemPath(sPath), ec);

	if (ec)
	{
		kDebug(2, "{}: {}", sPath, ec.message());
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
			kDebug(2, "entry exists, but is not a file: {}", sPath);
			return false;
		}
	}
	else if (bAsDirectory)
	{
		if (!fs::is_directory(status))
		{
			kDebug(2, "entry exists, but is not a directory: {}", sPath);
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
		kDebug(2, "entry exists, but is empty: {}", sPath);
		return false;
	}

	if (ec)
	{
		kDebug(2, ec.message());
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
			kDebug(2, "entry exists, but is not a file: {}", sPath);
			return false;   // <-- exists, but is not a file
		}
	}
	else if (bAsDirectory)
	{
		if ((StatStruct.st_mode & S_IFDIR) == 0)
		{
			kDebug(2, "entry exists, but is not a directory: {}", sPath);
			return false;   // <-- exists, but is not a directory
		}
		else
		{
			return true;
		}
	}
	if (!bTestForEmptyFile)
	{
		return true;    // <-- exists and is a file
	}
	if (StatStruct.st_size <= 0)
	{
		kDebug(2, "entry exists, but is empty: {}", sPath);
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
			return true;
		}
	}
	else
	{
		if (fs::create_directories(kToFilesystemPath(sPath), ec))
		{
			return true;
		}
	}

	if (ec)
	{
		kDebug(2, "{}: {}", sPath, ec.message());
	}
	else
	{
		kDebug(2, "failure creating '{}', but no errorcode", sPath);
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

	// else test each part of the directory chain
	std::vector<KStringView> Parts;
	kSplit(Parts, sPath, detail::kAllowedDirSep, "", 0, true, false);

	KString sNewPath;
	for (const auto& it : Parts)
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
#ifdef DEKAF2_HAS_STD_FILESYSTEM

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
#ifdef DEKAF2_IS_OSX
	return StatStruct.st_mtimespec.tv_sec;
#else
	return StatStruct.st_mtim.tv_sec;
#endif

#endif

} // kGetLastMod

//-----------------------------------------------------------------------------
size_t kFileSize(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	auto size = fs::file_size(kToFilesystemPath(sFilePath), ec);
	if (ec)
	{
		kDebug(2, "{}: {}", sFilePath, ec.message());
		return npos;
	}
	return size;

#else

	struct stat StatStruct;
	if (stat (sFilePath.c_str(), &StatStruct) < 0)
	{
		return npos;  // <-- file doesn't exist
	}
	return StatStruct.st_size;
	
#endif

} // kFileSize

//-----------------------------------------------------------------------------
KDirectory::DirEntry::DirEntry(KStringView BasePath, KStringView Name, EntryType Type)
//-----------------------------------------------------------------------------
: m_Path(BasePath)
, m_Type(Type)
{
	if (!BasePath.empty())
	{
		m_Path += kDirSep;
	}
	auto iPath = m_Path.size();
	m_Path += Name;
	m_Filename = m_Path.ToView(iPath);

} // DirEntry ctor

//-----------------------------------------------------------------------------
void KDirectory::clear()
//-----------------------------------------------------------------------------
{
	m_DirEntries.clear();

} // clear

//-----------------------------------------------------------------------------
size_t KDirectory::Open(KStringViewZ sDirectory, EntryType Type)
//-----------------------------------------------------------------------------
{
	clear();

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
			case EntryType::BLOCK:
				dtype = fs::file_type::block;
				break;
			case EntryType::CHARACTER:
				dtype = fs::file_type::character;
				break;
			case EntryType::DIRECTORY:
				dtype = fs::file_type::directory;
				break;
			case EntryType::FIFO:
				dtype = fs::file_type::fifo;
				break;
			case EntryType::LINK:
				dtype = fs::file_type::symlink;
				break;
			case EntryType::ALL:
			case EntryType::REGULAR:
				dtype = fs::file_type::regular;
				break;
			case EntryType::SOCKET:
				dtype = fs::file_type::socket;
				break;
			default:
			case EntryType::OTHER:
				dtype = fs::file_type::not_found;
				break;
		}

		if (Type == EntryType::ALL)
		{
			fs::file_type ftype = Entry.symlink_status(ec).type();

			if (ec)
			{
				kDebug(2, "{}: {}", sDirectory, ec.message());
				break;
			}

			EntryType ET;

			switch (ftype)
			{
				case fs::file_type::block:
					ET = EntryType::BLOCK;
					break;
				case fs::file_type::character:
					ET = EntryType::CHARACTER;
					break;
				case fs::file_type::directory:
					ET = EntryType::DIRECTORY;
					break;
				case fs::file_type::fifo:
					ET = EntryType::FIFO;
					break;
				case fs::file_type::symlink:
					ET = EntryType::LINK;
					break;
				case fs::file_type::regular:
					ET = EntryType::REGULAR;
					break;
				case fs::file_type::socket:
					ET = EntryType::SOCKET;
					break;
				default:
				case fs::file_type::not_found:
					ET = EntryType::OTHER;
					break;
			}
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().u8string(), ET);
		}
		else if (Entry.symlink_status().type() == dtype)
		{
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().u8string(), Type);
		}

	}

#else

	unsigned char dtype;
	switch (Type)
	{
		case EntryType::BLOCK:
			dtype = DT_BLK;
			break;
		case EntryType::CHARACTER:
			dtype = DT_CHR;
			break;
		case EntryType::DIRECTORY:
			dtype = DT_DIR;
			break;
		case EntryType::FIFO:
			dtype = DT_FIFO;
			break;
		case EntryType::LINK:
			dtype = DT_LNK;
			break;
		case EntryType::ALL:
		case EntryType::REGULAR:
			dtype = DT_REG;
			break;
		case EntryType::SOCKET:
			dtype = DT_SOCK;
			break;
		case EntryType::OTHER:
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
			if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, ".."))
			{
				if (Type == EntryType::ALL)
				{
					EntryType ET;
					switch (dir->d_type)
					{
						case DT_BLK:
							ET = EntryType::BLOCK;
							break;
						case DT_CHR:
							ET = EntryType::CHARACTER;
							break;
						case DT_DIR:
							ET = EntryType::DIRECTORY;
							break;
						case DT_FIFO:
							ET = EntryType::FIFO;
							break;
						case DT_LNK:
							ET = EntryType::LINK;
							break;
						case DT_REG:
							ET = EntryType::REGULAR;
							break;
						case DT_SOCK:
							ET = EntryType::SOCKET;
							break;
						default:
						case DT_UNKNOWN:
							ET = EntryType::OTHER;
							break;
					}
					m_DirEntries.emplace_back(sDirectory, dir->d_name, ET);
				}
				else if (dir->d_type == dtype)
				{
					m_DirEntries.emplace_back(sDirectory, dir->d_name, Type);
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
size_t KDirectory::Match(EntryType Type, bool bRemoveMatches)
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
bool KDirectory::Find(KStringView sWildCard) const
//-----------------------------------------------------------------------------
{
	KRegex Regex(kWildCard2Regex(sWildCard));
	for (auto& it : m_DirEntries)
	{
		if (Regex.Matches(it.Filename()))
		{
			return true;
		}
	}
	return false;

} // Find

//-----------------------------------------------------------------------------
void KDirectory::Sort()
//-----------------------------------------------------------------------------
{
	std::sort(m_DirEntries.begin(), m_DirEntries.end());

} // Sort

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

	fs::space_info fsinfo = fs::space(kToFilesystemPath(sPath), ec);

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

	uint64_t bsize = static_cast<uint64_t>(fsinfo.f_frsize);
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

		std::vector<KStringView> Words;
		kSplit(Words, sLine, " \t");

		if (Words.size() < 6)
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
bool kWriteFile (KStringViewZ sPath, KStringView sContents, int iMode /* = DEKAF2_MODE_CREATE_FILE */)
//-----------------------------------------------------------------------------
{
	{
		KOutFile file(sPath);
		if (!file.is_open())
		{
			kWarning ("kWriteFile: cannot open file: {}", sPath);
			return (false);
		}

		file.Write(sContents);
	}

	if (iMode != DEKAF2_MODE_CREATE_FILE)
	{
		kChangeMode (sPath, iMode);
	}

	return (true);

} // kWriteFile

//-----------------------------------------------------------------------------
bool kReadFile (KStringViewZ sPath, KString& sContents, bool bToUnixLineFeeds)
//-----------------------------------------------------------------------------
{
	kDebug (2, "{}", sPath);

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
			kSplit(CWD, sCWD, detail::kAllowedDirSep, "");
			// and add it to the normalized path
			for (auto it : CWD)
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

	std::vector<KStringView> Component;

	// split into path components
	kSplit(Component, sPath, detail::kAllowedDirSep, "");

	bool bWarned { false };

	for (auto it : Component)
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

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARS
namespace detail {
constexpr KStringViewZ kLineRightTrims;
constexpr KStringViewZ kAllowedDirSep;
constexpr KStringViewZ kCurrentDir;
constexpr KStringViewZ kCurrentDirWithSep;
}
#endif

} // end of namespace dekaf2

