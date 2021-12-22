/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

#include "kchunkedtransfer.h"
#include "kstringutils.h"
#include "klog.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
KChunkedSource::KChunkedSource(KInStream& src, bool bIsChunked, std::streamsize iContentLen, std::streamsize* pCount)
//-----------------------------------------------------------------------------
    : m_src         { src         }
    , m_iContentLen { iContentLen }
    , m_pCount      { pCount      }
    , m_State       { bIsChunked ? StartingUp : IsNotChunked }
{
} // ctor

//-----------------------------------------------------------------------------
KChunkedSource::~KChunkedSource()
//-----------------------------------------------------------------------------
{
	if (m_pCount)
	{
		*m_pCount = m_iCount;
	}

} // dtor

//-----------------------------------------------------------------------------
std::streamsize KChunkedSource::read(char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	std::streamsize iResult{0};

	if (n <= 0)
	{
		kDebug(1, "invalid read request of size {} deferred", n);
		return !n ? 0 : -1;
	}

	if (m_State == Finished)
	{
		return -1;
	}

	for (;;)
	{
		int c;

		if (m_State != ReadingChunk && m_State != IsNotChunked)
		{
			c = m_src.Read();

			if (c == EOF)
			{
				return iResult;
			}

			++m_iCount;
		}
		else
		{
			// gcc would warn otherwise
			c = 0;
		}

		switch (m_State)
		{
			default:
			case IsNotChunked:
			{
				// if this is no chunked transfer just return the input
				auto len = n;

				if (m_iContentLen >= 0)
				{
					len = std::min(m_iContentLen, n);
				}

				iResult = m_src.Read(s, len);

				if (iResult <= 0)
				{
					// make sure that any following requests get an end of stream..
					m_State = Finished;
				}

				m_iCount += iResult;

				if (m_iContentLen >= 0)
				{
					if (m_iContentLen >= iResult)
					{
						m_iContentLen -=  iResult;
					}
					else
					{
						m_iContentLen = 0;
					}
				}

				return iResult;
			}

			case ReadingChunk:
			{
				if (iResult == n)
				{
					return iResult;
				}

				auto len = std::min(n - iResult, m_iRemainingInChunk);

				auto ird = m_src.Read(s + iResult, len);

				m_iCount += ird;
				m_iRemainingInChunk -= ird;
				iResult += ird;

				if (!m_iRemainingInChunk)
				{
					// switch state to chunk end
					m_State = ReadingChunkEnd;
				}
				else if (ird <= 0)
				{
					kDebug(1, "cannot read from stream, return {} instead of {} bytes", iResult, n);
					// our stream got foul
					m_State = Finished;
					return iResult;
				}

				break;
			}

			case ReadingChunkEnd:
			{
				if (c == CARRIAGE_RETURN)
				{
					// skip CR
				}
				else if (c == LINEFEED)
				{
					// switch state to next chunk header
					m_State = StartingUp;
				}
				else
				{
					kDebug(1, "invalid chunk end");
					// this is a protocol failure
					m_State = IsNotChunked;
					return iResult;
				}
				break;
			}

			case StartingUp:
			case ReadingSize:
			{
				if (c == CARRIAGE_RETURN)
				{
					// skip CR
				}
				else if (c == LINEFEED)
				{
					// stop reading the size on LF
					if (m_iRemainingInChunk == 0)
					{
						if (m_State == StartingUp)
						{
							// we had not even seen a 0 ..
							kDebug(1, "invalid chunk start, switching to unchunked transfer");
							// this is a protocol error
							m_State = IsNotChunked;
							return iResult;
						}

						// this is the last chunk of a series.
						// Stop skipping at the next empty line
						m_State = SkipUntilEmptyLine;
					}
					else
					{
						// this was a valid chunk header, now
						// switch to chunk reading mode
						m_State = ReadingChunk;
					}
				}
				else
				{
					// convert next nibble
					auto iNibble = kFromBase36(c); // NOLINT: conversion from int to char is OK for hex chars

					if (iNibble <= MAX_HEX)
					{
						m_iRemainingInChunk <<= 4;
						m_iRemainingInChunk += iNibble;
						m_State = ReadingSize;
					}
					else
					{
						kDebug(1, "invalid chunk start, not a hex character - transfer will fail");
						m_State = IsNotChunked;
						// TODO: try to put back the hex chars read so far (or throw)
					}
				}
				break;
			}

			case SkipUntilEmptyLine:
			{
				if (c == CARRIAGE_RETURN)
				{
					// skip CR
				}
				else if (c == LINEFEED)
				{
					// this was an empty line
					m_State = Finished;
					return iResult;
				}
				else
				{
					m_State = HadNonEmptyLine;
				}
				break;
			}

			case HadNonEmptyLine:
			{
				if (c == LINEFEED)
				{
					m_State = SkipUntilEmptyLine;
				}
				break;
			}

		} // switch

	} // for (;;)

} // read

//-----------------------------------------------------------------------------
KString KChunkedSource::read()
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	std::array<char, KDefaultCopyBufSize> buffer;

	for (;;)
	{
		auto iRead = read(buffer.data(), buffer.size());

		if (iRead > 0)
		{
			sBuffer.append(buffer.data(), iRead);
		}

		if (iRead < static_cast<std::streamsize>(buffer.size()))
		{
			break;
		}
	}

	return sBuffer;

} // read

//-----------------------------------------------------------------------------
KChunkedSink::KChunkedSink(KOutStream& sink, bool bIsChunked, std::streamsize* pCount)
//-----------------------------------------------------------------------------
    : m_sink       { sink       }
    , m_pCount     { pCount     }
    , m_bIsChunked { bIsChunked }
{
} // ctor

//-----------------------------------------------------------------------------
KChunkedSink::~KChunkedSink()
//-----------------------------------------------------------------------------
{
	if (m_pCount)
	{
		*m_pCount = m_iCount;
	}

} // dtor

//-----------------------------------------------------------------------------
std::streamsize KChunkedSink::write(const char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	if (n <= 0 || !s)
	{
		return 0;
	}

	if (m_bIsChunked)
	{
		// write the chunk header
		auto sHeader = kFormat("{}\r\n", KString::to_hexstring(n));
		m_iCount += sHeader.size();
		m_sink.Write(sHeader);
	}

	m_sink.Write(s, n);
	m_iCount += n;

	if (m_bIsChunked)
	{
		// write end of chunk
		m_sink.Write("\r\n");
		m_iCount += 2;
	}

	return n;

} // write

//-----------------------------------------------------------------------------
void KChunkedSink::close()
//-----------------------------------------------------------------------------
{
	if (m_bIsChunked)
	{
		// write end of chunking header
		m_sink.Write("0\r\n\r\n");
		m_bIsChunked = false;
		m_iCount += 5;
	}

} // close

} // end of namespace dekaf2


