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
#include "kstring.h"
#include "klog.h"
#include "kregex.h"
#include "ksplit.h"
#include "kinshell.h"


namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kExists (KStringViewZ sPath, bool bAsFile, bool bAsDirectory, bool bTestForEmptyFile)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	fs::file_status status = fs::status(sPath.c_str(), ec);

	if (ec)
	{
		kDebug(2, ec.message());
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
			return false;
		}
	}
	else if (bAsDirectory)
	{
		if (!fs::is_directory(status))
		{
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

	if (fs::is_empty(sPath.c_str(), ec))
	{
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
		if (StatStruct.st_mode & S_IFDIR)
		{
			return false;   // <-- exists, but is a directory
		}
	}
	else if (bAsDirectory)
	{
		if ((StatStruct.st_mode & S_IFDIR) == 0)
		{
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
			return sFilePath.substr(pos + 1);
		}
	}

	return "";

} // kBasename

//-----------------------------------------------------------------------------
KStringView kBasename(KStringView sFilePath)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, return the "basename":
	// that is, just the filename itself without any directory elements
	if (!sFilePath.empty())
	{
#ifdef _WIN32
		auto pos = sFilePath.find_last_of("/\\:");
#else
		auto pos = sFilePath.rfind('/');
#endif

		if (pos != KStringView::npos)
		{
			return sFilePath.substr(pos + 1);
		}
	}

	return sFilePath;

} // kBasename

//-----------------------------------------------------------------------------
KStringView kDirname(KStringView sFilePath, bool bWithSlash)
//-----------------------------------------------------------------------------
{
	// Given a filesystem path, return the "dirname":
	// that is, the directory name component of the file path
	if (!sFilePath.empty())
	{
#ifdef _WIN32
		auto pos = sFilePath.find_last_of("/\\:");
#else
		auto pos = sFilePath.rfind('/');
#endif

		if (pos != KStringView::npos)
		{
			return sFilePath.substr(0, pos);
		}
	}

#ifdef _WIN32
	return (bWithSlash) ? ".\\" : ".";
#else
	return (bWithSlash) ? "./" : ".";
#endif

}  // kDirname()

//-----------------------------------------------------------------------------
bool kRemove (KStringViewZ sPath, bool bDir)
//-----------------------------------------------------------------------------
{
	if (bDir)
	{
		if (kFileExists (sPath))
		{
			kDebugLog (1, "cannot remove file with kRemoveDir: {}", sPath);
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
	fs::permissions (sPath.c_str(), fs::perms::all, ec); // chmod (ignore failures)
	ec.clear();
	if (bDir)
	{
		fs::remove_all (sPath.c_str(), ec);
	}
	else
	{
		fs::remove (sPath.c_str(), ec);
	}
	if (ec)
	{
		kDebugLog (1, "remove failed: {}: {}", sPath, ec.message());
	}
#else
	if (unlink (sPath.c_str()) != 0)
	{
		chmod (sPath.c_str(), S_IRUSR|S_IWUSR|S_IXUSR | S_IRGRP|S_IWGRP|S_IXGRP | S_IROTH|S_IWOTH|S_IXOTH);
		if (unlink (sPath.c_str()) != 0)
		{
			if (bDir && (rmdir (sPath.c_str()) != 0))
			{
				KString sCmd;
				sCmd.Format ("rm -rf \"{}\"", sPath);
				if (system (sCmd.c_str()) != 0)
				{
					kDebugLog (1, "remove failed: {}: {}", sPath, strerror (errno));
				}
			}
		}
	}
#endif

	return (true);

} // kRemove

//-----------------------------------------------------------------------------
bool kCreateDir(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM

	std::error_code ec;

	if (fs::create_directories(sPath.c_str(), ec))
	{
		return true;
	}

	if (ec)
	{
		kDebug(2, ec.message());
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
	kSplit(Parts, sPath, "/\\", "", 0, true, false);

	KString sNewPath;
	for (const auto& it : Parts)
	{
		if (!it.empty())
		{
			if (sNewPath.empty())
			{
				if (sPath.front() == '/')
				{
					sNewPath += '/';
				}
			}
			else
			{
				sNewPath += '/';
			}
			sNewPath += it;

			if (!kDirExists(sNewPath))
			{
				if (::mkdir(sNewPath.c_str(), 0755))
				{
					kDebug(2, "cannot create directory: {}, {}", sPath, strerror(errno));
					return false;
				}
			}
		}
	}

	return true;

#endif

} // kCreateDir

//-----------------------------------------------------------------------------
time_t kGetLastMod(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	auto ftime = fs::last_write_time(sFilePath.c_str(), ec);
	if (ec)
	{
		kDebug(2, ec.message());
		return -1;
	}
	return decltype(ftime)::clock::to_time_t(ftime);
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
size_t kGetNumBytes(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	auto size = fs::file_size(sFilePath.c_str(), ec);
	if (ec)
	{
		kDebug(2, ec.message());
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

} // kGetNumBytes

//-----------------------------------------------------------------------------
KDirectory::DirEntry::DirEntry(KStringView BasePath, KStringView Name, EntryType Type)
//-----------------------------------------------------------------------------
: m_Path(BasePath)
, m_Type(Type)
{
	if (!BasePath.empty())
	{
		m_Path += '/';
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

	for (const auto& Entry : fs::directory_iterator(sDirectory.c_str(), ec))
	{
		if (ec)
		{
			kDebug(2, ec.message());
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
			case EntryType::OTHER:
				dtype = fs::file_type::not_found;
				break;
		}

		if (Type == EntryType::ALL)
		{
			fs::file_type ftype = Entry.symlink_status(ec).type();

			if (ec)
			{
				kDebug(2, ec.message());
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
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().c_str(), ET);
		}
		else if (Entry.symlink_status().type() == dtype)
		{
			m_DirEntries.emplace_back(sDirectory, Entry.path().filename().c_str(), Type);
		}

	}

#else

#ifdef __linux__
	auto len = sizeof(struct dirent);
#else
	auto name_max = ::pathconf(sDirectory.c_str(), _PC_NAME_MAX);
	if (name_max == -1)
	{
		name_max = 255;
	}
	auto len = offsetof(struct dirent, d_name) + name_max + 1;
#endif

	auto entry = std::make_unique<uint8_t[]>(len);

	auto dir = reinterpret_cast<struct dirent*>(entry.get());

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
		struct dirent* result;
		while ((::readdir_r(d, dir, &result)) == 0 && result != nullptr)
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
                                          return elem.Filename().empty() || elem.Filename()[0] == '.';
									  }),
                       m_DirEntries.end());

} // RemoveHidden

//-----------------------------------------------------------------------------
size_t KDirectory::Match(EntryType Type, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
									  m_DirEntries.end(),
									  [Type, bRemoveMatches](const DirEntries::value_type& elem)
									  {
										  return bRemoveMatches == (elem.Type() == Type);
									  }),
					   m_DirEntries.end());

	return size();

} // Match

//-----------------------------------------------------------------------------
size_t KDirectory::Match(KStringView sRegex, bool bRemoveMatches)
//-----------------------------------------------------------------------------
{
	KRegex Regex(sRegex);

	// erase-remove idiom..
	m_DirEntries.erase(std::remove_if(m_DirEntries.begin(),
                                      m_DirEntries.end(),
                                      [&Regex, bRemoveMatches](const DirEntries::value_type& elem)
                                      {
										  return bRemoveMatches == Regex.Matches(KStringView(elem.Filename()));
                                      }),
                       m_DirEntries.end());

	return size();

} // Match

//-----------------------------------------------------------------------------
bool KDirectory::Find(KStringView sName, bool bRemoveMatch)
//-----------------------------------------------------------------------------
{
	auto it = std::find_if(m_DirEntries.begin(),
                           m_DirEntries.end(),
                           [&sName](const DirEntries::value_type& elem)
                           {
                               return elem.Filename() == sName;
                           });

	if (it != m_DirEntries.end())
	{
		if (bRemoveMatch)
		{
			m_DirEntries.erase(it);
		}
		return true;
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

	fs::space_info fsinfo = fs::space(sPath.c_str(), ec);

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

} // end of namespace dekaf2

