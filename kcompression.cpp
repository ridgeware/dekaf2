/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#include "kcompression.h"
#include "klog.h"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

namespace dekaf2 {

namespace bio = boost::iostreams;

//-----------------------------------------------------------------------------
KCompress::KCompress(KString& sTarget)
//-----------------------------------------------------------------------------
{
	m_KOStringStream = std::make_unique<KOStringStream>(sTarget);
	m_TargetStream = &*m_KOStringStream;

} // ctor

//-----------------------------------------------------------------------------
KCompress::KCompress(std::ostream& TargetStream)
//-----------------------------------------------------------------------------
: m_TargetStream(&TargetStream)
{
}

//-----------------------------------------------------------------------------
KCompress::KCompress(KOutStream& TargetStream)
//-----------------------------------------------------------------------------
: m_TargetStream(&TargetStream.OutStream())
{
}

//-----------------------------------------------------------------------------
KCompress::~KCompress()
//-----------------------------------------------------------------------------
{
	Close();

} // dtor

//-----------------------------------------------------------------------------
bool KCompress::SetOutput(std::ostream& TargetStream)
//-----------------------------------------------------------------------------
{
	m_TargetStream = &TargetStream;
	return m_TargetStream->good();

} // SetOutput

//-----------------------------------------------------------------------------
bool KCompress::SetOutput(KOutStream& TargetStream)
//-----------------------------------------------------------------------------
{
	return SetOutput(TargetStream.OutStream());

} // SetOutput

//-----------------------------------------------------------------------------
bool KCompress::SetOutput(KString& sTarget)
//-----------------------------------------------------------------------------
{
	m_KOStringStream = std::make_unique<KOStringStream>(sTarget);
	return SetOutput(*m_KOStringStream);

} // SetOutput

//-----------------------------------------------------------------------------
bool KCompress::Write(std::istream& InputStream)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	if (!m_Filter)
	{
		if (!CreateFilter())
		{
			return false;
		}
	}

	// This operator<< is actually not trying to format.
	*m_Filter << InputStream.rdbuf();

	return true;
	DEKAF2_LOG_EXCEPTION

	return false;

} // Write

//-----------------------------------------------------------------------------
bool KCompress::Write(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KIStringStream istream(sInput);
	return Write(istream);

} // Write

//-----------------------------------------------------------------------------
bool KCompress::Write(KInStream& InputStream)
//-----------------------------------------------------------------------------
{
	return Write(InputStream.InStream());

} // Write

//-----------------------------------------------------------------------------
bool KCompress::CreateFilter()
//-----------------------------------------------------------------------------
{
	if (!m_TargetStream)
	{
		kWarning("no output stream");
		return false;
	}

	m_Filter = std::make_unique<streamfilter>();

	switch (m_compression)
	{
		case NONE:
			break;

		case GZIP:
			m_Filter->push(bio::gzip_compressor(bio::gzip_params(bio::gzip::default_compression)));
			break;

		case BZIP2:
			m_Filter->push(bio::bzip2_compressor());
			break;

		case ZLIB:
			m_Filter->push(bio::zlib_compressor(bio::zlib_params(bio::zlib::default_compression)));
			break;

	}

	m_Filter->push(*m_TargetStream);

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KCompress::Close()
//-----------------------------------------------------------------------------
{
	m_Filter.reset();

} // Close

//-----------------------------------------------------------------------------
KUnCompress::KUnCompress(KStringView sSource)
//-----------------------------------------------------------------------------
{
	m_KIStringStream = std::make_unique<KIStringStream>(sSource);
	m_SourceStream = &*m_KIStringStream;

} // ctor

//-----------------------------------------------------------------------------
KUnCompress::KUnCompress(std::istream& SourceStream)
//-----------------------------------------------------------------------------
: m_SourceStream(&SourceStream)
{
}

//-----------------------------------------------------------------------------
KUnCompress::KUnCompress(KInStream& SourceStream)
//-----------------------------------------------------------------------------
: m_SourceStream(&SourceStream.InStream())
{
}

//-----------------------------------------------------------------------------
KUnCompress::~KUnCompress()
//-----------------------------------------------------------------------------
{
	Close();

} // dtor

//-----------------------------------------------------------------------------
bool KUnCompress::SetInput(std::istream& SourceStream)
//-----------------------------------------------------------------------------
{
	m_SourceStream = &SourceStream;
	return m_SourceStream->good();

} // SetInput

//-----------------------------------------------------------------------------
bool KUnCompress::SetInput(KInStream& SourceStream)
//-----------------------------------------------------------------------------
{
	return SetInput(SourceStream.InStream());

} // SetInput

//-----------------------------------------------------------------------------
bool KUnCompress::SetInput(KStringView sSource)
//-----------------------------------------------------------------------------
{
	m_KIStringStream = std::make_unique<KIStringStream>(sSource);
	return SetInput(*m_KIStringStream);

} // SetInput

//-----------------------------------------------------------------------------
size_t KUnCompress::Read(std::ostream& OutputStream, size_t iCount)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	if (!m_Filter)
	{
		if (!CreateFilter())
		{
			return 0;
		}
	}

	if (iCount == KString::npos)
	{
		// read until eof
		// This operator<< is actually not trying to format.
		OutputStream << m_Filter->rdbuf();
	}
	else
	{
		enum { COPY_BUFSIZE = 4096 };
		char sBuffer[COPY_BUFSIZE];

		auto* sb = m_Filter->rdbuf();

		if (!sb)
		{
			return 0;
		}

		size_t iRead { 0 };

		for (;iCount > iRead;)
		{
			auto iChunk = std::min(static_cast<size_t>(COPY_BUFSIZE), iCount - iRead);

			auto iReadChunk = sb->sgetn(sBuffer, iChunk);

			if (iReadChunk <= 0)
			{
				m_Filter->setstate(std::ios::eofbit);
				break;
			}

			OutputStream.write(sBuffer, iReadChunk);
			iRead += iReadChunk;
		}

		return iRead;
	}

	return iCount;
	DEKAF2_LOG_EXCEPTION

	return 0;

} // Read

//-----------------------------------------------------------------------------
size_t KUnCompress::Read(KString& sOutput, size_t len)
//-----------------------------------------------------------------------------
{
	KOStringStream ostream(sOutput);
	return Read(ostream, len);

} // Read

//-----------------------------------------------------------------------------
size_t KUnCompress::Read(KOutStream& OutputStream, size_t len)
//-----------------------------------------------------------------------------
{
	return Read(OutputStream.OutStream(), len);

} // Read

//-----------------------------------------------------------------------------
bool KUnCompress::CreateFilter()
//-----------------------------------------------------------------------------
{
	if (!m_SourceStream)
	{
		kWarning("no input stream");
		return false;
	}

	m_Filter = std::make_unique<streamfilter>();

	switch (m_compression)
	{
		case NONE:
			break;

		case GZIP:
			m_Filter->push(bio::gzip_decompressor());
			break;

		case BZIP2:
			m_Filter->push(bio::bzip2_decompressor());
			break;

		case ZLIB:
			m_Filter->push(bio::zlib_decompressor());
			break;

	}

	m_Filter->push(*m_SourceStream);

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KUnCompress::Close()
//-----------------------------------------------------------------------------
{
	m_Filter.reset();

} // Close

} // end of namespace dekaf2


