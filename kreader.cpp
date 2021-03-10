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

#include <iostream>
#include <fcntl.h>

#include "bits/kcppcompat.h"

#include "kreader.h"
#include "kwriter.h" // we need KOutStream
#include "klog.h"
#include "kfilesystem.h"
#include "ksystem.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

namespace dekaf2
{

KInStream KIn(std::cin);

//-----------------------------------------------------------------------------
bool kRewind(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::beg) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	DEKAF2_LOG_EXCEPTION

	return false;

} // kRewind

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

		auto curPos = streambuf->pubseekoff(0, std::ios_base::cur);

		if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "istream is not seekable ({})", 1);
			return curPos;
		}

		auto endPos = streambuf->pubseekoff(0, std::ios_base::end);

		if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "istream is not seekable ({})", 2);
			return endPos;
		}

		if (endPos != curPos)
		{
			streambuf->pubseekoff(curPos, std::ios_base::beg);
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
// we cannot use KStringView as we need to access a C API
ssize_t kGetSize(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	return kFileSize(sFileName);

} // kGetSize

//-----------------------------------------------------------------------------
bool kAppendAllUnseekable(std::istream& Stream, KString& sContent, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!Stream.good()))
	{
		return false;
	}

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		kDebug(1, "no streambuf");
		return false;
	}

	// We will simply try to read blocks until we fail or reach a
	// max size.

	constexpr std::size_t BUFSIZE { 4096 };
	std::array<char, BUFSIZE> buf;

	// This approach can be really dangerous on endless input streams,
	// therefore we do wrap it into a try-catch block and limit the
	// rx size to ~1 GB.

	auto iLimit = std::min(iMaxRead, std::min(std::size_t(1*1024*1024*1024), kGetPhysicalMemory() / 4));

	DEKAF2_TRY_EXCEPTION
	for (;;)
	{
		auto iRead = streambuf->sgetn(buf.data(), buf.size());

		if (DEKAF2_LIKELY(iRead > 0))
		{
			sContent.append(buf.data(), static_cast<std::size_t>(iRead));

			if (sContent.size() > iLimit)
			{
				// only warn if the limit was not iMaxRead
				if (sContent.size() < iMaxRead)
				{
					kDebug(1, "stepped over limit of {} MB for non-seekable input stream - aborted reading", iLimit / (1024*1024) );
				}
				break;
			}
		}

		if (DEKAF2_UNLIKELY(iRead < static_cast<std::streamsize>(buf.size())))
		{
			// either eof or other error
			break;
		}
	}
	DEKAF2_LOG_EXCEPTION

	Stream.setstate(std::ios_base::eofbit);

	return true;

} // kAppendAllUnseekable

//-----------------------------------------------------------------------------
bool kAppendAll(std::istream& Stream, KString& sContent, bool bFromStart, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	// get size of the file.
	auto iSize = kGetSize(Stream, bFromStart);

	if (DEKAF2_UNLIKELY(!iSize))
	{
		// empty file
		return true;
	}

	if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// minimalistic input stream buffer, or a non-seekable stream.

		// Note that we do not err out when bFromStart was set, because
		// semantically, doing a ReadAll or AppendAll on an unseekable
		// input stream means: return all that is available
		return kAppendAllUnseekable(Stream, sContent, iMaxRead);
	}

	if (static_cast<std::size_t>(iSize) > iMaxRead)
	{
		iSize = iMaxRead;
	}

	// position stream to the beginning
	if (bFromStart && !kRewind(Stream))
	{
		kDebug(1, "cannot rewind stream");
		return false;
	}

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		kDebug(1, "no streambuf");
		return false;
	}

	auto uiSize = static_cast<size_t>(iSize);

	auto uiContentSize = sContent.size();

	// create the read buffer

	// This saves one run of unnecessary construction.
	sContent.resize_uninitialized(uiContentSize + uiSize);

	auto iRead = static_cast<size_t>(streambuf->sgetn(&sContent[uiContentSize], iSize));

	if (iRead < uiSize)
	{
		sContent.resize(uiContentSize + iRead);
	}

	// only test for eof if the limit was not iMaxRead
	if (sContent.size() < iMaxRead)
	{
		// we should now be at the end of the input..
		if (std::istream::traits_type::eq_int_type(streambuf->sgetc(), std::istream::traits_type::eof()))
		{
			Stream.setstate(std::ios_base::eofbit);
		}
		else
		{
			kDebug (1, "stream grew during read, did not read new content");
		}
	}

	if (iRead != uiSize)
	{
		kWarning ("Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}

	return true;

} // kAppendAll

//-----------------------------------------------------------------------------
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	sContent.clear();
	return kAppendAll(Stream, sContent, bFromStart, iMaxRead);

} // kReadAll

//-----------------------------------------------------------------------------
KString kReadAll(std::istream& Stream, bool bFromStart, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	KString sContent;
	kAppendAll(Stream, sContent, bFromStart, iMaxRead);
	return sContent;

} // kReadAll

#ifndef DEKAF2_IS_OSX
#define DEKAF2_READALL_USE_IOSTREAMS
#endif

//-----------------------------------------------------------------------------
bool kAppendAll(KStringViewZ sFileName, KString& sContent, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	auto iSize(kGetSize(sFileName));

	if (!iSize)
	{
		// empty file
		return true;
	}
	else if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// proc file system entry or some other special file. Simply
		// let's try to open it and check if we can append by reading
		// until eof

		KInFile Stream(sFileName);

		if (!Stream.is_open())
		{
			kDebug(2, "cannot open file '{}'", sFileName);
		}
		else
		{
			return kAppendAllUnseekable(Stream, sContent, iMaxRead);
		}
	}
	else if (iSize > 0)
	{
		if (static_cast<std::size_t>(iSize) > iMaxRead)
		{
			iSize = iMaxRead;
		}

		// We use an unbuffered file descriptor read on MacOS because
		// with sub-optimal iostream implementations like the one coming
		// with clang on the mac it is about five times faster than
		// reading from the iostream..
		//
		// Current gcc implementations do not need this hack,
		// there, the speed is the same with iostreams as with simple
		// file descriptor reads. But they do not hurt there either.
		// However, we do switch to iostreams on Linux now as well.
		//
		// For Windows, we switch to iostreams anyways, as otherwise
		// we could not open UTF8 file names

#ifdef DEKAF2_READALL_USE_IOSTREAMS

		KInFile File(sFileName);
		if (File.is_open())
		{
			auto iContent = sContent.size();
			sContent.resize_uninitialized(iContent + iSize);
			auto iRead = File.Read(&sContent[iContent], iSize);

			if (iRead < static_cast<std::size_t>(iSize))
			{
				sContent.resize(iContent + iRead);
			}

			return iRead == static_cast<std::size_t>(iSize);
		}

		kDebug(2, "cannot open file '{}'", sFileName);

#else

		auto fd = open(sFileName.c_str(), O_RDONLY | DEKAF2_CLOSE_ON_EXEC_FLAG);

		if (fd >= 0)
		{
			auto iContent = sContent.size();
			sContent.resize_uninitialized(iContent + iSize);
			auto iRead = kReadFromFileDesc(fd, &sContent[iContent], iSize);

			close(fd);

			if (iRead < static_cast<std::size_t>(iSize))
			{
				sContent.resize(iContent + iRead);
			}

			return iRead == static_cast<std::size_t>(iSize);
		}

		kDebug(2, "cannot open file '{}': {}", sFileName, strerror(errno));

#endif

	}

	return false;

} // kAppendAll

//-----------------------------------------------------------------------------
bool kReadAll(KStringViewZ sFileName, KString& sContent, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	sContent.clear();
	return kAppendAll(sFileName, sContent, iMaxRead);

} // kReadAll

//-----------------------------------------------------------------------------
KString kReadAll(KStringViewZ sFileName, std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	KString sContent;
	kAppendAll(sFileName, sContent, iMaxRead);
	return sContent;

} // kReadAll

#ifdef DEKAF2_IS_OSX
#define DEKAF2_USE_OWN_GETLINE
#endif

#ifdef DEKAF2_USE_OWN_GETLINE
//-----------------------------------------------------------------------------
void myLocalGetline(std::istream& Stream, KString& sLine, KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		Stream.setstate(std::ios::failbit);

		return;
	}

	for (;;)
	{
		auto ch = streambuf->sbumpc();

		if (DEKAF2_UNLIKELY(std::istream::traits_type::eq_int_type(ch, std::istream::traits_type::eof())))
		{
			Stream.setstate(std::ios::eofbit);

			if (sLine.empty())
			{
				Stream.setstate(std::ios::failbit);
			}

			return;
		}

		if (DEKAF2_UNLIKELY(ch == delimiter))
		{
			return;
		}

		sLine += ch;
	}
}
#endif

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream,
               KString& sLine,
               KStringView sTrimRight,
               KStringView sTrimLeft,
               KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!Stream.good()))
	{
		sLine.clear();
		return false;
	}

#ifdef DEKAF2_USE_OWN_GETLINE
	myLocalGetline(Stream, sLine, delimiter);
#else
	// do not implement your own version of std::getline without performance checks ..
	std::getline(Stream, sLine, delimiter);
#endif

	if (DEKAF2_UNLIKELY(Stream.fail()))
	{
		return false;
	}

	// to avoid unnecessary reallocations do not add the delimiter to sLine
	// if it is part of sTrimRight and would thus be removed right afterwards..

	if (sTrimRight.empty())
	{
		if (!Stream.eof())
		{
			// std::getline does not store the EOL character, but we want to
			sLine += delimiter;
		}
	}
	else
	{
		// add the delimiter char only if it is not a member of sTrimRight
		if (sTrimRight.find(delimiter) == KString::npos)
		{
			if (!Stream.eof())
			{
				// std::getline does not store the EOL character, but we want to
				sLine += delimiter;
			}

			sLine.TrimRight(sTrimRight);
		}
		else if (sTrimRight.size() > 1)
		{
			// only trim if sTrimRight is > 1, as otherwise it only contains the delimiter
			sLine.TrimRight(sTrimRight);
		}
	}

	if (!sTrimLeft.empty())
	{
		sLine.TrimLeft(sTrimLeft);
	}

	return true;

} // kReadLine

//-----------------------------------------------------------------------------
std::size_t kReadFromFileDesc(int fd, void* sBuffer, std::size_t iCount)
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
				kDebug(1, "cannot read from file desc: {}", strerror(errno));
				// invalidate return, we did not fullfill our contract
				iTotal = 0;
				break;
			}
		}
	}

	return iTotal;

} // kReadFromFileDesc

//-----------------------------------------------------------------------------
KInStream::const_kreader_line_iterator::const_kreader_line_iterator(base_iterator& it, bool bToEnd)
//-----------------------------------------------------------------------------
    : m_it(bToEnd ? nullptr : &it)
{
	operator++();

} // ctor

//-----------------------------------------------------------------------------
KInStream::const_kreader_line_iterator::self_type& KInStream::const_kreader_line_iterator::operator++()
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(m_it != nullptr))
	{
		if (!kReadLine(m_it->InStream(),
		               m_sBuffer,
		               m_it->m_sTrimRight,
		               m_it->m_sTrimLeft,
		               m_it->m_chDelimiter))
		{
			m_it = nullptr;
		}
	}

	return *this;

} // prefix

//-----------------------------------------------------------------------------
const KInStream::const_kreader_line_iterator::self_type KInStream::const_kreader_line_iterator::operator++(int)
//-----------------------------------------------------------------------------
{
	const self_type i = *this;

	operator++();

	return i;

} // postfix

//-----------------------------------------------------------------------------
/// UnRead a character
bool KInStream::UnRead()
//-----------------------------------------------------------------------------
{
	auto streambuf = InStream().rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		InStream().setstate(std::ios::failbit);

		return false;
	}

	auto iCh = streambuf->sungetc();

	if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
	{
		// could not unread - note that this does not have to be a stream error,
		// therefore we do not set badbit or failbit
		return false;
	}

	// make sure we are no more in eof state if we were before
	InStream().clear();

	return true;

} // UnRead

//-----------------------------------------------------------------------------
/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
std::istream::int_type KInStream::Read()
//-----------------------------------------------------------------------------
{
	auto streambuf = InStream().rdbuf();

	if (DEKAF2_UNLIKELY(streambuf == nullptr))
	{
		InStream().setstate(std::ios::failbit);

		return std::istream::traits_type::eof();
	}

	auto iCh = streambuf->sbumpc();

	if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
	{
		InStream().setstate(std::ios::eofbit);
	}

	return iCh;

} // Read

//-----------------------------------------------------------------------------
/// Read a range of characters. Returns count of successfully read characters.
size_t KInStream::Read(void* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	auto streambuf = InStream().rdbuf();

	if (DEKAF2_UNLIKELY(streambuf == nullptr))
	{
		InStream().setstate(std::ios::failbit);

		return 0;
	}

	auto iRead = streambuf->sgetn(static_cast<std::istream::char_type*>(pAddress), iCount);

	if (DEKAF2_UNLIKELY(iRead < 0))
	{
		InStream().setstate(std::ios::badbit);
		iRead = 0;
	}
	else if (DEKAF2_UNLIKELY(static_cast<size_t>(iRead) < iCount))
	{
		InStream().setstate(std::ios::eofbit);
	}

	return static_cast<size_t>(iRead);

} // Read

//-----------------------------------------------------------------------------
/// Read a range of characters and append to sBuffer. Returns count of successfully read characters.
size_t KInStream::Read(KString& sBuffer, size_t iCount)
//-----------------------------------------------------------------------------
{
	auto iOldLen = sBuffer.size();

	if (iCount == npos)
	{
		kAppendAll(*this, sBuffer, false);
		return sBuffer.size() - iOldLen;
	}

	sBuffer.resize_uninitialized(iOldLen + iCount);

	auto iAddedLen = Read(&sBuffer[iOldLen], iCount);

	if (iAddedLen < iCount)
	{
		sBuffer.resize(iOldLen + iAddedLen);
	}

	return iAddedLen;

} // Read

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns count of successfully read characters.
size_t KInStream::Read(KOutStream& Stream, size_t iCount)
//-----------------------------------------------------------------------------
{
	constexpr std::size_t COPY_BUFSIZE { 4096 };
	std::array<char, COPY_BUFSIZE> Buffer;
	size_t iRead = 0;

	for (;iCount && Good();)
	{
		auto iChunk = std::min(Buffer.size(), iCount);
		auto iReadChunk = Read(Buffer.data(), iChunk);
		iRead  += iReadChunk;
		iCount -= iReadChunk;

		if (!Stream.Write(Buffer.data(), iReadChunk).Good() || iReadChunk < iChunk)
		{
			break;
		}
	}

	return iRead;

} // Read

template class KReader<std::ifstream>;

} // end of namespace dekaf2

