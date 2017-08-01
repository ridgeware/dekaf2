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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <experimental/filesystem>

#include "kfile.h"
#include "kstring.h"
#include "klog.h"
#include "kreader.h"

namespace dekaf2
{

namespace fs = std::experimental::filesystem;

namespace KFile
{

//-----------------------------------------------------------------------------
bool Exists (const KString& sPath, KFileFlags iFlags/*=NONE*/)
//-----------------------------------------------------------------------------
{
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

	if ((iFlags & TEST0) == 0)
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

} // Exists

//-----------------------------------------------------------------------------
bool GetContent (KString& sContent, const KString& sPath, KFileFlags eFlags/*=TEXT*/)
//-----------------------------------------------------------------------------
{
	KFileReader File(sPath, std::ios_base::in, ((eFlags & TEXT) != 0) ? "\r\n\t " : "", '\n');
	return File.ReadAll(sContent);

} // GetContent

//-----------------------------------------------------------------------------
size_t GetSize (const KString& sPath)
//-----------------------------------------------------------------------------
{
	KFileReader File(sPath);
	return File.GetSize();

} // GetSize

} // end of namespace KFile

} // end of namespace dekaf2

