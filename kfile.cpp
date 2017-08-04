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

#include "kcppcompat.h"

#ifdef DEKAF2_HAS_CPP_17
 #include <experimental/filesystem>
#else
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <cstdio>
#endif

#include "kfile.h"
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

#ifdef DEKAF2_HAS_CPP_17
namespace fs = std::experimental::filesystem;
#endif

//-----------------------------------------------------------------------------
bool kExists (const KString& sPath, bool bTestForEmptyFile /* = false */ )
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_17
	std::error_code ec;

	fs::file_status status = fs::status(sPath.s(), ec);

	if (ec)
	{
		return false;
	}

	if (!fs::exists(status))
	{
		return false;
	}

	if (fs::is_directory(status))
	{
		return false;
	}

	if (!bTestForEmptyFile)
	{
		// exists and is a file
		return true;
	}

	if (fs::is_empty(sPath.s(), ec))
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
		return false;  // <-- file doesn't exist
	}
	if (StatStruct.st_mode & S_IFDIR)
	{
		return false;   // <-- exists, but is a directory
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
} // Exists


//-----------------------------------------------------------------------------
KString kGetCWD ()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_17
	return fs::current_path().string();
#else
	enum { MAX_PATH = 1024 };
	KString str(MAX_PATH, '\0');
	if (::getcwd(&str[0], str.size()-1))
	{
		size_t iPathLen = ::strlen(str.c_str());
		str.erase(iPathLen);
	}
	else
	{
		KLog().warning("cannot get current working directory: {}", ::strerror(errno));
		str.erase();
	}
	return str;
#endif
} // kGetCWD

} // end of namespace dekaf2

