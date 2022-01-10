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
#include "kfilesystem.h"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#ifdef DEKAF2_HAS_LZMA_COMPRESSION
#include <boost/iostreams/filter/lzma.hpp>
#endif
#ifdef DEKAF2_HAS_ZSTD_COMPRESSION
#include <boost/iostreams/filter/zstd.hpp>
#endif

namespace dekaf2 {

namespace bio = boost::iostreams;

//-----------------------------------------------------------------------------
detail::KCompressionBase::COMPRESSION detail::KCompressionBase::GetCompressionMethodFromFilename(KStringView sFilename)
//-----------------------------------------------------------------------------
{
	KString sExt = kExtension(sFilename).ToLowerASCII();

	if (sExt == "gz" || sExt == "gzip" || sExt == "tgz")
	{
		return KCompressionBase::GZIP;
	}
	else if (sExt == "bz2" || sExt == "bzip2" || sExt == "tbz2" || sExt == "tbz")
	{
		return KCompressionBase::BZIP2;
	}
#ifdef DEKAF2_HAS_LZMA_COMPRESSION
	else if (sExt == "xz")
	{
		return KCompressionBase::LZMA;
	}
#endif
#ifdef DEKAF2_HAS_ZSTD_COMPRESSION
	else if (sExt == "zstd" || sExt == "zst")
	{
		return KCompressionBase::ZSTD;
	}
#endif
	else
	{
		return KCompressionBase::NONE;
	}

} // GetCompressionMethodFromFilename

//-----------------------------------------------------------------------------
bool KCompressOStream::open(KOutStream& TargetStream, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_TargetStream = &TargetStream;
	return CreateFilter(compression);

} // Open

//-----------------------------------------------------------------------------
bool KCompressOStream::open(KString& sTarget, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_KOutStringStream = std::make_unique<KOutStringStream>(sTarget);
	return open(*m_KOutStringStream, compression);

} // Open

//-----------------------------------------------------------------------------
bool KCompressOStream::CreateFilter(COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	compressor::reset();

	if (!m_TargetStream)
	{
		kWarning("no output stream");
		return false;
	}

	switch (compression)
	{
		case AUTO:
		case NONE:
			break;

		case GZIP:
			compressor::push(bio::gzip_compressor(bio::gzip_params(bio::gzip::default_compression)));
			break;

		case BZIP2:
			compressor::push(bio::bzip2_compressor());
			break;

		case ZLIB:
			compressor::push(bio::zlib_compressor(bio::zlib_params(bio::zlib::default_compression)));
			break;

#ifdef DEKAF2_HAS_LZMA_COMPRESSION
		case LZMA:
			compressor::push(bio::lzma_compressor(bio::lzma_params(bio::lzma::default_compression)));
			break;
#endif

#ifdef DEKAF2_HAS_ZSTD_COMPRESSION
		case ZSTD:
			compressor::push(bio::zstd_compressor(bio::zstd_params(bio::zstd::default_compression)));
			break;
#endif
	}

	compressor::push(m_TargetStream->OutStream());

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KCompressOStream::close()
//-----------------------------------------------------------------------------
{
	compressor::reset();

} // Close

//-----------------------------------------------------------------------------
bool KUnCompressIStream::open(KInStream& SourceStream, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_SourceStream = &SourceStream;
	return CreateFilter(compression);

} // Open

//-----------------------------------------------------------------------------
bool KUnCompressIStream::open(KStringView sSource, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_KInStringStream = std::make_unique<KInStringStream>(sSource);
	return open(*m_KInStringStream, compression);

} // Open

//-----------------------------------------------------------------------------
bool KUnCompressIStream::CreateFilter(COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	uncompressor::reset();

	if (!m_SourceStream)
	{
		kWarning("no input stream");
		return false;
	}

	switch (compression)
	{
		case AUTO:
		case NONE:
			break;

		case GZIP:
			uncompressor::push(bio::gzip_decompressor());
			break;

		case BZIP2:
			uncompressor::push(bio::bzip2_decompressor());
			break;

		case ZLIB:
			uncompressor::push(bio::zlib_decompressor());
			break;

#ifdef DEKAF2_HAS_LZMA_COMPRESSION
		case LZMA:
			uncompressor::push(bio::lzma_decompressor());
			break;
#endif

#ifdef DEKAF2_HAS_ZSTD_COMPRESSION
		case ZSTD:
			uncompressor::push(bio::zstd_decompressor());
			break;
#endif
	}

	uncompressor::push(m_SourceStream->InStream());

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KUnCompressIStream::close()
//-----------------------------------------------------------------------------
{
	uncompressor::reset();

} // Close

} // end of namespace dekaf2


