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

#include "kreader.h"
#include "kwriter.h" // we need KOutStream
#include "klog.h"

#ifdef DEKAF2_HAS_CPP_17
 #include <experimental/filesystem>
#else
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <cstdio>
#endif

namespace dekaf2
{

#ifdef DEKAF2_HAS_CPP_17
 namespace fs = std::experimental::filesystem;
#endif

//-----------------------------------------------------------------------------
bool kRewind(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	try {

		return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::beg) != std::streambuf::pos_type(std::streambuf::off_type(-1));
	}

	catch (std::exception& e)
	{
		kException(e);
	}

	return false;

} // kRewind

//-----------------------------------------------------------------------------
ssize_t kGetSize(std::istream& Stream, bool bFromStart)
//-----------------------------------------------------------------------------
{
	try {

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

	catch (std::exception& e)
	{
		kException(e);
	}

	return -1;

} // kGetSize

//-----------------------------------------------------------------------------
// we cannot use KStringView as we need to access a C API
ssize_t kGetSize(const char* sFileName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_17
	std::error_code ec;
	ssize_t iSize = static_cast<ssize_t>(fs::file_size(sFileName, ec));
	if (ec)
	{
		iSize = -1;
	}
	return iSize;
#else // default to posix interface
	struct stat buf;
	if (!::stat(sFileName, &buf))
	{
		return buf.st_size;
	}
	else
	{
		return -1;
	}
#endif
}

//-----------------------------------------------------------------------------
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		kDebug(1, "kReadAll: no streambuf");
		return false;
	}

	// get size of the file.
	ssize_t iSize = kGetSize(Stream, bFromStart);

	if (!iSize)
	{
		return true;
	}

	// position stream to the beginning
	if (bFromStart && !kRewind(Stream))
	{
		kDebug(1, "kReadAll: cannot rewind stream");
		return false;
	}

	if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// minimalistic input stream buffer, or a non-seekable stream.
		// We will simply try to read blocks until we fail.
		enum { BUFSIZE = 4096 };
		char buf[BUFSIZE];

		// as this approach can be really dangerous on endless input
		// streams we do wrap it at least into a try-catch block..
		try
		{
			for (;;)
			{
				size_t iRead = static_cast<size_t>(sb->sgetn(buf, BUFSIZE));
				if (iRead > 0)
				{
					sContent.append(buf, iRead);
				}
				if (iRead < BUFSIZE)
				{
					break;
				}
			}
		}

		catch (std::exception& e)
		{
			sContent.clear();
			kException(e);
		}

		Stream.setstate(std::ios_base::eofbit);

		return sContent.size();
	}

	size_t uiSize = static_cast<size_t>(iSize);

	// create the read buffer
	sContent.resize(uiSize);

	size_t iRead = static_cast<size_t>(sb->sgetn(sContent.data(), iSize));

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

} // kReadAll

//-----------------------------------------------------------------------------
bool kReadAll(KStringView sFileName, KString& sContent)
//-----------------------------------------------------------------------------
{
	KInFile File(sFileName);
	return File.ReadRemaining(sContent);
} // kReadAll

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream,
               KString& sLine,
               KStringView sTrimRight,
               KStringView sTrimLeft,
               KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
	if (!Stream.good())
	{
		sLine.clear();
		return false;
	}

	// do not implement your own version of std::getline without performance checks ..
	std::getline(Stream, sLine, delimiter);

	if (Stream.fail())
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
}

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

	return i;
} // postfix


//-----------------------------------------------------------------------------
KInStream::~KInStream()
//-----------------------------------------------------------------------------
{
}

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
}

//-----------------------------------------------------------------------------
/// Read a range of characters. Returns count of successfully read charcters.
size_t KInStream::Read(typename std::istream::char_type* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = InStream().rdbuf();
	if (sb)
	{
		size_t iRead = static_cast<size_t>(sb->sgetn(pAddress, static_cast<std::streamsize>(iCount)));
		if (iRead != iCount)
		{
			InStream().setstate(std::ios::eofbit);
		}
		return iRead;
	}
	return 0;
}

//-----------------------------------------------------------------------------
/// Read a range of characters and append to sBuffer. Returns count of successfully read charcters.
size_t KInStream::Read(KString& sBuffer, size_t iCount)
//-----------------------------------------------------------------------------
{
	KString::size_type iOldLen = sBuffer.size();
	sBuffer.resize(iOldLen + iCount);
	KString::size_type iAddedLen = Read(&sBuffer[iOldLen], iCount);
	if (iAddedLen < iCount)
	{
		sBuffer.resize(iOldLen + iAddedLen);
	}
	return iAddedLen;
}

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns count of successfully read charcters.
size_t KInStream::Read(KOutStream& Stream, size_t iCount)
//-----------------------------------------------------------------------------
{
	enum { COPY_BUFSIZE = 4096 };
	char sBuffer[COPY_BUFSIZE];
	size_t iRead = 0;

	for (;iCount;)
	{
		auto iChunk = std::min(4096UL, iCount);
		auto iReadChunk = Read(sBuffer, iChunk);
		iRead  += iReadChunk;
		iCount -= iReadChunk;

		if (!Stream.Write(sBuffer, iReadChunk).OutStream().good() || iReadChunk < iChunk)
		{
			break;
		}
	}

	return iRead;
}


} // end of namespace dekaf2

