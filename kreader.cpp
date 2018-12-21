/*
//-----------------------------------------------------------------------------//
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

#include "bits/kcppcompat.h"
#include "bits/kfilesystem.h"

#include "kreader.h"
#include "kwriter.h" // we need KOutStream
#include "klog.h"

namespace dekaf2
{

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

		std::streambuf* sb = Stream.rdbuf();

		if (!sb)
		{
			kDebug(1, "kGetSize: no streambuf");
			return -1;
		}

		std::streambuf::pos_type curPos = sb->pubseekoff(0, std::ios_base::cur);

		if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "kGetSize: istream is not seekable ({})", 1);
			return curPos;
		}

		std::streambuf::pos_type endPos = sb->pubseekoff(0, std::ios_base::end);

		if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
		{
			kDebug(3, "kGetSize: istream is not seekable ({})", 2);
			return endPos;
		}

		if (endPos != curPos)
		{
			sb->pubseekoff(curPos, std::ios_base::beg);
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
	ssize_t iSize = static_cast<ssize_t>(fs::file_size(sFileName.c_str(), ec));
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
	else
	{
		return -1;
	}
#endif

} // kGetSize

//-----------------------------------------------------------------------------
bool kAppendAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		kDebug(1, "kReadAll: no streambuf");
		return false;
	}

	// get size of the file.
	auto iSize = kGetSize(Stream, bFromStart);

	if (!iSize)
	{
		return true;
	}

	if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// minimalistic input stream buffer, or a non-seekable stream.
		// We will simply try to read blocks until we fail or reach a
		// max size.

		enum { BUFSIZE = 4096 };
		char buf[BUFSIZE];

		// This approach can be really dangerous on endless input streams,
		// therefore we do wrap it into a try-catch block and limit the
		// rx size to ~1 GB.

		size_t iTotal { 0 };
		static constexpr size_t iLimit = 1*1024*1024*1024;

		DEKAF2_TRY_EXCEPTION
		for (;;)
		{
			auto iRead = static_cast<size_t>(sb->sgetn(buf, BUFSIZE));

			if (iRead > 0)
			{
				sContent.append(buf, iRead);

				iTotal += iRead;

				if (iTotal > iLimit)
				{
					kDebug(1, "kAppendAll: stepped over limit of {} MB for non-seekable input stream - aborted reading", iLimit / (1024*1024) );
					break;
				}
			}

			if (iRead < BUFSIZE)
			{
				break;
			}
		}
		DEKAF2_LOG_EXCEPTION

		Stream.setstate(std::ios_base::eofbit);

		return iTotal > 0;
	}

	// position stream to the beginning
	if (bFromStart && !kRewind(Stream))
	{
		kDebug(1, "kReadAll: cannot rewind stream");
		return false;
	}

	auto uiSize = static_cast<size_t>(iSize);

	auto uiContentSize = sContent.size();

	// create the read buffer

	// This saves one run of unnecessary construction.
	sContent.resize_uninitialized(uiContentSize + uiSize);

	auto iRead = static_cast<size_t>(sb->sgetn(&sContent[uiContentSize], iSize));

	if (iRead < uiSize)
	{
		sContent.resize(uiContentSize + iRead);
	}

	// we should now be at the end of the input..
	if (std::istream::traits_type::eq_int_type(sb->sgetc(), std::istream::traits_type::eof()))
	{
		Stream.setstate(std::ios_base::eofbit);
	}
	else
	{
		kDebug (1, "kReadAll: stream grew during read, did not read all new content");
	}

	if (iRead != uiSize)
	{
		kWarning ("KReader: Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
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
bool kReadAll(KStringViewZ sFileName, KString& sContent)
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
		// We use an unbuffered file descriptor read because with
		// sub-optimal iostream implementations like the one coming
		// with clang on the mac it is about five times faster than
		// reading from the iostream..
		//
		// Current gcc implementations would not need this hack,
		// there, the speed is the same with iostreams as with simple
		// file descriptor reads. But it does not hurt there either.

		auto fd = open(sFileName.c_str(), O_RDONLY);

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
		else
		{
			kDebug(2, "cannot open file '{}': {}", sFileName, strerror(errno));
		}
	}

	return false;

} // kReadAll

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
	if (m_it != nullptr)
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
KInStream::const_kreader_line_iterator::self_type KInStream::const_kreader_line_iterator::operator++(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;

	operator++();

	return i;

} // postfix


//-----------------------------------------------------------------------------
KInStream::~KInStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
/// UnRead a character
bool KInStream::UnRead()
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = InStream().rdbuf();

	if (sb)
	{
		typename std::istream::int_type iCh = sb->sungetc();
		if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
		{
			return false;
		}
		return true;
	}

	return false;

} // UnRead

//-----------------------------------------------------------------------------
/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
typename std::istream::int_type KInStream::Read()
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = InStream().rdbuf();

	if (sb)
	{
		typename std::istream::int_type iCh = sb->sbumpc();
		if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
		{
			InStream().setstate(std::ios::eofbit);
		}
		return iCh;
	}

	return std::istream::traits_type::eof();

} // Read

//-----------------------------------------------------------------------------
/// Read a range of characters. Returns count of successfully read characters.
size_t KInStream::Read(void* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	if (iCount)
	{
		std::streambuf* sb = InStream().rdbuf();
		if (sb)
		{
			auto iRead = sb->sgetn(static_cast<std::istream::char_type*>(pAddress), iCount);
			if (iRead <= 0)
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
	char sBuffer[COPY_BUFSIZE];
	size_t iRead = 0;

	for (;iCount;)
	{
		auto iChunk = std::min(static_cast<size_t>(COPY_BUFSIZE), iCount);
		auto iReadChunk = Read(sBuffer, iChunk);
		iRead  += iReadChunk;
		iCount -= iReadChunk;

		if (!Stream.Write(sBuffer, iReadChunk).OutStream().good() || iReadChunk < iChunk)
		{
			break;
		}
	}

	return iRead;

} // Read

template class KReader<std::ifstream>;

} // end of namespace dekaf2

