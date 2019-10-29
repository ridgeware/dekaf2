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

#include <iostream>
#include <fstream>
#include "kwriter.h"
#include "kreader.h"
#include "klog.h"

namespace dekaf2
{

KOutStream KErr(std::cerr);
KOutStream KOut(std::cout);

//-----------------------------------------------------------------------------
KOutStream::~KOutStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
/// Write a character. Returns stream reference that resolves to false on failure
KOutStream::self_type& KOutStream::Write(KString::value_type ch)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = OutStream().rdbuf();
	if (sb != nullptr)
	{
		typename std::ostream::int_type iCh = sb->sputc(ch);
		if (std::ostream::traits_type::eq_int_type(iCh, std::ostream::traits_type::eof()))
		{
			OutStream().setstate(std::ios_base::badbit);
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
/// Write a range of characters. Returns stream reference that resolves to false on failure
KOutStream::self_type& KOutStream::Write(const void* pAddress, size_t iCount)
//-----------------------------------------------------------------------------
{
	if (iCount)
	{
		std::streambuf* sb = OutStream().rdbuf();
		if (sb != nullptr)
		{
			auto iWrote = static_cast<size_t>(sb->sputn(static_cast<const std::ostream::char_type*>(pAddress), iCount));
			if (iWrote != iCount)
			{
				OutStream().setstate(std::ios_base::badbit);
			}
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns count of successfully read charcters.
KOutStream::self_type& KOutStream::Write(KInStream& Stream, size_t iCount)
//-----------------------------------------------------------------------------
{
	enum { COPY_BUFSIZE = 4096 };
	std::array<char, COPY_BUFSIZE> Buffer;

	for (;iCount;)
	{
		auto iChunk = std::min(static_cast<size_t>(COPY_BUFSIZE), iCount);

		auto iReadChunk = Stream.Read(Buffer.data(), iChunk);

		if (iReadChunk != iChunk)
		{
			break;
		}

		Write(Buffer.data(), iReadChunk);
		iCount -= iReadChunk;
	}

	return *this;
}

template class KWriter<std::ofstream>;

} // end of namespace dekaf2

