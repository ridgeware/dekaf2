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

#include "kreader.h"
#include "kread.h"
#include "kwriter.h" // we need KOutStream
#include "klog.h"
#include "kfilesystem.h"
#include "ksystem.h"
#include "kstringutils.h"
#include "kstreambuf.h"
#include "kcompatibility.h"
#include <iostream>
#include <fcntl.h>

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

KInStream KIn(std::cin);

//-----------------------------------------------------------------------------
std::istream& kGetNullIStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<std::istream> NullStream = std::make_unique<std::istream>(&KNullStreamBuf::Create());
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
KInStream& kGetNullInStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<KInStream> NullStream = std::make_unique<KInStream>(kGetNullIStream(), "", "", '\n', true);
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
// we cannot use KStringView as we need to access a C API
ssize_t kGetSize(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	return kFileSize(sFileName);

} // kGetSize

//-----------------------------------------------------------------------------
bool kAppendAllUnseekable(std::istream& Stream, KStringRef& sContent, std::size_t iMaxRead)
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

	std::array<char, KDefaultCopyBufSize> buf;

	// This approach can be really dangerous on endless input streams,
	// therefore we do wrap it into a try-catch block and limit the
	// rx size to ~1 GB.

	auto iLimit = std::min(iMaxRead, std::min(std::size_t(1*1024*1024*1024), kGetPhysicalMemory() / 4));

	DEKAF2_TRY_EXCEPTION
	for (;;)
	{
		auto iRead = streambuf->sgetn(buf.data(), std::min(iLimit, buf.size()));

		if (DEKAF2_LIKELY(iRead > 0))
		{
			sContent.append(buf.data(), static_cast<std::size_t>(iRead));

			if (sContent.size() >= iMaxRead)
			{
				return true;
			}
			else if (sContent.size() > iLimit)
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
bool kAppendAll(std::istream& Stream, KStringRef& sContent, bool bFromStart, std::size_t iMaxRead)
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

	auto uiSize = static_cast<std::size_t>(iSize);

	auto uiContentSize = sContent.size();

	// create the read buffer

	// This saves one run of unnecessary construction.
#ifdef __cpp_lib_string_resize_and_overwrite
	sContent.resize_and_overwrite(uiContentSize + uiSize, [](KStringRef::pointer buf, KStringRef::size_type buf_size) noexcept { return buf_size; });
#else
	sContent.resize(uiContentSize + uiSize);
#endif

	auto iRead = static_cast<std::size_t>(streambuf->sgetn(&sContent[uiContentSize], iSize));

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
bool kReadAll(std::istream& Stream, KStringRef& sContent, bool bFromStart, std::size_t iMaxRead)
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
bool kAppendAll(KStringViewZ sFileName, KStringRef& sContent, std::size_t iMaxRead)
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
		// with suboptimal iostream implementations like the one coming
		// with clang on the mac it is about five times faster than
		// reading from the iostream..
		//
		// Current gcc implementations do not need this hack,
		// there, the speed is the same with iostreams as with simple
		// file descriptor reads. But they do not hurt there either.
		// However, we do switch to iostreams on Linux now as well.
		//
		// For Windows, we switch to iostreams anyway, as otherwise
		// we could not open UTF8 file names

#ifdef DEKAF2_READALL_USE_IOSTREAMS

		KInFile File(sFileName);

		if (File.is_open())
		{
			auto iContent = sContent.size();

			kResizeUninitialized(sContent, iContent + iSize);

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

			kResizeUninitialized(sContent, iContent + iSize);

			auto iRead = kRead(fd, &sContent[iContent], iSize);

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
bool kReadAll(KStringViewZ sFileName, KStringRef& sContent, std::size_t iMaxRead)
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

//-----------------------------------------------------------------------------
// own implementation, reads iMaxRead characters, returns true if delimiter was found
bool myLocalGetline(std::istream&       Stream,
					KStringRef&         sLine,
					KString::value_type delimiter,
					std::size_t         iMaxRead = npos)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	auto streambuf = Stream.rdbuf();

	if (DEKAF2_UNLIKELY(!streambuf))
	{
		Stream.setstate(std::ios::failbit);

		return false;
	}

	for (;;)
	{
		if (iMaxRead-- == 0)
		{
			return false;
		}

		auto ch = streambuf->sbumpc();

		if (DEKAF2_UNLIKELY(std::istream::traits_type::eq_int_type(ch, std::istream::traits_type::eof())))
		{
			Stream.setstate(std::ios::eofbit);

			if (sLine.empty())
			{
				Stream.setstate(std::ios::failbit);
			}

			return false;
		}

		if (DEKAF2_UNLIKELY(ch == delimiter))
		{
			return true;
		}

		sLine += ch;
	}
}

#ifdef DEKAF2_IS_OSX
#define DEKAF2_USE_OWN_GETLINE
#endif

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream,
               KStringRef& sLine,
               KStringView sTrimRight,
               KStringView sTrimLeft,
               KString::value_type delimiter,
			   std::size_t iMaxRead)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!Stream.good()))
	{
		sLine.clear();
		return false;
	}

	bool bFoundDelimiter;

#ifdef DEKAF2_USE_OWN_GETLINE

	bFoundDelimiter = myLocalGetline(Stream, sLine, delimiter, iMaxRead);

#else

	if (iMaxRead == npos)
	{
		// do not implement your own version of std::getline without
		// performance checks ..
		std::getline(Stream, sLine, delimiter);
		bFoundDelimiter = Stream.good();
	}
	else
	{
		// however, if you need a max read limitation ..
		bFoundDelimiter = myLocalGetline(Stream, sLine, delimiter, iMaxRead);
	}

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
			if (bFoundDelimiter)
			{
				// std::getline does not store the EOL character, but we want to
				sLine += delimiter;
			}
		}
	}
	else
	{
		// add the delimiter char only if it is not a member of sTrimRight
		if (sTrimRight.find(delimiter) == KString::npos)
		{
			if (bFoundDelimiter)
			{
				// std::getline does not store the EOL character, but we want to
				sLine += delimiter;
			}
			kTrimRight(sLine, sTrimRight);
		}
		else if (sTrimRight.size() > 1)
		{
			// only trim if sTrimRight is > 1, as otherwise it only contains the delimiter
			kTrimRight(sLine, sTrimRight);
		}
	}

	if (!sTrimLeft.empty())
	{
		kTrimLeft(sLine, sTrimLeft);
	}

	return true;

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
		if (!kReadLine(m_it->istream(),
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
KInStream::KInStream(std::istream&           InStream,
					 KStringView             sTrimRight,
					 KStringView             sTrimLeft,
					 KStringView::value_type chDelimiter,
					 bool                    bImmutable)
//-----------------------------------------------------------------------------
	: m_InStream   (&InStream  )
	, m_sTrimRight (sTrimRight )
	, m_sTrimLeft  (sTrimLeft  )
	, m_chDelimiter(chDelimiter)
	, m_bImmutable (bImmutable )
{
}

//-----------------------------------------------------------------------------
/// Read a range of characters and append to sBuffer. Returns count of successfully read characters.
std::size_t KInStream::Read(KStringRef& sBuffer, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto iOldLen = sBuffer.size();

	if (iCount == npos)
	{
		kAppendAll(*this, sBuffer, false);
		return sBuffer.size() - iOldLen;
	}

#ifdef __cpp_lib_string_resize_and_overwrite
	sBuffer.resize_and_overwrite(iOldLen + iCount, [](KStringRef::pointer buf, KStringRef::size_type buf_size) noexcept { return buf_size; });
#else
	sBuffer.resize(iOldLen + iCount);
#endif

	auto iAddedLen = Read(&sBuffer[iOldLen], iCount);

	if (iAddedLen < iCount)
	{
		sBuffer.resize(iOldLen + iAddedLen);
	}

	return iAddedLen;

} // Read

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns count of successfully read characters.
std::size_t KInStream::Read(KOutStream& Stream, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	std::array<char, KDefaultCopyBufSize> Buffer;
	std::size_t iRead = 0;

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

//-----------------------------------------------------------------------------
/// Set the end-of-line character (defaults to LF)
bool KInStream::SetReaderEndOfLine(char chDelimiter)
//-----------------------------------------------------------------------------
{
	if (!m_bImmutable)
	{
		m_chDelimiter = chDelimiter;
	}
	else
	{
		kDebug(2, "{} is made immutable - cannot change to {}", "line delimiter", chDelimiter);
	}
	return !m_bImmutable;

} // SetReaderEndOfLine

//-----------------------------------------------------------------------------
/// Set the left trim characters for line based reading (default to none)
bool KInStream::SetReaderLeftTrim(KStringView sTrimLeft)
//-----------------------------------------------------------------------------
{
	if (!m_bImmutable)
	{
		m_sTrimLeft  = sTrimLeft;
	}
	else
	{
		kDebug(2, "{} is made immutable - cannot change to {}", "trimming", sTrimLeft);
	}
	return !m_bImmutable;

} // SetReaderLeftTrim

//-----------------------------------------------------------------------------
/// Set the right trim characters for line based reading (default to LF)
bool KInStream::SetReaderRightTrim(KStringView sTrimRight)
//-----------------------------------------------------------------------------
{
	if (!m_bImmutable)
	{
		m_sTrimRight = sTrimRight;
	}
	else
	{
		kDebug(2, "{} is made immutable - cannot change to {}", "trimming", sTrimRight);
	}
	return !m_bImmutable;

} // SetReaderRightTrim

//-----------------------------------------------------------------------------
/// Set the right and left trim characters for line based reading (default to LF for right, none for left)
bool KInStream::SetReaderTrim(KStringView sTrimRight, KStringView sTrimLeft)
//-----------------------------------------------------------------------------
{
	return SetReaderRightTrim(sTrimRight) && SetReaderLeftTrim(sTrimLeft);

} // SetReaderTrim


template class KReader<std::ifstream>;

DEKAF2_NAMESPACE_END
