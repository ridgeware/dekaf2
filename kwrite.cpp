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

#include "kwrite.h"
#include "klog.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
std::size_t kWrite(int fd, const void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (fd < 0)
	{
		kDebug(1, "no file descriptor");
		return 0;
	}

	if (!iCount)
	{
		return 0;
	}

	std::streamsize iWrote { 0 };

#ifndef DEKAF2_IS_WINDOWS

	do
	{
		iWrote = ::write(fd, sBuffer, iCount);
	}
	while (iWrote < 0 && errno == EINTR);
	// we use these readers and writers in pipes and shells
	// which may die and generate a SIGCHLD, which interrupts
	// file reads and writes..

	if (iWrote < 0)
	{
		kDebug(1, "cannot write to file: {}", strerror(errno));

		return 0;
	}
	else if (static_cast<std::size_t>(iWrote) != iCount)
	{
		// do some logging
		kDebug(1, "could only write {} bytes instead of {} to file: {}", iWrote, iCount, strerror(errno));
	}

	return iWrote;

#else // IS WINDOWS

	// we might need to loop on the write, as _write() only allows INT32_MAX sized writes

	auto chBuffer = static_cast<const char*>(sBuffer);

	auto iWrite { iCount };
	std::size_t iTotal { 0 };

	for(;;)
	{
		iWrote = _write(fd, chBuffer,
						 static_cast<uint32_t>((iWrite > std::numeric_limits<int32_t>::max())
											   ? std::numeric_limits<int32_t>::max()
											   : iWrite));

		if (DEKAF2_LIKELY(iWrote > 0))
		{
			iTotal += iWrote;

			if (DEKAF2_LIKELY(iTotal >= iCount))
			{
				// all written
				break;
			}

			chBuffer += iWrote;
			iWrite   -= iWrote;
		}
		else if (iWrote == 0)
		{
			break;
		}
		else // if (iWrote < 0)
		{
			// repeat if we got interrupted
			if (errno != EINTR)
			{
				// else we got another error
				kDebug(1, "cannot write to file: {}", strerror(errno));
				// invalidate return, we did not fullfill our contract
				return 0;
			}
		}
	}

	if (static_cast<std::size_t>(iTotal) != iCount)
	{
		// do some logging
		kDebug(1, "could only write {} bytes instead of {} to file: {}", iTotal, iCount, strerror(errno));
	}

	return iTotal;

#endif

} // kWrite

//-----------------------------------------------------------------------------
std::size_t kWrite(FILE* fp, const void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (!fp)
	{
		kDebug(1, "no file descriptor");
		return 0;
	}

	if (!iCount)
	{
		return 0;
	}

	std::size_t iWrote { 0 };

	do
	{
		iWrote = std::fwrite(sBuffer, 1, iCount, fp);
	}
	while (iWrote == 0 && errno == EINTR);
	
	// we use these readers and writers in pipes and shells
	// which may die and generate a SIGCHLD, which interrupts
	// file reads and writes..
	// see https://stackoverflow.com/a/53245808 for a discussion of
	// possible behavior

	if (iWrote == 0)
	{
		kDebug(1, "cannot write to file: {}", strerror(errno));
	}
	else if (iWrote < iCount)
	{
		// do some logging
		kDebug(1, "could only write {} bytes instead of {} to file: {}", iWrote, iCount, strerror(errno));
	}

	return iWrote;

} // kWrite

//-----------------------------------------------------------------------------
std::size_t kWrite(std::ostream& Stream, const void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	std::size_t iWrote { 0 };

	if (DEKAF2_LIKELY(streambuf != nullptr))
	{
		if (iCount)
		{
			iWrote = static_cast<std::size_t>(streambuf->sputn(static_cast<const std::ostream::char_type*>(sBuffer), iCount));
		}
	}

	if (DEKAF2_UNLIKELY(iWrote != iCount))
	{
		Stream.setstate(std::ios_base::badbit);
	}

	return iWrote;

} // kWrite

//-----------------------------------------------------------------------------
std::size_t kWrite(std::ostream& Stream, char ch)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	std::ostream::int_type iCh;

	if (DEKAF2_LIKELY(streambuf != nullptr))
	{
		iCh = streambuf->sputc(ch);
	}
	else
	{
		iCh = std::ostream::traits_type::eof();
	}

	if (DEKAF2_UNLIKELY(std::ostream::traits_type::eq_int_type(iCh, std::ostream::traits_type::eof())))
	{
		Stream.setstate(std::ios_base::badbit);
		return 0;
	}

	return 1;

} // kWrite

//-----------------------------------------------------------------------------
ssize_t kGetWritePosition(const std::ostream& Stream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() ? Stream.rdbuf()->pubseekoff(0, std::ios_base::cur, std::ios_base::out) : std::streambuf::pos_type(-1);
	DEKAF2_LOG_EXCEPTION
	
	return -1;

} // kGetReadPosition

//-----------------------------------------------------------------------------
bool kSetWritePosition(std::ostream& Stream, std::size_t iPos)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(iPos, std::ios_base::beg, std::ios_base::out) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kSetWritePosition

//-----------------------------------------------------------------------------
bool kForward(std::ostream& Stream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::end, std::ios_base::out) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kForward

//-----------------------------------------------------------------------------
bool kForward(std::ostream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(iCount, std::ios_base::cur, std::ios_base::out) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kForward

//-----------------------------------------------------------------------------
bool kRewind(std::ostream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	auto iOffset = static_cast<std::streambuf::off_type>(iCount) * -1;
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(iOffset, std::ios_base::cur, std::ios_base::out) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kRewind

//-----------------------------------------------------------------------------
bool kFlush(FILE* fp)
//-----------------------------------------------------------------------------
{
	if (::fflush(fp) != 0)
	{
		kDebug(1, "cannot flush: {}", strerror(errno));
		return false;
	}

	return true;

} // kFlush

//-----------------------------------------------------------------------------
bool kFlush(std::ostream& Stream)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	if (DEKAF2_LIKELY(streambuf != nullptr))
	{
		return streambuf->pubsync() == 0;
	}

	return false;

} // kFlush

DEKAF2_NAMESPACE_END
