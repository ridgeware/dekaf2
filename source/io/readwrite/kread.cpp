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

#include <dekaf2/io/readwrite/kread.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <limits>

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

	if (!iCount)
	{
		return 0;
	}

	// we use these readers and writers in pipes and shells
	// which may die and generate a SIGCHLD, which interrupts
	// file reads and writes..
	// see https://stackoverflow.com/a/53245808 for a discussion of
	// possible behavior

	auto chBuffer = static_cast<char*>(sBuffer);

	std::size_t iWant  { iCount };
	std::size_t iTotal { 0 };

	for (;;)
	{
		auto iRead = std::fread(chBuffer, 1, iWant, fp);

		if (DEKAF2_LIKELY(iRead > 0))
		{
			iTotal += iRead;

			if (DEKAF2_LIKELY(iTotal >= iCount))
			{
				// we read the requested amount, this is the normal exit
				break;
			}

			chBuffer += iRead;
			iWant    -= iRead;
		}

		if (DEKAF2_UNLIKELY(iRead < iWant))
		{
			if (std::feof(fp))
			{
				// EOF, we read less than requested, but that is OK as we
				// reached the end of file
				break;
			}

			if (std::ferror(fp))
			{
				// repeat if we got interrupted
				if (errno == EINTR)
				{
					std::clearerr(fp);
					continue;
				}

				// else we got another error
				kDebug(1, "cannot read from file: {}", strerror(errno));
				// invalidate return, we did not fullfill our contract
				iTotal = 0;
				break;
			}

			// unexpected short read without feof/ferror - avoid infinite loop
			break;
		}
	}

	return iTotal;

} // kRead

//-----------------------------------------------------------------------------
std::size_t kRead(std::istream& Stream, void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(streambuf == nullptr))
	{
		Stream.setstate(std::ios::failbit);
		return 0;
	}

	if (!iCount)
	{
		return 0;
	}

	auto iRead = streambuf->sgetn(static_cast<std::istream::char_type*>(sBuffer), iCount);

	if (DEKAF2_UNLIKELY(iRead < 0))
	{
		Stream.setstate(std::ios::badbit);
		iRead = 0;
	}
	else if (DEKAF2_UNLIKELY(static_cast<std::size_t>(iRead) < iCount))
	{
		Stream.setstate(std::ios::eofbit);
	}

	return static_cast<std::size_t>(iRead);

} // kRead

//-----------------------------------------------------------------------------
std::istream::int_type kRead(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(streambuf == nullptr))
	{
		Stream.setstate(std::ios::failbit);

		return std::istream::traits_type::eof();
	}

	auto iCh = streambuf->sbumpc();

	if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
	{
		Stream.setstate(std::ios::eofbit);
	}

	return iCh;

} // kRead

//-----------------------------------------------------------------------------
std::size_t kRead(std::istream& Stream, char& ch)
//-----------------------------------------------------------------------------
{
	auto iCh = kRead(Stream);

	if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
	{
		ch = 0;
		return 0;
	}

	ch = std::istream::traits_type::to_char_type(iCh);
	return 1;

} // kRead

//-----------------------------------------------------------------------------
std::size_t kUnRead(std::istream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		Stream.setstate(std::ios::failbit);

		return iCount;
	}

	for (; iCount > 0; --iCount)
	{
		auto iCh = streambuf->sungetc();

		if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
		{
			// could not unread - note that this does not have to be a stream error,
			// therefore we do not set badbit or failbit
			return iCount; // > 0, fail
		}
	}

	// make sure we are no more in eof state if we were before
	Stream.clear();

	return iCount; // 0, success

} // kUnRead

//-----------------------------------------------------------------------------
ssize_t kGetSize(std::istream& Stream, bool bFromStart)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {

		auto streambuf = Stream.rdbuf();

		if (DEKAF2_UNLIKELY(!streambuf))
		{
			kDebug(1, "no streambuf");
			return -1;
		}

		auto curPos = streambuf->pubseekoff(0, std::ios_base::cur, std::ios_base::in);

		if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "istream is not seekable ({})", 1);
			return curPos;
		}

		auto endPos = streambuf->pubseekoff(0, std::ios_base::end, std::ios_base::in);

		if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "istream is not seekable ({})", 2);
			return endPos;
		}

		if (endPos != curPos)
		{
			streambuf->pubseekpos(curPos, std::ios_base::in);
		}

		if (bFromStart)
		{
			return endPos;
		}
		else
		{
			return endPos - curPos;
		}
	}

	DEKAF2_CATCH (std::exception& e)
	{
		kDebug(3, e.what());
	}

	return -1;

} // kGetSize

//-----------------------------------------------------------------------------
ssize_t kGetReadPosition(const std::istream& Stream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() ? Stream.rdbuf()->pubseekoff(0, std::ios_base::cur, std::ios_base::in) : std::streambuf::pos_type(-1);
	DEKAF2_LOG_EXCEPTION

	return -1;

} // kGetReadPosition

//-----------------------------------------------------------------------------
bool kSetReadPosition(std::istream& Stream, std::size_t iPos)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	if (iPos > static_cast<std::size_t>(std::numeric_limits<std::streambuf::off_type>::max()))
	{
		return false;
	}

	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(static_cast<std::streambuf::off_type>(iPos), std::ios_base::beg, std::ios_base::in) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kSetReadPosition

//-----------------------------------------------------------------------------
bool kForward(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::end, std::ios_base::in) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kForward

//-----------------------------------------------------------------------------
bool kForward(std::istream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	if (iCount > static_cast<std::size_t>(std::numeric_limits<std::streambuf::off_type>::max()))
	{
		return false;
	}

	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(static_cast<std::streambuf::off_type>(iCount), std::ios_base::cur, std::ios_base::in) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kForward

//-----------------------------------------------------------------------------
bool kRewind(std::istream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	if (iCount > static_cast<std::size_t>(std::numeric_limits<std::streambuf::off_type>::max()))
	{
		return false;
	}

	auto iOffset = -static_cast<std::streambuf::off_type>(iCount);
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(iOffset, std::ios_base::cur, std::ios_base::in) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kRewind

//-----------------------------------------------------------------------------
bool kMoveToLine(std::istream& Stream, std::size_t iLine, bool bBackward, char eol)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		return false;
	}

	if (!bBackward)
	{
		// forward: skip iLine line breaks from current position
		for (std::size_t iCount = 0; iCount < iLine; )
		{
			auto iCh = streambuf->sbumpc();

			if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
			{
				Stream.setstate(std::ios::eofbit);
				return false;
			}

			if (std::istream::traits_type::to_char_type(iCh) == eol)
			{
				++iCount;
			}
		}

		Stream.clear();
		return true;
	}
	else
	{
		// backward: move back from current position counting line breaks
		// we need to move back one extra position first, because the current
		// position might be right after a line break

		auto curPos = streambuf->pubseekoff(0, std::ios_base::cur, std::ios_base::in);

		if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			return false;
		}

		if (iLine == 0)
		{
			// special case: seek to end
			return streambuf->pubseekoff(0, std::ios_base::end, std::ios_base::in)
				!= std::streambuf::pos_type(std::streambuf::off_type(-1));
		}

		auto iPos = static_cast<std::size_t>(curPos);

		if (iPos == 0)
		{
			// already at beginning, cannot go backward
			return false;
		}

		// move backward one char at a time, counting line breaks
		std::size_t iCount = 0;

		while (iPos > 0)
		{
			--iPos;

			if (streambuf->pubseekpos(static_cast<std::streambuf::pos_type>(iPos), std::ios_base::in)
				== std::streambuf::pos_type(std::streambuf::off_type(-1)))
			{
				return false;
			}

			auto iCh = streambuf->sgetc();

			if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
			{
				return false;
			}

			if (std::istream::traits_type::to_char_type(iCh) == eol)
			{
				++iCount;

				if (iCount >= iLine)
				{
					// position after the line break we just found
					streambuf->sbumpc();
					Stream.clear();
					return true;
				}
			}
		}

		// reached beginning of file before finding enough line breaks -
		// if we found iLine-1 breaks, the first line is the target
		if (iCount + 1 >= iLine)
		{
			// we are at position 0, which is the start of the first line
			Stream.clear();
			return true;
		}

		// not enough lines - restore original position
		streambuf->pubseekpos(static_cast<std::streambuf::pos_type>(curPos), std::ios_base::in);
		return false;
	}

	DEKAF2_LOG_EXCEPTION

	return false;

} // kMoveToLine

//-----------------------------------------------------------------------------
bool kGoToLine(std::istream& Stream, std::size_t iLine, bool bFromEnd, char eol)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		return false;
	}

	if (!bFromEnd)
	{
		// seek to beginning, then skip iLine line breaks
		if (streambuf->pubseekpos(0, std::ios_base::in)
			== std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			return false;
		}

		Stream.clear();

		return kMoveToLine(Stream, iLine, false, eol);
	}
	else
	{
		// seek to end, then move backward iLine+1 line breaks
		// (iLine 0 = last line, so we need to find 1 line break going backward)
		if (streambuf->pubseekoff(0, std::ios_base::end, std::ios_base::in)
			== std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			return false;
		}

		Stream.clear();

		return kMoveToLine(Stream, iLine + 1, true, eol);
	}

	DEKAF2_LOG_EXCEPTION

	return false;

} // kGoToLine

DEKAF2_NAMESPACE_END
