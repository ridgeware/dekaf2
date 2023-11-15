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

#include "kbufferedreader.h"
#include "kstring.h"
#include "kstringutils.h"
#include "klog.h"
#include <algorithm>

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#endif

namespace dekaf2 {

//-----------------------------------------------------------------------------
KBufferedReader::~KBufferedReader()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KBufferedReader::ReadLine(KStringView& sLine, KStringView::value_type delimiter, KStringView sRightTrim)
//-----------------------------------------------------------------------------
{
	sLine.assign(m_Arena.pos, m_Arena.end);

	auto pos = sLine.find(delimiter);

	if (pos == KStringView::npos)
	{
		if (m_bEOF)
		{
			m_Arena.pos = m_Arena.end;
			return !sLine.empty();
		}

		auto iOldSize = sLine.size();

		sLine = ReadMore(m_Arena.iBufferSize);

		pos = sLine.find(delimiter, iOldSize);

		if (pos == KStringView::npos)
		{
			m_Arena.pos = m_Arena.end;
			return !sLine.empty();
		}
	}

	++pos;
	m_Arena.pos += pos;
	sLine.remove_suffix(sLine.size() - pos);

	if (!sRightTrim.empty())
	{
		sLine.TrimRight(sRightTrim);
	}

	return true;

} // ReadLine

//-----------------------------------------------------------------------------
bool KBufferedReader::ReadLine(KStringRef& sBuffer, KStringView::value_type delimiter, KStringView sRightTrim)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (DEKAF2_UNLIKELY(EndOfStream()))
	{
		return false;
	}

	bool bComplete;

	do {

		KStringView sLine;
		ReadLine(sLine, delimiter, {});

		sBuffer += sLine;

		bComplete = sLine.back() == delimiter;

	} while (!bComplete && !EndOfStream());

	// bComplete depends on the KStringView version of ReadLine, and has already
	// executed the right trim if not "complete"
	if (bComplete && !sRightTrim.empty())
	{
		kTrimRight(sBuffer, sRightTrim);
	}

	return true;

} // ReadLine

//-----------------------------------------------------------------------------
/// construct from any istream
KBufferedStreamReader::KBufferedStreamReader(KInStream& istream, size_t iBufferSize)
//-----------------------------------------------------------------------------
	: m_istream(&istream.istream())
	, m_buffer(std::make_unique<char[]>(iBufferSize))
{
	m_Arena = Arena(m_buffer.get(), iBufferSize);
}

//-----------------------------------------------------------------------------
std::istream::int_type KBufferedStreamReader::Fill()
//-----------------------------------------------------------------------------
{
	ssize_t iRead;

	if (DEKAF2_LIKELY(!m_bEOF))
	{
		m_istream->read(m_buffer.get(), m_Arena.iBufferSize);

		iRead = m_istream->gcount();

		if (DEKAF2_UNLIKELY(static_cast<size_t>(iRead) < m_Arena.iBufferSize))
		{
			m_bEOF = true;
			m_istream->setstate(std::ios::eofbit);
		}

		m_Arena.pos = m_Arena.start;
		m_Arena.end = m_Arena.start + iRead;

		if (DEKAF2_LIKELY(m_Arena.pos != m_Arena.end))
		{
			return *m_Arena.pos++;
		}
	}

	return std::istream::traits_type::eof();

} // Fill

//-----------------------------------------------------------------------------
KStringView KBufferedStreamReader::ReadMore(size_t iSize)
//-----------------------------------------------------------------------------
{
	if (m_bEOF)
	{
		return KStringView { m_Arena.pos, static_cast<size_t>(m_Arena.end - m_Arena.pos) };
	}

	if (DEKAF2_UNLIKELY(iSize > m_Arena.iBufferSize))
	{
		kWarning("request exceeds max size of {}", m_Arena.iBufferSize);
		iSize = m_Arena.iBufferSize;
	}

	auto len = m_Arena.end - m_Arena.pos;

	if (DEKAF2_LIKELY(len))
	{
		std::memmove(m_buffer.get(), m_Arena.pos, len);
	}

	m_Arena.pos = m_Arena.start;
	m_Arena.end = m_Arena.start + len;

	// now fill buffer

	auto iWant = m_Arena.iBufferSize - len;

	ssize_t iRead;

	m_istream->read(m_buffer.get() + len, iWant);

	iRead = m_istream->gcount();

	if (DEKAF2_UNLIKELY(static_cast<size_t>(iRead) < iWant))
	{
		m_bEOF = true;
		m_istream->setstate(std::ios::eofbit);
	}

	m_Arena.end += iRead;

	return KStringView { m_Arena.pos, std::min(iSize, static_cast<size_t>(m_Arena.end - m_Arena.pos)) };

} // ReadMore

//-----------------------------------------------------------------------------
/// UnRead a character
bool KBufferedStreamReader::UnReadStreamBuf(size_t iSize)
//-----------------------------------------------------------------------------
{
	// we only get called when the internal buffer is too small for the unread
	// which means we can be sure it has to be emptied
	iSize -= m_Arena.pos - m_Arena.start;
	m_Arena.pos = m_Arena.start;
	m_Arena.end = m_Arena.start;

	std::streambuf* sb = m_istream->rdbuf();

	if (DEKAF2_LIKELY(sb != nullptr))
	{
		for (;iSize--;)
		{
			typename std::istream::int_type iCh = sb->sungetc();
			if (DEKAF2_UNLIKELY(std::istream::traits_type::eq_int_type(iCh, std::istream::traits_type::eof())))
			{
				return false;
			}
		}
		return true;
	}

	return false;

} // UnReadStreamBuf

//-----------------------------------------------------------------------------
/// construct from a string view
KBufferedStringReader::KBufferedStringReader(KStringView sInput)
//-----------------------------------------------------------------------------
{
	m_Arena = Arena { sInput };
	m_bEOF = true;
}

//-----------------------------------------------------------------------------
/// construct from a file descriptor
KBufferedFileReader::KBufferedFileReader(int fd, size_t iBufferSize)
//-----------------------------------------------------------------------------
	: m_buffer(std::make_unique<char[]>(iBufferSize))
	, m_fd(fd)
{
	m_Arena = Arena { m_buffer.get(), iBufferSize } ;
}

//-----------------------------------------------------------------------------
/// construct from a file name
KBufferedFileReader::KBufferedFileReader(KStringViewZ sFilename, size_t iBufferSize)
//-----------------------------------------------------------------------------
	: m_buffer(std::make_unique<char[]>(iBufferSize))
	, m_fd(open(sFilename.c_str(), O_RDONLY | DEKAF2_CLOSE_ON_EXEC_FLAG))
	, m_bOwnsFileDescriptor(true)
{
	m_Arena = Arena { m_buffer.get(), iBufferSize } ;

	if (DEKAF2_UNLIKELY(m_fd < 0))
	{
		kDebug(1, "cannot open {}: {}", sFilename, strerror(errno));
	}

}

//-----------------------------------------------------------------------------
KBufferedFileReader::~KBufferedFileReader()
//-----------------------------------------------------------------------------
{
	if (m_bOwnsFileDescriptor)
	{
		if (close(m_fd))
		{
			kDebug(1, "cannot close: {}", strerror(errno));
		}
	}
}

//-----------------------------------------------------------------------------
std::istream::int_type KBufferedFileReader::Fill()
//-----------------------------------------------------------------------------
{
	ssize_t iRead;

	if (DEKAF2_LIKELY(!m_bEOF))
	{

#ifdef DEKAF2_IS_WINDOWS
		iRead = read(m_fd, m_buffer.get(), static_cast<unsigned int>(m_Arena.iBufferSize));
#else
		iRead = read(m_fd, m_buffer.get(), m_Arena.iBufferSize);
#endif

		if (DEKAF2_UNLIKELY(iRead < 0))
		{
			kDebug(1, "read(): {}", strerror(errno));
			iRead = 0;
		}

		if (DEKAF2_UNLIKELY(static_cast<size_t>(iRead) < m_Arena.iBufferSize))
		{
			m_bEOF = true;
		}

		m_Arena.pos = m_Arena.start;
		m_Arena.end = m_Arena.start + iRead;

		if (DEKAF2_LIKELY(m_Arena.pos != m_Arena.end))
		{
			return *m_Arena.pos++;
		}
	}

	return std::istream::traits_type::eof();

} // Fill

//-----------------------------------------------------------------------------
KStringView KBufferedFileReader::ReadMore(size_t iSize)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(m_bEOF))
	{
		return KStringView { m_Arena.pos, static_cast<size_t>(m_Arena.end - m_Arena.pos) };
	}

	if (DEKAF2_UNLIKELY(iSize > m_Arena.iBufferSize))
	{
		kWarning("request exceeds max size of {}", m_Arena.iBufferSize);
		iSize = m_Arena.iBufferSize;
	}

	auto len = m_Arena.end - m_Arena.pos;

	if (DEKAF2_LIKELY(len))
	{
		std::memmove(m_buffer.get(), m_Arena.pos, len);
	}

	m_Arena.pos = m_Arena.start;
	m_Arena.end = m_Arena.start + len;

	// now fill buffer

	auto iWant = m_Arena.iBufferSize - len;

	ssize_t iRead;

#ifdef DEKAF2_IS_WINDOWS
	iRead = read(m_fd, m_buffer.get() + len, static_cast<unsigned int>(iWant));
#else
	iRead = read(m_fd, m_buffer.get() + len, iWant);
#endif

	if (DEKAF2_UNLIKELY(iRead < 0))
	{
		kDebug(1, "read(): {}", strerror(errno));
		iRead = 0;
	}

	if (DEKAF2_UNLIKELY(static_cast<size_t>(iRead) < iWant))
	{
		m_bEOF = true;
	}

	m_Arena.end += iRead;

	return KStringView{ m_Arena.pos, std::min(iSize, static_cast<size_t>(m_Arena.end - m_Arena.pos)) };

} // ReadMore

//-----------------------------------------------------------------------------
/// UnRead a character
bool KBufferedFileReader::UnReadStreamBuf(size_t iSize)
//-----------------------------------------------------------------------------
{
	// we only get called when the internal buffer is too small for the unread
	// which means we can be sure it has to be emptied
	iSize -= m_Arena.pos - m_Arena.start;
	m_Arena.pos = m_Arena.start;
	m_Arena.end = m_Arena.start;

	if (DEKAF2_UNLIKELY(iSize))
	{
		kWarning("cannot unread size {}", iSize);
		// TODO: seek back
		return false;
	}

	return true;

} // UnReadStreamBuf

} // end of namespace dekaf2
