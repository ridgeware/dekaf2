/*
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

#include "kwriter.h"
#include "kreader.h"
#include "klog.h"
#include "kthreadpool.h"
#include "kurl.h"
#include "kstreambuf.h"
#include <iostream>
#include <fstream>

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

KOutStream KErr(std::cerr);
KOutStream KOut(std::cout);

//-----------------------------------------------------------------------------
std::ostream& kGetNullOStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<std::ostream> NullStream = std::make_unique<std::ostream>(&KNullStreamBuf::Create());
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
KOutStream& kGetNullOutStream()
//-----------------------------------------------------------------------------
{
	static std::unique_ptr<KOutStream> NullStream = std::make_unique<KOutStream>(kGetNullOStream(), "\n", true);
	return *NullStream.get();
}

//-----------------------------------------------------------------------------
/// value constructor
KOutStream::KOutStream(std::ostream& OutStream, KStringView sLineDelimiter, bool bImmutable)
//-----------------------------------------------------------------------------
	: m_OutStream     (&OutStream    )
	, m_sLineDelimiter(sLineDelimiter)
	, m_bImmutable    (bImmutable    )
{
}

//-----------------------------------------------------------------------------
/// Read a range of characters and append to Stream. Returns stream reference that resolves to false on failure.
KOutStream::self_type& KOutStream::Write(KInStream& Stream, size_t iCount)
//-----------------------------------------------------------------------------
{
	std::array<char, KDefaultCopyBufSize> Buffer;

	for (;iCount;)
	{
		auto iChunk = std::min(Buffer.size(), iCount);

		auto iReadChunk = Stream.Read(Buffer.data(), iChunk);

		if (!Write(Buffer.data(), iReadChunk).Good())
		{
			break;
		}
		
		iCount -= iReadChunk;

		if (iReadChunk != iChunk)
		{
			break;
		}
	}

	return *this;

} // Write

//-----------------------------------------------------------------------------
/// Fixates eol settings
KOutStream& KOutStream::SetWriterImmutable()
//-----------------------------------------------------------------------------
{
	m_bImmutable = true;
	return *this;

} // SetWriterImmutable

//-----------------------------------------------------------------------------
KOutStream& KOutStream::SetWriterEndOfLine(KStringView sLineDelimiter)
//-----------------------------------------------------------------------------
{
	if (m_bImmutable)
	{
		kDebug(2, "line delimiter is made immutable - cannot change to {}", sLineDelimiter);
	}
	else
	{
		m_sLineDelimiter = sLineDelimiter;
	}
	return *this;

} // SetWriterEndOfLine


template class KWriter<std::ofstream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KOutStream> kOpenOutStream(KStringViewZ sSchema, std::ios::openmode openmode)
//-----------------------------------------------------------------------------
{
	if (sSchema == KLog::STDOUT)
	{
		return std::make_unique<KOutStream>(std::cout);
	}
	else if (sSchema == KLog::STDERR)
	{
		return std::make_unique<KOutStream>(std::cerr);
	}
	else
	{
		return std::make_unique<KOutFile>(sSchema, openmode);
	}

} // kOpenOutStream

//-----------------------------------------------------------------------------
void kLogger(KOutStream& Stream, KString sMessage, bool bFlush)
//-----------------------------------------------------------------------------
{
	static KThreadPool LogWriter(1);

	LogWriter.push([](KOutStream* Stream, KString sMessage, bool bFlush)
	{
		Stream->WriteLine(sMessage);

		if (bFlush)
		{
			Stream->Flush();
		}

	}, &Stream, std::move(sMessage), bFlush);

} // kLogger

DEKAF2_NAMESPACE_END
