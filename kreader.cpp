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
#include "bits/kfilesystem.h"

#include "kreader.h"
#include "kwriter.h" // we need KOutStream
#include "klog.h"

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

		if (!streambuf)
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

		return endPos - curPos;
	}

	DEKAF2_CATCH (std::exception& e)
	{
		if (kContains(e.what(), "random"))
		{
			kDebug(3, e.what());
		}
		else
		{
#ifdef DEKAF2_EXCEPTIONS
			DEKAF2_THROW(e);
#else
			kException(e);
#endif
		}
	}

	return -1;

} // kGetSize

//-----------------------------------------------------------------------------
// we cannot use KStringView as we need to access a C API
ssize_t kGetSize(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;
	auto iSize = static_cast<ssize_t>(fs::file_size(kToFilesystemPath(sFileName), ec));
	if (ec)
	{
		iSize = -1;
	}
	return iSize;
#else // default to posix interface
	struct stat buf;
	if (!::stat(sFileName.c_str(), &buf))
	{
		return buf.st_size;
	}

	return -1;
#endif

} // kGetSize

//-----------------------------------------------------------------------------
bool kAppendAllUnseekable(std::istream& Stream, KString& sContent)
//-----------------------------------------------------------------------------
{
	if (!Stream.good())
	{
		return false;
	}

	auto streambuf = Stream.rdbuf();

	if (!streambuf)
	{
		kDebug(1, "no streambuf");
		return false;
	}

	// We will simply try to read blocks until we fail or reach a
	// max size.

	enum { BUFSIZE = 4096 };
	std::array<char, BUFSIZE> buf;

	// This approach can be really dangerous on endless input streams,
	// therefore we do wrap it into a try-catch block and limit the
	// rx size to ~1 GB.

	std::size_t iTotal { 0 };
	static constexpr std::size_t iLimit = 1*1024*1024*1024;

	DEKAF2_TRY_EXCEPTION
	for (;;)
	{
		auto iRead = streambuf->sgetn(buf.data(), buf.size());

		if (iRead > 0)
		{
			auto uiRead = static_cast<std::size_t>(iRead);

			sContent.append(buf.data(), uiRead);

			iTotal += uiRead;

			if (iTotal > iLimit)
			{
				kWarning("stepped over limit of {} MB for non-seekable input stream - aborted reading", iLimit / (1024*1024) );
				break;
			}
		}

		if (iRead <= 0)
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
bool kAppendAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	auto streambuf = Stream.rdbuf();

	if (!streambuf)
	{
		kDebug(1, "no streambuf");
		return false;
	}

	// get size of the file.
	auto iSize = kGetSize(Stream, bFromStart);

	if (!iSize)
	{
		// empty file
		return true;
	}

	if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// minimalistic input stream buffer, or a non-seekable stream.
		if (!bFromStart)
		{
			return kAppendAllUnseekable(Stream, sContent);
		}

		// bFromStart option was set, but this stream is not seekable
		kWarning("cannot rewind stream");
		return false;
	}

	// position stream to the beginning
	if (bFromStart && !kRewind(Stream))
	{
		kDebug(1, "cannot rewind stream");
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

	// we should now be at the end of the input..
	if (std::istream::traits_type::eq_int_type(streambuf->sgetc(), std::istream::traits_type::eof()))
	{
		Stream.setstate(std::ios_base::eofbit);
	}
	else
	{
		kDebug (1, "stream grew during read, did not read new content");
	}

	if (iRead != uiSize)
	{
		kWarning ("Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}

	return true;

} // kAppendAll

//-----------------------------------------------------------------------------
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	sContent.clear();
	return kAppendAll(Stream, sContent, bFromStart);

} // kReadAll

//-----------------------------------------------------------------------------
KString kReadAll(std::istream& Stream, bool bFromStart)
//-----------------------------------------------------------------------------
{
	KString sContent;
	kAppendAll(Stream, sContent, bFromStart);
	return sContent;

} // kReadAll

#ifndef DEKAF2_IS_OSX
#define DEKAF2_READALL_USE_IOSTREAMS
#endif

//-----------------------------------------------------------------------------
bool kAppendAll(KStringViewZ sFileName, KString& sContent)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	auto iSize(kGetSize(sFileName));

	if (!iSize)
	{
		// empty file
		return true;
	}

	if (iSize > 0)
	{
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

			if (iRead < static_cast<size_t>(iSize))
			{
				sContent.resize(iContent + iRead);
			}

			return iRead == static_cast<size_t>(iSize);
		}

		kDebug(2, "cannot open file '{}'", sFileName);

#else

		auto fd = open(sFileName.c_str(), O_RDONLY | DEKAF2_CLOSE_ON_EXEC_FLAG);

		if (fd >= 0)
		{
			auto iContent = sContent.size();
			sContent.resize_uninitialized(iContent + iSize);
			auto iRead = read(fd, &sContent[iContent], iSize);

			close(fd);

			if (iRead < iSize)
			{
				sContent.resize(iContent + iRead);
			}

			return iRead == iSize;
		}

		kDebug(2, "cannot open file '{}': {}", sFileName, strerror(errno));

#endif

	}

	return false;

} // kAppendAll

//-----------------------------------------------------------------------------
bool kReadAll(KStringViewZ sFileName, KString& sContent)
//-----------------------------------------------------------------------------
{
	sContent.clear();
	return kAppendAll(sFileName, sContent);

} // kReadAll

//-----------------------------------------------------------------------------
KString kReadAll(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	KString sContent;
	kAppendAll(sFileName, sContent);
	return sContent;

} // kReadAll

#ifndef DEKAF2_IS_OSX
#define DEKAF2_READLINE_USE_GETLINE
#endif

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream,
               KString& sLine,
               KStringView sTrimRight,
               KStringView sTrimLeft,
               KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_READLINE_USE_GETLINE
	
	sLine.clear();

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		return false;
	}
	
	for (;;)
	{
		auto ch = streambuf->sbumpc();

		if (DEKAF2_UNLIKELY(std::istream::traits_type::eq_int_type(ch, std::istream::traits_type::eof())))
		{
			Stream.setstate(std::ios::eofbit);

			return !sLine.empty();
		}

		if (DEKAF2_LIKELY(ch != delimiter))
		{
			sLine += ch;
		}
		else
		{
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
					sLine += delimiter;

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
		}
	}

#else

	if (DEKAF2_UNLIKELY(!Stream.good()))
	{
		sLine.clear();
		return false;
	}

	// do not implement your own version of std::getline without performance checks ..
	std::getline(Stream, sLine, delimiter);

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

#endif

} // kReadLine


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
		return false;
	}

	auto iCh = streambuf->sungetc();

	return !std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof());

} // UnRead

//-----------------------------------------------------------------------------
/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
std::istream::int_type KInStream::Read()
//-----------------------------------------------------------------------------
{
	auto streambuf = InStream().rdbuf();

	if (DEKAF2_UNLIKELY(streambuf == nullptr))
	{
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
	if (iCount)
	{
		auto streambuf = InStream().rdbuf();

		if (DEKAF2_LIKELY(streambuf != nullptr))
		{
			auto iRead = streambuf->sgetn(static_cast<std::istream::char_type*>(pAddress), iCount);

			if (DEKAF2_UNLIKELY(iRead <= 0))
			{
				InStream().setstate(std::ios::eofbit);
				iRead = 0;
			}

			return static_cast<size_t>(iRead);
		}
	}

	return 0;

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
	enum { COPY_BUFSIZE = 4096 };
	std::array<char, COPY_BUFSIZE> Buffer;
	size_t iRead = 0;

	for (;iCount;)
	{
		auto iChunk = std::min(Buffer.size(), iCount);
		auto iReadChunk = Read(Buffer.data(), iChunk);
		iRead  += iReadChunk;
		iCount -= iReadChunk;

		if (!Stream.Write(Buffer.data(), iReadChunk).OutStream().good() || iReadChunk < iChunk)
		{
			break;
		}
	}

	return iRead;

} // Read

template class KReader<std::ifstream>;

} // end of namespace dekaf2

