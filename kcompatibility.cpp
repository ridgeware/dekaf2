/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2025, Ridgeware, Inc.
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
//
*/

#define _CRT_SECURE_NO_WARNINGS

#include "kcompatibility.h"

#if DEKAF2_IS_WINDOWS
	#include <io.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void strcpy_n(char* sTarget, const char* sSource, std::size_t iMax)
//-----------------------------------------------------------------------------
{
	// we substract one for the 0 byte
	if (sTarget && iMax--)
	{
		if (sSource)
		{
			while (iMax-- && *sSource)
			{
				*sTarget++ = *sSource++;
			}
		}
		*sTarget = '\0';
	}

} // strcpy_n

#ifdef DEKAF2_IS_WINDOWS
//-----------------------------------------------------------------------------
const char* strerror (int errnum)
//-----------------------------------------------------------------------------
{
	static thread_local std::array<char, 128> Buffer;

	if (::strerror_s(Buffer.data(), Buffer.size(), errnum) != 0)
	{
		strcpy_n(Buffer.data(), "unknown error", Buffer.size());
	}

	return Buffer.data();
}

//-----------------------------------------------------------------------------
int open(const char *path, int oflag)
//-----------------------------------------------------------------------------
{
	return ::_open(path, oflag);
}

//-----------------------------------------------------------------------------
int open(const char *path, int oflag, int mode)
//-----------------------------------------------------------------------------
{
	return ::_open(path, oflag, mode);
}

//-----------------------------------------------------------------------------
int read(int fd, void *buf, size_t nbyte)
//-----------------------------------------------------------------------------
{
	return ::_read(fd, buf, nbyte);
}
#endif


DEKAF2_NAMESPACE_END
