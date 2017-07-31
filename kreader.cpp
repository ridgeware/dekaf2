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
#include <sys/stat.h>

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KReader_detail::Rewind(std::streambuf* Stream)
//-----------------------------------------------------------------------------
{
	return Stream && Stream->pubseekoff(0, std::ios_base::beg) != std::streambuf::pos_type(std::streambuf::off_type(-1));
}

//-----------------------------------------------------------------------------
ssize_t KReader_detail::GetSize(std::streambuf* Stream, bool bReposition)
//-----------------------------------------------------------------------------
{
	if (!Stream)
	{
		return std::streambuf::pos_type(std::streambuf::off_type(-1));
	}

	std::streambuf::pos_type curPos = Stream->pubseekoff(0, std::ios_base::cur);
	if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return curPos;
	}

	std::streambuf::pos_type endPos = Stream->pubseekoff(0, std::ios_base::end);
	if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return endPos;
	}

	if (bReposition && endPos != curPos)
	{
		Stream->pubseekoff(curPos, std::ios_base::beg);
	}

	return endPos;
}

//-----------------------------------------------------------------------------
ssize_t KReader_detail::GetRemainingSize(std::streambuf* Stream)
//-----------------------------------------------------------------------------
{
	if (!Stream)
	{
		return std::streambuf::pos_type(std::streambuf::off_type(-1));
	}

	std::streambuf::pos_type curPos = Stream->pubseekoff(0, std::ios_base::cur);
	if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return curPos;
	}

	std::streambuf::pos_type endPos = Stream->pubseekoff(0, std::ios_base::end);
	if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return endPos;
	}

	if (endPos != curPos)
	{
		Stream->pubseekoff(curPos, std::ios_base::beg);
	}

	return endPos - curPos;
}

//-----------------------------------------------------------------------------
//bool KReader::GetContent(KString& sContent, bool bIsText)
bool KReader_detail::ReadAll(std::streambuf* Stream, KString& sContent)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	if (!Stream)
	{
		return false;
	}

	// get size of the file.
	const auto iSize = GetSize(Stream, false);

	if (iSize < 0)
	{
		return false;
	}

	if (!iSize)
	{
		return true;
	}

	// position stream to the beginning
	if (!Rewind(Stream))
	{
		return false;
	}

	size_t uiSize = static_cast<size_t>(iSize);

	// create the read buffer
	sContent.resize(uiSize);

	size_t iRead = static_cast<size_t>(Stream->sgetn(sContent.data(), iSize));

	if (iRead != uiSize)
	{
		KLog().warning ("KReader: Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}
/*
	if (bIsText) // dos2unix
	{
		sContent.Replace("\r\n", "\n", true);
	}
*/
	return true;

} // ReadAll

//-----------------------------------------------------------------------------
bool KReader_detail::ReadRemaining(std::streambuf* Stream, KString& sContent)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	if (!Stream)
	{
		return false;
	}

	// get size of the file
	const auto iSize = GetRemainingSize(Stream);

	if (iSize < 0)
	{
		return false;
	}

	if (!iSize)
	{
		return true;
	}

	size_t uiSize = static_cast<size_t>(iSize);

	// create the read buffer
	sContent.resize(uiSize);

	size_t iRead = static_cast<size_t>(Stream->sgetn(sContent.data(), iSize));

	if (iRead != uiSize)
	{
		KLog().warning ("KReader: Unable to read remaining file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}
/*
	if (bIsText) // dos2unix
	{
		sContent.Replace("\r\n", "\n", true);
	}
*/
	return true;

} // ReadRemaining


namespace KReader_detail
{

//-----------------------------------------------------------------------------
bool ReadLine(std::streambuf* Stream, KString& sLine, KString::value_type delimiter, KStringView sTrimRight)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	if (!Stream)
	{
		return false;
	}

	for (;;)
	{
		std::streambuf::int_type iCh = Stream->sbumpc();
		if (std::streambuf::traits_type::eq_int_type(iCh, std::streambuf::traits_type::eof()))
		{
			// the EOF case
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

// KReader_detail::const_iterator

//-----------------------------------------------------------------------------
const_streambuf_iterator::const_streambuf_iterator(base_iterator it, bool bToEnd, KString::value_type chDelimiter, KStringView sTrimRight)
//-----------------------------------------------------------------------------
	: m_it(it)
    , m_chDelimiter(chDelimiter)
    , m_sTrimRight(sTrimRight)
{
	if (!bToEnd)
	{
		if (ReadLine(m_it, m_sBuffer, m_chDelimiter, m_sTrimRight))
		{
			++m_iCount;
		}
	}
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::const_streambuf_iterator(const self_type& other)
//-----------------------------------------------------------------------------
    : m_it(other.m_it)
    , m_iCount(other.m_iCount)
    , m_sBuffer(other.m_sBuffer)
    , m_chDelimiter(other.m_chDelimiter)
    , m_sTrimRight(other.m_sTrimRight)
{
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::const_streambuf_iterator(self_type&& other)
//-----------------------------------------------------------------------------
    : m_it(std::move(other.m_it))
    , m_iCount(std::move(other.m_iCount))
    , m_sBuffer(std::move(other.m_sBuffer))
    , m_chDelimiter(std::move(other.m_chDelimiter))
    , m_sTrimRight(std::move(other.m_sTrimRight))
{
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::self_type& const_streambuf_iterator::operator=(const self_type& other)
//-----------------------------------------------------------------------------
{
	m_it = other.m_it;
	m_iCount = other.m_iCount;
	m_sBuffer = other.m_sBuffer;
	m_chDelimiter = other.m_chDelimiter;
	m_sTrimRight = other.m_sTrimRight;
	return *this;
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::self_type& const_streambuf_iterator::operator=(self_type&& other)
//-----------------------------------------------------------------------------
{
	m_it = std::move(other.m_it);
	m_iCount = std::move(other.m_iCount);
	m_sBuffer = std::move(other.m_sBuffer);
	m_chDelimiter = std::move(other.m_chDelimiter);
	m_sTrimRight = std::move(other.m_sTrimRight);
	return *this;
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::self_type& const_streambuf_iterator::operator++()
//-----------------------------------------------------------------------------
{
	if (ReadLine(m_it, m_sBuffer, m_chDelimiter, m_sTrimRight))
	{
		++m_iCount;
	}
	else
	{
		m_iCount = 0;
	}

	return *this;
} // prefix

//-----------------------------------------------------------------------------
const_streambuf_iterator::self_type const_streambuf_iterator::operator++(int dummy)
//-----------------------------------------------------------------------------
{
	self_type i = *this;

	if (ReadLine(m_it, m_sBuffer, m_chDelimiter, m_sTrimRight))
	{
		++m_iCount;
	}
	else
	{
		m_iCount = 0;
	}

	return i;
} // postfix

//-----------------------------------------------------------------------------
const_streambuf_iterator::reference const_streambuf_iterator::operator*()
//-----------------------------------------------------------------------------
{
	return m_sBuffer;
}

//-----------------------------------------------------------------------------
const_streambuf_iterator::pointer const_streambuf_iterator::operator->()
//-----------------------------------------------------------------------------
{
	return &m_sBuffer;
}

//-----------------------------------------------------------------------------
bool const_streambuf_iterator::operator==(const self_type& rhs)
//-----------------------------------------------------------------------------
{
	return m_it == rhs.m_it && m_iCount == rhs.m_iCount;
}

//-----------------------------------------------------------------------------
bool const_streambuf_iterator::operator!=(const self_type& rhs)
//-----------------------------------------------------------------------------
{
	return m_it != rhs.m_it || m_iCount != rhs.m_iCount;
}

} // end of namespace KReader_detail

// KStringReader

} // end of namespace dekaf2

