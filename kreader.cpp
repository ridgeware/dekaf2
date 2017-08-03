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

#include "kreader.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KIStreamBuf::~KIStreamBuf()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
std::streamsize KIStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	return m_Callback(s, n, m_CustomPointer);
}

/*
//-----------------------------------------------------------------------------
KIStreamBuf::int_type KIStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	char ch;
	m_Callback(&ch, 1, m_CustomPointer);
	return static_cast<int_type>(ch);
}
*/

//-----------------------------------------------------------------------------
KIStreamBuf::int_type KIStreamBuf::uflow()
//-----------------------------------------------------------------------------
{
	char ch;
	if (m_Callback(&ch, 1, m_CustomPointer) == 1)
	{
		return static_cast<int_type>(ch);
	}
	else
	{
		return traits_type::eof();
	}
}

/*
std::streamsize KIStreamBuf::showmanyc()
{
}
*/

//-----------------------------------------------------------------------------
bool kRewind(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::beg) != std::streambuf::pos_type(std::streambuf::off_type(-1));
}

//-----------------------------------------------------------------------------
ssize_t kGetSize(std::istream& Stream, bool bFromStart)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		return std::streambuf::pos_type(std::streambuf::off_type(-1));
	}

	std::streambuf::pos_type curPos = sb->pubseekoff(0, std::ios_base::cur);
	if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return curPos;
	}

	std::streambuf::pos_type endPos = sb->pubseekoff(0, std::ios_base::end);
	if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
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

//-----------------------------------------------------------------------------
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
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
		return false;
	}

	if (iSize < 0)
	{
		// We could not determine the input size - this might be a
		// minimalistic input stream buffer, or a non-seekable stream.
		// We will simply try to read blocks until we fail.
		enum { BUFSIZE = 2048 };
		char buf[BUFSIZE];

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
		KLog().debug (1, "kReadAll: stream grew during read, did not read all new content");
	}

	if (iRead != uiSize)
	{
		KLog().warning ("KReader: Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}

	return true;

} // ReadAll

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream, KString& sLine, KStringView sTrimRight, KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		Stream.setstate(std::ios_base::badbit);

		return false;
	}

	for (;;)
	{
		std::streambuf::int_type iCh = sb->sbumpc();
		if (std::streambuf::traits_type::eq_int_type(iCh, std::streambuf::traits_type::eof()))
		{
			// the EOF case
			Stream.setstate(std::ios_base::eofbit);

			bool bSuccess = !sLine.empty();

			if (!sTrimRight.empty())
			{
				sLine.TrimRight(sTrimRight);
			}

			return bSuccess;
		}

		KString::value_type Ch = static_cast<KString::value_type>(iCh);

		sLine += Ch;

		if (Ch == delimiter)
		{
			if (!sTrimRight.empty())
			{
				sLine.TrimRight(sTrimRight);
			}
			return true;
		}
	}
}


//-----------------------------------------------------------------------------
KBasicReader::const_kreader_line_iterator::const_kreader_line_iterator(base_iterator& it, bool bToEnd)
//-----------------------------------------------------------------------------
    : m_it(bToEnd ? nullptr : &it)
{
	if (m_it != nullptr)
	{
		if (!kReadLine(*(m_it->m_sRef), m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
		{
			m_it = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
KBasicReader::const_kreader_line_iterator::self_type& KBasicReader::const_kreader_line_iterator::operator++()
//-----------------------------------------------------------------------------
{
	if (m_it != nullptr)
	{
		if (!kReadLine(*(m_it->m_sRef), m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
		{
			m_it = nullptr;
		}
	}

	return *this;
} // prefix

//-----------------------------------------------------------------------------
KBasicReader::const_kreader_line_iterator::self_type KBasicReader::const_kreader_line_iterator::operator++(int)
//-----------------------------------------------------------------------------
{
	self_type i = *this;

	if (m_it != nullptr)
	{
		if (!kReadLine(*(m_it->m_sRef), m_sBuffer, m_it->m_sTrimRight, m_it->m_chDelimiter))
		{
			m_it = nullptr;
		}
	}

	return i;
} // postfix


//-----------------------------------------------------------------------------
KBasicReader::~KBasicReader()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
typename std::istream::int_type KBasicReader::Read()
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = m_sRef->rdbuf();
	if (sb)
	{
		typename std::istream::int_type iCh = sb->sbumpc();
		if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
		{
			m_sRef->setstate(std::ios::eofbit);
		}
		return iCh;
	}
	return std::istream::traits_type::eof();
}

//-----------------------------------------------------------------------------
/// Read a range of characters. Returns count of successfully read charcters.
size_t KBasicReader::Read(typename std::istream::char_type* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = m_sRef->rdbuf();
	if (sb)
	{
		size_t iRead = static_cast<size_t>(sb->sgetn(pAddress, static_cast<std::streamsize>(iCount)));
		if (iRead != iCount)
		{
			m_sRef->setstate(std::ios::eofbit);
		}
		return iRead;
	}
	return 0;
}


} // end of namespace dekaf2

