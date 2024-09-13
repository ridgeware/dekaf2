/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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
/// test for include file locations for std::filesystem, and include if
/// available

#include "../kdefinitions.h"

// Starting 09/2024 we do not use std::filesystem anymore except when
// not on POSIX systems. Reason is its limited functionality (no user
// and group modification, multiple system calls instead of just one
// stat() to gather file information), and the use of std::filesystem::path,
// which forces a copy of file names even on systems which use UTF8 for
// their fs strings.

#if !DEKAF2_IS_UNIX
	#if DEKAF2_HAS_CPP_17 && !DEKAF2_IS_CLANG
		#if DEKAF2_HAS_INCLUDE(<filesystem>)
			#include <filesystem>
			#define DEKAF2_HAS_STD_FILESYSTEM 1
			#define DEKAF2_FS_NAMESPACE std::filesystem
		#elif DEKAF2_HAS_INCLUDE(<experimental/filesystem>)
			#include <experimental/filesystem>
			#define DEKAF2_HAS_STD_FILESYSTEM 1
			#define DEKAF2_FS_NAMESPACE std::experimental::filesystem
		#endif
	#endif
#endif

#ifndef DEKAF2_HAS_STD_FILESYSTEM
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/statvfs.h>
	#include <unistd.h>
	#include <cstdio>
	#include <dirent.h>
	#include <fcntl.h>
#endif

#ifdef DEKAF2_FS_NAMESPACE
	namespace fs = DEKAF2_FS_NAMESPACE;
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_HAS_STD_FILESYSTEM

/// convert a UTF8 string into native path format
template<class String>
fs::path kToFilesystemPath(const String& sPath)
{
#if DEKAF2_HAS_CPP_20 && DEKAF2_HAS_U8STRING
	return fs::path(std::u8string(sPath.begin(), sPath.end()));
#else
	return fs::u8path(sPath.begin(), sPath.end());
#endif
}

#else

/// convert a UTF8 string into native path format
/// (this is the fallback version if std::filesystem is not supported)
template<class String>
const char* kToFilesystemPath(const String& sPath)
{
	return sPath.c_str();
}

#endif

DEKAF2_NAMESPACE_END
