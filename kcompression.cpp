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
KCompressBase::KCompressBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
KCompressBase::KCompressBase(KString& sTarget)
//-----------------------------------------------------------------------------
{
	m_KOStringStream = std::make_unique<KOStringStream>(sTarget);
	m_TargetStream = &*m_KOStringStream;
}

//-----------------------------------------------------------------------------
KCompressBase::KCompressBase(std::ostream& TargetStream)
//-----------------------------------------------------------------------------
: m_TargetStream(&TargetStream)
{
}

//-----------------------------------------------------------------------------
KCompressBase::KCompressBase(KOutStream& TargetStream)
//-----------------------------------------------------------------------------
: m_TargetStream(&TargetStream.OutStream())
{
}

//-----------------------------------------------------------------------------
KCompressBase::~KCompressBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KCompressBase::SetOutput(std::ostream& TargetStream)
//-----------------------------------------------------------------------------
{
	m_TargetStream = &TargetStream;
	return m_TargetStream->good();
}

//-----------------------------------------------------------------------------
bool KCompressBase::SetOutput(KOutStream& TargetStream)
//-----------------------------------------------------------------------------
{
	return SetOutput(TargetStream.OutStream());
}

//-----------------------------------------------------------------------------
bool KCompressBase::SetOutput(KString& sTarget)
//-----------------------------------------------------------------------------
{
	m_KOStringStream = std::make_unique<KOStringStream>(sTarget);
	return SetOutput(*m_KOStringStream);
}

//-----------------------------------------------------------------------------
bool KCompressBase::Write(std::istream& InputStream)
//-----------------------------------------------------------------------------
{
	try
	{
		if (!m_Filter)
		{
			if (!CreateFilter())
			{
				return false;
			}
		}

		bio::copy(InputStream, *m_Filter);

		return true;
	}

	catch (const std::exception& e)
	{
		kException(e);
	}

	return false;
}

//-----------------------------------------------------------------------------
bool KCompressBase::Write(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KIStringStream istream(sInput);
	return Write(istream);
}

//-----------------------------------------------------------------------------
bool KCompressBase::Write(KInStream& InputStream)
//-----------------------------------------------------------------------------
{
	return Write(InputStream.InStream());
}

//-----------------------------------------------------------------------------
bool KCompressBase::CreateFilter()
//-----------------------------------------------------------------------------
{
	if (!m_TargetStream)
	{
		kWarning("no output stream");
		return false;
	}

	m_Filter = std::make_unique<streamfilter>();
	AddFilter(*m_Filter.get());
	m_Filter->push(*m_TargetStream);

	return true;
}

//-----------------------------------------------------------------------------
void KGZip::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::gzip_compressor(bio::gzip_params(bio::gzip::default_compression)));
}

//-----------------------------------------------------------------------------
void KGUnZip::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::gzip_decompressor());
}

//-----------------------------------------------------------------------------
void KBZip2::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::bzip2_compressor());
}

//-----------------------------------------------------------------------------
void KBUnZip2::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::bzip2_decompressor());
}

//-----------------------------------------------------------------------------
void KZlib::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::zlib_compressor(bio::zlib_params(bio::zlib::default_compression)));
}

//-----------------------------------------------------------------------------
void KUnZlib::AddFilter(streamfilter& stream)
//-----------------------------------------------------------------------------
{
	stream.push(bio::zlib_decompressor());
}

} // end of namespace dekaf2


