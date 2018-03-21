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

#pragma once

/// @file kchunkedtransfer.h
/// Implements chunked reader and writer as boost iostreams filters

#include <boost/iostreams/char_traits.hpp> // EOF, WOULD_BLOCK
#include <boost/iostreams/concepts.hpp>    // input_filter
#include <boost/iostreams/operations.hpp>  // get

#include "kstringutils.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KChunkedReader : public boost::iostreams::input_filter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	enum STATE
	{
		StartingUp,
		ReadingSize,
		SkipUntilEmptyLine,
		HadNonEmptyLine,
		IsNotChunked,
		ReadingChunk
	};

//------
public:
//------

	//-----------------------------------------------------------------------------
	template<typename Source>
	int get(Source& src)
	//-----------------------------------------------------------------------------
	{
		for (;;)
		{
			int c = boost::iostreams::get(src);

			if (c == EOF || c == boost::iostreams::WOULD_BLOCK)
			{
				return c;
			}

			switch (m_State)
			{
				case ReadingChunk:
				{
					if (!--m_iRemainingInChunk)
					{
						// switch state to next chunk header
						m_State = StartingUp;
					}

					return c;
				}

				case IsNotChunked:
					// if this is no chunked transfer just return the input
					return c;

				case StartingUp:
				case ReadingSize:
				{
					if (c == 0x0d)
					{
						// skip CR
					}
					else if (c == 0x0a)
					{
						// stop reading the size on LF
						if (m_iRemainingInChunk == 0)
						{
							if (m_State == StartingUp)
							{
								// we had not even seen a 0 ..
								// this is a protocol error
								m_State = IsNotChunked;
								return c;
							}

							// this is the last chunk of a series.
							// Stop skipping at the next empty line
							m_State = SkipUntilEmptyLine;
						}
						// this was a valid chunk header, now
						// switch to chunk reading mode
						m_State = ReadingChunk;
					}
					else
					{
						// convert next nibble
						auto iNibble = kFromHexChar(c);
						if (iNibble < 16)
						{
							m_iRemainingInChunk <<= 4;
							m_iRemainingInChunk += iNibble;
							m_State = ReadingSize;
						}
						else
						{
							m_State = IsNotChunked;
							// TODO try to put back the hex chars read so far
						}
					}
					break;
				}

				case SkipUntilEmptyLine:
				{
					if (c == 0x0d)
					{
						// skip CR
					}
					else if (c == 0x0a)
					{
						// this was an empty line
						m_State = StartingUp;
						return EOF;
					}
					else
					{
						m_State = HadNonEmptyLine;
					}
					break;
				}

				case HadNonEmptyLine:
				{
					if (c == 0x0a)
					{
						m_State = SkipUntilEmptyLine;
					}
					break;
				}

			} // switch
			
		} // for (;;)
	}

	//-----------------------------------------------------------------------------
	template<typename Source>
	void close(Source&)
	//-----------------------------------------------------------------------------
	{
		m_iRemainingInChunk = 0;
		m_State = StartingUp;
	}

//------
private:
//------

	size_t m_iRemainingInChunk { 0 };
	STATE m_State { StartingUp };

};

} // end of namespace dekaf2

