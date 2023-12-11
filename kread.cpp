/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "kread.h"
#include "klog.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
std::size_t kRead(int fd, void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (fd < 0)
	{
		kDebug(1, "no file descriptor");
		return 0;
	}

	auto chBuffer = static_cast<char*>(sBuffer);

	std::size_t iWant   { iCount };
	std::size_t iTotal  { 0 };

	for(;;)
	{
#ifndef DEKAF2_IS_WINDOWS
		auto iRead = ::read(fd, chBuffer, iWant);
#else
		auto iRead = _read(fd, chBuffer,
							static_cast<uint32_t>((iWant > std::numeric_limits<int32_t>::max())
												  ? std::numeric_limits<int32_t>::max()
												  : iWant));
#endif
		// iRead == 0 == EOF
		// iRead  < 0 == error
		// iRead  > 0 == read bytes
		// iRead  < iWant: check for eof (by doing one more read and check for 0)

		// We use these readers and writers in pipes and shells
		// which may die and generate a SIGCHLD, which interrupts
		// file reads and writes. Also, when reading the output of
		// a pipe or socket, we may read faster than the input gets
		// generated, therefore we have to test for EOF (iRead == 0)

		if (DEKAF2_LIKELY(iRead > 0))
		{
			iTotal   += iRead;

			if (DEKAF2_LIKELY(iTotal >= iCount))
			{
				// we read the requested amount, this is the normal exit
				break;
			}

			chBuffer += iRead;
			iWant    -= iRead;
		}
		else if (iRead == 0)
		{
			// EOF, we read less than requested, but that is OK as we
			// reached the end of file
			break;
		}
		else // if (iRead < 0)
		{
			// repeat if we got interrupted
			if (errno != EINTR)
			{
				// else we got another error
				kDebug(1, "cannot read from file: {}", strerror(errno));
				// invalidate return, we did not fullfill our contract
				iTotal = 0;
				break;
			}
		}
	}

	return iTotal;

} // kRead

//-----------------------------------------------------------------------------
std::size_t kRead(FILE* fp, void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (!fp)
	{
		kDebug(1, "no file descriptor");
		return 0;
	}

	std::size_t iRead;

	do
	{
		iRead = std::fread(sBuffer, 1, iCount, fp);
	}
	while (iRead == 0 && errno == EINTR);

	// we use these readers and writers in pipes and shells
	// which may die and generate a SIGCHLD, which interrupts
	// file reads and writes..
	// see https://stackoverflow.com/a/53245808 for a discussion of
	// possible behavior

	if (iRead == 0)
	{
		kDebug(1, "cannot read from file: {}", strerror(errno));
	}
	else if (iRead < iCount)
	{
		// do some logging
		kDebug(1, "could only read {} bytes instead of {} from file: {}", iRead, iCount, strerror(errno));
	}

	return iRead;

} // kRead

DEKAF2_NAMESPACE_END
