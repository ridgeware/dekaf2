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

#include "kwriter.h"
#include "kreader.h"
#include "klog.h"
#include "kthreadpool.h"
#include "kurl.h"
#include "kstreambuf.h"
#include <iostream>
#include <fstream>

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

KOutStream KErr(std::cerr);
KOutStream KOut(std::cout);

//-----------------------------------------------------------------------------
std::ostream& kGetNullOStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<std::ostream> NullStream = std::make_unique<std::ostream>(&KNullStreamBuf::Create());
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
KOutStream& kGetNullOutStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<KOutStream> NullStream = std::make_unique<KOutStream>(kGetNullOStream(), "\n", true);
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
std::size_t kWriteToFileDesc(int fd, const void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (fd < 0)
	{
		kDebug(1, "no file descriptor");
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

} // kWriteToFileDesc

//-----------------------------------------------------------------------------
std::size_t kWriteToFilePtr(FILE* fp, const void* sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (!fp)
	{
		kDebug(1, "no file descriptor");
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

} // kWriteToFilePtr

//-----------------------------------------------------------------------------
/// value constructor
KOutStream::KOutStream(std::ostream& OutStream, KStringView sLineDelimiter, bool bImmutable)
//-----------------------------------------------------------------------------
	: m_OutStream     (&OutStream    )
	, m_sLineDelimiter(sLineDelimiter)
	, m_bImmutable    (bImmutable    )
{
}

//-----------------------------------------------------------------------------
/// Write a character. Returns stream reference that resolves to false on failure
KOutStream::self_type& KOutStream::Write(KString::value_type ch)
//-----------------------------------------------------------------------------
{
	auto streambuf = ostream().rdbuf();

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
		ostream().setstate(std::ios_base::badbit);
	}

	return *this;

} // Write

//-----------------------------------------------------------------------------
/// Write a range of characters. Returns stream reference that resolves to false on failure.
KOutStream::self_type& KOutStream::Write(const void* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	auto streambuf = ostream().rdbuf();

	std::size_t iWrote { 0 };

	if (DEKAF2_LIKELY(streambuf != nullptr))
	{
		iWrote = static_cast<size_t>(streambuf->sputn(static_cast<const std::ostream::char_type*>(pAddress), iCount));
	}

	if (DEKAF2_UNLIKELY(iWrote != iCount))
	{
		ostream().setstate(std::ios_base::badbit);
	}

	return *this;

} // Write

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns stream reference that resolves to false on failure.
KOutStream::self_type& KOutStream::Write(KInStream& Stream, size_t iCount)
//-----------------------------------------------------------------------------
{
	std::array<char, KDefaultCopyBufSize> Buffer;

	for (;iCount;)
	{
		auto iChunk = std::min(Buffer.size(), iCount);

		auto iReadChunk = Stream.Read(Buffer.data(), iChunk);

		if (!Write(Buffer.data(), iReadChunk).Good())
		{
			break;
		}
		
		iCount -= iReadChunk;

		if (iReadChunk != iChunk)
		{
			break;
		}
	}

	return *this;

} // Write

//-----------------------------------------------------------------------------
/// Fixates eol settings
KOutStream& KOutStream::SetWriterImmutable()
//-----------------------------------------------------------------------------
{
	m_bImmutable = true;
	return *this;

} // SetWriterImmutable

//-----------------------------------------------------------------------------
KOutStream& KOutStream::SetWriterEndOfLine(KStringView sLineDelimiter)
//-----------------------------------------------------------------------------
{
	if (m_bImmutable)
	{
		kDebug(2, "line delimiter is made immutable - cannot change to {}", sLineDelimiter);
	}
	else
	{
		m_sLineDelimiter = sLineDelimiter;
	}
	return *this;

} // SetWriterEndOfLine


template class KWriter<std::ofstream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KOutStream> kOpenOutStream(KStringViewZ sSchema, std::ios::openmode openmode)
//-----------------------------------------------------------------------------
{
	if (sSchema == KLog::STDOUT)
	{
		return std::make_unique<KOutStream>(std::cout);
	}
	else if (sSchema == KLog::STDERR)
	{
		return std::make_unique<KOutStream>(std::cerr);
	}
	else
	{
		return std::make_unique<KOutFile>(sSchema, openmode);
	}

} // kOpenOutStream

//-----------------------------------------------------------------------------
void kLogger(KOutStream& Stream, KString sMessage, bool bFlush)
//-----------------------------------------------------------------------------
{
	static KThreadPool LogWriter(1);

	LogWriter.push([](KOutStream* Stream, KString sMessage, bool bFlush)
	{
		Stream->WriteLine(sMessage);

		if (bFlush)
		{
			Stream->Flush();
		}

	}, &Stream, std::move(sMessage), bFlush);

} // kLogger

DEKAF2_NAMESPACE_END
