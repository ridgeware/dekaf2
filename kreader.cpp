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
bool KReader::ReadLine(KString& sLine, bool bOnlyText)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	if (!IsOpen())
	{
		return false;
	}


	for (;;)
	{
		KString::value_type Ch;
		if (!Read(Ch))
		{
			// the EOF case
			return !sLine.empty();
		}

		switch (Ch)
		{
			case '\r':
				if (!bOnlyText)
				{
					sLine += Ch;
				}
				break;

			case '\n':
				if (!bOnlyText)
				{
					sLine += Ch;
				}
				return true;

			default:
				sLine += Ch;
				break;
		}
	}
}

//-----------------------------------------------------------------------------
bool KReader::GetContent(KString& sContent, bool bIsText)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	if (!IsOpen())
	{
		return false;
	}

	// get size of the file.
	const auto iSize = GetSize();

	if (!iSize)
	{
		return true;
	}

	// create the read buffer
	sContent.resize(iSize);

	size_t iRead = Read(sContent.data(), iSize);

	if (iRead != iSize)
	{
		KLog().warning ("KReader: Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}

	if (bIsText) // dos2unix
	{
		sContent.Replace("\r\n", "\n", true);
	}

	return true;

} // GetContent



// KReader::const_iterator

//-----------------------------------------------------------------------------
KReader::const_iterator::const_iterator(KReader* it, bool bToEnd)
//-----------------------------------------------------------------------------
	: m_it(it)
{
	if (!bToEnd)
	{
		if (m_it->ReadLine(m_sBuffer))
		{
			++m_iCount;
		}
	}
}

//-----------------------------------------------------------------------------
KReader::const_iterator::const_iterator(const self_type& other)
//-----------------------------------------------------------------------------
	: m_it(other.m_it)
	, m_iCount(other.m_iCount)
	, m_sBuffer(other.m_sBuffer)
{
}

//-----------------------------------------------------------------------------
KReader::const_iterator::const_iterator(self_type&& other)
//-----------------------------------------------------------------------------
	: m_it(std::move(other.m_it))
	, m_iCount(std::move(other.m_iCount))
	, m_sBuffer(std::move(other.m_sBuffer))
{
}

//-----------------------------------------------------------------------------
KReader::const_iterator::self_type& KReader::const_iterator::operator=(const self_type& other)
//-----------------------------------------------------------------------------
{
	m_it = other.m_it;
	m_iCount = other.m_iCount;
	m_sBuffer = other.m_sBuffer;
	return *this;
}

//-----------------------------------------------------------------------------
KReader::const_iterator::self_type& KReader::const_iterator::operator=(self_type&& other)
//-----------------------------------------------------------------------------
{
	m_it = std::move(other.m_it);
	m_iCount = std::move(other.m_iCount);
	m_sBuffer = std::move(other.m_sBuffer);
	return *this;
}

//-----------------------------------------------------------------------------
KReader::const_iterator::self_type& KReader::const_iterator::operator++()
//-----------------------------------------------------------------------------
{
	if (m_it->ReadLine(m_sBuffer))
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
KReader::const_iterator::self_type KReader::const_iterator::operator++(int dummy)
//-----------------------------------------------------------------------------
{
	self_type i = *this;

	if (m_it->ReadLine(m_sBuffer))
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
KReader::const_iterator::reference KReader::const_iterator::operator*()
//-----------------------------------------------------------------------------
{
	return m_sBuffer;
}

//-----------------------------------------------------------------------------
KReader::const_iterator::pointer KReader::const_iterator::operator->()
//-----------------------------------------------------------------------------
{
	return &m_sBuffer;
}

//-----------------------------------------------------------------------------
bool KReader::const_iterator::operator==(const self_type& rhs)
//-----------------------------------------------------------------------------
{
	return m_it == rhs.m_it && m_iCount == rhs.m_iCount;
}

//-----------------------------------------------------------------------------
bool KReader::const_iterator::operator!=(const self_type& rhs)
//-----------------------------------------------------------------------------
{
	return m_it != rhs.m_it || m_iCount != rhs.m_iCount;
}


// KFILEReader

//-----------------------------------------------------------------------------
KFILEReader::~KFILEReader()
//-----------------------------------------------------------------------------
{
	Close();
}

//-----------------------------------------------------------------------------
bool KFILEReader::Open(KStringView svName)
//-----------------------------------------------------------------------------
{
	Close();

	KString sName(svName);

	m_File = fopen(sName.c_str(), "r");

	if (!m_File)
	{
		KLog().warning ("KFILEReader: Unable to open file '{0}': {1}", sName.s(), strerror(errno));
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KFILEReader::Open(FILE* fileptr)
//-----------------------------------------------------------------------------
{
	Close();

	m_File = fileptr;

	return IsOpen();
}

//-----------------------------------------------------------------------------
bool KFILEReader::Open(int filedesc)
//-----------------------------------------------------------------------------
{
	return Open(fdopen(filedesc, "r"));
}

//-----------------------------------------------------------------------------
void KFILEReader::Close()
//-----------------------------------------------------------------------------
{
	if (m_File)
	{
		fclose(m_File);
		m_File = nullptr;
	}
}

//-----------------------------------------------------------------------------
bool KFILEReader::Read(KString::value_type& ch)
//-----------------------------------------------------------------------------
{
	if (!m_File)
	{
		return false;
	}

	int iCh = fgetc(m_File);

	if (iCh == EOF)
	{
		return false;
	}

	ch = static_cast<KString::value_type>(iCh);

	return true;
}

//-----------------------------------------------------------------------------
size_t KFILEReader::Read(void* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	if (!m_File)
	{
		return 0;
	}

	size_t iRead = std::fread(pAddress, 1, iCount, m_File);

	if (iRead != iCount)
	{
		// could be an error, or just EOF - we want to log if it is not EOF
		if (std::ferror(m_File))
		{
			KLog().warning("KFILEReader: Unable to read file: {0}", strerror(errno));
		}
	}

	return iRead;
}

//-----------------------------------------------------------------------------
bool KFILEReader::IsOpen() const
//-----------------------------------------------------------------------------
{
	return m_File;
}

//-----------------------------------------------------------------------------
bool KFILEReader::IsEOF() const
//-----------------------------------------------------------------------------
{
	return !m_File || feof(m_File);
}

//-----------------------------------------------------------------------------
size_t KFILEReader::GetSize() const
//-----------------------------------------------------------------------------
{
	if (!m_File)
	{
		return 0;
	}

	int fd = fileno(m_File);

	struct stat buf;

	if (fstat(fd, &buf))
	{
		KLog().warning("KFILEReader: Unable to stat file: {0}", strerror(errno));
		return 0;
	}

	return static_cast<size_t>(buf.st_size);
}

} // end of namespace dekaf2

