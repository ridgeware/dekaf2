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
KOutStream::Config::Config(KStringView sDelimiter, bool bImmutable, bool bIsStatic)
//-----------------------------------------------------------------------------
: m_sDelimiter   (sDelimiter)
, m_bIsImmutable (bImmutable)
, m_bIsStatic    (bIsStatic)
{
}

//-----------------------------------------------------------------------------
KOutStream::Config* KOutStream::Config::Create(KStringView sDelimiter, bool bImmutable)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView s_DefaultDelimiter = "\n";
	static constexpr KStringView s_HTTPDelimiter    = "\r\n";

	static Config s_Config_default   (s_DefaultDelimiter, false, true);
	static Config s_Config_http      (s_HTTPDelimiter   , false, true);
	static Config s_Config_immutable (s_DefaultDelimiter,  true, true);

	if (bImmutable)
	{
		if (sDelimiter == s_DefaultDelimiter) return &s_Config_immutable;
	}
	else
	{
		if (sDelimiter == s_DefaultDelimiter) return &s_Config_default;
		if (sDelimiter == s_HTTPDelimiter   ) return &s_Config_http;
	}

	return new Config(sDelimiter, bImmutable, false);

} // Config::Create

//-----------------------------------------------------------------------------
void KOutStream::Config::Delete(Config* Conf)
//-----------------------------------------------------------------------------
{
	if (Conf && !Conf->IsStatic())
	{
		delete Conf;
	}

} // Config::Delete

//-----------------------------------------------------------------------------
KOutStream::Config* KOutStream::Config::SetImmutable(Config* Conf)
//-----------------------------------------------------------------------------
{
	if (!Conf->IsImmutable())
	{
		if (Conf->IsStatic())
		{
			return Create(Conf->GetDelimiter(), true);
		}
		else
		{
			Conf->m_bIsImmutable = true;
		}
	}

	return Conf;

} // Config::SetImmutable

//-----------------------------------------------------------------------------
KOutStream::Config* KOutStream::Config::SetDelimiter(Config* Conf, KStringView sNewDelimiter)
//-----------------------------------------------------------------------------
{
	if (Conf->GetDelimiter() != sNewDelimiter)
	{
		if (Conf->IsImmutable())
		{
			kDebug(2, "class is made immutable - cannot change to {}", sNewDelimiter);
		}
		else
		{
			if (Conf->IsStatic())
			{
				return Create(sNewDelimiter, Conf->IsImmutable());
			}
			else
			{
				Conf->m_sDelimiter = sNewDelimiter;
			}
		}
	}

	return Conf;

} // Config::SetDelimiter

//-----------------------------------------------------------------------------
KOutStream::KOutStream(std::ostream& OutStream, KStringView sLineDelimiter, bool bImmutable)
//-----------------------------------------------------------------------------
	: m_OutStream (&OutStream)
	, m_Config    (Config::Create(sLineDelimiter, bImmutable))
{
}

//-----------------------------------------------------------------------------
KOutStream::KOutStream(self_type&& other)
//-----------------------------------------------------------------------------
{
	this->m_OutStream = other.m_OutStream;
	this->m_Config    = other.m_Config;
	other.m_OutStream = nullptr;
	other.m_Config    = nullptr;
}

//-----------------------------------------------------------------------------
KOutStream& KOutStream::operator=(KOutStream&& other)
//-----------------------------------------------------------------------------
{
	Config::Delete(this->m_Config);
	this->m_OutStream = other.m_OutStream;
	this->m_Config    = other.m_Config;
	other.m_OutStream = nullptr;
	other.m_Config    = nullptr;
	return *this;
}

//-----------------------------------------------------------------------------
KOutStream::~KOutStream()
//-----------------------------------------------------------------------------
{
	Config::Delete(m_Config);

} // dtor

//-----------------------------------------------------------------------------
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
