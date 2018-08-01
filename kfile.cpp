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

#include "bits/kfilesystem.h"
#include "kfile.h"
#include "kstring.h"
#include "klog.h"

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
			return sFilePath.substr(0, (pos + 1));
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
time_t kGetLastMod(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	auto ftime = fs::last_write_time(sFilePath.c_str(), ec);
	if (ec)
	{
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
}

//-----------------------------------------------------------------------------
size_t kGetNumBytes(KStringViewZ sFilePath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;

	auto size = fs::file_size(sFilePath.c_str(), ec);
	if (ec)
	{
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
}

} // end of namespace dekaf2

