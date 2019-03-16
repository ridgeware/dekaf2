/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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
//
*/

#include "kstreamparse.h"
#include "klog.h"
#include <algorithm>

namespace dekaf2 {

//-----------------------------------------------------------------------------
FDFile::FDFile(KStringViewZ sFilename, int mode)
//-----------------------------------------------------------------------------
: m_fd(open(sFilename.c_str(), mode))
{
	if (m_fd < 0)
	{
		kDebug(1, "cannot open {}: {}", sFilename, strerror(errno));
	}
}

//-----------------------------------------------------------------------------
FDFile::~FDFile()
//-----------------------------------------------------------------------------
{
	if (m_fd >= 0)
	{
		close(m_fd);
	}
}

//-----------------------------------------------------------------------------
/// construct from any istream
KStreamParser::KStreamParser(KInStream& istream, size_t iBufferSize)
//-----------------------------------------------------------------------------
	: m_istream(&istream.InStream())
	, m_buffer(std::make_unique<char[]>(iBufferSize))
	, m_start(m_buffer.get())
	, m_pos(m_start)
	, m_end(m_start)
	, m_iBufferSize(iBufferSize)
{
}

//-----------------------------------------------------------------------------
/// construct from a string view
KStreamParser::KStreamParser(KStringView sInput)
//-----------------------------------------------------------------------------
	: m_start(sInput.data())
	, m_pos(sInput.data())
	, m_end(sInput.data() + sInput.size())
	, m_bEOF(true) // we cannot read more
{
}

//-----------------------------------------------------------------------------
/// construct from a file descriptor
KStreamParser::KStreamParser(int fd, size_t iBufferSize)
//-----------------------------------------------------------------------------
	: m_buffer(std::make_unique<char[]>(iBufferSize))
	, m_start(m_buffer.get())
	, m_pos(m_start)
	, m_end(m_start)
	, m_iBufferSize(iBufferSize)
	, m_fd(fd)
{
}

//-----------------------------------------------------------------------------
std::istream::int_type KStreamParser::Fill()
//-----------------------------------------------------------------------------
{
	ssize_t iRead;

	if (!m_bEOF)
	{

		if (m_fd >= 0)
		{
			iRead = read(m_fd, m_buffer.get(), m_iBufferSize);

			if (iRead < 0)
			{
				kDebug(1, "read(): {}", strerror(errno));
				iRead = 0;
			}

			if (static_cast<size_t>(iRead) < m_iBufferSize)
			{
				m_bEOF = true;
			}
		}
		else
		{
			// we never get here for the stringview ctor because it sets m_bEOF = true
			m_istream->read(m_buffer.get(), m_iBufferSize);

			iRead = m_istream->gcount();

			if (static_cast<size_t>(iRead) < m_iBufferSize)
			{
				m_bEOF = true;
				m_istream->setstate(std::ios::eofbit);
			}
		}

		m_pos = m_start;
		m_end = m_start + iRead;

		if (DEKAF2_LIKELY(m_pos != m_end))
		{
			return *m_pos++;
		}
	}

	return std::istream::traits_type::eof();

} // Fill

//-----------------------------------------------------------------------------
KStringView KStreamParser::ReadMore(size_t iSize)
//-----------------------------------------------------------------------------
{
	if (m_bEOF)
	{
		return KStringView { m_pos, static_cast<size_t>(m_end - m_pos) };
	}

	if (iSize > m_iBufferSize)
	{
		kWarning("request exceeds max size of {}", m_iBufferSize);
		iSize = m_iBufferSize;
	}

	auto len = m_end - m_pos;

	if (len)
	{
		std::memmove(m_buffer.get(), m_pos, len);
	}

	m_pos = m_start;
	m_end = m_start + len;

	// now fill buffer

	auto iWant = m_iBufferSize - len;

	ssize_t iRead;

	if (m_fd >= 0)
	{
		iRead = read(m_fd, m_buffer.get() + len, iWant);

		if (iRead < 0)
		{
			kDebug(1, "read(): {}", strerror(errno));
			iRead = 0;
		}

		if (static_cast<size_t>(iRead) < iWant)
		{
			m_bEOF = true;
		}
	}
	else
	{
		m_istream->read(m_buffer.get() + len, iWant);

		iRead = m_istream->gcount();

		if (static_cast<size_t>(iRead) < iWant)
		{
			m_bEOF = true;
			m_istream->setstate(std::ios::eofbit);
		}
	}

	m_end += iRead;

	return KStringView{ m_pos, std::min(iSize, static_cast<size_t>(m_end - m_pos)) };

} // ReadMore

//-----------------------------------------------------------------------------
/// UnRead a character
bool KStreamParser::UnReadStreamBuf(size_t iSize)
//-----------------------------------------------------------------------------
{
	// we only get called when the internal buffer is too small for the unread
	// which means we can be sure it has to be emptied
	iSize -= m_pos - m_start;
	m_pos = m_start;
	m_end = m_start;

	if (m_fd >= 0)
	{
		// TODO seek back

		return !iSize;
	}

	if (!m_istream)
	{
		// this should never happen
		return !iSize;
	}

	std::streambuf* sb = m_istream->rdbuf();

	if (sb)
	{
		for (;iSize--;)
		{
			typename std::istream::int_type iCh = sb->sungetc();
			if (std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof()))
			{
				return false;
			}
		}
		return true;
	}

	return false;

} // UnReadStreamBuf

//-----------------------------------------------------------------------------
KStringView KStreamParser::ReadLine(KStringView::value_type delimiter, KStringView sRightTrim)
//-----------------------------------------------------------------------------
{
	KStringView sLine(m_pos, m_end - m_pos);

	auto pos = sLine.find(delimiter);

	if (pos == KStringView::npos)
	{
		if (m_bEOF)
		{
			m_pos = m_end;
			return sLine;
		}

		auto iOldSize = sLine.size();

		sLine = ReadMore(m_iBufferSize);

		pos = sLine.find(delimiter, iOldSize);

		if (pos == KStringView::npos)
		{
			m_pos = m_end;
			return sLine;
		}
	}

	++pos;
	m_pos += pos;
	sLine.remove_suffix(sLine.size() - pos);

	if (!sRightTrim.empty())
	{
		sLine.TrimRight(sRightTrim);
	}

	return sLine;

} // ReadLine

//-----------------------------------------------------------------------------
bool KStreamParser::ReadLine(KString& sBuffer, KStringView::value_type delimiter, KStringView sRightTrim)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (EndOfStream())
	{
		return false;
	}

	bool bComplete;

	do {

		auto sLine = ReadLine(delimiter, {});

		sBuffer += sLine;

		bComplete = sLine.back() == delimiter;

	} while (!bComplete && !EndOfStream());

	if (bComplete && !sRightTrim.empty())
	{
		sBuffer.TrimRight(sRightTrim);
	}

	return true;

} // ReadLine

} // end of namespace dekaf2
